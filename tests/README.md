# CCAP Testing Framework Documentation

## Overview

This directory contains a comprehensive test suite for `ccap_convert.h`, built using the Google Test framework. The test suite covers all pixel format conversion functions, including correctness verification, AVX2 vs CPU performance comparison, edge case handling, and detailed debugging tools.

## Testing Architecture

### Core Technology Stack

- **Testing Framework**: Google Test 1.14.0 (gtest/gmock)
- **Build System**: CMake 3.14+
- **C++ Standard**: C++17
- **SIMD Support**: AVX2 optimization with fallback to pure CPU implementation
- **Memory Alignment**: 32-byte alignment for optimized SIMD operations

### Test File Structure (16 files)

```
tests/
├── CMakeLists.txt                      # CMake build configuration
├── test_utils.h                        # Core testing utility classes
├── test_utils.cpp                      # Testing utility implementation
├── test_utils_avx2.h                   # AVX2-specific testing utilities
│
├── == Core Functionality Tests ==
├── test_convert_yuv_rgb.cpp            # YUV single-pixel conversion tests
├── test_convert_color_shuffle.cpp      # Color channel shuffling tests
├── test_convert_yuv_format.cpp         # YUV format conversion (NV12/I420)
├── test_convert_comprehensive.cpp      # Comprehensive functionality tests
├── test_convert_platform_features.cpp  # Platform feature detection tests
│
├── == Dual Implementation Comparison Tests ==
├── test_dual_implementation.cpp        # AVX2 vs CPU implementation comparison
├── test_accuracy.cpp                   # Accuracy tests and reference implementation
│
├── == Performance Benchmark Tests ==
├── test_performance.cpp                # Performance benchmarks and AVX2 comparison
│
└── == Debugging Tools ==
├── test_debug_simple.cpp              # Simple debugging tests
├── test_debug_detailed.cpp            # Detailed debugging analysis
└── test_debug_shuffle_map.cpp         # Color mapping debugging
```

## Test Executables

After building, the following 4 independent test executables are generated:

### 1. `ccap_convert_test` - Core Functionality Tests
- **Mode**: Debug (functionality verification priority)
- **Includes**: Color conversion, YUV processing, accuracy verification, platform feature detection
- **Runtime**: ~1-2 seconds

### 2. `ccap_performance_test` - Performance Benchmark Tests  
- **Mode**: Release (performance optimization)
- **Includes**: AVX2 vs CPU comparison, FPS measurement, bandwidth analysis, speedup calculation
- **Runtime**: ~3-5 seconds

### 3. `ccap_debug_detailed_test` - Detailed Debugging
- **Mode**: Debug (detailed output)
- **Includes**: Pixel-by-pixel analysis, intermediate result output, memory layout inspection
- **Runtime**: ~1-2 seconds

### 4. `ccap_debug_shuffle_test` - Color Mapping Debugging
- **Mode**: Debug
- **Includes**: Color channel mapping verification, SIMD instruction debugging
- **Runtime**: <1 second

## Core Test Coverage

### 1. Color Channel Shuffling

- ✅ 4→4 channel shuffling (`colorShuffle4To4`)
- ✅ 3→4 channel shuffling (`colorShuffle3To4`) - Add Alpha channel
- ✅ 4→3 channel shuffling (`colorShuffle4To3`) - Remove Alpha channel  
- ✅ 3→3 channel shuffling (`colorShuffle3To3`)
- ✅ Predefined conversion functions (`rgbaToRgb`, `rgbToBgra`, etc.)
- ✅ Function alias verification
- ✅ Round-trip conversion tests (ensure data integrity)
- ✅ AVX2 vs CPU output comparison

### 2. YUV Single-Pixel Conversion

- ✅ BT.601 video range (`yuv2rgb601v`)
- ✅ BT.709 video range (`yuv2rgb709v`)
- ✅ BT.601 full range (`yuv2rgb601f`)
- ✅ BT.709 full range (`yuv2rgb709f`)
- ✅ Boundary value tests (0, 255, 16, 235, etc.)
- ✅ Known test value verification
- ✅ Dual implementation accuracy comparison

