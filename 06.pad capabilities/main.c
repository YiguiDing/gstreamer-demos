#include <gst/gst.h>

/**
 * 重新梳理并总结一下概念：
 *
 * -> Element 元素
 *      |    + GStreamer的基本构成单位
 *      |    |  + source元素 生产数据
 *      |    |  + filter元素 处理数据
 *      |    |  + sink元素 消费数据
 *      |    + 元素可以两两连接：
 *      |        + {source元素} -> {sink元素}
 *      |        + {source元素} -> {filter元素} -> {sink元素}
 *      |-> Pad 插槽
 *          |    + 元素通过Pad连接
 *          |       + 连接顺序只能是: [src插槽] -> [sink插槽]
 *          |       |   + {source元素[src插槽]} -> {[sink插槽]filter元素[src插槽]} -> {[src插槽]sink元素}
 *          |       + source元素: 有src插槽
 *          |       + filter元素: 有sink插槽、src插槽
 *          |       + sink元素: 有sink插槽
 *          |-> Pad templates 插槽模板
 *                  + templates用于生成一个pad
 *                  + 描述pad类型：
 *                  |   + SRC template: src 生产数据的pad
 *                  |   + SRC template: sink 消费数据的pad
 *                  + 描述能力的可获得性：
 *                  |   + Availability: Always 总是
 *                  |   + Availability: Sometimes 有时
 *                  |   + Availability: On request 当请求时
 *                  + Capabilities描述了一个pad的所有能力
 *                      + 连接两个Element元素 必须保证两个pad的caps能力有交集，比如解码器输出YUV格式 播放器接收YUV格式。
 *                      + Caps的属性是一个范围 如rate: [ 1, 2147483647 ]，layout: { (string)interleaved, (string)non-interleaved }
 *                      + 当两个element的pad协商完成建立连接后，Caps的属性是一个具体的值如：rate: 48000，layout: interleaved
 *                      + Capabilities: 支持两种(S16LE、U8)音频格式的能力
 *                          audio/x-raw
 *                          format: S16LE
 *                          rate: [ 1, 2147483647 ]
 *                          channels: [ 1, 2 ]
 *                          audio/x-raw
 *                          format: U8
 *                          rate: [ 1, 2147483647 ]
 *                          channels: [ 1, 2 ]
 *                      + Capabilities: 支持任何能力
 *                          Any
 */

/* Functions below print the Capabilities in a human-friendly format */
/**
 * 输出一个caps的一个域
 *
 * 如（属性值是具体值）：
    rate: 48000

 * 如（属性值是范围）：
    rate: [ 1, 2147483647 ]
 */
static gboolean
print_field(GQuark field_id, const GValue *value, gpointer user_data)
{
    gchar *prefix = (gchar *)user_data;
    const gchar *name = g_quark_to_string(field_id); // 获取字段名
    gchar *svalue = gst_value_serialize(value);      // 获取字段值
    g_print("%s %15s: %s\n", prefix, name, svalue);
    g_free(svalue);
    return TRUE;
}
/**
 * 打印输出Caps
 *
 * 如（属性值是范围）：
     audio/x-raw
           format: { (string)S16LE, (string)S16BE, (string)U16LE, (string)U16BE, (string)S24_32LE, (string)S24_32BE, (string)U24_32LE, (string)U24_32BE, (string)S32LE, (string)S32BE, (string)U32LE, (string)U32BE, (string)S24LE, (string)S24BE, (string)U24LE, (string)U24BE, (string)S20LE, (string)S20BE, (string)U20LE, (string)U20BE, (string)S18LE, (string)S18BE, (string)U18LE, (string)U18BE, (string)F32LE, (string)F32BE, (string)F64LE, (string)F64BE, (string)S8, (string)U8 }
           layout: { (string)interleaved, (string)non-interleaved }
             rate: [ 1, 2147483647 ]
         channels: [ 1, 2147483647 ]
 *
 * 如（属性值是具体值）：
     audio/x-raw
           format: F32LE
           layout: interleaved
             rate: 48000
         channels: 2
     channel-mask: 0x0000000000000003

 * 如：
 * ANY

 *
 * 如：
 * EMPTY

 */
