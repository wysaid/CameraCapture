# Implementation Details

Deep dive into ccap's internals and advanced usage patterns for professional developers.

## 📚 Contents

- [Memory Management & Video Playback](#memory-management--video-playback)
- [CMake Build Options](#cmake-build-options)
- [C Interface](#c-interface)
- [Performance Optimization](#performance-optimization)

---

## Memory Management & Video Playback

Understanding ccap's memory management for efficient camera and video file processing.

### Key Concepts

| Mode | Frame Dropping | Memory | Best For |
|------|---------------|---------|----------|
| Camera Mode | Yes (keeps latest) | Bounded | Live streaming, real-time |
| File Mode | No (backpressure) | Bounded | Video analysis, processing |

**Topics Covered:**
- Camera vs File mode differences
- Backpressure mechanism explained
- Buffer configuration strategies
- Playback speed control
- Memory footprint analysis

**[Read Full Guide →](video-memory-management.md)** | **[阅读完整指南 →](video-memory-management.zh.md)**

### Quick Example

```cpp
// File mode: Process ALL frames without drops
ccap::Provider provider;
provider.open("video.mp4", true);  // Default PlaybackSpeed=0

while (auto frame = provider.grab(1000)) {
    // Backpressure ensures no frames are lost
    // Memory stays constant (~25-30MB for HD)
    processFrame(frame);
}
```

---

## CMake Build Options

Complete reference for customizing ccap builds for different deployment scenarios.

### Quick Presets

**Development Build:**
```bash
cmake -B build -DCCAP_BUILD_EXAMPLES=ON -DCCAP_BUILD_TESTS=ON
```

**Production Build:**
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCCAP_NO_LOG=ON
```

**Standalone CLI:**
```bash
cmake -B build -DCCAP_BUILD_CLI_STANDALONE=ON
```

**Topics Covered:**
- Core library options
- Platform-specific configurations
- Development vs production builds
- Cross-compilation strategies
- Static runtime linking
- Troubleshooting common issues

**[Read Full Guide →](cmake-options.md)** | **[阅读完整指南 →](cmake-options.zh.md)**

### Options Reference

| Option | Default | Purpose |
|--------|---------|---------|
| `CCAP_BUILD_SHARED` | OFF | Build as shared library |
| `CCAP_NO_LOG` | OFF | Disable logging for smaller binaries |
| `CCAP_WIN_NO_DEVICE_VERIFY` | OFF | Fix crashes with buggy drivers |
| `CCAP_BUILD_CLI_STANDALONE` | OFF | Portable CLI with static runtime |
| `CCAP_FORCE_ARM64` | OFF | Force ARM64 compilation |

---

## C Interface

Pure C99 API for language bindings and traditional C projects.

**[View Full C Interface Documentation →](c-interface.md)**

### Why Use C Interface?

- ✅ Language bindings (Python, Go, Rust, etc.)
- ✅ Embedded systems integration
- ✅ Legacy C codebase compatibility
- ✅ Stable ABI for shared libraries

### Example

```c
#include <ccap_c.h>

int main() {
    CcapProvider* provider = ccap_provider_create();
    
    if (ccap_provider_open(provider, NULL, true)) {
        CcapVideoFrame* frame = ccap_provider_grab(provider, 3000);
        if (frame) {
            CcapVideoFrameInfo info;
            ccap_video_frame_get_info(frame, &info);
            printf("Captured: %dx%d\n", info.width, info.height);
            ccap_video_frame_release(frame);
        }
    }
    
    ccap_provider_destroy(provider);
    return 0;
}
```

**[Read Full C API Documentation →](c-interface.md)**

### Key Features

- Opaque pointers for encapsulation
- Manual memory management with clear ownership
- Thread-safe operations
- Full feature parity with C++ API

---

## Performance Optimization

### Hardware Acceleration

ccap automatically uses platform-specific optimizations:

| Platform | Technology | Speedup |
|----------|-----------|---------|
| x86_64 | AVX2 SIMD | ~10x |
| macOS | Apple Accelerate | ~8x |
| ARM64 | NEON SIMD | ~6x |

**Pixel Format Conversion Performance:**

```
Benchmark: 1920×1080 RGB → NV12
├── Baseline (C): 12.5ms
├── AVX2 (x86): 1.2ms (10.4x faster)
├── Accelerate (macOS): 1.5ms (8.3x faster)
└── NEON (ARM): 2.1ms (6.0x faster)
```

### Tips for Maximum Performance

1. **Use Native Formats When Possible:**
   ```cpp
   // Avoid conversion overhead
   provider.set(ccap::PropertyName::PixelFormat, 
                ccap::PixelFormat::NV12);  // Native on many platforms
   ```

2. **Adjust Buffer Size:**
   ```cpp
   // Lower latency for real-time applications
   provider.setMaxAvailableFrameSize(1);
   
   // Smoother playback for video files
   provider.setMaxAvailableFrameSize(5);
   ```

3. **Use Callback Mode for Zero-Copy:**
   ```cpp
   provider.setFrameCallback([](auto frame) {
       // Process frame immediately, no queue overhead
       processFrame(frame);
   });
   ```

4. **Enable Hardware Acceleration:**
   Hardware acceleration is enabled by default. To verify:
   ```bash
   # Check if AVX2 is available (x86_64)
   ./scripts/test_arch.sh
   
   # Verify NEON usage (ARM64)
   ./scripts/verify_neon.sh
   ```

### Benchmarking

Run performance tests:
```bash
cmake -B build -DCCAP_BUILD_TESTS=ON
cmake --build build
./build/tests/ccap_tests --gtest_filter=*Performance*
```

Or use the script:
```bash
./scripts/run_tests.sh performance
```

---

## Platform-Specific Notes

### Windows

**DirectShow Backend:**
- Mature, stable API
- Good driver compatibility
- Known issue: Some VR headsets crash during enumeration
  - **Solution:** Use `CCAP_WIN_NO_DEVICE_VERIFY=ON`

**Virtual Cameras:**
- OBS Virtual Camera: ✅ Supported
- ManyCam: ✅ Supported
- NDI Virtual Input: ✅ Supported

### macOS / iOS

**AVFoundation Backend:**
- Modern, efficient API
- Excellent hardware integration
- Native format support: NV12, BGRA

**Camera Permissions:**
```xml
<!-- Info.plist -->
<key>NSCameraUsageDescription</key>
<string>Camera access required for video capture</string>
```

### Linux

**V4L2 Backend:**
- Direct kernel interface
- Low overhead
- Wide format support

**Camera Access:**
```bash
# Add user to video group
sudo usermod -a -G video $USER

# Verify devices
ls -l /dev/video*
```

---

## Development Tools

### Custom Build Configuration

Use `cmake/dev.cmake` (git-ignored) for persistent local settings:

```cmake
# cmake/dev.cmake

# Enable all development features
set(CCAP_BUILD_EXAMPLES ON CACHE BOOL "" FORCE)
set(CCAP_BUILD_TESTS ON CACHE BOOL "" FORCE)
set(CCAP_BUILD_CLI ON CACHE BOOL "" FORCE)

# Add custom compiler warnings
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")

# Custom installation prefix
set(CMAKE_INSTALL_PREFIX "$ENV{HOME}/.local" CACHE PATH "" FORCE)
```

See `cmake/dev.cmake.example` for more options.

### Available Scripts

```bash
# Build and install
./scripts/build_and_install.sh

# Format all source files
./scripts/format_all.sh

# Run all tests
./scripts/run_tests.sh

# Run specific test category
./scripts/run_tests.sh functional
./scripts/run_tests.sh performance
./scripts/run_tests.sh shuffle

# Update version across all files
./scripts/update_version.sh 1.2.3

# Test architecture-specific optimizations
./scripts/test_arch.sh       # x86_64 AVX2
./scripts/verify_neon.sh     # ARM64 NEON

# Cross-compilation
./scripts/build_arm64.sh     # ARM64 Linux
./scripts/build_arm64_win.sh # ARM64 Windows
./scripts/build_macos_universal.sh  # Universal macOS binary
```

---

## Testing

### Test Categories

**Functional Tests:**
```bash
./scripts/run_tests.sh functional
```
- Basic capture operations
- Format conversions
- Device enumeration
- Error handling

**Performance Tests:**
```bash
./scripts/run_tests.sh performance
```
- Conversion speed benchmarks
- Memory usage analysis
- Latency measurements

**Shuffle Tests:**
```bash
./scripts/run_tests.sh shuffle
```
- Randomized execution order
- Race condition detection
- Thread safety validation

**Video Tests** (macOS/Windows only):
```bash
./scripts/run_tests.sh video
```
- File mode backpressure
- Playback speed control
- Memory safety with long videos

### Continuous Integration

All tests run automatically on:
- ✅ Windows (x64, ARM64)
- ✅ macOS (x64, ARM64 universal)
- ✅ Linux (x64, ARM64)

GitHub Actions workflows:
- `.github/workflows/windows-build.yml`
- `.github/workflows/macos-build.yml`
- `.github/workflows/linux-build.yml`

---

## Contributing

### Code Guidelines

From `.github/copilot-instructions.md`:

- ✅ Add tests for bug fixes or new features
- ✅ Run all tests before committing
- ✅ Use English for commit messages and PRs
- ✅ `.md` files in `docs/` must be in English
- ✅ Run `./scripts/format_all.sh` before committing
- ✅ Use `./scripts/update_version.sh <version>` to update versions

### Development Workflow

1. **Create feature branch:**
   ```bash
   git checkout -b feature/my-feature
   ```

2. **Make changes and test:**
   ```bash
   ./scripts/format_all.sh
   ./scripts/run_tests.sh
   ```

3. **Commit and push:**
   ```bash
   git add .
   git commit -m "Add: Description of changes"
   git push origin feature/my-feature
   ```

4. **Create Pull Request**

---

## Need Help?

- 📖 [Main Documentation](documentation.md)
- 📖 [CLI Tool Guide](cli.md)
- 🐛 [Report Issues](https://github.com/wysaid/CameraCapture/issues)
- 💬 [Discussions](https://github.com/wysaid/CameraCapture/discussions)
- 📧 Email: wysaid@gmail.com
- 🌐 Website: [ccap.work](https://ccap.work)

---

## Related Resources

- [Build and Install Guide](https://github.com/wysaid/CameraCapture/blob/main/BUILD_AND_INSTALL.md)
- [Example Programs](https://github.com/wysaid/CameraCapture/tree/main/examples)
- [GitHub Repository](https://github.com/wysaid/CameraCapture)
- [Release Notes](https://github.com/wysaid/CameraCapture/releases)
