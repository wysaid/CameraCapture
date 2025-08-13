# ccap C Interface

本文档描述了如何使用 ccap 库的纯 C 语言接口。

## 概述

ccap 的 C 接口为 C 语言程序提供了完整的相机捕获功能，包括：

- 设备发现和管理
- 相机配置和控制
- 同步和异步帧捕获
- 内存管理

## 核心概念

### 不透明指针 (Opaque Pointers)

C 接口使用不透明指针来隐藏 C++ 对象的实现细节：

- `CcapProvider*` - 封装 `ccap::Provider` 对象
- `CcapVideoFrame*` - 封装 `ccap::VideoFrame` 共享指针

### 内存管理

C 接口遵循以下内存管理原则：

1. **创建与销毁**: 所有通过 `ccap_xxx_create()` 创建的对象必须通过对应的 `ccap_xxx_destroy()` 释放
2. **数组释放**: 返回的字符串数组和结构体数组有专门的释放函数
3. **帧管理**: 通过 `ccap_provider_grab()` 获取的帧必须通过 `ccap_video_frame_release()` 释放

## 基本使用流程

### 1. 创建 Provider

```c
#include "ccap_c.h"

// 创建 provider
CcapProvider* provider = ccap_provider_create();
if (!provider) {
    printf("Failed to create provider\n");
    return -1;
}
```

### 2. 设备发现

```c
// 查找可用设备
char** deviceNames;
size_t deviceCount;
if (ccap_provider_find_device_names(provider, &deviceNames, &deviceCount)) {
    printf("Found %zu devices:\n", deviceCount);
    for (size_t i = 0; i < deviceCount; i++) {
        printf("  %zu: %s\n", i, deviceNames[i]);
    }
    
    // 释放设备名称数组
    ccap_provider_free_device_names(deviceNames, deviceCount);
}
```

### 3. 打开设备

```c
// 打开默认设备
if (!ccap_provider_open(provider, NULL, false)) {
    printf("Failed to open camera\n");
    ccap_provider_destroy(provider);
    return -1;
}

// 或者按索引打开
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

### 5. 开始捕获

```c
if (!ccap_provider_start(provider)) {
    printf("Failed to start camera\n");
    return -1;
}
```

### 6. 帧捕获

#### 同步方式 (grab)

```c
// 抓取一帧 (超时 1 秒)
CcapVideoFrame* frame = ccap_provider_grab(provider, 1000);
if (frame) {
    CcapVideoFrameInfo frameInfo;
    if (ccap_video_frame_get_info(frame, &frameInfo)) {
        printf("Frame: %dx%d, format=%d, size=%u bytes\n", 
               frameInfo.width, frameInfo.height, 
               frameInfo.pixelFormat, frameInfo.sizeInBytes);
        
        // 访问帧数据
        uint8_t* data = frameInfo.data[0];  // 第一个平面的数据
        uint32_t stride = frameInfo.stride[0];  // 第一个平面的步长
    }
    
    // 释放帧
    ccap_video_frame_release(frame);
}
```

#### 异步方式 (callback)

```c
// 回调函数
bool frame_callback(const CcapVideoFrame* frame, void* userData) {
    CcapVideoFrameInfo frameInfo;
    if (ccap_video_frame_get_info(frame, &frameInfo)) {
        printf("Callback frame: %dx%d\n", frameInfo.width, frameInfo.height);
    }
    
    // 返回 false 保留帧供 grab() 使用
    // 返回 true 消费帧 (grab() 将不会获取到此帧)
    return false;
}

// 设置回调
ccap_provider_set_new_frame_callback(provider, frame_callback, NULL);
```

### 7. 清理资源

```c
// 停止捕获
ccap_provider_stop(provider);

// 关闭设备
ccap_provider_close(provider);

// 销毁 provider
ccap_provider_destroy(provider);
```

## 完整示例

参见 `examples/ccap_c_example.c` 获取完整的使用示例。

## 编译和链接

### 使用 CMake

1. 确保 ccap 库已经构建并安装
2. 复制 `examples/CMakeLists_c_example.txt` 为 `CMakeLists.txt`
3. 构建:

```bash
mkdir build
cd build
cmake ..
make
./ccap_c_example
```

### 手动编译

#### macOS

```bash
gcc -std=c99 ccap_c_example.c -o ccap_c_example \
    -I/path/to/ccap/include \
    -L/path/to/ccap/lib -lccap \
    -framework Foundation -framework AVFoundation \
    -framework CoreMedia -framework CoreVideo
```

#### Windows (MSVC)

```cmd
cl ccap_c_example.c /I"path\to\ccap\include" \
   /link "path\to\ccap\lib\ccap.lib" strmiids.lib ole32.lib oleaut32.lib uuid.lib
