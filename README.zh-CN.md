# ccap (CameraCapture)

[![Windows Build](https://github.com/wysaid/CameraCapture/actions/workflows/windows-build.yml/badge.svg)](https://github.com/wysaid/CameraCapture/actions/workflows/windows-build.yml)
[![macOS Build](https://github.com/wysaid/CameraCapture/actions/workflows/macos-build.yml/badge.svg)](https://github.com/wysaid/CameraCapture/actions/workflows/macos-build.yml)
[![Linux Build](https://github.com/wysaid/CameraCapture/actions/workflows/linux-build.yml/badge.svg)](https://github.com/wysaid/CameraCapture/actions/workflows/linux-build.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++17](https://img.shields.io/badge/C++-17-blue.svg)](https://isocpp.org/)
[![C99](https://img.shields.io/badge/C-99-blue.svg)](https://en.wikipedia.org/wiki/C99)
[![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20macOS%20%7C%20iOS%20%7C%20Linux-brightgreen)](https://github.com/wysaid/CameraCapture)

[English](./README.md) | [中文](./README.zh-CN.md)

高性能、轻量级的跨平台相机捕获库，支持硬件加速的像素格式转换，提供完整的 C++ 和纯 C 语言接口。

> 🌐 **官方网站：** [ccap.work](https://ccap.work)

## 目录

- [特性](#特性)
- [快速开始](#快速开始)
- [系统要求](#系统要求)
- [示例代码](#示例代码)
- [API 参考](#api-参考)
- [测试](#测试)
- [构建和安装](#构建和安装)
- [许可证](#许可证)

## 特性

- **高性能**：硬件加速的像素格式转换，提升高达 10 倍性能（AVX2、Apple Accelerate、NEON）
- **轻量级**：零外部依赖，仅使用系统框架
- **跨平台**：Windows（DirectShow）、macOS/iOS（AVFoundation）、Linux（V4L2）
- **多种格式**：RGB、BGR、YUV（NV12/I420）及自动转换
- **双语言接口**：✨ **新增完整纯 C 接口**，同时提供现代化 C++ API 和传统 C99 接口，支持各种项目集成和语言绑定
- **命令行工具**：开箱即用的命令行工具，快速实现相机操作 - 列出设备、捕获图像、实时预览（[文档](./docs/content/cli.zh.md)）
- **生产就绪**：完整测试套件，95%+ 精度验证
- **虚拟相机支持**：兼容 OBS Virtual Camera 等工具

## 快速开始

### 安装

1. 从源码编译并安装 (在 Windows 下需要 git-bash 执行)

    ```bash
    git clone https://github.com/wysaid/CameraCapture.git
    cd CameraCapture
    ./scripts/build_and_install.sh
    ```

2. 使用 CMake FetchContent 直接集成

    在你的 `CMakeLists.txt` 中添加如下内容：

    ```cmake
    include(FetchContent)
    FetchContent_Declare(
        ccap
        GIT_REPOSITORY https://github.com/wysaid/CameraCapture.git
        GIT_TAG        main
    )
    FetchContent_MakeAvailable(ccap)

    target_link_libraries(your_app PRIVATE ccap::ccap)
    ```

    然后即可在你的项目中直接使用 ccap 的头文件和功能。

3. 在 macOS 下使用 Homebrew 安装并使用

    - 首先使用 homebrew 安装二进制:

        ```bash
        brew tap wysaid/ccap
        brew install ccap
        ```

    - 之后可以直接在 cmake 中使用

        ```cmake
        find_package(ccap REQUIRED)
        target_link_libraries(your_app ccap::ccap)
        ```

### 基本用法

ccap 同时提供了完整的 **C++** 和 **纯 C 语言**接口，满足不同项目和开发需求：

- **C++ 接口**：现代化的 C++ API，支持智能指针、lambda 回调等特性
- **纯 C 接口**：完全兼容 C99 标准，支持其他语言绑定和传统 C 项目集成

#### C++ 接口

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

#### 纯 C 接口

```c
#include <ccap_c.h>
#include <ccap_utils_c.h>

int main() {
    // 创建 provider
    CcapProvider* provider = ccap_provider_create();
    if (!provider) return -1;
    
    // 查找可用设备
    CcapDeviceNamesList deviceList;
    if (ccap_provider_find_device_names_list(provider, &deviceList)) {
        printf("找到 %zu 个摄像头设备:\n", deviceList.deviceCount);
        for (size_t i = 0; i < deviceList.deviceCount; i++) {
            printf("  %zu: %s\n", i, deviceList.deviceNames[i]);
        }
    }
    
    // 打开默认相机
    if (ccap_provider_open(provider, NULL, false)) {
        // 设置输出格式
        ccap_provider_set_property(provider, CCAP_PROPERTY_PIXEL_FORMAT_OUTPUT, 
                                   CCAP_PIXEL_FORMAT_BGR24);
        
        // 开始捕获
        if (ccap_provider_start(provider)) {
            // 抓取一帧
            CcapVideoFrame* frame = ccap_provider_grab(provider, 3000);
            if (frame) {
                CcapVideoFrameInfo frameInfo;
                if (ccap_video_frame_get_info(frame, &frameInfo)) {
                    // 获取像素格式字符串
                    char formatStr[64];
                    ccap_pixel_format_to_string(frameInfo.pixelFormat, formatStr, sizeof(formatStr));
                    
                    printf("捕获: %dx%d, 格式=%s\n", 
                           frameInfo.width, frameInfo.height, formatStr);
                }
                ccap_video_frame_release(frame);
            }
        }
        
        ccap_provider_stop(provider);
        ccap_provider_close(provider);
    }
    
    ccap_provider_destroy(provider);
    return 0;
}
```

## 命令行工具

ccap 包含一个功能强大的命令行工具，无需编写代码即可快速进行相机操作：

```bash
# 启用 CLI 工具构建
mkdir build && cd build
cmake .. -DCCAP_BUILD_CLI=ON
cmake --build .

# 列出可用相机
./ccap --list-devices

# 从默认相机捕获 5 张图像
./ccap -c 5 -o ./captures

# 实时预览（需要 GLFW）
./ccap --preview
```

**主要功能：**
- 📷 列出和选择相机设备
- 🎯 捕获单张或多张图像
- 👁️ 实时预览窗口（需要 GLFW）
- ⚙️ 配置分辨率、格式和帧率
- 💾 保存为多种图像格式（JPEG、PNG、BMP 等）
- ⏱️ 基于时长或数量的捕获模式

完整的 CLI 文档请参阅 [CLI 工具指南](./docs/content/cli.zh.md)。

## 系统要求

| 平台 | 编译器 | 系统要求 |
|------|--------|----------|
| **Windows** | MSVC 2019+（包括 2026）/ MinGW-w64 | DirectShow |
| **macOS** | Xcode 11+ | macOS 10.13+ |
| **iOS** | Xcode 11+ | iOS 13.0+ |
| **Linux** | GCC 7+ / Clang 6+ | V4L2 (Linux 2.6+) |

**构建要求**：CMake 3.14+（推荐使用 3.31+ 以支持 MSVC 2026），C++17（C++ 接口），C99（C 接口）

### 支持的 Linux 发行版

- [x] **Ubuntu/Debian** - 所有带有 Linux 2.6+ 内核的版本  
- [x] **CentOS/RHEL/Fedora** - 所有带有 Linux 2.6+ 内核的版本  
- [x] **SUSE/openSUSE** - 所有版本  
- [x] **Arch Linux** - 所有版本  
- [x] **Alpine Linux** - 所有版本  
- [x] **嵌入式 Linux** - 任何支持 V4L2 的发行版  

## 示例代码

| 示例 | 描述 | 语言 | 平台 |
|------|------|------|------|
| [0-print_camera](./examples/desktop/0-print_camera.cpp) / [0-print_camera_c](./examples/desktop/0-print_camera_c.c) | 列出可用相机 | C++ / C | 桌面端 |
| [1-minimal_example](./examples/desktop/1-minimal_example.cpp) / [1-minimal_example_c](./examples/desktop/1-minimal_example_c.c) | 基本帧捕获 | C++ / C | 桌面端 |
| [2-capture_grab](./examples/desktop/2-capture_grab.cpp) / [2-capture_grab_c](./examples/desktop/2-capture_grab_c.c) | 连续捕获 | C++ / C | 桌面端 |
| [3-capture_callback](./examples/desktop/3-capture_callback.cpp) / [3-capture_callback_c](./examples/desktop/3-capture_callback_c.c) | 回调式捕获 | C++ / C | 桌面端 |
| [4-example_with_glfw](./examples/desktop/4-example_with_glfw.cpp) / [4-example_with_glfw_c](./examples/desktop/4-example_with_glfw_c.c) | OpenGL 渲染 | C++ / C | 桌面端 |
| [iOS Demo](./examples/) | iOS 应用程序 | Objective-C++ | iOS |

### 构建和运行示例

```bash
mkdir build && cd build
cmake .. -DCCAP_BUILD_EXAMPLES=ON
cmake --build .

# 运行示例
./0-print_camera
./1-minimal_example
```

```bash
# 运行纯 C 版本（如果你启用了 C 示例构建）
./0-print_camera_c
./1-minimal_example_c
```

> 说明：每个桌面示例均包含 C++ (.cpp) 和纯 C (.c) 两个版本。C 语言版本对应的文件名带有 `_c` 后缀（例如 `1-minimal_example_c.c`）。

## API 参考

ccap 提供完整的 C++ 和纯 C 语言接口，满足不同项目的需求。

### C++ 核心类

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
    bool hasNEON();
    
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

### C 语言接口

ccap 提供完整的纯 C 语言接口，方便 C 项目或需要与其他语言绑定的场景使用。

#### 核心 API

##### Provider 生命周期

```c
// 创建和销毁 Provider
CcapProvider* ccap_provider_create(void);
void ccap_provider_destroy(CcapProvider* provider);

// 设备发现
bool ccap_provider_find_device_names_list(CcapProvider* provider, 
                                          CcapDeviceNamesList* deviceList);

// 设备管理
bool ccap_provider_open(CcapProvider* provider, const char* deviceName, bool autoStart);
bool ccap_provider_open_by_index(CcapProvider* provider, int deviceIndex, bool autoStart);
void ccap_provider_close(CcapProvider* provider);
bool ccap_provider_is_opened(CcapProvider* provider);

// 捕获控制
bool ccap_provider_start(CcapProvider* provider);
void ccap_provider_stop(CcapProvider* provider);
bool ccap_provider_is_started(CcapProvider* provider);
```

##### 帧捕获和处理

```c
// 同步帧捕获
CcapVideoFrame* ccap_provider_grab(CcapProvider* provider, uint32_t timeoutMs);
void ccap_video_frame_release(CcapVideoFrame* frame);

// 异步回调
typedef bool (*CcapNewFrameCallback)(const CcapVideoFrame* frame, void* userData);
void ccap_provider_set_new_frame_callback(CcapProvider* provider, 
                                          CcapNewFrameCallback callback, void* userData);

// 帧信息
typedef struct {
    uint8_t* data[3];           // 像素数据平面
    uint32_t stride[3];         // 每个平面的步长
    uint32_t width;             // 宽度
    uint32_t height;            // 高度
    uint32_t sizeInBytes;       // 总字节数
    uint64_t timestamp;         // 时间戳
    uint64_t frameIndex;        // 帧索引
    CcapPixelFormat pixelFormat; // 像素格式
    CcapFrameOrientation orientation; // 方向
} CcapVideoFrameInfo;

// 设备名称列表
typedef struct {
    char deviceNames[CCAP_MAX_DEVICES][CCAP_MAX_DEVICE_NAME_LENGTH];
    size_t deviceCount;
} CcapDeviceNamesList;

bool ccap_video_frame_get_info(const CcapVideoFrame* frame, CcapVideoFrameInfo* info);
```

##### 属性配置

```c
// 属性设置和获取
bool ccap_provider_set_property(CcapProvider* provider, CcapPropertyName prop, double value);
double ccap_provider_get_property(CcapProvider* provider, CcapPropertyName prop);

// 主要属性
typedef enum {
    CCAP_PROPERTY_WIDTH = 0x10001,
    CCAP_PROPERTY_HEIGHT = 0x10002,
    CCAP_PROPERTY_FRAME_RATE = 0x20000,
    CCAP_PROPERTY_PIXEL_FORMAT_OUTPUT = 0x30002,
    CCAP_PROPERTY_FRAME_ORIENTATION = 0x40000
} CcapPropertyName;

// 像素格式
typedef enum {
    CCAP_PIXEL_FORMAT_UNKNOWN = 0,
    CCAP_PIXEL_FORMAT_NV12 = 1 | (1 << 16),
    CCAP_PIXEL_FORMAT_NV12F = CCAP_PIXEL_FORMAT_NV12 | (1 << 17),
    CCAP_PIXEL_FORMAT_RGB24 = (1 << 3) | (1 << 18),
    CCAP_PIXEL_FORMAT_BGR24 = (1 << 4) | (1 << 18),
    CCAP_PIXEL_FORMAT_RGBA32 = CCAP_PIXEL_FORMAT_RGB24 | (1 << 19),
    CCAP_PIXEL_FORMAT_BGRA32 = CCAP_PIXEL_FORMAT_BGR24 | (1 << 19)
} CcapPixelFormat;
```

#### 编译和链接

##### macOS

```bash
gcc -std=c99 your_code.c -o your_app \
    -I/path/to/ccap/include \
    -L/path/to/ccap/lib -lccap \
    -framework Foundation -framework AVFoundation \
    -framework CoreMedia -framework CoreVideo
```

##### Windows (MSVC)

```cmd
cl your_code.c /I"path\to\ccap\include" \
   /link "path\to\ccap\lib\ccap.lib" ole32.lib oleaut32.lib uuid.lib
```

##### Linux

```bash
gcc -std=c99 your_code.c -o your_app \
    -I/path/to/ccap/include \
    -L/path/to/ccap/lib -lccap \
    -lpthread
```

#### 完整文档

C 接口的详细使用说明和示例请参见：[C 接口文档](./docs/C_Interface.md)

**额外的 C 工具函数**：如需像素格式字符串转换和文件 I/O 功能，还需包含：

- `#include <ccap_utils_c.h>` - 提供 `ccap_pixel_format_to_string()`、`ccap_dump_frame_to_file()` 等函数
- `#include <ccap_convert_c.h>` - 提供像素格式转换函数

## 测试

完整的测试套件包含 50+ 测试用例，覆盖所有功能：

- 多后端测试（CPU、AVX2、Apple Accelerate、NEON）
- 性能基准测试和精度验证
- 像素格式转换 95%+ 精度

```bash
./scripts/run_tests.sh
```

## 构建和安装

完整的构建和安装说明请参见 [BUILD_AND_INSTALL.md](./BUILD_AND_INSTALL.md)。

```bash
git clone https://github.com/wysaid/CameraCapture.git
cd CameraCapture
./scripts/build_and_install.sh
```

## 许可证

MIT 许可证。详情请参见 [LICENSE](./LICENSE)。
