#include <gst/gst.h>
#include <gst/audio/audio.h>
#include <string.h>

#define CHUNK_SIZE 1024   /* Amount of bytes we are sending in each buffer */
#define SAMPLE_RATE 44100 /* Samples per second we are sending */

/* Structure to contain all our information, so we can pass it to callbacks */
typedef struct _CustomData
{
    GstElement *pipeline,                                                    //
        *app_src, *tee,                                                      //
        *audio_queue, *audio_convert1, *audio_resample, *audio_sink,         //
        *video_queue, *audio_convert2, *visual, *video_convert, *video_sink, //
        *app_queue, *app_sink;                                               //
    guint64 num_samples;                                                     /* Number of samples generated so far (for timestamp generation) */
    gfloat a, b, c, d;                                                       /* For waveform generation */
    guint sourceid;                                                          /* To control the GSource */
    GMainLoop *main_loop;                                                    /* GLib's Main Loop */
} CustomData;

/**
 * This method is called by the idle GSource in the mainloop, to feed CHUNK_SIZE bytes into appsrc.
 * The idle handler is added to the mainloop when appsrc requests us to start sending data (need-data signal)
 * and is removed when appsrc has enough data (enough-data signal).
 *
 * 实现数据生成 -> 触发appsrc元素的`push-buffer`事件 -> 传递生成的数据
 *
 */
static gboolean
push_data(CustomData *data)
{
    GstBuffer *buffer;
    GstFlowReturn ret;
    int i;
    GstMapInfo map;
    gint16 *raw;
    gint num_samples = CHUNK_SIZE / 2; /* Because each sample is 16 bits */
    gfloat freq;

    /* Create a new empty buffer */
    /** 创建(new)结构体并分配(alloc)指定大小的数据区（ GstBuffer的数据结构 + 结构体的数据字段*pool ） */
    buffer = gst_buffer_new_and_alloc(CHUNK_SIZE);

    /* Set its timestamp and duration */
    /** 将缓冲区看作是GstBuffer结构体指针，在指定的字段写入数据 */
    /** 
     * gst_util_uint64_scale(a,b,c) 使用uint64完成缩放计算: res=a*b/c
     * 
     * 比如将A∈[0,2PI] => 映射到 B∈[0,MAX_INT32]
     * 可以写成：
     * B= A / 2PI * MAX_INT32 
     *  = A * MAX_INT32 / 2PI   (尽量先乘上大数再除小数，避免精度丢失)
     * 
     * 于是可以调用：
     *  B = gst_util_uint64_scale(A,MAX_INT32,2PI)
     * 
     */
    // 每个缓冲区都附有计时器和持续时间，描述了缓冲区内容应该被解码、渲染或播放的时刻。
    // buffer->pts =  采样计数 * 单位时间(ns) / 采样率(hz)
    // 注：单位时间/采样率=每个采样的时间长度，所以pts=总采样计数*每个采样时间长度=总时间（ns）
    GST_BUFFER_TIMESTAMP(buffer) = gst_util_uint64_scale(data->num_samples, GST_SECOND, SAMPLE_RATE);
    // buffer->duration = 本缓冲区的采样个数 * 单位时间(ns) / 采样率(hz)
    // 注：duration= 本缓冲区的采样个数 * 每个采样的时间长度 = 本缓冲区数据的耗时（ns）
    GST_BUFFER_DURATION(buffer) = gst_util_uint64_scale(num_samples, GST_SECOND, SAMPLE_RATE);

    // 将buffer结构体重新映射到map结构体（以可写入方式）
    gst_buffer_map(buffer, &map, GST_MAP_WRITE);
    // 获取数据区指针
    raw = (gint16 *)map.data;

    /* Generate some psychodelic waveforms */
    // ################## 开始生成波形 ##################
    data->c += data->d;
    data->d -= data->c / 1000;
    freq = 1100 + 1000 * data->d;
    for (i = 0; i < num_samples; i++) // 生成采样
    {
        data->a += data->b;
        data->b -= data->a / freq;
        raw[i] = (gint16)(500 * data->a);
    }
    // ################## 结束生成波形 ##################
    // 解除内存映射
    gst_buffer_unmap(buffer, &map);
    data->num_samples += num_samples; // 总采样计数器自增

    /* Push the buffer into the appsrc */
    // 触发app_source的push-buffer事件（简单理解为函数调用），传输buffer数据。
    g_signal_emit_by_name(data->app_src, "push-buffer", buffer, &ret);

    /* Free the buffer now that we are done with it */
    // 释放buffer缓冲区内存
    gst_buffer_unref(buffer);

    // 返回值判断
    if (ret != GST_FLOW_OK)
    {
        /* We got some error, stop sending data */
        return FALSE;
    }
    return TRUE;
}

