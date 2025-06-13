# ccap (CameraCapture)

[![Windows Build](https://github.com/wysaid/CameraCapture/actions/workflows/windows-build.yml/badge.svg)](https://github.com/wysaid/CameraCapture/actions/workflows/windows-build.yml)
[![macOS Build](https://github.com/wysaid/CameraCapture/actions/workflows/macos-build.yml/badge.svg)](https://github.com/wysaid/CameraCapture/actions/workflows/macos-build.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++17](https://img.shields.io/badge/C++-17-blue.svg)](https://isocpp.org/)
[![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20macOS%20%7C%20iOS-brightgreen)](https://github.com/wysaid/CameraCapture)

[English](./README.md) | [中文](./README.zh-CN.md)

高性能、轻量级的跨平台 C++ 相机捕获库，支持硬件加速的像素格式转换。

## 目录

- [特性](#特性)
- [快速开始](#快速开始)
- [系统要求](#系统要求)
- [示例代码](#示例代码)
- [API 参考](#api-参考)
- [测试](#测试)
- [构建与安装](#构建与安装)
- [许可证](#许可证)

## 特性

- **高性能**：硬件加速的像素格式转换，提升高达 10 倍性能（AVX2、Apple Accelerate）
- **轻量级**：零外部依赖，仅使用系统框架
- **跨平台**：Windows（DirectShow）、macOS/iOS（AVFoundation）
- **多种格式**：RGB、BGR、YUV（NV12/I420）及自动转换
- **生产就绪**：完整测试套件，95%+ 精度验证
- **虚拟相机支持**：兼容 OBS Virtual Camera 等工具

## 快速开始

### 安装

```bash
git clone https://github.com/wysaid/CameraCapture.git
cd CameraCapture
./scripts/build_and_install.sh
```

### CMake 集成

```cmake
find_package(ccap REQUIRED)
target_link_libraries(your_app ccap::ccap)
```

### 基本用法

```cpp
#include <ccap.h>

int main() {
    ccap::Provider provider;
    
    // 列出可用相机
    auto devices = provider.findDeviceNames();
    for (size_t i = 0; i < devices.size(); ++i) {
        printf("[%zu] %s\n", i, devices[i].c_str());
    }
    
    // 打开并启动相机
    if (provider.open("", true)) {  // 空字符串 = 默认相机
        auto frame = provider.grab(3000);  // 3 秒超时
        if (frame) {
            printf("捕获: %dx%d, %s 格式\n", 
                   frame->width, frame->height,
                   ccap::pixelFormatToString(frame->pixelFormat).data());
        }
    }
    return 0;
}
```

## 系统要求

| 平台 | 编译器 | 系统要求 |
|------|--------|----------|
| **Windows** | MSVC 2019+ | DirectShow |
| **macOS** | Xcode 11+ | macOS 10.13+ |
| **iOS** | Xcode 11+ | iOS 13.0+ |

**构建要求**：CMake 3.14+，C++17

## 示例代码

| 示例 | 描述 | 平台 |
|------|------|------|
| [0-print_camera](./examples/desktop/0-print_camera.cpp) | 列出可用相机 | 桌面端 |
| [1-minimal_example](./examples/desktop/1-minimal_example.cpp) | 基本帧捕获 | 桌面端 |
| [2-capture_grab](./examples/desktop/2-capture_grab.cpp) | 连续捕获 | 桌面端 |
| [3-capture_callback](./examples/desktop/3-capture_callback.cpp) | 回调式捕获 | 桌面端 |
| [4-example_with_glfw](./examples/desktop/4-example_with_glfw.cpp) | OpenGL 渲染 | 桌面端 |
| [iOS Demo](./examples/) | iOS 应用程序 | iOS |

### 构建和运行示例

```bash
mkdir build && cd build
cmake .. -DCCAP_BUILD_EXAMPLES=ON
cmake --build .

# 运行示例
./0-print_camera
./1-minimal_example
```

## API 参考

### 核心类

#### ccap::Provider

```cpp
class Provider {
public:
    // 构造函数
    Provider();
    Provider(std::string_view deviceName, std::string_view extraInfo = "");
    Provider(int deviceIndex, std::string_view extraInfo = "");
    
    // 设备发现
    std::vector<std::string> findDeviceNames();
    
    // 相机生命周期
    bool open(std::string_view deviceName = "", bool autoStart = true);  
    bool open(int deviceIndex, bool autoStart = true);
    bool isOpened() const;
    void close(); 
    
    // 捕获控制
    bool start();
    void stop();
    bool isStarted() const;
    
    // 帧捕获
    std::shared_ptr<VideoFrame> grab(uint32_t timeoutInMs = 0xffffffff);
    void setNewFrameCallback(std::function<bool(const std::shared_ptr<VideoFrame>&)> callback);
    
    // 属性配置
    bool set(PropertyName prop, double value);
    template<class T> bool set(PropertyName prop, T value);
    double get(PropertyName prop);
    
    // 设备信息和高级配置
    std::optional<DeviceInfo> getDeviceInfo() const;
    void setFrameAllocator(std::function<std::shared_ptr<Allocator>()> allocatorFactory);
    void setMaxAvailableFrameSize(uint32_t size);
    void setMaxCacheFrameSize(uint32_t size);
};
```

#### ccap::VideoFrame

```cpp
struct VideoFrame {
    
    // 帧数据
    uint8_t* data[3] = {};                  // 原始像素数据平面
    uint32_t stride[3] = {};                // 每个平面的步长
    
    // 帧属性
    PixelFormat pixelFormat = PixelFormat::Unknown;  // 像素格式
    uint32_t width = 0;                     // 帧宽度（像素）
    uint32_t height = 0;                    // 帧高度（像素）
    uint32_t sizeInBytes = 0;               // 帧数据总大小
    uint64_t timestamp = 0;                 // 帧时间戳（纳秒）
    uint64_t frameIndex = 0;                // 唯一递增帧索引
    FrameOrientation orientation = FrameOrientation::Default;  // 帧方向
    
    // 内存管理和平台特性
    std::shared_ptr<Allocator> allocator;   // 内存分配器
    void* nativeHandle = nullptr;           // 平台特定句柄
};
```

#### 配置选项

```cpp
enum class PropertyName {
    Width, Height, FrameRate,
    PixelFormatInternal,        // 相机内部格式
    PixelFormatOutput,          // 输出格式（带转换）
    FrameOrientation
};

enum class PixelFormat : uint32_t {
    Unknown = 0,
    NV12, NV12f,               // YUV 4:2:0 半平面
    I420, I420f,               // YUV 4:2:0 平面
    RGB24, BGR24,              // 24位 RGB/BGR
    RGBA32, BGRA32             // 32位 RGBA/BGRA
};
```

### 工具函数

```cpp
namespace ccap {
    // 硬件能力检测
    bool hasAVX2();
    bool hasAppleAccelerate();
    
    // 后端管理
    ConvertBackend getConvertBackend();
    bool setConvertBackend(ConvertBackend backend);
    
    // 格式工具
    std::string_view pixelFormatToString(PixelFormat format);
    
    // 文件操作
    std::string dumpFrameToFile(VideoFrame* frame, std::string_view filename);
    
    // 日志
    enum class LogLevel { None, Error, Warning, Info, Verbose };
    void setLogLevel(LogLevel level);
}
```

### OpenCV 集成

```cpp
#include <ccap_opencv.h>

auto frame = provider.grab();
cv::Mat mat = ccap::convertRgbFrameToMat(*frame);
```

### 精细配置

```cpp
// 设置特定分辨率
provider.set(ccap::PropertyName::Width, 1920);
provider.set(ccap::PropertyName::Height, 1080);

// 设置相机内部实际使用的格式 (有助于明确行为以及优化性能)
provider.set(ccap::PropertyName::PixelFormatInternal, 
             static_cast<double>(ccap::PixelFormat::NV12));

// 设置相机输出的实际格式
provider.set(ccap::PropertyName::PixelFormatOutput, 
             static_cast<double>(ccap::PixelFormat::BGR24));
```

## 测试

完整的测试套件包含 50 个测试用例，覆盖所有功能：

- 多后端测试（CPU、AVX2、Apple Accelerate）
- 性能基准测试和精度验证
- 像素格式转换 95%+ 精度保证

```bash
./scripts/run_tests.sh
```

## 构建与安装

详细说明请参见 [BUILD_AND_INSTALL.md](./BUILD_AND_INSTALL.md)。

```bash
git clone https://github.com/wysaid/CameraCapture.git
cd CameraCapture
./scripts/build_and_install.sh
```

## 许可证

MIT 许可证。详情请参见 [LICENSE](./LICENSE) 文件。
