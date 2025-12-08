# ccap C Interface

This document describes how to use the pure C language interface of the ccap library.

## Overview

The ccap C interface provides complete camera capture functionality for C language programs, including:

- Device discovery and management
- Camera configuration and control
- Synchronous and asynchronous frame capture
- Memory management

## Core Concepts

### Opaque Pointers

The C interface uses opaque pointers to hide C++ object implementation details:

- `CcapProvider*` - Encapsulates `ccap::Provider` object
- `CcapVideoFrame*` - Encapsulates `ccap::VideoFrame` shared pointer

### Memory Management

The C interface follows these memory management principles:

1. **Creation and Destruction**: All objects created via `ccap_xxx_create()` must be released via the corresponding `ccap_xxx_destroy()`
2. **Array Release**: String arrays and struct arrays returned have dedicated release functions
3. **Frame Management**: Frames acquired via `ccap_provider_grab()` must be released via `ccap_video_frame_release()`

## Basic Usage Flow

### 1. Create Provider

```c
#include "ccap_c.h"

// Create provider
CcapProvider* provider = ccap_provider_create();
if (!provider) {
    printf("Failed to create provider\n");
    return -1;
}
```

### 2. Device Discovery

```c
// Find available devices
CcapDeviceNamesList deviceList;
if (ccap_provider_find_device_names_list(provider, &deviceList)) {
    printf("Found %zu devices:\n", deviceList.deviceCount);
    for (size_t i = 0; i < deviceList.deviceCount; i++) {
        printf("  %zu: %s\n", i, deviceList.deviceNames[i]);
    }
}
```

### 3. Open Device

```c
// Open default device
if (!ccap_provider_open(provider, NULL, false)) {
    printf("Failed to open camera\n");
    ccap_provider_destroy(provider);
    return -1;
}

// Or open by index
// ccap_provider_open_by_index(provider, 0, false);
```

### 4. Configure Camera Properties

```c
// Set resolution and frame rate
ccap_provider_set_property(provider, CCAP_PROPERTY_WIDTH, 640);
ccap_provider_set_property(provider, CCAP_PROPERTY_HEIGHT, 480);
ccap_provider_set_property(provider, CCAP_PROPERTY_FRAME_RATE, 30.0);

// Set pixel format
ccap_provider_set_property(provider, CCAP_PROPERTY_PIXEL_FORMAT_OUTPUT, 
                          CCAP_PIXEL_FORMAT_BGR24);
```

### 5. Start Capture

```c
if (!ccap_provider_start(provider)) {
    printf("Failed to start camera\n");
    return -1;
}
```

### 6. Frame Capture

#### Synchronous Method (grab)

```c
// Grab one frame (1 second timeout)
CcapVideoFrame* frame = ccap_provider_grab(provider, 1000);
if (frame) {
    CcapVideoFrameInfo frameInfo;
    if (ccap_video_frame_get_info(frame, &frameInfo)) {
        printf("Frame: %dx%d, format=%d, size=%u bytes\n", 
               frameInfo.width, frameInfo.height, 
               frameInfo.pixelFormat, frameInfo.sizeInBytes);
        
        // Access frame data
        uint8_t* data = frameInfo.data[0];  // Data of the first plane
        uint32_t stride = frameInfo.stride[0];  // Stride of the first plane
    }
    
    // Release frame
    ccap_video_frame_release(frame);
}
```

#### Asynchronous Method (callback)

```c
// Callback function
bool frame_callback(const CcapVideoFrame* frame, void* userData) {
    CcapVideoFrameInfo frameInfo;
    if (ccap_video_frame_get_info(frame, &frameInfo)) {
        printf("Callback frame: %dx%d\n", frameInfo.width, frameInfo.height);
    }
    
    // Return false to keep the frame for grab() use
    // Return true to consume the frame (grab() will not get this frame)
    return false;
}

// Set callback
ccap_provider_set_new_frame_callback(provider, frame_callback, NULL);
```

### 7. Cleanup Resources

```c
// Stop capture
ccap_provider_stop(provider);

// Close device
ccap_provider_close(provider);

// Destroy provider
ccap_provider_destroy(provider);
```

## Complete Example

See `examples/ccap_c_example.c` for a complete usage example.

## Build and Link

### Using CMake

1. Ensure the ccap library is built and installed
2. Copy `examples/CMakeLists_c_example.txt` as `CMakeLists.txt`
3. Build:

```bash
mkdir build
cd build
cmake ..
make
./ccap_c_example
```

### Manual Compilation

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
   /link "path\to\ccap\lib\ccap.lib" ole32.lib oleaut32.lib uuid.lib
