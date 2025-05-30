# CameraCapture - A Lightweight C++ Camera Library

[![windows Build](https://github.com/wysaid/CameraCapture/actions/workflows/windows-build.yml/badge.svg)](https://github.com/wysaid/CameraCapture/actions/workflows/windows-build.yml) [![macos Build](https://github.com/wysaid/CameraCapture/actions/workflows/macos-build.yml/badge.svg)](https://github.com/wysaid/CameraCapture/actions/workflows/macos-build.yml)

[English](./README.md) | [中文](./README.zh-CN.md)

## Overview

ccap `(C)amera(CAP)ture` is an efficient, easy-to-use, and lightweight C++ camera capture library designed to simplify the process of capturing and processing camera images. It supports `Windows`、 `MacOS`、`iOS`, and except for the system's built-in low-level libraries, it does not depend on OpenCV, FFmpeg, or any other large third-party libraries. It provides a simple and user-friendly API, making it ideal for developers who need to quickly implement camera capture functionality.

## Build

- Windows/MacOS:
  - Requires a C++17 or newer compiler (MSVC 2019+, GCC 7.1+, Clang 5.0+)
  - CMake 3.14 or newer
  - System dependencies:
    - Windows: DirectShow (better compatibility for camera devices) / MSMF (to be supported in future versions)
    - MacOS 10.13+: Foundation, AVFoundation, CoreVideo, CoreMedia, Accelerate
- iOS:
  - Latest version of XCode
  - System support: iOS 13.0+
- Android:
  - WIP...

> [More build steps](./examples/README.md)

## Compatibility

- Tested (partial): Mainstream laptops and external cameras on both Windows and macOS platforms.
- Tested: `OBS Virtual Camera` on both Windows and macOS platforms.
- Tested: Front and rear main cameras on mainstream iPhones.

If you encounter unsupported cases, you are welcome to submit a PR to help fix them.

## How to Use

Usage is very simple. This project provides a header file and a static library, which can be directly added to your project.  
Alternatively, you can add the source code of this project directly to your own project.

Several examples are included in this project for your reference:

1. [Print Camera Devices](./examples/desktop/0-print_camera.cpp)
2. [Simple Example for Grabbing a VideoFrame](./examples/desktop/1-minimal_example.cpp)
3. [Example for Continuously Grabbing Frames](./examples/desktop/2-capture_grab.cpp)
4. [Example for Callback Grabbing](./examples/desktop/3-capture_callback.cpp)
5. [GLFW GUI Example](./examples/desktop/4-example_with_glfw.cpp)

Sample usage:

1. Start the camera and grab a frame:

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

## Using ccap::VideoFrame with Other Popular Libraries

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