### 3. YUV Format Conversion

- ✅ NV12 → RGB24/BGR24
- ✅ NV12 → RGBA32/BGRA32
- ✅ I420 → RGB24/BGR24
- ✅ I420 → RGBA32/BGRA32
- ✅ Different color space standards (BT.601/BT.709)
- ✅ Different ranges (video range/full range)
- ✅ Alpha channel correctness verification

### 4. Platform Feature Detection

- ✅ AVX2 instruction set detection (`ccap::hasAVX2()`)
- ✅ AVX2 dynamic enable/disable (`ccap::disableAVX2()`)
- ✅ Runtime feature switching tests
- ✅ Cross-platform compatibility verification

### 5. Dual Implementation Comparison Tests

- ✅ AVX2 vs CPU output consistency
- ✅ Pixel-level exact comparison
- ✅ Performance difference analysis
- ✅ Automatic fallback mechanism verification

### 6. Accuracy Tests

- ✅ Comparison with reference implementation
- ✅ MSE and PSNR calculation
- ✅ Statistical accuracy analysis
- ✅ NV12 and I420 consistency tests
- ✅ RGB/BGR relationship verification

### 7. AVX2 Performance Optimization Tests

- ✅ **Color shuffling acceleration**: 6-10x performance improvement
- ✅ **YUV conversion acceleration**: 8-12x performance improvement
- ✅ **Detailed performance analysis**: FPS, bandwidth, speedup ratio
- ✅ **Multi-resolution testing**: VGA to 4K
- ✅ **Efficiency assessment**: EXCELLENT/GOOD/MODERATE classification

### 8. Performance Benchmark Tests

- ✅ 1080p conversion performance benchmarks
- ✅ Different resolution performance comparison (640x480 to 3840x2160)
- ✅ Memory bandwidth testing (up to 13+ GB/s)
- ✅ AVX2 detection and acceleration verification
- ✅ Throughput and FPS calculation

### 9. Boundary and Error Cases

- ✅ Minimum/maximum image dimensions
- ✅ Memory alignment testing
- ✅ Vertical flip support (negative height)
- ✅ Boundary YUV value handling
- ✅ Pixel value range verification

## AVX2 Optimization Testing Framework

### AVX2TestRunner Utility Class

Specialized AVX2 testing tools (`test_utils_avx2.h`) providing:

- **`runWithBothImplementations()`**: Automatically test both AVX2 and CPU implementations
- **`runImageComparisonTest()`**: Pixel-level output comparison
- **`benchmarkAVX2Comparison()`**: Performance comparison analysis

### Performance Comparison Tests

Each performance test includes detailed AVX2 vs CPU comparison:

```
=== AVX2 vs CPU Performance Analysis ===
AVX2 Implementation:  4.2ms (1500+ FPS)
CPU Implementation:   28.7ms (35 FPS) 
Speedup: 6.8x faster with AVX2
Efficiency: EXCELLENT
Bandwidth: 13.2 GB/s
```

### Intelligent Fallback Mechanism

- Automatic AVX2 support detection
- Graceful fallback to CPU implementation when not supported
- Runtime dynamic switching tests

## Running Tests

### Method 1: Using Test Script (Recommended)

```bash
# Run from ccap root directory
./scripts/run_tests.sh

# Or run specific types of tests
./scripts/run_tests.sh --functional    # Only run functional tests (Debug mode)
./scripts/run_tests.sh --performance   # Only run performance tests (Release mode)
./scripts/run_tests.sh --avx2         # Only run AVX2 performance tests
```

This script will:

- Automatically configure and build Debug/Release versions
- Run all functional tests (Debug mode)
- Run performance benchmark tests (Release mode)
- Generate XML test reports
- Display colored output and test summary
- Provide detailed AVX2 performance analysis

### Method 2: Manual Build and Run