```

#### Linux

```bash
gcc -std=c99 ccap_c_example.c -o ccap_c_example \
    -I/path/to/ccap/include \
    -L/path/to/ccap/lib -lccap \
    -lpthread
```

## API Reference

### Data Types

- `CcapProvider*` - Provider object pointer
- `CcapVideoFrame*` - Video frame object pointer
- `CcapPixelFormat` - Pixel format enumeration
- `CcapPropertyName` - Property name enumeration
- `CcapVideoFrameInfo` - Frame information structure
- `CcapDeviceInfo` - Device information structure

### Main Functions

#### Provider Lifecycle
- `ccap_provider_create()` - Create provider
- `ccap_provider_destroy()` - Destroy provider

#### Device Management
- `ccap_provider_find_device_names_list()` - Find devices
- `ccap_provider_open()` - Open device
- `ccap_provider_close()` - Close device
- `ccap_provider_is_opened()` - Check if opened

#### Capture Control
- `ccap_provider_start()` - Start capture
- `ccap_provider_stop()` - Stop capture
- `ccap_provider_is_started()` - Check if capturing

#### Frame Acquisition
- `ccap_provider_grab()` - Synchronously acquire frame
- `ccap_provider_set_new_frame_callback()` - Set asynchronous callback

#### Property Configuration
- `ccap_provider_set_property()` - Set property
- `ccap_provider_get_property()` - Get property

## Error Handling

The C interface uses the following error handling strategy:

1. **Return Values**: Most functions return `bool` type, `true` indicates success, `false` indicates failure
2. **Null Pointers**: Pointer-returning functions return `NULL` when operations fail
3. **NaN**: Numeric-returning functions return `NaN` on failure
4. **Error Callback**: An error callback function can be set to receive detailed error information

### Error Callback

Starting from v1.2.0, ccap supports setting an error callback function to receive detailed error information:

#### Error Codes

```c
typedef enum {
    CCAP_ERROR_NONE = 0,                        // No error
    CCAP_ERROR_NO_DEVICE_FOUND = 0x1001,       // No camera device found
    CCAP_ERROR_INVALID_DEVICE = 0x1002,        // Invalid device name or index
    CCAP_ERROR_DEVICE_OPEN_FAILED = 0x1003,    // Camera device open failed
    CCAP_ERROR_DEVICE_START_FAILED = 0x1004,   // Camera start failed
    CCAP_ERROR_UNSUPPORTED_RESOLUTION = 0x2001, // Unsupported resolution
    CCAP_ERROR_UNSUPPORTED_PIXEL_FORMAT = 0x2002, // Unsupported pixel format
    CCAP_ERROR_FRAME_CAPTURE_TIMEOUT = 0x3001, // Frame capture timeout
    CCAP_ERROR_FRAME_CAPTURE_FAILED = 0x3002,  // Frame capture failed
    // More error codes...
} CcapErrorCode;
```

#### Error Callback Function

```c
// Error callback function type
typedef void (*CcapErrorCallback)(CcapErrorCode errorCode, const char* errorDescription, void* userData);

// Set error callback
bool ccap_set_error_callback(CcapErrorCallback callback, void* userData);

// Get error code description
const char* ccap_error_code_to_string(CcapErrorCode errorCode);
```

#### Usage Example

```c
// Error callback function
void error_callback(CcapErrorCode errorCode, const char* errorDescription, void* userData) {
    printf("Camera Error - Code: %d, Description: %s\n", (int)errorCode, errorDescription);
}

int main() {
    // Set error callback
    ccap_set_error_callback(error_callback, NULL);
    
    CcapProvider* provider = ccap_provider_create();
    
    // Perform camera operations, callback will be called if errors occur
    if (!ccap_provider_open_by_index(provider, 0, true)) {
        printf("Failed to open camera\n");
    }
    
    ccap_provider_destroy(provider);
    return 0;
}
```

## Notes

1. **Thread Safety**: The C interface is not thread-safe, external synchronization is required
2. **Exception Handling**: All C++ exceptions are caught and converted to error return values
3. **Memory Alignment**: Frame data is guaranteed to be 32-byte aligned, supporting SIMD optimization
4. **Lifecycle**: Ensure all created objects are properly released to avoid memory leaks

## Correspondence with C++ Interface

| C Interface | C++ Interface |
|--------|----------|
| `CcapProvider*` | `ccap::Provider` |
| `CcapVideoFrame*` | `std::shared_ptr<ccap::VideoFrame>` |
| `ccap_provider_xxx()` | `ccap::Provider::xxx()` |
| `CCAP_PIXEL_FORMAT_XXX` | `ccap::PixelFormat::XXX` |
| `CCAP_PROPERTY_XXX` | `ccap::PropertyName::XXX` |
