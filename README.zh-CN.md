# CameraCapture - 一个 C++ 轻量相机库

[![windows Build](https://github.com/wysaid/CameraCapture/actions/workflows/windows-build.yml/badge.svg)](https://github.com/wysaid/CameraCapture/actions/workflows/windows-build.yml) [![macos Build](https://github.com/wysaid/CameraCapture/actions/workflows/macos-build.yml/badge.svg)](https://github.com/wysaid/CameraCapture/actions/workflows/macos-build.yml)

[English](./README.md) | [中文](./README.zh-CN.md)

## 概要

ccap `(C)amera(CAP)ture` 是一个高效的、易用的、轻量级的 C++ 相机捕获库，旨在简化相机图像捕获和处理的过程。支持 Windows、MacOS、iOS 平台,
除了系统自带的底层库之外, 不依赖 OpenCV 或者 FFmpeg 等任何其他大小第三方库。它提供了简单易用的 API，适合需要快速实现相机捕获功能的开发者。

## 编译

- Windows/MacOS:
  - 支持 C++17 或更高版本的编译器 (MSVC 2019+/GCC 7.1+/Clang 5.0+)
  - CMake 3.14 或更高版本
  - 系统依赖:
    - Windows: DirectShow (对于相机设备兼容性略好于 MSMF) / MSMF (后续版本提供)
    - MacOS 10.13+: Foundation, AVFoundation, CoreVideo, CoreMedia, Accelerate
- iOS:
  - 最新版本的 XCode.
  - 系统支持: iOS 13.0+
- Android:
  - 开发中...

> [更多编译步骤](./examples/README.md)

## 兼容性

- 测试通过: Windows、macOS 两个平台的部分主流笔记本以及外接摄像头。
- 测试通过: Windows、macOS 两个平台的 `OBS Virtual Camera`
- 测试通过: 主流 iPhone 上的前后置主摄.

> TODO: 支持 Android.

> 如果发现不支持的情况, 欢迎提供 PR 进行修复。

## 如何使用

使用非常简单, 本项目提供一个头文件和一个静态库, 直接添加到你的项目中即可。
也可以直接将本项目源码添加至你的项目

本项目内置数个 Example, 可以直接参考:

1. [打印相机设备](./examples/desktop/0-print_camera.cpp)
2. [抓取一帧的简单Example](./examples/desktop/1-minimal_example.cpp)
3. [持续主动抓取帧的Example](./examples/desktop/2-capture_grab.cpp)
4. [通过回调获取帧的Example](./examples/desktop/3-capture_callback.cpp)
5. [基于 glfw 的 Gui Example](./examples/desktop/4-example_with_glfw.cpp)

下面是使用代码参考:

1. 启动相机， 并获取一帧数据:

    ```cpp
    ccap::Provider cameraProvider(""); // Pass empty string to open the default camera

    if (cameraProvider.isStarted())
    {
        auto frame = cameraProvider.grab(true);
        if (frame)
        {
            printf("VideoFrame %lld grabbed: width = %d, height = %d, bytes: %d\n", frame->frameIndex, frame->width, frame->height, frame->sizeInBytes);
        }
    }
    ```

2. 列举当前可用的相机设备名, 并打印出来

    ```cpp
    ccap::Provider cameraProvider;
    if (auto deviceNames = cameraProvider.findDeviceNames(); !deviceNames.empty())
    {
        printf("## Found %zu video capture device: \n", deviceNames.size());
        int deviceIndex = 0;
        for (const auto& name : deviceNames)
        {
            printf("    %d: %s\n", deviceIndex++, name.c_str());
        }
    }
    ```

## ccap::VideoFrame 和其他知名库一起使用

1. [OpenCV](include/ccap_opencv.h)

## 常见问题 (FAQ)

1. 如何选择 PixelFormat

    - 在 Windows 上， 不设置的情况下会默认选择 `BGR24`, 它一般来说会被支持. 如果要手动选择 YUV 格式, 那么 `NV12`、`I420` 这两者都可以.  
    如果是虚拟相机 (比如 `Obs Virtual Camera`), 可能会跟虚拟相机所设置的输出格式有关。  
    如果选择的格式不被支持, 那么底层会尝试进行格式转换, 会有一定的开销. 转换会尝试使用 AVX2 等方式加速, 如果加速不可用, 会使用纯 CPU 代码直接转换.

   - 在 Mac 上, 不设置的情况下会默认选择 `BGRA32`, 它一般来说会被支持. 如果要手动选择 YUV 格式, 那么 `NV12`、`NV12f`(Full-Range) 这两者都可以.  
    如果是虚拟相机 (比如 `Obs Virtual Camera`), 可能会跟虚拟相机所设置的输出格式有关。  
    如果选择的格式不被支持, 会使用 Accelerate Framework 进行转换.

   > 当转换不被支持时, 会输出实际的格式, 可以从 `frame->pixelFormat` 获得.

2. 如何选择不同的相机设备

   创建了 `ccap::Provider` 之后, 可以通过 `findDeviceNames` 来获取可用的所有相机设备。

3. 如何运行时禁用所有的日志, 包括错误日志?

   代码: `ccap::setLogLevel(ccap::LogLevel::None);`

4. 如何在编译阶段去掉所有日志以减少包体积?

   - 如果是编译本项目, 给 CMake 传入参数 `-DCCAP_NO_LOG=ON` 即可.
   - 如果是引用源码, 编译时加上全局宏定义 `CCAP_NO_LOG=1` 即可.
