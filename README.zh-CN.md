# CameraCapture - 一个 C++ 轻量相机库

[![Windows Build with VS2022](https://github.com/wysaid/CameraCapture/workflows/Windows%20Build%20with%20VS2022/badge.svg)](https://github.com/wysaid/CameraCapture/actions/workflows/windows-build.yml)

[English](./README.md) | [中文](./README.zh-CN.md)

## 概要

CameraCapture 是一个高效的、易用的、轻量级的 C++ 相机捕获库，旨在简化相机图像捕获和处理的过程。支持 Windows、MacOS 平台, 除了系统自带的底层库之外, 不依赖 OpenCV 或者 FFmpeg 等任何其他大小第三方库。它提供了简单易用的 API，适合需要快速实现相机捕获功能的开发者。

## 依赖

- C++17 或更高版本
- CMake 3.10 或更高版本
- 系统依赖:
  - Windows: DirectShow
  - MacOS: Foundation, AVFoundation, CoreVideo, CoreMedia

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
    auto cameraProvider = ccap::createProvider();
    cameraProvider->open();
    cameraProvider->start();

    if (cameraProvider->isStarted())
    {
        if (auto frame = cameraProvider->grab(true))
        {
            printf("Frame %lld grabbed: width = %d, height = %d, bytes: %d\n", frame->frameIndex, frame->width, frame->height, frame->sizeInBytes);
        }
    }
    ```

2. 列举当前可用的相机设备名, 并打印出来

  ```cpp
  auto cameraProvider = ccap::createProvider();
  if (auto deviceNames = cameraProvider->findDeviceNames(); !deviceNames.empty())
  {
      for (const auto& name : deviceNames)
      {
          std::cout << "## Found video capture device: " << name << std::endl;
      }
  }
  ```

## 常见问题 (FAQ)

1. 如何选择 PixelFormat

    在 Windows 上， 不设置的情况下会默认选择 `BGR888`, 它一般来说会被支持. 如果要手动选择 YUV 格式, 那么 `NV12f`、`NV12v` 这两者都可以.
    如果是虚拟相机 (比如 `Obs Virtual Camera`), 可能会跟虚拟相机所设置的输出格式有关。
  
    在 Mac 上, 不设置的情况下会默认选择 `BGRA8888`, 它一般来说会被支持. 如果要手动选择 YUV 格式, 那么 `NV12f`、`NV12v` 这两者都可以.
    如果是虚拟相机 (比如 `Obs Virtual Camera`), 可能会跟虚拟相机所设置的输出格式有关。
  
    当 ccap 遇到设置的 PixelFormat 不被支持的时候， 会尝试选择一个较为接近的被支持的格式, 具体格式需要从获取到的 `frame->PixelFormat` 来判断.
  
2. 如何选择不同的相机设备

    创建了 `ccap::Provider` 之后, 可以通过 `findDeviceNames` 来获取可用的所有相机设备。

3. 如何禁用所有运行时的日志, 包括错误日志

    代码: `ccap::setLogLevel(ccap::LogLevel::None);`
