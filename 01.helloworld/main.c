#include <gst/gst.h>

int main(int argc, char *argv[])
{
    GstElement *pipeline;
    GstBus *bus;
    GstMessage *msg;

    /* Initialize GStreamer */
    /**
     * GStreamer程序必须执行gst_init(可以传递两个NULL参数);
     * 
     * gst_init实现：
     *  + 初始化所有内部结构体
     *  + 检查平台所有可用的插件
     *  + 执行任何用于GStreamer的命令行选项
     */
    gst_init(&argc, &argv);

    /* Build the pipeline */
    /**
     * 关于gst_parse_launch('playbin uri=xxx')
     * - 构建了一个只有playbin插件的pipeline
     * - 通常通过手动组装各个独立的element来构建pipeline
     * - 当pipeline足够简单，可以使用gst_parse_launch()自动解析并实例化一条文本形式表示的pipeline
     * - 注意：gst_parse_launch() 内部的事件处理机制和Pad动态创建机制可能导致管道无法重复使用。
     * 
     * 关于playbin
     * - 特殊的element, 具备一个pipeline的所有特点，既有source，又有sink。
     * - 它在内部自动创建和连接所有播放媒体需要的element
     *  - 自动为不同的源初始化合适的source element：http源、file源、rtsp源
     *  - 自动创建filter element：解封装、解码。
     */
    pipeline = gst_parse_launch("playbin uri=https://www.freedesktop.org/software/gstreamer-sdk/data/media/sintel_trailer-480p.webm",
                                NULL);

    /* Start playing */
    /**
     * 将pipeline的状态设置为PLAYING以开始播放
     */
    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    /* Wait until error or EOS */
    /**
     * 监听整个pipeline总线
     * + gst_element_get_bus获取pipeline的bus
     * + gst_bus_timed_pop_filtered阻塞程序直到程序运行引发ERROR或者到达EOS
     */
    bus = gst_element_get_bus(pipeline);
    msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE,
                                     GST_MESSAGE_ERROR | GST_MESSAGE_EOS);

    /* Free resources */
    /**
     * 回收所有手动申请的资源
     * 
     * 释放GstMessage
     *  + 流程：gst_bus_timed_pop_filtered获取 -> gst_message_unref释放
     * 
     * 释放GstBus
     *  + 流程：gst_element_get_bus获取 -> gst_object_unref释放
     * 
     * 释放pipeline
     *  + 在释放pipeline及其下的所有element之前需要将pipeline的状态设置为NULL
     *  + 流程：gst_parse_launch获取 -> gst_element_set_state设置状态 -> gst_object_unref释放
     * 
     */
    if (msg != NULL)
        gst_message_unref(msg);
    
    gst_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    return 0;
}