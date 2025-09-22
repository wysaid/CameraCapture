# Android Backend Implementation for CCAP

This directory contains the Android Camera2 backend implementation for the CCAP (Camera Capture) library.

## Overview

The Android backend provides native camera access using the Android Camera2 API through JNI (Java Native Interface). This implementation follows the same architecture as other CCAP platform backends (Linux V4L2, Windows DirectShow, macOS AVFoundation) while leveraging Android's modern camera framework.

## Features

- **Camera2 API Integration**: Uses Android's Camera2 API for modern camera access
- **JNI Bridge**: Seamless integration between C++ and Java/Kotlin code
- **Multi-format Support**: YUV_420_888, NV21, RGB565, RGBA8888
- **Thread-safe Operations**: Proper synchronization for camera callbacks
- **Memory Management**: Efficient frame buffer management with lifecycle control
- **Error Handling**: Comprehensive error reporting and camera state management

## Architecture

### Core Components

1. **ProviderAndroid** (`ccap_imp_android.h/cpp`)
   - Main provider implementation following the `ProviderImp` interface
   - Handles camera lifecycle, capture sessions, and frame delivery
   - Manages JNI interactions with Java camera helper

2. **CameraHelper** (`CameraHelper.java`)
   - Java helper class that interfaces with Android Camera2 API
   - Handles camera discovery, session configuration, and image capture
   - Bridges between Java Camera2 callbacks and native C++ code

3. **Android Utils** (`ccap_android_utils.cpp`, `ccap_android.h`)
   - Utility functions for Android-specific operations
   - Context management and permission checking
   - Recommended configuration helpers

### Integration Points

The Android backend integrates with the main CCAP library through:

```cpp
// In ccap_core.cpp - Platform detection
#elif defined(__ANDROID__)
    return createProviderAndroid();
```

## Build Configuration

### CMake Setup

The Android backend can be built using the Android NDK with CMake:

```cmake
# Add to your Android CMakeLists.txt
find_library(log-lib log)
find_library(camera2ndk-lib camera2ndk)
find_library(mediandk-lib mediandk)

target_link_libraries(your_app
    ccap_android
    ${log-lib}
    ${camera2ndk-lib}
    ${mediandk-lib}
    android
)
```

### Android Configuration

Minimum requirements:
- **API Level 21+** (Android 5.0) - Required for Camera2 API
- **NDK r21+** - For C++17 support
- **Camera Permission** - Declared in AndroidManifest.xml

### Gradle Configuration

```gradle
android {
    compileSdk 34
    defaultConfig {
        minSdk 21
        ndk {
            abiFilters 'arm64-v8a', 'armeabi-v7a'
        }
        externalNativeBuild {
            cmake {
                cppFlags "-std=c++17"
                arguments "-DANDROID_STL=c++_shared"
            }
        }
    }
}
```

## Usage

### Basic Integration

1. **Initialize JNI** (in `JNI_OnLoad` or Application.onCreate()):
```cpp
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    ccap::android::setJavaVM(vm);
    return JNI_VERSION_1_6;
}
```

2. **Initialize Android Context**:
```cpp
// In your Activity/Application
ccap::android::initialize(env, applicationContext);
```

3. **Use CCAP Provider**:
```cpp
ccap::Provider provider;
auto cameras = provider.findDeviceNames();
provider.open(cameras[0]);
provider.start();
```

### Example Application

See `examples/android/android_camera_example.cpp` for a complete example showing:
- Camera discovery and selection
- Configuration management
- Frame capture and processing
- Proper resource cleanup

## Implementation Details

### Camera Discovery

The Android backend discovers cameras by:
1. Querying `CameraManager.getCameraIdList()`
2. Getting characteristics for each camera
3. Filtering by supported formats and capabilities

### Format Support

Supported pixel formats:
- `YUV_420_888` (preferred) → `PixelFormat::YUV420P`
- `NV21` → `PixelFormat::NV21`
- `NV16` → `PixelFormat::NV16`
- `RGB_565` → `PixelFormat::RGB565`
- `RGBA_8888` → `PixelFormat::RGBA`

### Memory Management

The implementation uses:
- **Smart pointers** for automatic resource management
- **Weak references** to prevent circular dependencies
- **Frame lifecycle tracking** to ensure proper buffer cleanup
- **Thread-safe queues** for frame delivery

### Error Handling

Common error scenarios handled:
- Camera permission denied
- Camera device disconnection
- Capture session configuration failures
- JNI environment issues
- Memory allocation failures

## Design Inspiration

This implementation draws inspiration from:
- **OpenCV's highgui VideoCapture Android backend**
- **CCAP's existing platform implementations** (V4L2, DirectShow, AVFoundation)
- **Android Camera2 best practices** from Google's documentation

## Threading Model

- **Main Thread**: Provider lifecycle management
- **Background Thread**: Camera operations and callbacks (via HandlerThread)
- **Capture Thread**: Frame processing and delivery
- **JNI Thread Safety**: Proper environment management across threads

## Future Enhancements

Potential improvements:
1. **CameraX Integration**: Alternative to Camera2 for simpler usage
2. **Hardware Acceleration**: Use of Android's graphics pipeline
3. **Multi-camera Support**: Simultaneous capture from multiple cameras
4. **Advanced Features**: HDR, burst capture, manual controls
5. **Performance Optimizations**: Zero-copy frame delivery where possible

## Limitations

Current limitations:
1. **Java Dependency**: Requires Java/Kotlin bridge code
2. **API Level 21+**: Cannot support older Android versions
3. **Permission Handling**: Application must handle camera permissions
4. **Format Conversion**: Some formats may require software conversion

## Testing

The Android backend can be tested by:
1. Building the example application
2. Running on a physical Android device (emulator camera support varies)
3. Verifying camera discovery and frame capture
4. Testing different camera configurations and formats

## Contributing

When contributing to the Android backend:
1. Follow the existing CCAP code style
2. Test on multiple Android versions and devices
3. Ensure proper resource cleanup
4. Update documentation for new features
5. Add appropriate error handling