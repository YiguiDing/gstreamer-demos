#include <gst/gst.h>
int main(int argc, char *argv[])
{
    GstElement *pipeline, *source, *filter, *sink;
    GstBus *bus;
    GstMessage *msg;
    GstStateChangeReturn ret;

    /* Initialize GStreamer */
    gst_init(&argc, &argv);

    /* Create the elements */
    /**
     * 创建element
     *  + gst_element_factory_make(element插件名称,自定义名称)
     *
     * videotestsrc
     *  + source element(生产数据)
     *  + 按照模式(pattern=<number>)生成测试视频
     *
     * videotestsrc
     *  + filter element(处理数据)
     *
     * autovideosink
     *  + sink element(消费数据)
     *  + 自动(根据操作系统选择实例程序)创建窗口播放它接收到的图像
     */
    source = gst_element_factory_make("videotestsrc", "source");
    filter = gst_element_factory_make("vertigotv", "filter");
    sink = gst_element_factory_make("autovideosink", "sink");

    /* Create the empty pipeline */
    /**
     * 创建pipeline
     *  + gst_pipeline_new(自定义管道名称)
     *  + GStreamer中的所有元素在使用之前通常必须包含在一条pipeline中，
     *  + pipeline将负责一些时钟和消息功能。
     */
    pipeline = gst_pipeline_new("test-pipeline");
    if (!pipeline || !source || !filter || !sink)
    {
        g_printerr("Not all elements could be created.\n");
        return -1;
    }

    /* Build the pipeline */
    /**
     * 关于bin
     * + bin是一个逻辑上的element，用来从整体上控制和管理其内部的所有elements
     * + pipeline是特殊的bin
     *
     * 将element元素添加到pipeline(bin)统一管理和控制
     *  + gst_bin_add_many(bin,element1,element2,...,NULL)
     *  + 因为不确定可变参数列表长度所以需要以NULL结尾。
     */
    gst_bin_add_many(GST_BIN(pipeline), source, filter, sink, NULL);
    /**
     * 链接element元素
     * + pipeline:[source -> filter -> sink]
     * + gst_element_link(element1,element2) 实现将elements链接起来
     */
    if (gst_element_link(source, filter) != TRUE ||
        gst_element_link(filter, sink) != TRUE)
    {
        g_printerr("Elements could not be linked.\n");
        gst_object_unref(pipeline);
        return -1;
    }

    /* Modify the source's properties */
    /**
     * 修改元素属性
     * 
     * gst-inspect-1.0 videotestsrc
     *  + pattern: [0,25]
     *      + [0] : smpte - SMPTE 100% color bars
     *      + [1] : snow - Random (television snow)
     *      + [2] : black - 100% Black
     *      + ....
     * 
     * gst-inspect-1.0 vertigotv
     *  + speed:[0.01,100]
     *  + zoom-speed:[1.01,1.10]
     */
    g_object_set(source, "pattern", 0, NULL);
    g_object_set(filter, "speed", 0.01, NULL);

    /* Start playing */
    /**
     * + 修改pipeline的状态为播放
     * + 检查返回值判断是否修改成功
     */
    ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE)
    {
        g_printerr("Unable to set the pipeline to the playing state.\n");
        gst_object_unref(pipeline);
        return -1;
    }

    /* Wait until error or EOS */
    /**
     * 获取pipeline的bus
     * 阻塞程序监听bus直到收到ERROR或者EOS消息
     */
    bus = gst_element_get_bus(pipeline);
    msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE,
                                     GST_MESSAGE_ERROR | GST_MESSAGE_EOS);

    /* Parse message */
    /**
     * 解析并输出运行时的错误消息
     */
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
            break;
        case GST_MESSAGE_EOS:
            g_print("End-Of-Stream reached.\n");
            break;
        default:
            /* We should not reach here because we only asked for ERRORs and EOS */
            g_printerr("Unexpected message received.\n");
            break;
        }
        gst_message_unref(msg);
    }

    /* Free resources */
    /**
     * 释放资源
     */
    gst_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    return 0;
}