# Changelog

All notable changes to the ccap (CameraCapture) project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Updated C interface API descriptions and complete backend documentation

### Fixed
- Various documentation improvements and clarifications

## [1.2.1] - 2025-08-24

### Added
- Comprehensive error callback system for both C++ and C interfaces
- Global error callback system with improved error handling
- Static assertions for enums between C and C++ interfaces
- Enhanced documentation for error callback system

### Changed
- Optimized error callback implementation
- Removed 'global' keyword from error callback function names
- Updated all examples to use new error callback system

### Fixed
- Improved error handling throughout the library
- Better optimization information printing

## [1.2.0] - 2025-08-14

### Added
- Complete NEON acceleration support for ARM platforms
- YUYV and UYVY format conversion with NEON backend
- Windows ARM64 platform support
- Comprehensive build scripts for cross-platform development
- Enhanced iOS demo with more configuration options
- High frame rate support (60/120 FPS) for Apple devices
- Pure C interface implementation with full API coverage
- Unit testing framework with improved coverage

### Changed
- Complete API optimization with removal of legacy functions
- Optimized `CcapDeviceInfo` structure with fixed-size arrays
- Simplified examples with clearer structure
- Improved workflow configurations and build tasks
- Better code practices and compilation warning fixes

### Fixed
- NEON acceleration bugs and compilation issues
- Cross-platform compatibility issues
- Exception handling improvements
- ARM64 compilation restrictions on latest system versions
- Callback handling errors in C interface

### Performance
- NEON backend performance optimizations
- Better support for high frame rate modes on Apple devices

## [1.1.0] - 2025-08-06

### Added
- Full Linux platform support with Ubuntu and Fedora compatibility
- Linux ARM64 build support and CI/CD pipelines
- AVX2 acceleration for YUYV and UYVY formats on x86_64
- Pure C interface with comprehensive API coverage
- YUYV/UYVY pixel format support
- Zero-copy mode optimizations
- Enhanced unit testing framework

### Changed
- Enhanced compatibility for building on Linux
- Refactored Linux implementation with improved lifecycle management
- Optimized format conversion with fallback mechanisms
- Improved AVX2 backend implementation
- Better logging system for Linux platform

### Fixed
- Format conversion failure handling
- Zero-copy mode issues on Linux
- AVX2 backend bugs and edge cases
- Cross-compilation issues for testing
- Orientation handling on Linux platform

### Performance
- Significant performance improvements with AVX2 acceleration
- Optimized pixel format conversion pipeline
- Better memory management and lifecycle optimization

## [1.0.2] - 2025-07-28

### Fixed
- Critical bug in `__mingw_aligned_malloc` parameter handling
- Compilation errors on MinGW-w64 and MINGW platforms
- Windows build stability improvements

## [1.0.1] - 2025-07-01

### Added
- Linux platform compilation support
- Enhanced installation instructions with more details

### Changed
- Improved CMake configuration for cross-platform builds
- Better artifact generation for Linux pipelines

### Fixed
- Compilation issues on Linux platforms
- CMake configuration errors

## [1.0.0] - 2025-06-14

### Added
- Initial stable release of ccap (CameraCapture) library
- Cross-platform camera capture support (Windows, macOS, iOS)
- Multiple backend support:
  - DirectShow and Media Foundation on Windows
  - AVFoundation on macOS/iOS
  - V4L2 on Linux (preliminary)
- Hardware acceleration support:
  - Apple Accelerate (vImage) on macOS/iOS
  - AVX2 SIMD acceleration on x86_64
- Pixel format conversion support:
  - RGB/BGR color channel shuffling
  - YUV to RGB conversion (NV12, I420, YUYV, UYVY)
  - Vertical flip operations
- OpenCV integration with `cv::Mat` adapter
- Comprehensive examples and demos:
  - Console-based capture examples
  - GUI demos with GLFW
  - iOS Metal and OpenGL ES demos
- Zero-copy mode for optimal performance
- Flexible camera device enumeration and selection
- CMake-based build system with pkg-config support

### Performance
- SIMD-optimized color conversions outperforming libyuv in many cases
- Efficient memory management with 64-byte aligned allocations
- Hardware-accelerated operations where available

### Documentation
- Comprehensive API documentation
- Cross-platform build instructions
- Example usage and integration guides
- Performance benchmarking results

---

## Development History (2025-04-22 to 2025-06-14)

### Project Initialization (April 2025)
- **2025-04-22**: Repository initialization with basic interface design
- **2025-04-23**: Windows backend architecture completed with DirectShow support
- **2025-04-24**: macOS implementation added with AVFoundation backend
- **2025-04-25**: Initial macOS demo application
- **2025-04-27**: Complete macOS implementation with logging module
- **2025-04-28**: Auto-resolution detection and FPS optimization

### Major Feature Development (May 2025)
- **2025-05-06**: Zero-copy mode implementation and enum optimizations
- **2025-05-17**: Removed third-party dependencies, implemented custom SIMD acceleration
- **2025-05-19**: OpenCV adapter and performance optimizations
- **2025-05-21**: Project structure refactoring and AVX2 conversion fixes
- **2025-05-27**: iOS support implementation with native handle
- **2025-05-28**: Apple Accelerate backend optimization

### Testing and Optimization (June 2025)
- **2025-06-03**: Performance monitoring and optimization tools
- **2025-06-14**: Complete unit testing framework implementation
- **2025-06-14**: CMake installation support and optimized build system

## Migration Notes

### Breaking Changes in 1.2.0
- Legacy API functions have been completely removed
- `CcapDeviceInfo` structure has been optimized with fixed-size arrays
- Error callback system has been redesigned - update your error handling code

### Upgrading from 1.1.x to 1.2.x
- Update error handling to use the new global error callback system
- Replace any usage of deprecated API functions
- Recompile with updated header files for `CcapDeviceInfo` changes

### Platform Support Matrix
| Platform | Version | Architecture | Status |
|----------|---------|--------------|--------|
| Windows | 10+ | x86_64, ARM64 | ✅ Full Support |
| macOS | 10.13+ | x86_64, ARM64 | ✅ Full Support |
| iOS | 11.0+ | ARM64 | ✅ Full Support |
| Linux | Ubuntu 20.04+, Fedora 34+ | x86_64, ARM64 | ✅ Full Support |

### Acceleration Backend Support
| Backend | Platform | Status | Performance |
|---------|----------|--------|-------------|
| Apple Accelerate | macOS, iOS | ✅ Stable | High |
| AVX2 | x86_64 | ✅ Stable | High |
| NEON | ARM64 | ✅ Stable | High |
| CPU Fallback | All | ✅ Stable | Baseline |