/**
 * This signal callback triggers when appsrc needs data. Here, we add an idle handler
 * to the mainloop to start pushing data into the appsrc
 * 该函数为appsrc元素need-data事件触发后调用，
 * 即appsrc元素需要数据，调用该函数开始推送数据
 */
static void
start_feed(GstElement *source, guint size, CustomData *data)
{
    if (data->sourceid == 0)
    {
        g_print("Start feeding\n");
        // g_idle_add是属于glib库的函数，用于注册了一个GLib空闲函数。
        // 会在GMainLoop主循环空闲时候执行（push_data）。
        data->sourceid = g_idle_add((GSourceFunc)push_data, data);
    }
}

/** This callback triggers when appsrc has enough data and we can stop sending.
 * We remove the idle handler from the mainloop
 *
 * 该函数为appsrc元素enough-data事件触发后调用，
 * 即appsrc元素不再需要数据时，触发调用该函数停止推送数据
 *
 * */
static void
stop_feed(GstElement *source, CustomData *data)
{
    if (data->sourceid != 0)
    {
        g_print("Stop feeding\n");
        g_source_remove(data->sourceid);
        data->sourceid = 0;
    }
}

/* The appsink has received a buffer */
static GstFlowReturn
new_sample(GstElement *sink, CustomData *data)
{
    GstSample *sample;

    /* Retrieve the buffer */
    g_signal_emit_by_name(sink, "pull-sample", &sample);
    if (sample)
    {
        /* The only thing we do in this example is print a * to indicate a received buffer */
        g_print("*");
        gst_sample_unref(sample);
        return GST_FLOW_OK;
    }
    return GST_FLOW_ERROR;
}

