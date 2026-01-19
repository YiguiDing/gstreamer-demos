#include <gst/gst.h>

/* Structure to contain all our information, so we can pass it to callbacks */
// 用结构体包含所有元素为结构体，以便传递给回调函数。
typedef struct _CustomData
{
    GstElement *pipeline;
    GstElement *source;

    GstElement *videoconvert;
    GstElement *videosink;

    GstElement *audioconvert;
    GstElement *audioresample;
    GstElement *audiosink;

} CustomData;

/* Handler for the pad-added signal */
static void
pad_added_handler(GstElement *src, GstPad *pad, CustomData *data);
int main(int argc, char *argv[])
{
    CustomData data;
    GstBus *bus;
    GstMessage *msg;
    GstStateChangeReturn ret;
    gboolean terminate = FALSE;

    /* Initialize GStreamer */
    gst_init(&argc, &argv);

    /* Create the elements */
    /* 创建元素 */
    // uri解码元素
    data.source = gst_element_factory_make("uridecodebin", "source");
    // 视频处理相关元素
    data.videoconvert = gst_element_factory_make("videoconvert", "videoconvert"); /* 视频格式转换 */
    data.videosink = gst_element_factory_make("autovideosink", "videosink");      /* 视频播放 */
    // 音频处理相关元素
    /* audioconvert 转换格式以匹配前后音频编解码器的能力差异（不同平台下） */
    data.audioconvert = gst_element_factory_make("audioconvert", "audioconvert");
    /* audioresample 重新采样以匹配前后音频编解码器的不同采样率 */
    data.audioresample = gst_element_factory_make("audioresample", "resample");
    /* autoaudiosink 把音频流渲染到声卡上 */
    data.audiosink = gst_element_factory_make("autoaudiosink", "audiosink");

    /* Create the empty pipeline */
    /* 创建空管道 */
    data.pipeline = gst_pipeline_new("test-pipeline");
    if (
        !data.pipeline                                                  // pileline
        || !data.source                                                 // source
        || !data.videoconvert || !data.videosink                        // video
        || !data.audioconvert || !data.audioresample || !data.audiosink // audio
    )
    {
        g_printerr("Not all elements could be created.\n");
        return -1;
    }

    /* Build the pipeline. Note that we are NOT linking the source at this
     * point. We will do it later. */
    /**
     * 构建管道。
     * 将所有元素统一添加到管道管理
     */
    gst_bin_add_many(
        GST_BIN(data.pipeline),                                // pileline
        data.source,                                           // source
        data.videoconvert, data.videosink,                     // video
        data.audioconvert, data.audioresample, data.audiosink, // audio
        NULL);
    /**
     * 连接元素
     * source
     *  - pad:src_0 -> videoconvert -> videosink
     *  - pad:src_1 -> audioconvert -> audioresample -> audiosink
     *
     * 由于源元素uridecodebin的source_pad是在当收到一些数据后才会自动动态创建，
     * 所以这里无法连接，只能后续等待pad-added信号在回调函数中连接。
     */
    if (
        !gst_element_link_many(data.videoconvert, data.videosink, NULL)                        // video
        || !gst_element_link_many(data.audioconvert, data.audioresample, data.audiosink, NULL) // audio
    )
    {
        g_printerr("Elements could not be linked.\n");
        gst_object_unref(data.pipeline);
        return -1;
    }

    /* Set the URI to play */
    g_object_set(data.source, "uri", "https://www.freedesktop.org/software/gstreamer-sdk/data/media/sintel_trailer-480p.webm", NULL);

    /* Connect to the pad-added signal */
    /**
     * 绑定事件处理函数
     *
     * - 当收到pad-added信号时，执行回调连接src_pad和后续元素
     * - 查看所有支持的所有信号: `gst-inspect-1.0 uridecodebin`
     */
    g_signal_connect(data.source, "pad-added", G_CALLBACK(pad_added_handler), &data);

    /* Start playing */
    ret = gst_element_set_state(data.pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE)
    {
        g_printerr("Unable to set the pipeline to the playing state.\n");
        gst_object_unref(data.pipeline);
        return -1;
    }

    /* Listen to the bus */
    /**
     * 事件循环
     *  - 收到ERROR和EOS消息终止循环退出程序
     *  - 收到State_changed消息打印输出
     */
    bus = gst_element_get_bus(data.pipeline);
    do
    {
        msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE,
                                         GST_MESSAGE_STATE_CHANGED | GST_MESSAGE_ERROR | GST_MESSAGE_EOS);

        /* Parse message */
        if (msg != NULL)
        {
            GError *err;
            gchar *debug_info;
            switch (GST_MESSAGE_TYPE(msg))
            {
            case GST_MESSAGE_ERROR:
                gst_message_parse_error(msg, &err, &debug_info);
                g_printerr("Error received from element %s: %s\n", GST_OBJECT_NAME(msg->src), err->message);
                g_printerr("Debugging information: %s\n", debug_info ? debug_info : "none");
                g_clear_error(&err);
                g_free(debug_info);
                terminate = TRUE;
                break;
            case GST_MESSAGE_EOS:
                g_print("End-Of-Stream reached.\n");
                terminate = TRUE;
                break;
            case GST_MESSAGE_STATE_CHANGED:
                /* We are only interested in state-changed messages from the pipeline */
                /** 仅输出来自管道的消息 */
                if (GST_MESSAGE_SRC(msg) == GST_OBJECT(data.pipeline))
                {
                    GstState old_state, new_state, pending_state;
                    gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
                    g_print("Pipeline state changed from %s to %s:\n",
                            gst_element_state_get_name(old_state), gst_element_state_get_name(new_state));
                }
                break;
            default:
                /* We should not reach here */
                g_printerr("Unexpected message received.\n");
                break;
            }
            gst_message_unref(msg);
        }
    } while (!terminate);

    /* Free resources */
    gst_object_unref(bus);
    gst_element_set_state(data.pipeline, GST_STATE_NULL);
    gst_object_unref(data.pipeline);
    return 0;
}

