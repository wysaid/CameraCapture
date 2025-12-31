# ccap C 接口

本文档介绍如何使用 ccap 库的纯 C 语言接口。

## 概述

ccap C 接口为 C 语言程序提供完整的相机捕获功能，包括：

- 设备发现和管理
- 相机配置和控制
- 同步和异步帧捕获
- 内存管理

## 核心概念

### 不透明指针

C 接口使用不透明指针隐藏 C++ 对象实现细节：

- `CcapProvider*` - 封装 `ccap::Provider` 对象
- `CcapVideoFrame*` - 封装 `ccap::VideoFrame` 共享指针

### 内存管理

C 接口遵循以下内存管理原则：

1. **创建和销毁**：通过 `ccap_xxx_create()` 创建的所有对象必须通过相应的 `ccap_xxx_destroy()` 释放
2. **数组释放**：返回的字符串数组和结构体数组有专用的释放函数
3. **帧管理**：通过 `ccap_provider_grab()` 获取的帧必须通过 `ccap_video_frame_release()` 释放

## 基本使用流程

### 1. 创建 Provider

```c
#include "ccap_c.h"

// 创建 provider
CcapProvider* provider = ccap_provider_create();
if (!provider) {
    printf("创建 provider 失败\n");
    return -1;
}
```

### 2. 发现设备

```c
// 查找可用设备
CcapDeviceNamesList deviceList;
if (ccap_provider_find_device_names_list(provider, &deviceList)) {
    printf("找到 %zu 个设备：\n", deviceList.deviceCount);
    for (size_t i = 0; i < deviceList.deviceCount; i++) {
        printf("  %zu: %s\n", i, deviceList.deviceNames[i]);
    }
}
```

### 3. 打开设备

```c
// 打开默认设备
if (!ccap_provider_open(provider, NULL, false)) {
    printf("打开相机失败\n");
    ccap_provider_destroy(provider);
    return -1;
}

// 或通过索引打开
// ccap_provider_open_by_index(provider, 0, false);
```

### 4. 配置相机属性

```c
// 设置分辨率和帧率
ccap_provider_set_property(provider, CCAP_PROPERTY_WIDTH, 640);
ccap_provider_set_property(provider, CCAP_PROPERTY_HEIGHT, 480);
ccap_provider_set_property(provider, CCAP_PROPERTY_FRAME_RATE, 30.0);

// 设置像素格式
ccap_provider_set_property(provider, CCAP_PROPERTY_PIXEL_FORMAT_OUTPUT, 
                           CCAP_PIXEL_FORMAT_BGR24);
```

### 5. 启动捕获

```c
if (!ccap_provider_start(provider)) {
    printf("启动捕获失败\n");
    ccap_provider_close(provider);
    ccap_provider_destroy(provider);
    return -1;
}
```

### 6. 捕获帧（同步方式）

```c
// 同步捕获帧（3秒超时）
CcapVideoFrame* frame = ccap_provider_grab(provider, 3000);
if (frame) {
    // 获取帧信息
    CcapVideoFrameInfo info;
    ccap_video_frame_get_info(frame, &info);
    
    printf("捕获帧：%dx%d, 格式=%d\n", 
           info.width, info.height, info.pixelFormat);
    
    // 访问像素数据
    uint8_t* data = ccap_video_frame_get_data(frame, 0);
    uint32_t stride = ccap_video_frame_get_stride(frame, 0);
    
    // 处理帧数据...
    
    // 释放帧
    ccap_video_frame_release(frame);
}
```

### 7. 捕获帧（异步回调方式）

```c
// 定义回调函数
bool frame_callback(CcapVideoFrame* frame, void* userData) {
    CcapVideoFrameInfo info;
    ccap_video_frame_get_info(frame, &info);
    
    printf("回调接收帧：%dx%d\n", info.width, info.height);
    
    // 处理帧数据...
    
    return true;  // 继续接收帧
}

// 设置回调
ccap_provider_set_frame_callback(provider, frame_callback, NULL);

// 回调会自动接收帧，无需调用 grab()
```

### 8. 清理资源

```c
// 停止捕获
ccap_provider_stop(provider);

// 关闭设备
ccap_provider_close(provider);

// 释放设备名称列表
ccap_provider_free_device_names_list(&deviceList);

// 销毁 provider
ccap_provider_destroy(provider);
```

## 完整示例

### 基础示例

