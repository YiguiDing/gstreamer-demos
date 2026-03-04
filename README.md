## 安装依赖

对于 `MSYS2/MINGW64` 环境

```bash
pacman -S mingw-w64-x86_64-gstreamer mingw-w64-x86_64-gst-plugins-base mingw-w64-x86_64-gst-plugins-good mingw-w64-x86_64-gst-plugins-bad mingw-w64-x86_64-gst-plugins-ugly

pkg-config --list-all | grep gst
```

## 编译运行

```
cd ./01.helloworld
make all
./main.out
```

## 学习笔记

[notes/index.md](./notes/index.md)

- 01. Hello World
- 02. 手动构建 Pipeline
- 03. 动态构建 Pipeline
- 06. Pad Capabilities（能力协商）
- 07. 多线程与 Tee Element
- 08. Appsrc 和 Appsink
- 09. 流信息与动态切换
- 10. 将 Appsrc 链接到 Playbin
- 11. 自定义 Playbin 音频 Sink