/* This function is called when an error message is posted on the bus */
static void
error_cb(GstBus *bus, GstMessage *msg, CustomData *data)
{
    GError *err;
    gchar *debug_info;

    /* Print error details on the screen */
    gst_message_parse_error(msg, &err, &debug_info);
    g_printerr("Error received from element %s: %s\n", GST_OBJECT_NAME(msg->src), err->message);
    g_printerr("Debugging information: %s\n", debug_info ? debug_info : "none");
    g_clear_error(&err);
    g_free(debug_info);
    g_main_loop_quit(data->main_loop);
}
int main(int argc, char *argv[])
{
    CustomData data;
    GstPad *tee_pad_1, *tee_pad_2, *tee_pad_3;
    GstPad *queue_audio_pad, *queue_video_pad, *queue_app_pad;
    GstAudioInfo info;
    GstCaps *audio_caps;
    GstBus *bus;

    /* Initialize custom data structure */
    memset(&data, 0, sizeof(data));
    data.b = 1; /* For waveform generation */
    data.d = 1;

    /* Initialize GStreamer */
    // 初始化
    gst_init(&argc, &argv);

    /* Create the elements */
    // 创建元素
    data.app_src = gst_element_factory_make("appsrc", "audio_source");
    data.tee = gst_element_factory_make("tee", "tee");
    data.audio_queue = gst_element_factory_make("queue", "audio_queue");
    data.audio_convert1 = gst_element_factory_make("audioconvert", "audio_convert1");
    data.audio_resample = gst_element_factory_make("audioresample", "audio_resample");
    data.audio_sink = gst_element_factory_make("autoaudiosink", "audio_sink");
    data.video_queue = gst_element_factory_make("queue", "video_queue");
    data.audio_convert2 = gst_element_factory_make("audioconvert", "audio_convert2");
    data.visual = gst_element_factory_make("wavescope", "visual");
    data.video_convert = gst_element_factory_make("videoconvert", "video_convert");
    data.video_sink = gst_element_factory_make("autovideosink", "video_sink");
    data.app_queue = gst_element_factory_make("queue", "app_queue");
    data.app_sink = gst_element_factory_make("appsink", "app_sink");

    /* Create the empty pipeline */
    // 创建管道
    data.pipeline = gst_pipeline_new("test-pipeline");
    if (!data.pipeline                                                                                          //
        || !data.app_src || !data.tee                                                                           //
        || !data.audio_queue || !data.audio_convert1 || !data.audio_resample || !data.audio_sink                //
        || !data.video_queue || !data.audio_convert2 || !data.visual || !data.video_convert || !data.video_sink //
        || !data.app_queue || !data.app_sink                                                                    //
    )
    {
        g_printerr("Not all elements could be created.\n");
        return -1;
    }

    /* Configure wavescope */
    // 配置波形生成的元素
    g_object_set(data.visual, "shader", 0, "style", 0, NULL);

    /* Configure appsrc */
    // 配置appsrc元素
    // 生成caps属性
    gst_audio_info_set_format(&info, GST_AUDIO_FORMAT_S16, SAMPLE_RATE, 1, NULL);
    audio_caps = gst_audio_info_to_caps(&info);
    // 配置appsrc元素的属性
    g_object_set(data.app_src,
                 "caps", audio_caps,        // 能力描述 appsrc.caps={type:audio/x-raw,format:s16,rate:44100,channels:1}
                 "format", GST_FORMAT_TIME, // 格式为时间(根据提供的纳秒级时间戳来安排缓冲区的播放顺序和时间)
                 NULL                       // 结束标志
    );
    // 设置appsrc元素的事件回调函数。
    // 在appsrc内部队列数据不足或快满时分别被触发。
    // GStreamer有单独线程处理流，appsrc的数据来源一般也是是其他线程，并不一定来自主线程。
    // 数据的消耗一般也是由程序单独管理，甚至由于数据消耗足够快而不需要处理"enough-data"信号。
    g_signal_connect(data.app_src, "need-data", G_CALLBACK(start_feed), &data);
    g_signal_connect(data.app_src, "enough-data", G_CALLBACK(stop_feed), &data);

    /* Configure appsink */
    // 配置appsink元素属性
    g_object_set(data.app_sink,
                 "emit-signals", TRUE, // 允许发出例如`new-sample`的信号(appsink的emit-signals属性的默认值为false)
                 "caps", audio_caps,   // 能力描述
                 NULL                  //
    );
    // 配置appsink元素的事件回调函数
    g_signal_connect(data.app_sink, "new-sample", G_CALLBACK(new_sample), &data);

    // 释放caps能力描述结构体
    gst_caps_unref(audio_caps);

    /* Link all elements that can be automatically linked because they have "Always" pads */
    // 将所有元素添加到管道
    gst_bin_add_many(
        GST_BIN(data.pipeline),                                                                  //
        data.app_src, data.tee,                                                                  //
        data.audio_queue, data.audio_convert1, data.audio_resample, data.audio_sink,             //
        data.video_queue, data.audio_convert2, data.visual, data.video_convert, data.video_sink, //
        data.app_queue, data.app_sink,                                                           //
        NULL                                                                                     //
    );
    // 连接元素
    // app_src ->   tee
    //              tee.src_1 -> audio_queue -> audio_convert1 -> audio_resample -> audio_sink
    //              tee.src_2 -> video_queue -> audio_convert2 -> visual -> video_convert -> video_sink
    //              tee.src_3 -> app_queue -> app_sink
    if (gst_element_link_many(data.app_src, data.tee, NULL) != TRUE ||
        gst_element_link_many(data.audio_queue, data.audio_convert1, data.audio_resample, data.audio_sink, NULL) != TRUE ||
        gst_element_link_many(data.video_queue, data.audio_convert2, data.visual, data.video_convert, data.video_sink, NULL) != TRUE ||
        gst_element_link_many(data.app_queue, data.app_sink, NULL) != TRUE)
    {
        g_printerr("Elements could not be linked.\n");
        gst_object_unref(data.pipeline);
        return -1;
    }

    /* Manually link the Tee, which has "Request" pads */
    // 获取queue.sink_pad
    queue_audio_pad = gst_element_get_static_pad(data.audio_queue, "sink");
    queue_video_pad = gst_element_get_static_pad(data.video_queue, "sink");
    queue_app_pad = gst_element_get_static_pad(data.app_queue, "sink");
    // 获取tee.src_pad
    tee_pad_1 = gst_element_request_pad_simple(data.tee, "src_%u");
    tee_pad_2 = gst_element_request_pad_simple(data.tee, "src_%u");
    tee_pad_3 = gst_element_request_pad_simple(data.tee, "src_%u");
    // print msg
    g_print("Obtained request pad %s for audio branch.\n", gst_pad_get_name(tee_pad_1));
    g_print("Obtained request pad %s for video branch.\n", gst_pad_get_name(tee_pad_2));
    g_print("Obtained request pad %s for app branch.\n", gst_pad_get_name(tee_pad_3));

    // 手动连接pad
    // tee.pad_1 -> queue_audio.pad
    // tee.pad_2 -> queue_video.pad
    // tee.pad_3 -> queue_app.pad
    if (gst_pad_link(tee_pad_1, queue_audio_pad) != GST_PAD_LINK_OK ||
        gst_pad_link(tee_pad_2, queue_video_pad) != GST_PAD_LINK_OK ||
        gst_pad_link(tee_pad_3, queue_app_pad) != GST_PAD_LINK_OK)
    {
        g_printerr("Tee could not be linked\n");
        gst_object_unref(data.pipeline);
        return -1;
    }
    gst_object_unref(queue_audio_pad);
    gst_object_unref(queue_video_pad);
    gst_object_unref(queue_app_pad);

    /* Instruct the bus to emit signals for each received message, and connect to the interesting signals */
    bus = gst_element_get_bus(data.pipeline); // 获取管道总线
    gst_bus_add_signal_watch(bus);            // 给总线添加信号(事件)监听
    // 给总线添加事件(error)处理回调函数
    g_signal_connect(G_OBJECT(bus), "message::error", (GCallback)error_cb, &data);
    gst_object_unref(bus);

    /* Start playing the pipeline */
    // 开始播放
    gst_element_set_state(data.pipeline, GST_STATE_PLAYING);
    /* Create a GLib Main Loop and set it to run */
    // 创建glib主循环
    // 上面的事件回调函数绑定、事件触发，都发生在默认上下文。
    // 这里创建主循环，上下文为NULL，代表默认上下文。
    // 主循环的作用是等待事件触发、调用绑定的事件处理函数。
    // （参考nodejs的事件处理机制，也是有一个循环）
    data.main_loop = g_main_loop_new(NULL, FALSE);
    // 运行主循环，实际上也起到了阻塞的作用。
    // 程序在其他地方通过调用g_main_loop_quit退出主循环
    g_main_loop_run(data.main_loop);

    /* Release the request pads from the Tee, and unref them */
    // 释放手动申请的tee.src_pad_[1,2,3]
    gst_element_release_request_pad(data.tee, tee_pad_1);
    gst_element_release_request_pad(data.tee, tee_pad_2);
    gst_element_release_request_pad(data.tee, tee_pad_3);
    // 释放手动申请的tee.src_pad_[1,2,3] 结构体
    gst_object_unref(tee_pad_1);
    gst_object_unref(tee_pad_2);
    gst_object_unref(tee_pad_3);

    /* Free resources */
    // 修改并释放管道状态
    gst_element_set_state(data.pipeline, GST_STATE_NULL);
    gst_object_unref(data.pipeline);
    return 0;
}