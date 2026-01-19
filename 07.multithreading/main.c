#include <gst/gst.h>

int main(int argc, char *argv[])
{
    GstElement *pipeline, *audio_source, *tee, *audio_queue, *audio_convert, *audio_resample, *audio_sink;
    GstElement *video_queue, *visual, *video_convert, *video_sink;
    GstBus *bus;
    GstMessage *msg;
    GstPad *tee_audio_pad, *tee_video_pad;
    GstPad *queue_audio_pad, *queue_video_pad;

    /* Initialize GStreamer */
    gst_init(&argc, &argv);

    /* Create the elements */
    /**
     * GStreamer是一个多线程的框架，在内部，它根据需要创建和销毁线程
     * - 插件可以自由创建线程来处理它们的任务，如视频解码器可以创建四个线程以充分利用CPU的四个核。
     * - 可以利用queue元素使得pipeline的不同分支运行在不同的线程上实现音频和视频的同时解码播放。
     * 
     * 转换element(audioconvert，audioresample和videoconvert)是必须的，可以保证pipeline可以正确地连接
     * 音频和视频元素的sink的Caps是由硬件确定的，如果audiotestsrc和wavescope的Caps能够匹配，
     * 这些element的行为就类似于直通——对信号不做任何修改，这对于性能的影响基本可以忽略不计。
     */
    audio_source = gst_element_factory_make("audiotestsrc", "audio_source");
    tee = gst_element_factory_make("tee", "tee");
    audio_queue = gst_element_factory_make("queue", "audio_queue");
    audio_convert = gst_element_factory_make("audioconvert", "audio_convert");
    audio_resample = gst_element_factory_make("audioresample", "audio_resample");
    audio_sink = gst_element_factory_make("autoaudiosink", "audio_sink");
    video_queue = gst_element_factory_make("queue", "video_queue");
    visual = gst_element_factory_make("wavescope", "visual"); // 消费一个音频信号并且将它渲染成波形，简易的示波器
    video_convert = gst_element_factory_make("videoconvert", "csp");
    video_sink = gst_element_factory_make("autovideosink", "video_sink");

    /* Create the empty pipeline */
    pipeline = gst_pipeline_new("test-pipeline");

    if (!pipeline || !audio_source || !tee || !audio_queue || !audio_convert || !audio_resample || !audio_sink ||
        !video_queue || !visual || !video_convert || !video_sink)
    {
        g_printerr("Not all elements could be created.\n");
        return -1;
    }

    /* Configure elements */
    g_object_set(audio_source, "freq", 215.0f, NULL);
    g_object_set(visual, "shader", 0, "style", 1, NULL);

    /* Link all elements that can be automatically linked because they have "Always" pads */
    gst_bin_add_many(GST_BIN(pipeline), audio_source, tee, audio_queue, audio_convert, audio_resample, audio_sink,
                     video_queue, visual, video_convert, video_sink, NULL);

    /**
     * 链接元素
     * pipeline:
     * - audio_source(线程1) -> tee
     *                          tee.src_0 -> audio_queue(线程2) -> audio_convert -> audio_resample -> audio_sink
     *                          tee.src_1 -> video_queue(线程3) -> visual -> video_convert -> video_sink
     * - queue元素会创建新的线程
     * - 整条pipeline有三个线程：主线程（线程1）、线程2、线程3
     */
    if (
        gst_element_link_many(audio_source, tee, NULL) != TRUE ||
        gst_element_link_many(audio_queue, audio_convert, audio_resample, audio_sink, NULL) != TRUE ||
        gst_element_link_many(video_queue, visual, video_convert, video_sink, NULL) != TRUE)
    {
        g_printerr("Elements could not be linked.\n");
        gst_object_unref(pipeline);
        return -1;
    }

    /* Manually link the Tee, which has "Request" pads */
    /**
     * 手动为tee元素申请`src pad`
     * 
     * `tee`元素的`src pad`是`Request Pads`, 这类pad是按需创建的，需要手动创建。
     * （实际上可以用`gst_element_link_many`直接连接，但这会导致无法手动释放`src pad`）
     */
    // 获取 audio_queue.sink 具名插槽
    // 获取 video_queue.sink 具名插槽
    queue_audio_pad = gst_element_get_static_pad(audio_queue, "sink");
    queue_video_pad = gst_element_get_static_pad(video_queue, "sink");
    // 手动申请 tee.src_0
    // 手动申请 tee.src_1
    tee_audio_pad = gst_element_request_pad_simple(tee, "src_%u");
    tee_video_pad = gst_element_request_pad_simple(tee, "src_%u");
    g_print("Obtained request pad %s for audio branch.\n", gst_pad_get_name(tee_audio_pad));
    g_print("Obtained request pad %s for video branch.\n", gst_pad_get_name(tee_video_pad));

    /**
     * 手动连接tee元素
     * tee.src_0 -> audio_queue
     * tee.src_1 -> video_queue
     */
    if (gst_pad_link(tee_audio_pad, queue_audio_pad) != GST_PAD_LINK_OK ||
        gst_pad_link(tee_video_pad, queue_video_pad) != GST_PAD_LINK_OK)
    {
        g_printerr("Tee could not be linked.\n");
        gst_object_unref(pipeline);
        return -1;
    }
    gst_object_unref(queue_audio_pad); // 释放结构体内存(并不会释放pad)
    gst_object_unref(queue_video_pad); // 释放结构体内存(并不会释放pad)

    /* Start playing the pipeline */
    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    /* Wait until error or EOS */
    // 阻塞直到ERROR或EOS事件触发
    bus = gst_element_get_bus(pipeline);
    msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, GST_MESSAGE_ERROR | GST_MESSAGE_EOS);

    /* Release the request pads from the Tee, and unref them */
    gst_element_release_request_pad(tee, tee_audio_pad); // 释放申请的tee.src_0 pad插槽
    gst_element_release_request_pad(tee, tee_video_pad); // 释放申请的tee.src_1 pad插槽
    gst_object_unref(tee_audio_pad);                     // 释放结构体内存
    gst_object_unref(tee_video_pad);                     // 释放结构体内存

    /* Free resources */
    if (msg != NULL)
        gst_message_unref(msg);
    gst_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);

    gst_object_unref(pipeline);
    return 0;
}