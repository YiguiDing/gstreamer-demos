// playback-tutorial-1.c
#include <gst/gst.h>
#include <stdio.h>

/* Structure to contain all our information, so we can pass it around */
typedef struct _CustomData
{
    GstElement *playbin; /* Our one and only element */

    gint n_video; /* Number of embedded video streams */
    gint n_audio; /* Number of embedded audio streams */
    gint n_text;  /* Number of embedded subtitle streams */

    gint current_video; /* Currently playing video stream */
    gint current_audio; /* Currently playing audio stream */
    gint current_text;  /* Currently playing subtitle stream */

    GMainLoop *main_loop; /* GLib's Main Loop */
} CustomData;

/* playbin flags */
typedef enum
{
    GST_PLAY_FLAG_VIDEO = (1 << 0), /* We want video output */
    GST_PLAY_FLAG_AUDIO = (1 << 1), /* We want audio output */
    GST_PLAY_FLAG_TEXT = (1 << 2)   /* We want subtitle output */
} GstPlayFlags;

/* Forward definition for the message and keyboard processing functions */
static gboolean handle_message(GstBus *bus, GstMessage *msg, CustomData *data);
static gboolean handle_keyboard(GIOChannel *source, GIOCondition cond, CustomData *data);

int main(int argc, char *argv[])
{
    CustomData data;
    GstBus *bus;
    GstStateChangeReturn ret;
    gint flags;
    GIOChannel *io_stdin;

    /* Initialize GStreamer */
    gst_init(&argc, &argv);

    /* Create the elements */
    data.playbin = gst_element_factory_make("playbin", "playbin");

    if (!data.playbin)
    {
        g_printerr("Not all elements could be created.\n");
        return -1;
    }

    /* Set the URI to play */
    g_object_set(data.playbin, "uri", "https://www.freedesktop.org/software/gstreamer-sdk/data/media/sintel_cropped_multilingual.webm", NULL);

    /* Set flags to show Audio and Video but ignore Subtitles */
    g_object_get(data.playbin, "flags", &flags, NULL); // 获取flags默认值
    // 使用音频和视频，取消字幕，其他值保留默认值
    flags |= GST_PLAY_FLAG_VIDEO | GST_PLAY_FLAG_AUDIO; // 修改flag
    flags &= ~GST_PLAY_FLAG_TEXT;
    g_object_set(data.playbin, "flags", flags, NULL); // 写入flags

    /* Set connection speed. This will affect some internal decisions of playbin */
    // Network connection speed in kbps. Default: (0 = unknown)
    // 告知playbin当前网络连接的最大速度，playbin会选择最合适版本的视频流。
    g_object_set(data.playbin, "connection-speed", 56, NULL); // 56kbps

    /* Add a bus watch, so we get notified when a message arrives */
    bus = gst_element_get_bus(data.playbin);
    gst_bus_add_watch(bus, (GstBusFunc)handle_message, &data); // 给playbin元素的总线添加监听器

    /* Add a keyboard watch so we get notified of keystrokes */
    // 从标准输入创建GIOChannel
#ifdef G_OS_WIN32
    io_stdin = g_io_channel_win32_new_fd(fileno(stdin));
#else
    io_stdin = g_io_channel_unix_new(fileno(stdin));
#endif

    // 绑定输入事件的处理函数。
    // 主要在这里处理用户输入并实时修改播放的音频流
    g_io_add_watch(io_stdin, G_IO_IN, (GIOFunc)handle_keyboard, &data);

    /* Start playing */
    // 设置状态为播放
    ret = gst_element_set_state(data.playbin, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE)
    {
        g_printerr("Unable to set the pipeline to the playing state.\n");
        gst_object_unref(data.playbin);
        return -1;
    }

    /* Create a GLib Main Loop and set it to run */
    // 创建并运行主函数
    data.main_loop = g_main_loop_new(NULL, FALSE); // 创建glib主循环
    g_main_loop_run(data.main_loop);               // 运行主循环，阻塞，直到主循环break

    /* Free resources */
    // 释放资源
    g_main_loop_unref(data.main_loop);
    g_io_channel_unref(io_stdin);
    gst_object_unref(bus);
    gst_element_set_state(data.playbin, GST_STATE_NULL);
    gst_object_unref(data.playbin);
    return 0;
}

