#include <gst/gst.h>

int main(int argc, char *argv[])
{
    GstElement *pipeline, *bin, *equalizer, *convert, *sink;
    GstPad *pad, *ghost_pad;
    GstBus *bus;
    GstMessage *msg;

    /* Initialize GStreamer */
    gst_init(&argc, &argv);

    /* Build the pipeline */
    pipeline = gst_parse_launch("playbin uri=https://www.freedesktop.org/software/gstreamer-sdk/data/media/sintel_trailer-480p.webm", NULL);

    /* Create the elements inside the sink bin */
    equalizer = gst_element_factory_make("equalizer-3bands", "equalizer");
    convert = gst_element_factory_make("audioconvert", "convert");
    sink = gst_element_factory_make("autoaudiosink", "audio_sink");
    if (!equalizer || !convert || !sink)
    {
        g_printerr("Not all elements could be created.\n");
        return -1;
    }

    /* Create the sink bin, add the elements and link them */
    // 创建自定义的GstBin，实现音频处理（均衡器）和播放
    // GstBin myAudioBin = [equalizer-3bands => audioconvert => autoaudiosink ]
    bin = gst_bin_new("audio_sink_bin");
    gst_bin_add_many(GST_BIN(bin), equalizer, convert, sink, NULL);
    gst_element_link_many(equalizer, convert, sink, NULL);
    // 获取 equalizer.sink_pad
    pad = gst_element_get_static_pad(equalizer, "sink");
    // 创建ghost_pad, 名为sink、实际指向equalizer.sink_pad
    ghost_pad = gst_ghost_pad_new("sink", pad);
    // 激活ghost_pad
    gst_pad_set_active(ghost_pad, TRUE);
    // 添加到bin
    gst_element_add_pad(bin, ghost_pad);
    gst_object_unref(pad);

    /* Configure the equalizer */
    // band0: 100Hz增益 ∈ [-24,+12] 默认0
    // band1: 1100Hz增益 ∈ [-24,+12] 默认0
    // band2: 11000Hz增益 ∈ [-24,+12] 默认0
    // 保留低频，完全衰减中高频。
    g_object_set(G_OBJECT(equalizer), "band0", (gdouble)0, NULL);
    g_object_set(G_OBJECT(equalizer), "band1", (gdouble)-24.0, NULL);
    g_object_set(G_OBJECT(equalizer), "band2", (gdouble)-24.0, NULL);

    /* Set playbin's audio sink to be our sink bin */
    // 将playbin播放器的音频部分修改为用自定义的音频播放器来播放
    //  playbin.audio-sink => myAudioBin
    g_object_set(GST_OBJECT(pipeline), "audio-sink", bin, NULL);

    /* Start playing */
    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    /* Wait until error or EOS */
    bus = gst_element_get_bus(pipeline);
    msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, GST_MESSAGE_ERROR | GST_MESSAGE_EOS);

    /* Free resources */
    if (msg != NULL)
        gst_message_unref(msg);
    gst_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    return 0;
}