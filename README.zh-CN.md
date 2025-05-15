# CameraCapture - 一个 C++ 轻量相机库

[![windows Build](https://github.com/wysaid/CameraCapture/actions/workflows/windows-build.yml/badge.svg)](https://github.com/wysaid/CameraCapture/actions/workflows/windows-build.yml) [![macos Build](https://github.com/wysaid/CameraCapture/actions/workflows/macos-build.yml/badge.svg)](https://github.com/wysaid/CameraCapture/actions/workflows/macos-build.yml)

[English](./README.md) | [中文](./README.zh-CN.md)

## 概要

CameraCapture 是一个高效的、易用的、轻量级的 C++ 相机捕获库，旨在简化相机图像捕获和处理的过程。支持 Windows、MacOS 平台,
除了系统自带的底层库之外, 不依赖 OpenCV 或者 FFmpeg 等任何其他大小第三方库。它提供了简单易用的 API，适合需要快速实现相机捕获功能的开发者。

## 依赖

- C++17 或更高版本
- CMake 3.10 或更高版本
- 系统依赖:
  - Windows: DirectShow (对于相机设备兼容性略好于 MSMF) / MSMF (后续版本提供)
  - MacOS 10.13+: Foundation, AVFoundation, CoreVideo, CoreMedia, Accelerate
- 可选的第三方依赖:
  - Windows: libyuv (如果禁用 libyuv, 那么需要格式转换的时候会使用内置转换)

## 如何使用

使用非常简单, 本项目提供一个头文件和一个静态库, 直接添加到你的项目中即可。
也可以直接将本项目源码添加至你的项目

本项目内置数个 Demo, 可以直接参考:

1. [打印相机设备](./demo/0-print_camera.cpp)
2. [抓取一帧的简单Demo](./demo/1-minimal_demo.cpp)
3. [持续主动抓取帧的Demo](./demo/2-capture_grab.cpp)
4. [通过回调获取帧的Demo](./demo/3-capture_callback.cpp)

下面是使用代码参考:

1. 启动相机， 并获取一帧数据:

    ```cpp
    ccap::Provider cameraProvider(0); // Open the default camera

    if (cameraProvider.isStarted())
    {
        auto frame = cameraProvider.grab(true);
        if (frame)
        {
            printf("Frame %lld grabbed: width = %d, height = %d, bytes: %d\n", frame->frameIndex, frame->width, frame->height, frame->sizeInBytes);
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

## 常见问题 (FAQ)

1. 如何选择 PixelFormat

   在 Windows 上， 不设置的情况下会默认选择 `BGR24`, 它一般来说会被支持. 如果要手动选择 YUV 格式, 那么 `NV12f`、`NV12v`
   这两者都可以.
   如果是虚拟相机 (比如 `Obs Virtual Camera`), 可能会跟虚拟相机所设置的输出格式有关。

   在 Mac 上, 不设置的情况下会默认选择 `BGRA32`, 它一般来说会被支持. 如果要手动选择 YUV 格式, 那么 `NV12f`、`NV12v`
   这两者都可以.
   如果是虚拟相机 (比如 `Obs Virtual Camera`), 可能会跟虚拟相机所设置的输出格式有关。

   ccap 还提供了 `RGB24_Force`、 `BGR24_Force`, `RGBA32_Force`, `BGRA32_Force` 四种格式, 这四种格式会强制转换为对应的格式,
   在不同平台下, 会调用底层的加速库进行转换, 例如在 Mac 上会使用 `vImage` 来加速转换, 在 Windows 上会使用 `DXGI` 来加速转换.

2. 如何选择不同的相机设备

   创建了 `ccap::Provider` 之后, 可以通过 `findDeviceNames` 来获取可用的所有相机设备。

3. 如何运行时禁用所有的日志, 包括错误日志?

   代码: `ccap::setLogLevel(ccap::LogLevel::None);`

4. 如何在编译阶段去掉所有日志以减少包体积?

   - 如果是编译本项目, 给 CMake 传入参数 `-DCCAP_NO_LOG=ON` 即可.
   - 如果是引用源码, 编译时加上全局宏定义 `CCAP_NO_LOG=1` 即可.

5. 如何在 Windows 下启用 libyuv 以加速转换?

   本项目在 Windows 上可以便捷引用 libyuv 对像素格式进行转换, 在 macOS 上无第三方依赖. 下面针对 libyuv 说明
   - 如果是编译本项目, 给CMake 传入参数 `-DENABLE_LIBYUV=ON` 即可.
   - 如果是引用源码, 默认不带 libyuv, 可以加上全局宏定义 `ENABLE_LIBYUV=1`
