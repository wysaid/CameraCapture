# ccap (CameraCapture)

[![Windows Build](https://github.com/wysaid/CameraCapture/actions/workflows/windows-build.yml/badge.svg)](https://github.com/wysaid/CameraCapture/actions/workflows/windows-build.yml)
[![macOS Build](https://github.com/wysaid/CameraCapture/actions/workflows/macos-build.yml/badge.svg)](https://github.com/wysaid/CameraCapture/actions/workflows/macos-build.yml)
[![Linux Build](https://github.com/wysaid/CameraCapture/actions/workflows/linux-build.yml/badge.svg)](https://github.com/wysaid/CameraCapture/actions/workflows/linux-build.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++17](https://img.shields.io/badge/C++-17-blue.svg)](https://isocpp.org/)
[![C99](https://img.shields.io/badge/C-99-blue.svg)](https://en.wikipedia.org/wiki/C99)
[![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20macOS%20%7C%20iOS%20%7C%20Linux-brightgreen)](https://github.com/wysaid/CameraCapture)

[English](./README.md) | [中文](./README.zh-CN.md)

A high-performance, lightweight cross-platform camera capture library with hardware-accelerated pixel format conversion, providing complete C++ and pure C language interfaces.

## Table of Contents

- [Features](#features)
- [Quick Start](#quick-start)
- [System Requirements](#system-requirements)
- [Examples](#examples)
- [API Reference](#api-reference)
- [Testing](#testing)
- [Build and Install](#build-and-install)
- [License](#license)

## Features

- **High Performance**: Hardware-accelerated pixel format conversion with up to 10x speedup (AVX2, Apple Accelerate)
- **Lightweight**: Zero external dependencies - uses only system frameworks
- **Cross Platform**: Windows (DirectShow), macOS/iOS (AVFoundation), Linux (V4L2)
- **Multiple Formats**: RGB, BGR, YUV (NV12/I420) with automatic conversion
- **Dual Language APIs**: ✨ **New Complete Pure C Interface** - Both modern C++ API and traditional C99 interface for various project integration and language bindings
- **Production Ready**: Comprehensive test suite with 95%+ accuracy validation
- **Virtual Camera Support**: Compatible with OBS Virtual Camera and similar tools

## Quick Start

### Installation

1. Build and install from source (on Windows, use git-bash):

    ```bash
    git clone https://github.com/wysaid/CameraCapture.git
    cd CameraCapture
    ./scripts/build_and_install.sh
    ```

2. Integrate directly using CMake FetchContent:

    Add the following to your `CMakeLists.txt`:

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

    You can then use ccap headers and features directly in your project.

3. Install and use via Homebrew on macOS:

    - First, install the binary with Homebrew:

        ```bash
        brew tap wysaid/ccap
        brew install ccap
        ```

    - Then, use it in CMake:

        ```cmake
        find_package(ccap REQUIRED)
        target_link_libraries(your_app ccap::ccap)
        ```

### Basic Usage

ccap provides both complete **C++** and **pure C language** interfaces to meet different project and development requirements:

- **C++ Interface**: Modern C++ API with smart pointers, lambda callbacks, and other advanced features
- **Pure C Interface**: Fully compatible with C99 standard, supporting language bindings and traditional C project integration

#### C++ Interface

```cpp
#include <ccap.h>

int main() {
    ccap::Provider provider;
    
    // List available cameras
    auto devices = provider.findDeviceNames();
    for (size_t i = 0; i < devices.size(); ++i) {
        printf("[%zu] %s\n", i, devices[i].c_str());
    }
    
    // Open and start camera
    if (provider.open("", true)) {  // Empty string = default camera
        auto frame = provider.grab(3000);  // 3 second timeout
        if (frame) {
            printf("Captured: %dx%d, %s format\n", 
                   frame->width, frame->height,
                   ccap::pixelFormatToString(frame->pixelFormat).data());
        }
    }
    return 0;
}
```

#### Pure C Interface

```c
#include <ccap_c.h>

int main() {
    // Create provider
    CcapProvider* provider = ccap_provider_create();
    if (!provider) return -1;
    
    // Find available devices
    char** deviceNames;
    size_t deviceCount;
    if (ccap_provider_find_device_names(provider, &deviceNames, &deviceCount)) {
        printf("Found %zu camera device(s):\n", deviceCount);
        for (size_t i = 0; i < deviceCount; i++) {
            printf("  %zu: %s\n", i, deviceNames[i]);
        }
        ccap_provider_free_device_names(deviceNames, deviceCount);
    }
    
    // Open default camera
    if (ccap_provider_open(provider, NULL, false)) {
        // Set output format
        ccap_provider_set_property(provider, CCAP_PROPERTY_PIXEL_FORMAT_OUTPUT, 
                                   CCAP_PIXEL_FORMAT_BGR24);
        
        // Start capture
        if (ccap_provider_start(provider)) {
            // Grab a frame
            CcapVideoFrame* frame = ccap_provider_grab(provider, 3000);
            if (frame) {
                CcapVideoFrameInfo frameInfo;
                if (ccap_video_frame_get_info(frame, &frameInfo)) {
                    printf("Captured: %dx%d, format=%d\n", 
                           frameInfo.width, frameInfo.height, frameInfo.pixelFormat);
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

## System Requirements

| Platform | Compiler | System Requirements |
|----------|----------|---------------------|
| **Windows** | MSVC 2019+ | DirectShow |
| **macOS** | Xcode 11+ | macOS 10.13+ |
| **iOS** | Xcode 11+ | iOS 13.0+ |
| **Linux** | GCC 7+ / Clang 6+ | V4L2 (Linux 2.6+) |

**Build Requirements**: CMake 3.14+, C++17 (C++ interface), C99 (C interface)

### Supported Linux Distributions

- [x] **Ubuntu/Debian** - All versions with Linux 2.6+ kernel  
- [x] **CentOS/RHEL/Fedora** - All versions with Linux 2.6+ kernel  
- [x] **SUSE/openSUSE** - All versions with Linux 2.6+ kernel  
- [x] **Arch Linux** - All versions  
- [x] **Alpine Linux** - All versions  
- [x] **Embedded Linux** - Any distribution with V4L2 support  

## Examples

| Example | Description | Language | Platform |
|---------|-------------|----------|----------|
| [0-print_camera](./examples/desktop/0-print_camera.cpp) | List available cameras | C++ | Desktop |
| [0-print_camera_c](./examples/desktop/0-print_camera_c.c) | List available cameras | C | Desktop |
| [1-minimal_example](./examples/desktop/1-minimal_example.cpp) | Basic frame capture | C++ | Desktop |
| [2-capture_grab](./examples/desktop/2-capture_grab.cpp) | Continuous capture | C++ | Desktop |
| [3-capture_callback](./examples/desktop/3-capture_callback.cpp) | Callback-based capture | C++ | Desktop |
| [4-example_with_glfw](./examples/desktop/4-example_with_glfw.cpp) | OpenGL rendering | C++ | Desktop |
| [iOS Demo](./examples/) | iOS application | Objective-C++ | iOS |

### Build and Run Examples

```bash
mkdir build && cd build
cmake .. -DCCAP_BUILD_EXAMPLES=ON
cmake --build .

# Run examples
./0-print_camera
./1-minimal_example
```

## API Reference

ccap provides both complete C++ and pure C interfaces to meet different project requirements.

### Core Classes

#### ccap::Provider

```cpp
class Provider {
public:
    // Constructors
    Provider();
    Provider(std::string_view deviceName, std::string_view extraInfo = "");
    Provider(int deviceIndex, std::string_view extraInfo = "");
    
    // Device discovery
    std::vector<std::string> findDeviceNames();
    
    // Camera lifecycle
    bool open(std::string_view deviceName = "", bool autoStart = true);
    bool open(int deviceIndex, bool autoStart = true);
    bool isOpened() const;
    void close();
    
    // Capture control
    bool start();
    void stop();
    bool isStarted() const;
    
    // Frame capture
    std::shared_ptr<VideoFrame> grab(uint32_t timeoutInMs = 0xffffffff);
    void setNewFrameCallback(std::function<bool(const std::shared_ptr<VideoFrame>&)> callback);
    
    // Property configuration
    bool set(PropertyName prop, double value);
    template<class T> bool set(PropertyName prop, T value);
    double get(PropertyName prop);
    
    // Device info and advanced configuration
    std::optional<DeviceInfo> getDeviceInfo() const;
    void setFrameAllocator(std::function<std::shared_ptr<Allocator>()> allocatorFactory);
    void setMaxAvailableFrameSize(uint32_t size);
    void setMaxCacheFrameSize(uint32_t size);
};
```

#### ccap::VideoFrame

```cpp
struct VideoFrame {
    
    // Frame data
    uint8_t* data[3] = {};                  // Raw pixel data planes
    uint32_t stride[3] = {};                // Stride for each plane
    
    // Frame properties
    PixelFormat pixelFormat = PixelFormat::Unknown;  // Pixel format
    uint32_t width = 0;                     // Frame width in pixels
    uint32_t height = 0;                    // Frame height in pixels
    uint32_t sizeInBytes = 0;               // Total frame data size
    uint64_t timestamp = 0;                 // Frame timestamp in nanoseconds
    uint64_t frameIndex = 0;                // Unique incremental frame index
    FrameOrientation orientation = FrameOrientation::Default;  // Frame orientation
    
    // Memory management and platform features
    std::shared_ptr<Allocator> allocator;   // Memory allocator
    void* nativeHandle = nullptr;           // Platform-specific handle
};
```

#### Configuration

```cpp
enum class PropertyName {
    Width, Height, FrameRate,
    PixelFormatInternal,        // Camera's internal format
    PixelFormatOutput,          // Output format (with conversion)
    FrameOrientation
};

enum class PixelFormat : uint32_t {
    Unknown = 0,
    NV12, NV12f,               // YUV 4:2:0 semi-planar
    I420, I420f,               // YUV 4:2:0 planar  
    RGB24, BGR24,              // 24-bit RGB/BGR
    RGBA32, BGRA32             // 32-bit RGBA/BGRA
};
```

### Utility Functions

```cpp
namespace ccap {
    // Hardware capabilities
    bool hasAVX2();
    bool hasAppleAccelerate();
    
    // Backend management
    ConvertBackend getConvertBackend();
    bool setConvertBackend(ConvertBackend backend);
    
    // Format utilities
    std::string_view pixelFormatToString(PixelFormat format);
    
    // File operations
    std::string dumpFrameToFile(VideoFrame* frame, std::string_view filename);
    
    // Logging
    enum class LogLevel { None, Error, Warning, Info, Verbose };
    void setLogLevel(LogLevel level);
}
```

### OpenCV Integration

```cpp
#include <ccap_opencv.h>

auto frame = provider.grab();
cv::Mat mat = ccap::convertRgbFrameToMat(*frame);
```

### Fine-tuned Configuration

```cpp
// Set specific resolution
provider.set(ccap::PropertyName::Width, 1920);
provider.set(ccap::PropertyName::Height, 1080);

// Set camera's internal format (helps clarify behavior and optimize performance)
provider.set(ccap::PropertyName::PixelFormatInternal, 
             static_cast<double>(ccap::PixelFormat::NV12));

// Set camera's output format
provider.set(ccap::PropertyName::PixelFormatOutput, 
             static_cast<double>(ccap::PixelFormat::BGR24));
```

## Testing

Comprehensive test suite with 50+ test cases covering all functionality:

- Multi-backend testing (CPU, AVX2, Apple Accelerate)
- Performance benchmarks and accuracy validation  
- 95%+ precision for pixel format conversions

```bash
./scripts/run_tests.sh
```

## Build and Install

See [BUILD_AND_INSTALL.md](./BUILD_AND_INSTALL.md) for complete instructions.

```bash
git clone https://github.com/wysaid/CameraCapture.git
cd CameraCapture
./scripts/build_and_install.sh
```

## License

MIT License. See [LICENSE](./LICENSE) for details.

### C Language Interface

ccap provides a complete pure C language interface for C projects or scenarios requiring language bindings.

#### Core API

##### Provider Lifecycle

```c
// Create and destroy Provider
CcapProvider* ccap_provider_create(void);
void ccap_provider_destroy(CcapProvider* provider);

// Device discovery
bool ccap_provider_find_device_names(CcapProvider* provider, 
                                     char*** deviceNames, size_t* count);
void ccap_provider_free_device_names(char** deviceNames, size_t count);

// Device management
bool ccap_provider_open(CcapProvider* provider, const char* deviceName, bool autoStart);
bool ccap_provider_open_by_index(CcapProvider* provider, int deviceIndex, bool autoStart);
void ccap_provider_close(CcapProvider* provider);
bool ccap_provider_is_opened(CcapProvider* provider);

// Capture control
bool ccap_provider_start(CcapProvider* provider);
void ccap_provider_stop(CcapProvider* provider);
bool ccap_provider_is_started(CcapProvider* provider);
```

##### Frame Capture and Processing

```c
// Synchronous frame capture
CcapVideoFrame* ccap_provider_grab(CcapProvider* provider, uint32_t timeoutMs);
void ccap_video_frame_release(CcapVideoFrame* frame);

// Asynchronous callback
typedef bool (*CcapNewFrameCallback)(const CcapVideoFrame* frame, void* userData);
void ccap_provider_set_new_frame_callback(CcapProvider* provider, 
                                          CcapNewFrameCallback callback, void* userData);

// Frame information
typedef struct {
    uint8_t* data[3];           // Pixel data planes
    uint32_t stride[3];         // Stride for each plane
    uint32_t width;             // Width
    uint32_t height;            // Height
    uint32_t sizeInBytes;       // Total bytes
    uint64_t timestamp;         // Timestamp
    uint64_t frameIndex;        // Frame index
    CcapPixelFormat pixelFormat; // Pixel format
    CcapFrameOrientation orientation; // Orientation
} CcapVideoFrameInfo;

bool ccap_video_frame_get_info(const CcapVideoFrame* frame, CcapVideoFrameInfo* info);
```

##### Property Configuration

```c
// Property setting and getting
bool ccap_provider_set_property(CcapProvider* provider, CcapPropertyName prop, double value);
double ccap_provider_get_property(CcapProvider* provider, CcapPropertyName prop);

// Main properties
typedef enum {
    CCAP_PROPERTY_WIDTH = 0x10001,
    CCAP_PROPERTY_HEIGHT = 0x10002,
    CCAP_PROPERTY_FRAME_RATE = 0x20000,
    CCAP_PROPERTY_PIXEL_FORMAT_OUTPUT = 0x30002,
    CCAP_PROPERTY_FRAME_ORIENTATION = 0x40000
} CcapPropertyName;

// Pixel formats
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

#### Compilation and Linking

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
   /link "path\to\ccap\lib\ccap.lib" strmiids.lib ole32.lib oleaut32.lib uuid.lib
```

##### Linux

```bash
gcc -std=c99 your_code.c -o your_app \
    -I/path/to/ccap/include \
    -L/path/to/ccap/lib -lccap \
    -lpthread
```

#### Complete Documentation

For detailed usage instructions and examples of the C interface, see: [C Interface Documentation](./docs/C_Interface.md)
