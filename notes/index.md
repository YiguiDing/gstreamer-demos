---
title: "GStreamer 学习笔记索引"
date: 2026-03-04T00:00:00+08:00
tags: [gstreamer, notes]
---

# GStreamer 学习笔记索引

本目录包含 GStreamer 学习过程中的笔记文档，按照学习顺序组织。

## 基础篇

### 01. Hello World
**文件**: [01.helloworld.md](./01.helloworld.md)

- GStreamer 的第一个示例
- 使用 `gst_parse_launch()` 快速构建 pipeline
- 使用 `playbin` 播放媒体
- 消息监听和资源管理
- Makefile 的两种写法（简洁版和标准版）

### 02. 手动构建 Pipeline
**文件**: [02.build-pipeline-manually.md](./02.build-pipeline-manually.md)

- 使用 `gst_element_factory_make()` 创建元素
- 使用 `gst_pipeline_new()` 创建 pipeline
- 使用 `gst_bin_add_many()` 添加元素
- 使用 `gst_element_link()` 链接元素
- 使用 `g_object_set()` 配置元素属性
- 状态管理和错误处理

### 03. 动态构建 Pipeline
**文件**: [03.build-pipeline-dynamically.md](./03.build-pipeline-dynamically.md)

- 处理动态 Pad（Dynamically Pads）
- 使用 `pad-added` 信号处理异步事件
- 根据 Pad 的 Capabilities 类型进行路由
- 复杂 media pipeline 的构建
- 事件循环和状态变化监听

## 进阶篇

### 06. Pad Capabilities（能力协商）
**文件**: [06.pad-capabilities.md](./06.pad-capabilities.md)

- Element、Pad、Pad Templates 概念
- Capabilities 的作用和协商过程
- 打印和分析 Capabilities
- 能力范围到具体值的转换

### 07. 多线程与 Tee Element
**文件**: [07.multithreading.md](./07.multithreading.md)

- GStreamer 多线程模型
- Tee 元素的数据分发
- Request Pads 的手动申请和释放
- 多线程 Pipeline 构建

### 08. Appsrc 和 Appsink
**文件**: [08.appsrc-appsink.md](./08.appsrc-appsink.md)

- 自定义数据源（appsrc）
- 数据接收（appsink）
- 时间戳管理
- 信号机制和 GLib 主循环

### 09. 流信息与动态切换
**文件**: [09.streams-info-change.md](./09.streams-info-change.md)

- Playbin 高级功能
- 流信息提取（视频、音频、字幕）
- 动态切换流
- 事件监听和键盘输入处理

### 10. 将 Appsrc 链接到 Playbin
**文件**: [10.link-appsrc-playbin.md](./10.link-appsrc-playbin.md)

- 使用 `appsrc://` 协议
- `source-setup` 信号
- 向 playbin 注入自定义数据
- 与手动构建 pipeline 的区别

### 11. 自定义 Playbin 音频 Sink
**文件**: [11.custom-playbin-audio-sink.md](./11.custom-playbin-audio-sink.md)

- GstBin 和 Ghost Pad
- 创建自定义音频处理链
- 三段均衡器的使用
- 扩展自定义 sink

## 参考资料

- [GStreamer 官方文档](https://gstreamer.freedesktop.org/documentation/)
- [GStreamer 教程](https://gstreamer.freedesktop.org/documentation/tutorials/)
- [Gst-Inspect 工具](https://gstreamer.freedesktop.org/documentation/tools/gst-inspect.html)