/* This function will be called by the pad-added signal */
/**
 * 回调处理函数
 */
static void
pad_added_handler(GstElement *src, GstPad *new_pad, CustomData *data)
{
    // 获取目标pad
    GstPad *video_sink_pad = gst_element_get_static_pad(data->videoconvert, "sink");
    GstPad *audio_sink_pad = gst_element_get_static_pad(data->audioconvert, "sink");
    GstPadLinkReturn ret;
    GstCaps *new_pad_caps = NULL;
    GstStructure *new_pad_struct = NULL;
    const gchar *new_pad_type = NULL;

    // 打印输出 源pad名称 和 源element名称
    g_print("Received new pad '%s' from '%s':\n", GST_PAD_NAME(new_pad), GST_ELEMENT_NAME(src));

    /* If our converter is already linked, we have nothing to do here */
    // 检测目标pad是否已经被连接
    if (gst_pad_is_linked(video_sink_pad) && gst_pad_is_linked(audio_sink_pad))
    {
        g_print("We are already linked. Ignoring.\n");
        goto exit;
    }

    /* Check the new pad's type */
    /** 检查当前pad支持的所有capabilities能力（GstCaps[]） */
    new_pad_caps = gst_pad_get_current_caps(new_pad);
    /** 获取所有capabilities能力的第一个capability（GstCaps结构体） */
    new_pad_struct = gst_caps_get_structure(new_pad_caps, 0);
    /** 获取capability能力（GstCaps结构体）的name属性 */
    new_pad_type = gst_structure_get_name(new_pad_struct);
    /** 根据源pad支持的能力名称分别链接后续元素 */
    if (g_str_has_prefix(new_pad_type, "video/x-raw"))
    {
        /* Attempt the link */
        // 顺序必须是src_pad -> sink_pad
        ret = gst_pad_link(new_pad, video_sink_pad);
        if (GST_PAD_LINK_FAILED(ret))
            g_print("Type is '%s' but link failed.\n", new_pad_type);
        else
            g_print("Link succeeded (type '%s').\n", new_pad_type);
    }
    else if (g_str_has_prefix(new_pad_type, "audio/x-raw"))
    {
        /* Attempt the link */
        ret = gst_pad_link(new_pad, audio_sink_pad);
        if (GST_PAD_LINK_FAILED(ret))
            g_print("Type is '%s' but link failed.\n", new_pad_type);
        else
            g_print("Link succeeded (type '%s').\n", new_pad_type);
    }
    else
    {
        g_print("It has type '%s' which is not support. Ignoring.\n", new_pad_type);
    }

exit:
    /* Unreference the new pad's caps, if we got them */
    if (new_pad_caps != NULL)
        gst_caps_unref(new_pad_caps);

    /* Unreference the sink pad */
    gst_object_unref(video_sink_pad);
    gst_object_unref(audio_sink_pad);
}