static void print_caps(const GstCaps *caps, const gchar *pfx)
{
    guint i;
    g_return_if_fail(caps != NULL);
    if (gst_caps_is_any(caps)) // 支持任何能力
    {
        g_print("%sANY\n", pfx);
        return;
    }
    if (gst_caps_is_empty(caps)) // 没有任何能力
    {
        g_print("%sEMPTY\n", pfx);
        return;
    }
    for (i = 0; i < gst_caps_get_size(caps); i++) // 遍历所有caps
    {
        // 获取一个cap能力
        GstStructure *cap = gst_caps_get_structure(caps, i);
        g_print("%s%s\n", pfx, gst_structure_get_name(cap));

        // 字段cap能力结构体字段
        // 打印输出这个cap能力所有属性
        gst_structure_foreach(cap, print_field, (gpointer)pfx);
    }
}

/* Prints information about a Pad Template, including its Capabilities */
/**
 * 打印输出一个element的pad template
 * 如：
 * SRC template: 'src'
 * Availability: Always
 * Capabilities:
 *   audio/x-raw
 *     format: F32LE
 *     layout: interleaved
 *     ...
 *   audio/x-raw
 *     format: U8
 *     layout: interleaved
 *     ...
 */
static void
print_pad_templates_information(GstElementFactory *factory)
{
    const GList *temps;
    GstStaticPadTemplate *temp;
    // 获取并输出element全名
    g_print("Pad Templates for %s:\n", gst_element_factory_get_longname(factory));
    // 获取element pad template数量
    // 如果为0 输出none
    if (!gst_element_factory_get_num_pad_templates(factory))
    {
        g_print(" none\n");
        return;
    }
    // 获取element 静态pad_templates
    // 我的理解：一个element下有多个pad，每个pad有一个template
    temps = gst_element_factory_get_static_pad_templates(factory);
    while (temps)
    {
        // 遍历pad_template
        temp = temps->data;
        temps = g_list_next(temps);

        // 输出pad_template方向：src/sink
        if (temp->direction == GST_PAD_SRC)
            g_print(" SRC template: '%s'\n", temp->name_template);
        else if (temp->direction == GST_PAD_SINK)
            g_print(" SINK template: '%s'\n", temp->name_template);
        else
            g_print(" UNKNOWN!!! template: '%s'\n", temp->name_template);

        // 输出pad_template存在方式
        g_print(" Availability: %s\n",                               // pad存在方式：
                temp->presence == GST_PAD_ALWAYS      ? "Always"     // 总是存在
                : temp->presence == GST_PAD_SOMETIMES ? "Sometimes"  // 有时存在
                : temp->presence == GST_PAD_REQUEST   ? "On request" // 请求存在
                                                      : "UNKNOWN!!!"   // 未知
        );
        // 输出pad静态能力
        if (temp->static_caps.string)
        {
            GstCaps *caps;
            g_print(" Capabilities:\n");
            caps = gst_static_caps_get(&temp->static_caps); // 获取静态caps
            print_caps(caps, " ");                          // 打印输出caps
            gst_caps_unref(caps);
        }
        g_print("\n");
    }
}

/* Shows the CURRENT capabilities of the requested pad in the given element */
/**
 * 打印输出一个元素的pad的能力
 *
 * 如：
 *  Caps for the sink pad:
        audio/x-raw
            format: F32LE
            format: F32LE
            layout: interleaved
            rate: 48000
            channels: 2
 */