```bash
# Create build directories
mkdir -p build/Debug build/Release

# Configure and build Debug version (functional tests)
cd build/Debug
cmake ../.. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)

# Configure and build Release version (performance tests)
cd ../Release
cmake ../.. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Run all tests
cd Debug/tests
./ccap_convert_test          # Core functionality tests
./ccap_debug_detailed_test   # Detailed debugging tests
./ccap_debug_shuffle_test    # Color mapping debugging

cd ../../Release/tests
./ccap_performance_test      # Performance benchmark tests
```

### Method 3: Using VS Code Tasks

The project has configured VS Code tasks that can be run via:

1. Open Command Palette (Cmd+Shift+P)
2. Select "Tasks: Run Task"
3. Choose the appropriate build and test tasks

## Testing Utility Classes

### TestImage

Memory-aligned image data container supporting:

- Automatic memory management
- 32-byte alignment (SIMD optimization)
- Multiple fill patterns (gradient, random, solid color, checkerboard)

### TestYUVImage

YUV format image container supporting:

- NV12 and I420 formats
- Separate Y, U, V planes
- Known pattern generation

### AVX2TestRunner

AVX2-specific testing utility class (`test_utils_avx2.h`):

- **Dual implementation testing**: Automatically run both AVX2 and CPU implementations
- **Performance comparison**: Detailed performance analysis and speedup calculation
- **Intelligent fallback**: Graceful degradation on platforms without AVX2 support

### PixelTestUtils

Pixel comparison and analysis tools:

- Image comparison (with tolerance support)
- MSE/PSNR calculation
- RGB value validity checking
- Reference YUV→RGB conversion

### PerformanceTimer

High-precision performance timer:

- Millisecond precision
- Simple start/stop interface

## Performance Benchmarks

### Typical Performance Metrics (1920x1080)

- **NV12→RGB24**: < 10ms (>100 FPS)
- **I420→RGB24**: < 12ms (>80 FPS)
- **RGBA→RGB**: < 3ms (>300 FPS)
- **4-channel shuffling**: < 5ms (>200 FPS)

### Performance Testing Includes

- Different resolution comparisons (VGA to 4K)
- Memory bandwidth benchmarks
- AVX2 acceleration effects
- Throughput analysis

## Accuracy Requirements

### Tolerance Standards

- **Color shuffling**: 0 (exact match)
- **YUV conversion**: ≤2 (due to rounding errors)
- **PSNR**: >40 dB
- **Accurate pixels**: >95%

## Continuous Integration

The test suite can be easily integrated into CI/CD pipelines:

```yaml
# Example GitHub Actions configuration
- name: Run Tests
  run: |
    cd ccap
    ./run_tests.sh
    
- name: Upload Test Results
  uses: actions/upload-artifact@v2
  with:
    name: test-results
    path: build/test_results.xml
```

## Extending Tests

### Adding New Tests

1. Add new `TEST_F` functions in appropriate test files
2. Or create new test files and update `CMakeLists.txt`
3. Use existing testing utility classes to simplify implementation

### Custom Benchmarks

Add new performance tests in `test_performance.cpp`:

```cpp
TEST_F(PerformanceTest, MyCustomBenchmark) {
    benchmarkFunction("My Function", [&]() {
        // Your code here
    }, iterations);
}
```

## Troubleshooting

### Common Issues

1. **Memory alignment errors**: Ensure using `TestImage` class instead of raw pointers
2. **Performance test failures**: Adjust performance thresholds on slower hardware
3. **Accuracy test failures**: Check if YUV input values are within valid range

### Debugging Tips

- Use `EXPECT_NEAR` instead of `EXPECT_EQ` for floating-point comparisons
- Add detailed failure messages for debugging
- Use `--gtest_filter` to run specific tests

## Contribution Guidelines

When adding new features to `ccap_convert.h`, please:

1. Add corresponding unit tests
2. Include edge case tests
3. Add performance benchmarks (for compute-intensive functions)
4. Update this documentation

Through this comprehensive test suite, the quality and reliability of `ccap_convert.h` are fully ensured.