```c
#include "ccap_c.h"
#include <stdio.h>

int main() {
    // 创建 provider
    CcapProvider* provider = ccap_provider_create();
    if (!provider) {
        printf("创建 provider 失败\n");
        return -1;
    }
    
    // 列出设备
    CcapDeviceNamesList deviceList;
    if (ccap_provider_find_device_names_list(provider, &deviceList)) {
        printf("可用相机：\n");
        for (size_t i = 0; i < deviceList.deviceCount; i++) {
            printf("[%zu] %s\n", i, deviceList.deviceNames[i]);
        }
    }
    
    // 打开并启动
    if (ccap_provider_open(provider, NULL, true)) {
        // 捕获一帧
        CcapVideoFrame* frame = ccap_provider_grab(provider, 3000);
        if (frame) {
            CcapVideoFrameInfo info;
            ccap_video_frame_get_info(frame, &info);
            printf("已捕获：%dx%d\n", info.width, info.height);
            ccap_video_frame_release(frame);
        }
    }
    
    // 清理
    ccap_provider_free_device_names_list(&deviceList);
    ccap_provider_destroy(provider);
    
    return 0;
}
```

### 回调模式示例

```c
#include "ccap_c.h"
#include <stdio.h>
#include <stdbool.h>

static volatile bool g_running = true;
static int g_frameCount = 0;

bool callback(CcapVideoFrame* frame, void* userData) {
    CcapVideoFrameInfo info;
    ccap_video_frame_get_info(frame, &info);
    
    g_frameCount++;
    printf("帧 %d: %dx%d\n", g_frameCount, info.width, info.height);
    
    // 捕获 10 帧后停止
    if (g_frameCount >= 10) {
        g_running = false;
        return false;
    }
    
    return true;
}

int main() {
    CcapProvider* provider = ccap_provider_create();
    
    if (ccap_provider_open(provider, NULL, false)) {
        ccap_provider_set_frame_callback(provider, callback, NULL);
        ccap_provider_start(provider);
        
        // 等待回调处理帧
        while (g_running) {
            // 在实际应用中，这里应该有适当的事件循环
            usleep(100000);  // 100ms
        }
        
        ccap_provider_stop(provider);
    }
    
    ccap_provider_destroy(provider);
    return 0;
}
```

## API 参考

### Provider 管理

#### 创建和销毁

```c
// 创建 provider
CcapProvider* ccap_provider_create(void);

// 销毁 provider
void ccap_provider_destroy(CcapProvider* provider);
```

#### 设备发现

```c
// 获取设备名称列表
bool ccap_provider_find_device_names_list(
    CcapProvider* provider, 
    CcapDeviceNamesList* outList);

// 释放设备名称列表
void ccap_provider_free_device_names_list(CcapDeviceNamesList* list);

// 获取设备信息
bool ccap_provider_get_device_info(
    CcapProvider* provider,
    CcapDeviceInfo* outInfo);
```

#### 设备控制

```c
// 打开设备（通过名称）
bool ccap_provider_open(
    CcapProvider* provider,
    const char* deviceName,  // NULL 表示默认设备
    bool autoStart);

// 打开设备（通过索引）
bool ccap_provider_open_by_index(
    CcapProvider* provider,
    int deviceIndex,
    bool autoStart);

// 关闭设备
void ccap_provider_close(CcapProvider* provider);

// 检查是否已打开
bool ccap_provider_is_opened(const CcapProvider* provider);

// 启动捕获
bool ccap_provider_start(CcapProvider* provider);

// 停止捕获
void ccap_provider_stop(CcapProvider* provider);

// 检查是否正在捕获
bool ccap_provider_is_started(const CcapProvider* provider);
```

### 帧捕获

#### 同步捕获

```c
// 抓取一帧（阻塞）
CcapVideoFrame* ccap_provider_grab(
    CcapProvider* provider,
    uint32_t timeoutMs);  // 超时时间（毫秒）
```

#### 异步捕获

```c
// 帧回调函数类型
typedef bool (*CcapFrameCallback)(
    CcapVideoFrame* frame,
    void* userData);

// 设置帧回调
void ccap_provider_set_frame_callback(
    CcapProvider* provider,
    CcapFrameCallback callback,
    void* userData);
```

### 属性配置

```c
// 设置属性
bool ccap_provider_set_property(
    CcapProvider* provider,
    CcapPropertyName property,
    double value);

// 获取属性
double ccap_provider_get_property(
    const CcapProvider* provider,
    CcapPropertyName property);
```

### VideoFrame 操作

```c
// 获取帧信息
void ccap_video_frame_get_info(
    const CcapVideoFrame* frame,
    CcapVideoFrameInfo* outInfo);

// 获取数据指针（平面索引）
uint8_t* ccap_video_frame_get_data(
    const CcapVideoFrame* frame,
    int planeIndex);

// 获取步长
uint32_t ccap_video_frame_get_stride(
    const CcapVideoFrame* frame,
    int planeIndex);

// 释放帧
void ccap_video_frame_release(CcapVideoFrame* frame);
```

## 数据结构

### CcapDeviceNamesList

