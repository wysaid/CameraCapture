# CMake Build Options

Complete reference for customizing ccap builds.

## Quick Start Presets

Common build configurations for different scenarios:

### Default Build (Static Library)
```bash
cmake -B build
cmake --build build
```

### Shared Library Build
```bash
cmake -B build -DCCAP_BUILD_SHARED=ON
cmake --build build
```

### Development Build (All Features)
```bash
cmake -B build \
  -DCCAP_BUILD_EXAMPLES=ON \
  -DCCAP_BUILD_TESTS=ON \
  -DCCAP_BUILD_CLI=ON
cmake --build build
```

### Production Build (Optimized)
```bash
cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCCAP_NO_LOG=ON \
  -DCCAP_INSTALL=ON
cmake --build build
cmake --install build --prefix /usr/local
```

### Standalone CLI (No Runtime Dependencies)
```bash
cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCCAP_BUILD_CLI_STANDALONE=ON
cmake --build build
```

## Build Options by Category

### Core Library Options

#### `CCAP_BUILD_SHARED`
**Build as shared library instead of static**

- **Type**: Boolean (ON/OFF)
- **Default**: `OFF`
- **Usage**: `-DCCAP_BUILD_SHARED=ON`

**When to use:**
- Multiple executables sharing the library
- Plugins or dynamic loading scenarios
- Reducing total binary size across multiple apps

**Output:**
- Windows: `ccap.dll` + `ccap.lib`
- Linux: `libccap.so`
- macOS: `libccap.dylib`

#### `CCAP_NO_LOG`
**Disable all logging for smaller binaries**

- **Type**: Boolean (ON/OFF)
- **Default**: `OFF`
- **Usage**: `-DCCAP_NO_LOG=ON`

**Benefits:**
- Reduces binary size by ~5-10%
- Slight performance improvement
- Cleaner console output in production

**Trade-offs:**
- No diagnostic information
- Harder to debug issues

**Recommendation:** Use for production builds only.

#### `CCAP_INSTALL`
**Enable installation targets**

- **Type**: Boolean (ON/OFF)
- **Default**: `ON` (if root project), `OFF` (if subdirectory)
- **Usage**: `-DCCAP_INSTALL=ON`

Installs:
- Headers → `include/`
- Library → `lib/`
- CMake config → `lib/cmake/ccap/`
- pkg-config → `lib/pkgconfig/`

### Platform-Specific Options

#### `CCAP_WIN_NO_DEVICE_VERIFY` (Windows Only)
**Skip device verification during enumeration**

- **Type**: Boolean (ON/OFF)
- **Default**: `OFF`
- **Usage**: `-DCCAP_WIN_NO_DEVICE_VERIFY=ON`

**Problem it solves:**
Some buggy camera drivers (e.g., VR headsets, faulty webcams) crash during `IMoniker::BindToObject()` calls when enumerating devices.

**How it helps:**
- Prevents crashes caused by problematic drivers
- Still enumerates device names safely
- Device properties verified via `enumerateDevices()` later

**When to enable:**
- You encounter crashes during device enumeration
- Working with VR headsets or unusual capture devices
- Safe to enable by default for better compatibility

