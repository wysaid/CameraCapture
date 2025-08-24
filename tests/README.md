# CCAP Testing Framework

## Overview

Comprehensive test suite for ccap library built with Google Test, covering pixel format conversion, multi-backend optimization, and performance validation.

## Test Structure

- **Core Tests**: Color conversion, YUV processing, platform features
- **Performance Tests**: Multi-backend benchmarks (CPU/AVX2/Apple Accelerate/NEON)  
- **Debug Tools**: Detailed analysis and debugging utilities
- **Technology**: Google Test 1.14.0, CMake 3.14+, C++17

## Running Tests

### Automated Testing

```bash
# Run all tests
./scripts/run_tests.sh

# Specific test types
./scripts/run_tests.sh --functional    # Core functionality
./scripts/run_tests.sh --performance   # Performance benchmarks
```

### Manual Testing

```bash
# Build tests
mkdir build && cd build
cmake .. -DCCAP_BUILD_TESTS=ON
cmake --build .

# Run individual test executables
./tests/ccap_convert_test          # Core functionality
./tests/ccap_performance_test      # Performance benchmarks
```

## Test Coverage

### Core Functionality

- Color channel shuffling (RGB/BGR conversions)
- YUV format conversion (NV12/I420 to RGB/BGR)
- Single-pixel YUV conversion with BT.601/BT.709 standards
- Platform feature detection (AVX2, Apple Accelerate, NEON)

### Performance Validation

- Multi-backend comparison across CPU, AVX2, Apple Accelerate, NEON
- CCAP vs LibYUV benchmarks
- Memory bandwidth testing
- FPS and throughput analysis

### Quality Assurance

- High accuracy validation for pixel format conversions
- Round-trip conversion integrity tests
- Boundary condition handling
- Cross-platform compatibility verification

## Performance Metrics

Performance results vary by hardware and configuration. The test suite provides benchmarks for:

- **Color Shuffling**: Optimized implementations show significant speedup over CPU baseline
- **YUV Conversion**: Performance varies by operation type and backend
- **Accuracy**: High pixel accuracy with good PSNR values

## Contributing

When adding new features:

1. Add corresponding unit tests
2. Include edge case validation
3. Add performance benchmarks for compute-intensive functions
4. Update documentation
