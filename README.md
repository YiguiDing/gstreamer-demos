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