static void
print_pad_capabilities(GstElement *element, gchar *pad_name)
{
    GstPad *pad = NULL;
    GstCaps *caps = NULL;

    /* Retrieve pad */
    /** 获取一个元素的静态pad */
    pad = gst_element_get_static_pad(element, pad_name);
    if (!pad)
    {
        g_printerr("Could not retrieve pad '%s'\n", pad_name);
        return;
    }

    /* Retrieve negotiated caps (or acceptable caps if negotiation is not finished yet) */
    /* 获取pad当前的caps能力 */
    caps = gst_pad_get_current_caps(pad);
    if (!caps)
        caps = gst_pad_query_caps(pad, NULL);

    /* Print and free */
    g_print("Caps for the %s pad:\n", pad_name);
    /** 打印输出caps能力 */
    print_caps(caps, " ");
    gst_caps_unref(caps);
    gst_object_unref(pad);
}
int main(int argc, char *argv[])
{
    GstElement *pipeline, *source, *sink;
    GstElementFactory *source_factory, *sink_factory;
    GstBus *bus;
    GstMessage *msg;
    GstStateChangeReturn ret;
    gboolean terminate = FALSE;

    /* Initialize GStreamer */
    gst_init(&argc, &argv);

    /* Create the element factories */
    source_factory = gst_element_factory_find("audiotestsrc");
    sink_factory = gst_element_factory_find("autoaudiosink");
    if (!source_factory || !sink_factory)
    {
        g_printerr("Not all element factories could be created.\n");
        return -1;
    }

    /* Print information about the pad templates of these factories */

    /**
     * source_factory 的打印输出结果(caps的属性是一个范围，如rate: [ 1, 2147483647 ])：
     *
     * Pad Templates for Audio test source:
        SRC template: 'src'
        Availability: Always
        Capabilities:
            audio/x-raw
                format: { (string)S16LE, (string)S16BE, (string)U16LE, (string)U16BE, (string)S24_32LE, (string)S24_32BE, (string)U24_32LE, (string)U24_32BE, (string)S32LE, (string)S32BE, (string)U32LE, (string)U32BE, (string)S24LE, (string)S24BE, (string)U24LE, (string)U24BE, (string)S20LE, (string)S20BE, (string)U20LE, (string)U20BE, (string)S18LE, (string)S18BE, (string)U18LE, (string)U18BE, (string)F32LE, (string)F32BE, (string)F64LE, (string)F64BE, (string)S8, (string)U8 }
                layout: { (string)interleaved, (string)non-interleaved }
                rate: [ 1, 2147483647 ]
                channels: [ 1, 2147483647 ]
     */
    print_pad_templates_information(source_factory);

    /**
     * sink_factory 的打印输出结果：
     *
     * Pad Templates for Auto audio sink:
        SINK template: 'sink'
        Availability: Always
        Capabilities:
        ANY
     */
    print_pad_templates_information(sink_factory);

    /* Ask the factories to instantiate actual elements */
    /** 实例化元素 */
    source = gst_element_factory_create(source_factory, "source");
    sink = gst_element_factory_create(sink_factory, "sink");

    /* Create the empty pipeline */
    pipeline = gst_pipeline_new("test-pipeline");
    if (!pipeline || !source || !sink)
    {
        g_printerr("Not all elements could be created.\n");
        return -1;
    }

    /* Build the pipeline */
    gst_bin_add_many(GST_BIN(pipeline), source, sink, NULL);
    if (gst_element_link(source, sink) != TRUE)
    {
        g_printerr("Elements could not be linked.\n");
        gst_object_unref(pipeline);
        return -1;
    }

    /* Print initial negotiated caps (in NULL state) */
    g_print("In NULL state:\n");
    /**
     * 打印输出结果：
     * In NULL state:
        Caps for the sink pad:
         ANY
     */
    print_pad_capabilities(sink, "sink");

    /* Start playing */
    ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE)
    {
        g_printerr("Unable to set the pipeline to the playing state (check the bus for error messages).\n");
    }

    /* Wait until error, EOS or State Change */
    bus = gst_element_get_bus(pipeline);
    do
    {
        msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, GST_MESSAGE_ERROR | GST_MESSAGE_EOS | GST_MESSAGE_STATE_CHANGED);

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
                if (GST_MESSAGE_SRC(msg) == GST_OBJECT(pipeline))
                {
                    GstState old_state, new_state, pending_state;
                    gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
                    g_print("\nPipeline state changed from %s to %s:\n",
                            gst_element_state_get_name(old_state), gst_element_state_get_name(new_state));
                    /* Print the current capabilities of the sink element */
                    /**
                     * 打印输出结果(caps的属性变成了具体的值，如rate: 48000)：
                     * Pipeline state changed from NULL to READY:
                        Caps for the sink pad:
                            audio/x-raw
                                format: F32LE
                                layout: interleaved
                                rate: 48000
                                channels: 2
                                channel-mask: 0x0000000000000003
                     */
                    print_pad_capabilities(sink, "sink");

                    /**
                     * 打印输出结果：
                     * Caps for the src pad:
                            audio/x-raw
                                format: F32LE
                                layout: interleaved
                                rate: 48000
                                channels: 2
                                channel-mask: 0x0000000000000003
                     */
                    print_pad_capabilities(source, "src");
                }
                break;
            default:
                /* We should not reach here because we only asked for ERRORs, EOS and STATE_CHANGED */
                g_printerr("Unexpected message received.\n");
                break;
            }
            gst_message_unref(msg);
        }
    } while (!terminate);

    /* Free resources */
    gst_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    gst_object_unref(source_factory);
    gst_object_unref(sink_factory);
    return 0;
}