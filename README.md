# CameraCapture - A Lightweight C++ Camera Library

[![windows Build](https://github.com/wysaid/CameraCapture/actions/workflows/windows-build.yml/badge.svg)](https://github.com/wysaid/CameraCapture/actions/workflows/windows-build.yml) [![macos Build](https://github.com/wysaid/CameraCapture/actions/workflows/macos-build.yml/badge.svg)](https://github.com/wysaid/CameraCapture/actions/workflows/macos-build.yml)

[English](./README.md) | [中文](./README.zh-CN.md)

## Overview

ccap `(C)amera(CAP)ture` is an efficient, easy-to-use, and lightweight C++ camera capture library designed to simplify the process of capturing and processing camera images. It supports Windows and MacOS platforms, and except for the system's built-in low-level libraries, it does not depend on OpenCV, FFmpeg, or any other large third-party libraries. It provides a simple and user-friendly API, making it ideal for developers who need to quickly implement camera capture functionality.

## Build

- C++17 or higher
- CMake 3.10 or higher
- System dependencies:
  - Windows: DirectShow (better compatibility than MSMF for camera devices; MSMF support may be added in future versions)
  - MacOS 10.13+: Foundation, AVFoundation, CoreVideo, CoreMedia, Accelerate

## Compatibility

- Tested (partial): Mainstream laptops and external cameras on both Windows and macOS platforms.
- Tested: `OBS Virtual Camera` on both Windows and macOS platforms.

If you encounter unsupported cases, you are welcome to submit a PR to help fix them.

## How to Use

Usage is very simple. This project provides a header file and a static library, which can be directly added to your project.  
Alternatively, you can add the source code of this project directly to your own project.

Several demos are included in this project for your reference:

1. [Print Camera Devices](./demo/0-print_camera.cpp)
2. [Simple Demo for Grabbing a Frame](./demo/1-minimal_demo.cpp)
3. [Demo for Continuously Grabbing Frames](./demo/2-capture_grab.cpp)
4. [Demo for Getting Frames via Callback](./demo/3-capture_callback.cpp)

Sample usage:

1. Start the camera and grab a frame:

    ```cpp
    ccap::Provider cameraProvider(""); // Pass empty string to open the default camera

    if (cameraProvider.isStarted())
    {
        auto frame = cameraProvider.grab(true);
        if (frame)
        {
            printf("Frame %lld grabbed: width = %d, height = %d, bytes: %d\n", frame->frameIndex, frame->width, frame->height, frame->sizeInBytes);
        }
    }
    ```

2. List available camera device names and print them:

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

## Using ccap::Frame with Other Popular Libraries

1. [OpenCV](include/ccap_opencv.h)

## FAQ

1. How to select PixelFormat

    - On Windows, if not set, the default is `BGR24`, which is generally supported. If you want to manually select a YUV format, both `NV12` and `I420` are available.  
    For virtual cameras (such as `Obs Virtual Camera`), the supported format may depend on the output format set by the virtual camera.  
    If the selected format is not supported, the underlying library will attempt format conversion, which may incur some overhead. Conversion will try to use AVX2 acceleration if available; otherwise, pure CPU code will be used.

    - On Mac, if not set, the default is `BGRA32`, which is generally supported. If you want to manually select a YUV format, both `NV12` and `NV12f` (Full-Range) are available.  
    For virtual cameras (such as `Obs Virtual Camera`), the supported format may depend on the output format set by the virtual camera.  
    If the selected format is not supported, the Accelerate Framework will be used for conversion.

    > When conversion is not supported, the actual format will be output, which can be checked from `frame->pixelFormat`.

2. How to select different camera devices

    After creating a `ccap::Provider`, you can use `findDeviceNames` to get all available camera devices.

3. How to disable all runtime logs, including error logs

    Code: `ccap::setLogLevel(ccap::LogLevel::None);`

4. How to remove all logs at compile time to reduce package size?

    - If you are building this project, pass the parameter `-DCCAP_NO_LOG=ON` to CMake.
    - If you are including the source code, add the global macro definition `CCAP_NO_LOG=1` during compilation.
