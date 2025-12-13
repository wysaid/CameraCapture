# ccap Documentation

**ccap** is a high-performance, lightweight cross-platform camera capture library with hardware-accelerated pixel format conversion. It provides both modern C++ and pure C99 interfaces for maximum compatibility.

## Key Features

- Zero external dependencies - uses only system frameworks
- Hardware-accelerated format conversion (AVX2, Apple Accelerate, NEON)
- Cross-platform: Windows, macOS, iOS, Linux
- Dual API: Modern C++17 and pure C99

## Installation

### Build from Source

```bash
git clone https://github.com/wysaid/CameraCapture.git
cd CameraCapture
./scripts/build_and_install.sh
```

### CMake FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(
    ccap
    GIT_REPOSITORY https://github.com/wysaid/CameraCapture.git
    GIT_TAG main
)
FetchContent_MakeAvailable(ccap)

target_link_libraries(your_app PRIVATE ccap::ccap)
```

### Homebrew (macOS)

```bash
brew tap wysaid/ccap
brew install ccap
```

## Basic Usage

### C++ Example

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
    if (provider.open("", true)) {
        auto frame = provider.grab(3000);
        if (frame) {
            printf("Captured: %dx%d\n", frame->width, frame->height);
        }
    }
    return 0;
}
```

## C++ API Reference

### ccap::Provider

```cpp
class Provider {
public:
    // Device discovery
    std::vector<std::string> findDeviceNames();
    
    // Device management
    bool open(std::string_view deviceName, bool autoStart = true);
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
    
    // Device info
    std::optional<DeviceInfo> getDeviceInfo() const;
};
```

### ccap::VideoFrame

```cpp
struct VideoFrame {
    uint8_t* data[3] = {};              // Raw pixel data planes
    uint32_t stride[3] = {};            // Stride for each plane
    
    PixelFormat pixelFormat = PixelFormat::Unknown;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t sizeInBytes = 0;
    uint64_t timestamp = 0;             // Timestamp in nanoseconds
    uint64_t frameIndex = 0;
    FrameOrientation orientation = FrameOrientation::Default;
    
    std::shared_ptr<Allocator> allocator;
    void* nativeHandle = nullptr;
};
```

### Property Configuration

```cpp
// Set specific resolution
provider.set(ccap::PropertyName::Width, 1920);
provider.set(ccap::PropertyName::Height, 1080);

// Set camera's internal format
provider.set(ccap::PropertyName::PixelFormatInternal, 
             static_cast<double>(ccap::PixelFormat::NV12));

// Set camera's output format
provider.set(ccap::PropertyName::PixelFormatOutput, 
             static_cast<double>(ccap::PixelFormat::BGR24));
```

## OpenCV Integration

```cpp
#include <ccap_opencv.h>

auto frame = provider.grab();
cv::Mat mat = ccap::convertRgbFrameToMat(*frame);
```

## Properties

| Property | Description |
|----------|-------------|
| `Width` | Frame width in pixels |
| `Height` | Frame height in pixels |
| `FrameRate` | Capture frame rate |
| `PixelFormatInternal` | Camera's internal pixel format |
| `PixelFormatOutput` | Output pixel format (with conversion) |
| `FrameOrientation` | Frame orientation/rotation |

## Pixel Formats

| Format | Description |
|--------|-------------|
| `NV12` | YUV 4:2:0 semi-planar |
| `NV12f` | YUV 4:2:0 semi-planar (full range) |
| `I420` | YUV 4:2:0 planar |
| `I420f` | YUV 4:2:0 planar (full range) |
| `RGB24` | 24-bit RGB |
| `BGR24` | 24-bit BGR |
| `RGBA32` | 32-bit RGBA |
| `BGRA32` | 32-bit BGRA |

## Platform Guides

### Windows

Uses DirectShow for camera access. Requires MSVC 2019 or later.

```cmd
cl your_code.c /I"path\to\ccap\include" ^
   /link "path\to\ccap\lib\ccap.lib" ole32.lib oleaut32.lib uuid.lib
```

### macOS

Uses AVFoundation for camera access. Requires Xcode 11+ and macOS 10.13+.

```bash
gcc -std=c99 your_code.c -o your_app \
    -I/path/to/ccap/include \
    -L/path/to/ccap/lib -lccap \
    -framework Foundation -framework AVFoundation \
    -framework CoreMedia -framework CoreVideo
```

### Linux

Uses V4L2 for camera access. Requires GCC 7+ or Clang 6+.

```bash
gcc -std=c99 your_code.c -o your_app \
    -I/path/to/ccap/include \
    -L/path/to/ccap/lib -lccap \
    -lpthread
```

### iOS

Uses AVFoundation for camera access. Requires Xcode 11+ and iOS 13.0+.

**CocoaPods:**

```ruby
pod 'ccap'
```

**Camera Permission** - Add the following to your Info.plist:

```xml
<key>NSCameraUsageDescription</key>
<string>This app requires camera access to capture video.</string>
```
