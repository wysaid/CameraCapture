# ccap Build and Installation Guide

This document describes how to build, install, and use the ccap library.

## Prerequisites

- **macOS**: Xcode Command Line Tools or full Xcode
- **Windows**: Visual Studio 2019+ or Build Tools for Visual Studio
- **Common**: CMake 3.14+

## Quick Start

### Standard Build and Installation

```bash
# Build and install to ./install directory
./scripts/build_and_install.sh

# Specify installation directory
./scripts/build_and_install.sh /path/to/install

# Specify build type
./scripts/build_and_install.sh /path/to/install Release
```

### Shared Library Build

By default, ccap builds as a static library. To build a shared library instead:

```bash
mkdir build && cd build

# Build shared library
cmake .. -DCCAP_BUILD_SHARED=ON
make -j$(nproc)

# Or with installation
cmake .. -DCCAP_BUILD_SHARED=ON -DCMAKE_INSTALL_PREFIX=/your/install/path
make -j$(nproc)
make install
```

**Benefits of shared library:**
- Smaller executable size when linking multiple applications
- Runtime library updates without recompiling applications
- Better for Java JNI integration (as mentioned in issue #14)

**Static vs Shared Library:**
- **Static library** (default): `libccap.a` (Linux/macOS) or `ccap.lib` (Windows)
- **Shared library**: `libccap.so` (Linux), `libccap.dylib` (macOS) or `ccap.dll` (Windows)

### CMake Options

- `CCAP_BUILD_SHARED`: Build as shared library instead of static (default: OFF)
- `CCAP_BUILD_EXAMPLES`: Build example applications (default: ON for root project)
- `CCAP_BUILD_TESTS`: Build unit tests (default: OFF)
- `CCAP_NO_LOG`: Disable logging functionality (default: OFF)

### macOS Universal Binary Build

```bash
# Build universal binary library supporting both x86_64 and arm64
./scripts/build_macos_universal.sh

# Build results are located in ./build/universal
```

### Windows Build

```bash
# Windows build and install (run in Git Bash)
# Automatically builds both Debug and Release versions
./scripts/build_and_install.sh

# Specify installation directory
./scripts/build_and_install.sh /path/to/install

# Windows will automatically generate:
# - ccap.lib (Release version)
# - ccapd.lib (Debug version with 'd' suffix)
```

## Verify Installation

```bash
# Test standard installation
./scripts/test_installation.sh

# Test universal binary installation
./scripts/test_installation.sh build/universal

# Test custom installation path
./scripts/test_installation.sh /path/to/install
```

## Using the Installed Library

### Using CMake

```cmake
cmake_minimum_required(VERSION 3.14)
project(my_project)

set(CMAKE_CXX_STANDARD 17)

# Find ccap package
find_package(ccap REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app ccap::ccap)

# System frameworks required for macOS
if(APPLE)
    target_link_libraries(my_app 
        "-framework Foundation"
        "-framework AVFoundation" 
        "-framework CoreVideo"
        "-framework CoreMedia"
        "-framework Accelerate"
    )
endif()
```

### Using pkg-config

```bash
# Set PKG_CONFIG_PATH
export PKG_CONFIG_PATH=/path/to/install/lib/pkgconfig:$PKG_CONFIG_PATH

# Compile example
g++ -std=c++17 main.cpp $(pkg-config --cflags --libs ccap) -o my_app
```

### Basic C++ Code Example

```cpp
#include <ccap.h>
#include <iostream>

int main() {
    ccap::Provider provider;
    
    // Get available device list
    auto deviceNames = provider.findDeviceNames();
    std::cout << "Found " << deviceNames.size() << " camera device(s)" << std::endl;
    
    // Print device names
    for (size_t i = 0; i < deviceNames.size(); ++i) {
        std::cout << "  " << i << ": " << deviceNames[i] << std::endl;
    }
    
    // Open default camera
    if (provider.open()) {
        std::cout << "Camera opened successfully!" << std::endl;
        
        // Capture one frame
        if (auto frame = provider.grab(1000)) {
            std::cout << "Frame captured: " 
                      << frame->width << "x" << frame->height 
                      << std::endl;
        }
    }
    
    return 0;
}
```

## Output File Structure

### Standard Installation (./install)

```
install/
├── include/              # Header files
│   ├── ccap.h
│   ├── ccap_core.h
│   ├── ccap_convert.h
│   ├── ccap_def.h
│   ├── ccap_opencv.h
│   └── ccap_utils.h
└── lib/                  # Library files and configurations
    ├── libccap.a         # Static library
    ├── cmake/ccap/       # CMake configuration files
    │   ├── ccapConfig.cmake
    │   ├── ccapConfigVersion.cmake
    │   ├── ccapTargets.cmake
    │   └── ccapTargets-release.cmake
    └── pkgconfig/        # pkg-config files
        └── ccap.pc
```

### Universal Binary Installation (./build/universal)

```
build/universal/          # Contains x86_64 + arm64 universal binary
├── include/              # Header files (same as above)
└── lib/                  # Universal library files and configurations (same as above)
    └── libccap.a         # Universal binary static library
```

## Build Options

### CMake Options

- `CCAP_INSTALL`: Enable install target (default: ON)
- `CCAP_BUILD_EXAMPLES`: Build examples (default: OFF when used as subproject)
- `CCAP_BUILD_TESTS`: Build tests (default: OFF when used as subproject)

### Advanced Usage

```bash
# Custom CMake configuration
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release \
         -DCMAKE_INSTALL_PREFIX=/usr/local \
         -DCCAP_BUILD_EXAMPLES=ON
make -j$(nproc)
make install
```

## Troubleshooting

### Common Issues

1. **Camera devices not found**
   - macOS: Ensure camera permissions are granted
   - Check if other applications are using the camera

2. **Build failures**
   - Ensure correct build tools are installed
   - Check CMake version >= 3.14

3. **Library files not found**
   - Ensure `CMAKE_PREFIX_PATH` includes the installation directory
   - Or set `PKG_CONFIG_PATH` for pkg-config

### Clean Build

```bash
# Clean all build files
git clean -fdx build/
git clean -fdx install/
```

## Supported Platforms

- ✅ macOS (x86_64, arm64, Universal Binary)
- ✅ Windows (x86, x64)
- ⚠️ Linux (to be implemented)

## Version Information

Current version: 1.4.1

This is the first official release of the ccap project, including complete CMake configuration and cross-platform build support.