```

#### Linux

```bash
gcc -std=c99 ccap_c_example.c -o ccap_c_example \
    -I/path/to/ccap/include \
    -L/path/to/ccap/lib -lccap \
    -lpthread
```

## API 参考

### 数据类型

- `CcapProvider*` - Provider 对象指针
- `CcapVideoFrame*` - 视频帧对象指针
- `CcapPixelFormat` - 像素格式枚举
- `CcapPropertyName` - 属性名枚举
- `CcapVideoFrameInfo` - 帧信息结构体
- `CcapDeviceInfo` - 设备信息结构体

### 主要函数

#### Provider 生命周期
- `ccap_provider_create()` - 创建 provider
- `ccap_provider_destroy()` - 销毁 provider

#### 设备管理
- `ccap_provider_find_device_names()` - 查找设备
- `ccap_provider_open()` - 打开设备
- `ccap_provider_close()` - 关闭设备
- `ccap_provider_is_opened()` - 检查是否已打开

#### 捕获控制
- `ccap_provider_start()` - 开始捕获
- `ccap_provider_stop()` - 停止捕获
- `ccap_provider_is_started()` - 检查是否正在捕获

#### 帧获取
- `ccap_provider_grab()` - 同步获取帧
- `ccap_provider_set_new_frame_callback()` - 设置异步回调

#### 属性配置
- `ccap_provider_set_property()` - 设置属性
- `ccap_provider_get_property()` - 获取属性

## 错误处理

C 接口使用以下错误处理策略：

1. **返回值**: 大多数函数返回 `bool` 类型，`true` 表示成功，`false` 表示失败
2. **空指针**: 当操作失败时，指针返回函数返回 `NULL`
3. **NaN**: 数值返回函数在失败时返回 `NaN`
4. **错误回调**: 可以设置错误回调函数来接收详细的错误信息

### 错误回调

从 v1.1.0 开始，ccap 支持设置错误回调函数来接收详细的错误信息：

#### 错误码

```c
typedef enum {
    CCAP_ERROR_NONE = 0,                        // 无错误
    CCAP_ERROR_NO_DEVICE_FOUND = 0x1001,       // 未找到相机设备
    CCAP_ERROR_INVALID_DEVICE = 0x1002,        // 设备名称或索引无效
    CCAP_ERROR_DEVICE_OPEN_FAILED = 0x1003,    // 相机设备打开失败
    CCAP_ERROR_DEVICE_START_FAILED = 0x1004,   // 相机启动失败
    CCAP_ERROR_UNSUPPORTED_RESOLUTION = 0x2001, // 不支持的分辨率
    CCAP_ERROR_UNSUPPORTED_PIXEL_FORMAT = 0x2002, // 不支持的像素格式
    CCAP_ERROR_FRAME_CAPTURE_TIMEOUT = 0x3001, // 帧捕获超时
    CCAP_ERROR_FRAME_CAPTURE_FAILED = 0x3002,  // 帧捕获失败
    // 更多错误码...
} CcapErrorCode;
```

#### 错误回调函数

```c
// 错误回调函数类型
typedef void (*CcapErrorCallback)(CcapErrorCode errorCode, const char* errorDescription, void* userData);

// 设置错误回调
bool ccap_provider_set_error_callback(CcapProvider* provider, CcapErrorCallback callback, void* userData);

// 获取错误码描述
const char* ccap_error_code_to_string(CcapErrorCode errorCode);
```

#### 使用示例

```c
// 错误回调函数
void error_callback(CcapErrorCode errorCode, const char* errorDescription, void* userData) {
    printf("Camera Error - Code: %d, Description: %s\n", (int)errorCode, errorDescription);
}

int main() {
    CcapProvider* provider = ccap_provider_create();
    
    // 设置错误回调
    ccap_provider_set_error_callback(provider, error_callback, NULL);
    
    // 执行相机操作，如果出错会调用回调函数
    if (!ccap_provider_open_by_index(provider, 0, true)) {
        printf("Failed to open camera\n");
    }
    
    ccap_provider_destroy(provider);
    return 0;
}
```

## 注意事项

1. **线程安全**: C 接口不是线程安全的，需要外部同步
2. **异常处理**: 所有 C++ 异常都被捕获并转换为错误返回值
3. **内存对齐**: 帧数据保证 32 字节对齐，支持 SIMD 优化
4. **生命周期**: 确保所有创建的对象都被正确释放，避免内存泄漏

## 与 C++ 接口的对应关系

| C 接口 | C++ 接口 |
|--------|----------|
| `CcapProvider*` | `ccap::Provider` |
| `CcapVideoFrame*` | `std::shared_ptr<ccap::VideoFrame>` |
| `ccap_provider_xxx()` | `ccap::Provider::xxx()` |
| `CCAP_PIXEL_FORMAT_XXX` | `ccap::PixelFormat::XXX` |
| `CCAP_PROPERTY_XXX` | `ccap::PropertyName::XXX` |