/* Extract some metadata from the streams and print it on the screen */
static void analyze_streams(CustomData *data)
{
    gint idx;
    GstTagList *tags;
    gchar *str;
    guint rate;

    /* Read some properties */
    g_object_get(data->playbin, "n-video", &data->n_video, NULL); // number of videos 获取视频流数
    g_object_get(data->playbin, "n-audio", &data->n_audio, NULL); // number of audios 获取音频流数
    g_object_get(data->playbin, "n-text", &data->n_text, NULL);   // number of text 获取字幕流数

    g_print("%d video stream(s), %d audio stream(s), %d text stream(s)\n",
            data->n_video, //
            data->n_audio, //
            data->n_text   //
    );

    g_print("\n");
    for (idx = 0; idx < data->n_video; idx++)
    {
        tags = NULL;
        /* Retrieve the stream's video tags */
        // 获取第idx个视频流的所有标签(metadata)
        g_signal_emit_by_name(data->playbin, "get-video-tags", idx, &tags);
        if (tags)
        {
            g_print("video stream %d:\n", idx);
            // 获取并打印输出视频编码器名称
            if (gst_tag_list_get_string(tags, GST_TAG_VIDEO_CODEC, &str))
            {
                g_print("\t codec: %s\n", str); // 打印输出：codec: On2 VP8
                g_free(str);                    // 释放内存
                gst_tag_list_free(tags);        // 释放内存
            }
        }
    }

    g_print("\n");
    for (idx = 0; idx < data->n_audio; idx++)
    {
        tags = NULL;
        /* Retrieve the stream's audio tags */
        // 获取第idx个音频流的所有标签
        g_signal_emit_by_name(data->playbin, "get-audio-tags", idx, &tags);
        if (tags)
        {
            g_print("audio stream %d:\n", idx);
            // 获取音频编码器名称
            if (gst_tag_list_get_string(tags, GST_TAG_AUDIO_CODEC, &str))
            {
                g_print("\t codec: %s\n", str); // 打印输出 案例：codec: Vorbis
                g_free(str);                    // 释放内存
            }
            // 获取语言缩写代码，如: en es pt
            if (gst_tag_list_get_string(tags, GST_TAG_LANGUAGE_CODE, &str))
            {
                g_print("\t language: %s\n", str); // 打印输出 案例：language: pt
                g_free(str);                       // 释放内存
            }
            // 获取比特率
            if (gst_tag_list_get_uint(tags, GST_TAG_BITRATE, &rate))
            {
                g_print("\t bitrate: %d\n", rate); // 打印输出 案例：bitrate: 160000
                // 注：rate是局部变量，不需要释放内存
            }
            gst_tag_list_free(tags); // 释放内存
        }
    }

    g_print("\n");
    for (idx = 0; idx < data->n_text; idx++)
    {
        tags = NULL;
        /* Retrieve the stream's subtitle tags */
        g_signal_emit_by_name(data->playbin, "get-text-tags", idx, &tags);
        if (tags)
        {
            g_print("subtitle stream %d:\n", idx);
            if (gst_tag_list_get_string(tags, GST_TAG_LANGUAGE_CODE, &str))
            {
                g_print("\t language: %s\n", str);
                g_free(str);
            }
            gst_tag_list_free(tags);
        }
    }

    g_object_get(data->playbin, "current-video", &data->current_video, NULL); // 获取当前播放视频流idx
    g_object_get(data->playbin, "current-audio", &data->current_audio, NULL); // 获取当前播放音频流idx
    g_object_get(data->playbin, "current-text", &data->current_text, NULL);   // 获取当前播放字幕流idx

    g_print("\n");
    g_print("Currently playing video stream %d, audio stream %d and text stream %d\n",
            data->current_video, // 输出视频流idx: 0
            data->current_audio, // 输出音频流idx: 0
            data->current_text   // 输出字幕流idx: -1
    );
    g_print("Type any number and hit ENTER to select a different audio stream\n");
}

/* Process messages from GStreamer */
static gboolean handle_message(GstBus *bus, GstMessage *msg, CustomData *data)
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
        g_main_loop_quit(data->main_loop);
        break;
    case GST_MESSAGE_EOS:
        g_print("End-Of-Stream reached.\n");
        g_main_loop_quit(data->main_loop);
        break;
    case GST_MESSAGE_STATE_CHANGED: // 状态改变
    {
        // 从消息中解析各种状态
        //      old_state-> new_state ->pending_state
        //      old_state: 旧状态
        //      new_state: 新状态（当前状态）
        //      pending_state: 元素将要转变到的下一个状态
        //
        GstState old_state, new_state, pending_state;
        gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);

        // 消息来源是playbin
        if (GST_MESSAGE_SRC(msg) == GST_OBJECT(data->playbin))
        {
            if (new_state == GST_STATE_PLAYING) // 当前状态是播放
            {
                /* Once we are in the playing state, analyze the streams */
                // 分析playbin中的流信息
                analyze_streams(data);
            }
        }
    }
    break;
    }

    /* We want to keep receiving messages */
    return TRUE;
}

/* Process keyboard input */
static gboolean handle_keyboard(GIOChannel *source, GIOCondition cond, CustomData *data)
{
    gchar *str = NULL;

    // 读取一行输入
    if (g_io_channel_read_line(source, &str, NULL, NULL, NULL) == G_IO_STATUS_NORMAL)
    {
        // 解析为数字
        int index = g_ascii_strtoull(str, NULL, 0);
        if (index < 0 || index >= data->n_audio) // 确保index ∈ [0,n_audio]
        {
            g_printerr("Index out of bounds\n");
        }
        else
        {
            /* If the input was a valid audio stream index, set the current audio stream */
            // 设置为当前音频流编号
            // 这种切换不是立即生效的
            // 一些之前解码好的音频数据将仍然在pipeline中流动，虽然新的流已经开始解码。
            // 延迟取决于容器中流的特定多路复用和playbin的内部queue的长度（这取决于网络状况）。
            g_print("Setting current audio stream to %d\n", index);
            g_object_set(data->playbin, "current-audio", index, NULL);
        }
    }
    g_free(str);
    return TRUE;
}