**Related:** [Issue #26](https://github.com/wysaid/CameraCapture/issues/26)

#### `CCAP_FORCE_ARM64`
**Force ARM64 architecture compilation**

- **Type**: Boolean (ON/OFF)
- **Default**: `OFF`
- **Platforms**: macOS, Windows
- **Usage**: `-DCCAP_FORCE_ARM64=ON`

**Effects:**
- macOS: Sets `CMAKE_OSX_ARCHITECTURES=arm64`
- Windows: Sets `CMAKE_GENERATOR_PLATFORM=ARM64`
- Enables NEON optimizations

**Use cases:**
- Cross-compilation for ARM devices
- Building universal macOS binaries
- Windows on ARM development

### Development Options

#### `CCAP_BUILD_EXAMPLES`
**Build example programs**

- **Type**: Boolean (ON/OFF)
- **Default**: `ON` (if root project), `OFF` (if subdirectory)
- **Usage**: `-DCCAP_BUILD_EXAMPLES=ON`

Builds:
- `0-print_camera` - List available cameras
- `1-minimal_example` - Basic capture
- `2-capture_grab` - Synchronous capture
- `3-capture_callback` - Asynchronous capture
- `4-example_with_glfw` - Real-time preview with OpenGL
- `5-play_video` - Video file playback (macOS/Windows)

Plus C interface versions (`*_c`)

**Requirements:** GLFW for preview examples

#### `CCAP_BUILD_TESTS`
**Build unit tests**

- **Type**: Boolean (ON/OFF)
- **Default**: `OFF`
- **Usage**: `-DCCAP_BUILD_TESTS=ON`

**Includes:**
- 50+ test cases
- GoogleTest integration
- CTest support
- Performance benchmarks

**Run tests:**
```bash
cmake -B build -DCCAP_BUILD_TESTS=ON
cmake --build build
cd build && ctest --output-on-failure
```

#### `CCAP_BUILD_CLI`
**Build command-line tool**

- **Type**: Boolean (ON/OFF)
- **Default**: `OFF` (auto-enabled with tests or standalone CLI)
- **Usage**: `-DCCAP_BUILD_CLI=ON`

Builds the `ccap` CLI tool with features:
- List camera devices
- Capture images
- Real-time preview
- Video file processing

See [CLI Documentation](cli.md) for usage details.

**Related options:**
- `CCAP_CLI_WITH_GLFW` - Enable preview mode
- `CCAP_BUILD_CLI_STANDALONE` - Build with static runtime

#### `CCAP_BUILD_CLI_STANDALONE`
**Build standalone CLI with static runtime**

- **Type**: Boolean (ON/OFF)
- **Default**: `OFF`
- **Usage**: `-DCCAP_BUILD_CLI_STANDALONE=ON`

**What it does:**
- Automatically enables `CCAP_BUILD_CLI=ON`
- **Windows**: Uses `/MT` flag (static MSVC runtime)
  - No VCRUNTIME DLL required
  - Portable executable
- **Linux**: Statically links libstdc++ and libgcc when available
  - Better compatibility across distros

**Requirements:**
- Must use `CCAP_BUILD_SHARED=OFF`
- Release build recommended for smaller binaries

**Example:**
```bash
cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCCAP_BUILD_CLI_STANDALONE=ON
cmake --build build

# Result: Fully standalone ccap.exe or ccap binary
```

## Advanced Scenarios

### Building Universal Binary (macOS)

```bash
cmake -B build \
  -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Or use the provided script:
```bash
./scripts/build_macos_universal.sh
```

### Cross-Compilation for ARM64 (Windows)

```bash
cmake -B build -G "Visual Studio 17 2022" -A ARM64
cmake --build build --config Release
```

Or:
```bash
./scripts/build_arm64_win.sh
```

### Static Runtime Linking (Windows)

For truly portable binaries:

```bash
cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCCAP_BUILD_SHARED=OFF \
  -DCCAP_BUILD_CLI_STANDALONE=ON
cmake --build build --config Release
```

The resulting executable has no external dependencies except system DLLs.

### Minimal Size Build

```bash
cmake -B build \
  -DCMAKE_BUILD_TYPE=MinSizeRel \
  -DCCAP_NO_LOG=ON \
  -DCCAP_BUILD_SHARED=OFF
cmake --build build
```

Add `-DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON` for LTO (Link Time Optimization).

### Custom Installation Prefix

```bash
cmake -B build -DCCAP_INSTALL=ON
cmake --build build
sudo cmake --install build --prefix /usr/local
```

Or install to a custom directory:
```bash
cmake --install build --prefix $HOME/.local
```

## Environment Variables

### Standard CMake Variables

#### `CMAKE_BUILD_TYPE`
```bash
-DCMAKE_BUILD_TYPE=Debug          # Debug symbols, no optimization
-DCMAKE_BUILD_TYPE=Release        # Optimized, no debug info
-DCMAKE_BUILD_TYPE=RelWithDebInfo # Optimized + debug symbols
-DCMAKE_BUILD_TYPE=MinSizeRel     # Optimize for size
```

#### `CMAKE_CXX_STANDARD`
Fixed to C++17 by ccap. Not user-configurable.

#### `CMAKE_OSX_DEPLOYMENT_TARGET` (macOS)
```bash
-DCMAKE_OSX_DEPLOYMENT_TARGET=10.13  # Default minimum
-DCMAKE_OSX_DEPLOYMENT_TARGET=11.0   # macOS Big Sur and later
```

### ccap Custom Variables

#### `CCAP_CLI_WITH_GLFW`
Enable GLFW-based preview mode in CLI:
```bash
-DCCAP_BUILD_CLI=ON -DCCAP_CLI_WITH_GLFW=ON
```

Requires GLFW library installed.

## Troubleshooting

### Common Issues

**Problem:** "Cannot find ccap package"
```bash
# Solution: Set CMAKE_PREFIX_PATH
cmake -B build -DCMAKE_PREFIX_PATH=/usr/local
```

**Problem:** Shared library not found at runtime
```bash
# Linux solution:
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH

# macOS solution:
export DYLD_LIBRARY_PATH=/usr/local/lib:$DYLD_LIBRARY_PATH

# Windows solution:
# Add installation directory to PATH or copy DLL next to executable
```

**Problem:** "Cannot open camera" on Linux
```bash
# Ensure user has camera access permissions
sudo usermod -a -G video $USER
# Log out and back in
```

**Problem:** Build fails with "C++17 required"
```bash
# Update compiler:
# - GCC: Upgrade to 7+
# - Clang: Upgrade to 6+
# - MSVC: Use Visual Studio 2019+
```

### Platform-Specific Notes

#### Windows
- MSVC 2019+ recommended
- MinGW and Clang also supported
- Use Developer Command Prompt for best results

#### macOS
- Xcode 11+ required
- Homebrew recommended for dependencies
- Code signing may be needed for distribution

#### Linux
- V4L2 headers required (usually in linux-headers)
- pthread automatically linked
- Camera permissions may need configuration

## Using dev.cmake for Custom Builds

Create `cmake/dev.cmake` (git-ignored) to override defaults without modifying CMakeLists.txt:

```cmake
# cmake/dev.cmake - Custom development settings

# Force shared library
set(CCAP_BUILD_SHARED ON CACHE BOOL "" FORCE)

# Enable all features
set(CCAP_BUILD_EXAMPLES ON CACHE BOOL "" FORCE)
set(CCAP_BUILD_TESTS ON CACHE BOOL "" FORCE)
set(CCAP_BUILD_CLI ON CACHE BOOL "" FORCE)

# Custom compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
```

See `cmake/dev.cmake.example` for more examples.

**Note:** Remove `dev.cmake` for standard builds or CI/CD.

## Related Documentation

- [Build and Install Guide](https://github.com/wysaid/CameraCapture/blob/main/BUILD_AND_INSTALL.md)
- [CLI Tool Documentation](cli.md)
- [C Interface Documentation](c-interface.md)
- [Rust Bindings](rust-bindings.md)

## Quick Reference Table

| Option | Default | Purpose | When to Enable |
|--------|---------|---------|----------------|
| `CCAP_BUILD_SHARED` | OFF | Shared library | Multiple apps, plugins |
| `CCAP_NO_LOG` | OFF | Disable logging | Production builds |
| `CCAP_INSTALL` | ON/OFF* | Installation | System-wide install |
| `CCAP_WIN_NO_DEVICE_VERIFY` | OFF | Skip device check | Buggy drivers, VR |
| `CCAP_BUILD_EXAMPLES` | ON/OFF* | Example programs | Learning, testing |
| `CCAP_BUILD_TESTS` | OFF | Unit tests | Development, CI |
| `CCAP_BUILD_CLI` | OFF | CLI tool | Automation, scripting |
| `CCAP_BUILD_CLI_STANDALONE` | OFF | Portable CLI | Distribution |
| `CCAP_FORCE_ARM64` | OFF | ARM compilation | ARM devices, M1/M2 |

*Depends on whether ccap is the root project

## Need Help?

- 📖 [Full Documentation](documentation.md)
- 🐛 [Report Issues](https://github.com/wysaid/CameraCapture/issues)
- 💬 [Discussions](https://github.com/wysaid/CameraCapture/discussions)
- 📧 Email: wysaid@gmail.com
