# ccap 文档

**ccap** 是一个高性能、轻量级的跨平台相机捕获库，支持硬件加速的像素格式转换。它同时提供现代 C++ 和纯 C99 接口，以实现最大兼容性。

## 主要特性

- 零外部依赖 - 仅使用系统框架
- 硬件加速格式转换（AVX2、Apple Accelerate、NEON）
- 跨平台：Windows、macOS、iOS、Linux
- 双 API：现代 C++17 和纯 C99
- 用于脚本和自动化的命令行工具

## 快速链接

- [CLI 工具文档](cli.zh.html) - 相机操作的命令行界面
- [C 接口文档](https://github.com/wysaid/CameraCapture/blob/main/docs/C_Interface.md) - C99 API 参考
- [CMake 构建选项](https://github.com/wysaid/CameraCapture/blob/main/docs/CMAKE_OPTIONS.md) - 构建配置详情

## 安装

### 从源码构建

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

## 基本用法

### C++ 示例

```cpp
#include <ccap.h>

int main() {
    ccap::Provider provider;
    
    // 列出可用摄像头
    auto devices = provider.findDeviceNames();
    for (size_t i = 0; i < devices.size(); ++i) {
        printf("[%zu] %s\n", i, devices[i].c_str());
    }
    
    // 打开并启动摄像头
    if (provider.open("", true)) {
        auto frame = provider.grab(3000);
        if (frame) {
            printf("已捕获: %dx%d\n", frame->width, frame->height);
        }
    }
    return 0;
}
```

## C++ API 参考

### ccap::Provider

```cpp
class Provider {
public:
    // 设备发现
    std::vector<std::string> findDeviceNames();
    
    // 设备管理
    bool open(std::string_view deviceName, bool autoStart = true);
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
    
    // 设备信息
    std::optional<DeviceInfo> getDeviceInfo() const;
};
```

### ccap::VideoFrame

```cpp
struct VideoFrame {
    uint8_t* data[3] = {};              // 原始像素数据平面
    uint32_t stride[3] = {};            // 每个平面的步长
    
    PixelFormat pixelFormat = PixelFormat::Unknown;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t sizeInBytes = 0;
    uint64_t timestamp = 0;             // 时间戳（纳秒）
    uint64_t frameIndex = 0;
    FrameOrientation orientation = FrameOrientation::Default;
    
    std::shared_ptr<Allocator> allocator;
    void* nativeHandle = nullptr;
};
```

### 属性配置

```cpp
// 设置分辨率
provider.set(ccap::PropertyName::Width, 1920);
provider.set(ccap::PropertyName::Height, 1080);

// 设置相机内部格式
provider.set(ccap::PropertyName::PixelFormatInternal, 
             static_cast<double>(ccap::PixelFormat::NV12));

// 设置输出格式
provider.set(ccap::PropertyName::PixelFormatOutput, 
             static_cast<double>(ccap::PixelFormat::BGR24));
```

## OpenCV 集成

```cpp
#include <ccap_opencv.h>

auto frame = provider.grab();
cv::Mat mat = ccap::convertRgbFrameToMat(*frame);
```

## 属性列表

| 属性 | 描述 |
|------|------|
| `Width` | 帧宽度（像素） |
| `Height` | 帧高度（像素） |
| `FrameRate` | 捕获帧率 |
| `PixelFormatInternal` | 相机内部像素格式 |
| `PixelFormatOutput` | 输出像素格式（带转换） |
| `FrameOrientation` | 帧方向/旋转 |

## 像素格式

| 格式 | 描述 |
|------|------|
| `NV12` | YUV 4:2:0 半平面 |
| `NV12f` | YUV 4:2:0 半平面（全范围） |
| `I420` | YUV 4:2:0 平面 |
| `I420f` | YUV 4:2:0 平面（全范围） |
| `RGB24` | 24 位 RGB |
| `BGR24` | 24 位 BGR |
| `RGBA32` | 32 位 RGBA |
| `BGRA32` | 32 位 BGRA |

## 平台指南

### Windows

使用 DirectShow 访问相机。需要 MSVC 2019 或更高版本。

```shell
cl your_code.c /I"path\to\ccap\include" ^
   /link "path\to\ccap\lib\ccap.lib" ole32.lib oleaut32.lib uuid.lib
```

### macOS

使用 AVFoundation 访问相机。需要 Xcode 11+ 和 macOS 10.13+。

```bash
gcc -std=c99 your_code.c -o your_app \
    -I/path/to/ccap/include \
    -L/path/to/ccap/lib -lccap \
    -framework Foundation -framework AVFoundation \
    -framework CoreMedia -framework CoreVideo
```

### Linux

使用 V4L2 访问相机。需要 GCC 7+ 或 Clang 6+。

```bash
gcc -std=c99 your_code.c -o your_app \
    -I/path/to/ccap/include \
    -L/path/to/ccap/lib -lccap \
    -lpthread
```

### iOS

使用 AVFoundation 访问相机。需要 Xcode 11+ 和 iOS 13.0+。

**CocoaPods:**

```ruby
pod 'ccap'
```

**相机权限** - 在 Info.plist 中添加以下内容：

```xml
<key>NSCameraUsageDescription</key>
<string>此应用需要访问相机以捕获视频。</string>
```
