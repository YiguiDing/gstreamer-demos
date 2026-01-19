## 安装依赖

`MSYS2/MINGW64` 环境

```bash
pacman -S mingw-w64-x86_64-gstreamer mingw-w64-x86_64-gst-plugins-base mingw-w64-x86_64-gst-plugins-good mingw-w64-x86_64-gst-plugins-bad mingw-w64-x86_64-gst-plugins-ugly

pkg-config --list-all | grep gst
```