```c
typedef struct {
    const char** deviceNames;  // 设备名称数组
    size_t deviceCount;        // 设备数量
} CcapDeviceNamesList;
```

### CcapDeviceInfo

```c
typedef struct {
    const char* name;          // 设备名称
    const char* uniqueId;      // 唯一标识符
    int index;                 // 设备索引
} CcapDeviceInfo;
```

### CcapVideoFrameInfo

```c
typedef struct {
    uint32_t width;                    // 宽度
    uint32_t height;                   // 高度
    uint32_t sizeInBytes;              // 总字节数
    uint64_t timestamp;                // 时间戳（纳秒）
    uint64_t frameIndex;               // 帧索引
    CcapPixelFormat pixelFormat;       // 像素格式
    CcapFrameOrientation orientation;  // 方向
} CcapVideoFrameInfo;
```

### CcapPropertyName

```c
typedef enum {
    CCAP_PROPERTY_WIDTH,                    // 帧宽度
    CCAP_PROPERTY_HEIGHT,                   // 帧高度
    CCAP_PROPERTY_FRAME_RATE,               // 帧率
    CCAP_PROPERTY_PIXEL_FORMAT_INTERNAL,    // 内部格式
    CCAP_PROPERTY_PIXEL_FORMAT_OUTPUT,      // 输出格式
    CCAP_PROPERTY_FRAME_ORIENTATION,        // 帧方向
} CcapPropertyName;
```

### CcapPixelFormat

```c
typedef enum {
    CCAP_PIXEL_FORMAT_UNKNOWN = 0,
    CCAP_PIXEL_FORMAT_NV12,      // YUV 4:2:0 半平面
    CCAP_PIXEL_FORMAT_NV12F,     // YUV 4:2:0 半平面（全范围）
    CCAP_PIXEL_FORMAT_I420,      // YUV 4:2:0 平面
    CCAP_PIXEL_FORMAT_I420F,     // YUV 4:2:0 平面（全范围）
    CCAP_PIXEL_FORMAT_RGB24,     // 24 位 RGB
    CCAP_PIXEL_FORMAT_BGR24,     // 24 位 BGR
    CCAP_PIXEL_FORMAT_RGBA32,    // 32 位 RGBA
    CCAP_PIXEL_FORMAT_BGRA32,    // 32 位 BGRA
} CcapPixelFormat;
```

## 注意事项

### 内存安全

1. **始终释放资源**：确保调用相应的 `_destroy()` 或 `_release()` 函数
2. **避免重复释放**：不要多次释放同一对象
3. **不要释放 NULL**：检查指针有效性

### 线程安全

- `CcapProvider` 对象**不是线程安全的**
- 回调函数在内部线程中调用，处理时注意线程安全
- 如需从多个线程访问，需要外部同步

### 错误处理

```c
// 检查返回值
if (!ccap_provider_open(provider, NULL, false)) {
    // 处理错误
    printf("打开失败\n");
    return -1;
}

// 检查指针
CcapVideoFrame* frame = ccap_provider_grab(provider, 3000);
if (!frame) {
    // 超时或错误
    printf("未能抓取帧\n");
}
```

## 编译和链接

### Windows

```shell
cl your_code.c /I"path\to\ccap\include" ^
   /link "path\to\ccap\lib\ccap.lib" ole32.lib oleaut32.lib uuid.lib
```

### macOS

```bash
gcc -std=c99 your_code.c -o your_app \
    -I/path/to/ccap/include \
    -L/path/to/ccap/lib -lccap \
    -framework Foundation -framework AVFoundation \
    -framework CoreMedia -framework CoreVideo
```

### Linux

```bash
gcc -std=c99 your_code.c -o your_app \
    -I/path/to/ccap/include \
    -L/path/to/ccap/lib -lccap \
    -lpthread
```

## 更多资源

- [主文档](documentation.zh.md) - 完整的 C++ API 文档
- [CLI 工具](cli.zh.md) - 命令行工具使用指南
- [实现细节](implementation-details.zh.md) - 内存管理、性能优化等

## 常见问题

**Q: C 接口和 C++ 接口有什么区别？**  
A: C 接口使用不透明指针和手动内存管理，适合纯 C 项目和语言绑定。C++ 接口使用智能指针和 RAII，更现代和安全。

**Q: 可以在同一程序中混用 C 和 C++ 接口吗？**  
A: 不建议。两个接口管理不同的内部状态，混用可能导致问题。

**Q: 如何在 Python/Go/Rust 中使用？**  
A: 使用 C 接口创建语言绑定。C 接口提供稳定的 ABI，适合跨语言调用。

**Q: 回调函数可以返回 false 吗？**  
A: 可以。返回 `false` 会停止后续帧的回调。
