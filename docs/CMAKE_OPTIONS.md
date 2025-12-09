# CMake Build Options

This document describes all available CMake options for building the ccap (Camera Capture) library.

## General Options

### CCAP_NO_LOG
- **Description**: Disable logging throughout the library
- **Type**: Boolean (ON/OFF)
- **Default**: OFF
- **Example**: `-DCCAP_NO_LOG=ON`
- **Notes**: When enabled, all logging macros (CCAP_LOG_*) are disabled, reducing binary size and improving performance

### CCAP_BUILD_SHARED
- **Description**: Build ccap as a shared library instead of static
- **Type**: Boolean (ON/OFF)
- **Default**: OFF
- **Example**: `-DCCAP_BUILD_SHARED=ON`
- **Notes**: When enabled, the library is built as a shared library (.dll on Windows, .so on Linux, .dylib on macOS)

### CCAP_INSTALL
- **Description**: Generate installation target for the library
- **Type**: Boolean (ON/OFF)
- **Default**: ON (if ccap is the root project), OFF (if used as a subdirectory)
- **Example**: `-DCCAP_INSTALL=ON`
- **Notes**: When enabled, allows installation of headers, library, and CMake config files via `cmake --install`

## Windows-Specific Options

### CCAP_WIN_NO_DEVICE_VERIFY
- **Description**: Skip device verification during enumeration on Windows
- **Type**: Boolean (ON/OFF)
- **Default**: OFF
- **Example**: `-DCCAP_WIN_NO_DEVICE_VERIFY=ON`
- **Purpose**: Prevents crashes caused by buggy camera drivers (e.g., Oculus Quest 3 VR headset when unplugged) that fail during `IMoniker::BindToObject()` or `filter->Release()` calls
- **Notes**: 
  - Only affects Windows platform
  - When enabled, device names are added without instantiating filters
  - Device properties are still verified via `enumerateDevices()`
  - Safe to enable for compatibility with problematic drivers
  - Related to issue #26

## Build Configuration Options

### CCAP_BUILD_EXAMPLES
- **Description**: Build example programs
- **Type**: Boolean (ON/OFF)
- **Default**: ON (if ccap is the root project), OFF (if used as a subdirectory)
- **Example**: `-DCCAP_BUILD_EXAMPLES=ON`
- **Notes**: Includes desktop examples with GLFW for visualization

### CCAP_BUILD_TESTS
- **Description**: Build unit tests
- **Type**: Boolean (ON/OFF)
- **Default**: OFF
- **Example**: `-DCCAP_BUILD_TESTS=ON`
- **Notes**: Requires GoogleTest framework; enables `enable_testing()` and CTest integration

## Architecture Options

### CCAP_FORCE_ARM64
- **Description**: Force ARM64 architecture compilation
- **Type**: Boolean (ON/OFF)
- **Default**: OFF
- **Example**: `-DCCAP_FORCE_ARM64=ON`
- **Platforms**: macOS, Windows
- **Notes**: 
  - On macOS: Sets `CMAKE_OSX_ARCHITECTURES` to "arm64"
  - On Windows: Sets `CMAKE_GENERATOR_PLATFORM` to "ARM64"
  - Enables NEON support on ARM64
  - Useful for cross-compilation or universal binary generation

## Usage Examples

### Build with default settings (static library)
```bash
cmake -B build
cmake --build build
```

### Build as shared library
```bash
cmake -B build -DCCAP_BUILD_SHARED=ON
cmake --build build
```

### Build with examples and tests
```bash
cmake -B build -DCCAP_BUILD_EXAMPLES=ON -DCCAP_BUILD_TESTS=ON
cmake --build build
```

### Build with Windows device verification disabled
```bash
cmake -B build -DCCAP_WIN_NO_DEVICE_VERIFY=ON
cmake --build build
```

### Build for ARM64 with all features disabled
```bash
cmake -B build -DCCAP_FORCE_ARM64=ON -DCCAP_NO_LOG=ON
cmake --build build
```

### Install the library
```bash
cmake -B build -DCCAP_INSTALL=ON
cmake --build build
cmake --install build --prefix /usr/local
```

## Related CMake Variables

The following CMake variables can also be used to customize the build:

- `CMAKE_BUILD_TYPE`: Set to Debug, Release, RelWithDebInfo, or MinSizeRel
- `CMAKE_OSX_DEPLOYMENT_TARGET`: Minimum macOS version (default: 10.13)
- `CMAKE_CXX_STANDARD`: C++ standard version (fixed to 17 by ccap)

## Platform-Specific Notes

### Windows
- When `CCAP_WIN_NO_DEVICE_VERIFY` is enabled, the library skips filter instantiation during device enumeration
- MSVC is the primary compiler; MinGW and Clang are also supported

### macOS
- Requires framework linking for AVFoundation, CoreVideo, CoreMedia, and Accelerate
- ARM64 support is fully integrated

### Linux
- Requires pthread library for thread support
- Standard V4L2 device access

## Related Files

- `CMakeLists.txt` - Main build configuration
- `src/ccap_imp_windows.cpp` - Windows implementation (affected by CCAP_WIN_NO_DEVICE_VERIFY)
- `include/ccap_config.h` - Configuration macros
