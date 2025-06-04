# CCAP 测试框架说明

## 概述

这个目录包含了 `ccap_convert.h` 的完整测试套件，使用 Google Test 框架构建。测试套件涵盖了所有像素格式转换功能，包括正确性验证、AVX2 vs CPU 性能对比、边界情况处理和详细的调试工具。

## 测试架构

### 核心技术栈

- **测试框架**: Google Test 1.14.0 (gtest/gmock)
- **构建系统**: CMake 3.14+
- **C++ 标准**: C++17
- **SIMD 支持**: AVX2 优化与回退到纯 CPU 实现
- **内存对齐**: 32 字节对齐，优化 SIMD 操作

### 测试文件结构 (16 个文件)

```
tests/
├── CMakeLists.txt                      # CMake 构建配置
├── test_utils.h                        # 核心测试工具类
├── test_utils.cpp                      # 测试工具实现
├── test_utils_avx2.h                   # AVX2 测试专用工具
│
├── == 核心功能测试 ==
├── test_convert_yuv_rgb.cpp            # YUV 单像素转换测试
├── test_convert_color_shuffle.cpp      # 颜色通道重排测试
├── test_convert_yuv_format.cpp         # YUV 格式转换 (NV12/I420)
├── test_convert_comprehensive.cpp      # 综合功能测试
├── test_convert_platform_features.cpp  # 平台特性检测测试
│
├── == 双实现对比测试 ==
├── test_dual_implementation.cpp        # AVX2 vs CPU 实现对比
├── test_accuracy.cpp                   # 精度测试和参考实现
│
├── == 性能基准测试 ==
├── test_performance.cpp                # 性能基准和 AVX2 对比
│
└── == 调试工具 ==
├── test_debug_simple.cpp              # 简单调试测试
├── test_debug_detailed.cpp            # 详细调试分析
└── test_debug_shuffle_map.cpp         # 颜色映射调试
```

## 测试可执行文件

构建后会生成以下 4 个独立的测试可执行文件：

### 1. `ccap_convert_test` - 核心功能测试
- **模式**: Debug (功能验证优先)
- **包含**: 颜色转换、YUV 处理、精度验证、平台特性检测
- **运行时间**: ~1-2 秒

### 2. `ccap_performance_test` - 性能基准测试  
- **模式**: Release (性能优化)
- **包含**: AVX2 vs CPU 对比、FPS 测量、带宽分析、加速比计算
- **运行时间**: ~3-5 秒

### 3. `ccap_debug_detailed_test` - 详细调试
- **模式**: Debug (详细输出)
- **包含**: 逐像素分析、中间结果输出、内存布局检查
- **运行时间**: ~1-2 秒

### 4. `ccap_debug_shuffle_test` - 颜色映射调试
- **模式**: Debug
- **包含**: 颜色通道映射验证、SIMD 指令调试
- **运行时间**: <1 秒

## 核心测试覆盖范围

### 1. 颜色通道重排 (Color Shuffle)

- ✅ 4→4 通道重排 (`colorShuffle4To4`)
- ✅ 3→4 通道重排 (`colorShuffle3To4`) - 添加 Alpha 通道
- ✅ 4→3 通道重排 (`colorShuffle4To3`) - 移除 Alpha 通道  
- ✅ 3→3 通道重排 (`colorShuffle3To3`)
- ✅ 预定义转换函数 (`rgbaToRgb`, `rgbToBgra` 等)
- ✅ 函数别名验证
- ✅ 往返转换测试 (确保数据完整性)
- ✅ AVX2 vs CPU 输出对比

### 2. YUV 单像素转换

- ✅ BT.601 视频范围 (`yuv2rgb601v`)
- ✅ BT.709 视频范围 (`yuv2rgb709v`)
- ✅ BT.601 全范围 (`yuv2rgb601f`)
- ✅ BT.709 全范围 (`yuv2rgb709f`)
- ✅ 边界值测试 (0, 255, 16, 235 等)
- ✅ 已知测试值验证
- ✅ 双实现精度对比

### 3. YUV 格式转换

- ✅ NV12 → RGB24/BGR24
- ✅ NV12 → RGBA32/BGRA32
- ✅ I420 → RGB24/BGR24
- ✅ I420 → RGBA32/BGRA32
- ✅ 不同色彩空间标准 (BT.601/BT.709)
- ✅ 不同范围 (视频范围/全范围)
- ✅ Alpha 通道正确性验证

### 4. 平台特性检测

- ✅ AVX2 指令集检测 (`ccap::hasAVX2()`)
- ✅ AVX2 动态启用/禁用 (`ccap::disableAVX2()`)
- ✅ 运行时特性切换测试
- ✅ 不同平台兼容性验证

### 5. 双实现对比测试

- ✅ AVX2 vs CPU 输出一致性
- ✅ 像素级精确对比
- ✅ 性能差异分析
- ✅ 自动回退机制验证

### 6. 精度测试

- ✅ 与参考实现对比
- ✅ MSE 和 PSNR 计算
- ✅ 统计精度分析
- ✅ NV12 和 I420 一致性测试
- ✅ RGB/BGR 关系验证

### 7. AVX2 性能优化测试

- ✅ **颜色重排加速**: 6-10x 性能提升
- ✅ **YUV 转换加速**: 8-12x 性能提升
- ✅ **详细性能分析**: FPS、带宽、加速比
- ✅ **多分辨率测试**: VGA 到 4K
- ✅ **效率评估**: EXCELLENT/GOOD/MODERATE 分级

### 8. 性能基准测试

- ✅ 1080p 转换性能基准
- ✅ 不同分辨率性能对比 (640x480 到 3840x2160)
- ✅ 内存带宽测试 (高达 13+ GB/s)
- ✅ AVX2 检测和加速验证
- ✅ 吞吐量和 FPS 计算

### 6. 边界和错误情况

- ✅ 最小/最大图像尺寸
- ✅ 内存对齐测试
- ✅ 垂直翻转支持 (负高度)
- ✅ 边界 YUV 值处理
- ✅ 像素值范围验证

## AVX2 优化测试框架

### AVX2TestRunner 工具类

专门的 AVX2 测试工具 (`test_utils_avx2.h`)，提供：

- **`runWithBothImplementations()`**: 自动测试 AVX2 和 CPU 两种实现
- **`runImageComparisonTest()`**: 像素级输出对比
- **`benchmarkAVX2Comparison()`**: 性能对比分析

### 性能对比测试

每个性能测试都包含详细的 AVX2 vs CPU 对比：

```
=== AVX2 vs CPU Performance Analysis ===
AVX2 Implementation:  4.2ms (1500+ FPS)
CPU Implementation:   28.7ms (35 FPS) 
Speedup: 6.8x faster with AVX2
Efficiency: EXCELLENT
Bandwidth: 13.2 GB/s
```

### 智能回退机制

- 自动检测 AVX2 支持
- 不支持时优雅回退到 CPU 实现
- 运行时动态切换测试

## 运行测试

### 方法 1: 使用测试脚本 (推荐)

```bash
# 在 ccap 根目录下运行
./scripts/run_tests.sh

# 或者运行特定类型的测试
./scripts/run_tests.sh --functional    # 只运行功能测试 (Debug 模式)
./scripts/run_tests.sh --performance   # 只运行性能测试 (Release 模式)
./scripts/run_tests.sh --avx2         # 只运行 AVX2 性能测试
```

这个脚本会：

- 自动配置和构建 Debug/Release 版本
- 运行所有功能测试 (Debug 模式)
- 运行性能基准测试 (Release 模式)
- 生成 XML 测试报告
- 显示彩色输出和测试摘要
- 提供详细的 AVX2 性能分析

### 方法 2: 手动构建和运行

```bash
# 创建构建目录
mkdir -p build/Debug build/Release

# 配置和构建 Debug 版本 (功能测试)
cd build/Debug
cmake ../.. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)

# 配置和构建 Release 版本 (性能测试)
cd ../Release
cmake ../.. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# 运行所有测试
cd Debug/tests
./ccap_convert_test          # 核心功能测试
./ccap_debug_detailed_test   # 详细调试测试
./ccap_debug_shuffle_test    # 颜色映射调试

cd ../../Release/tests
./ccap_performance_test      # 性能基准测试
```

### 方法 3: 使用 VS Code 任务

项目已配置 VS Code 任务，可以通过以下方式运行：

1. 打开命令面板 (Cmd+Shift+P)
2. 选择 "Tasks: Run Task"
3. 选择相应的构建和测试任务

## 测试工具类

### TestImage

内存对齐的图像数据容器，支持：

- 自动内存管理
- 32 字节对齐 (SIMD 优化)
- 多种填充模式 (梯度、随机、纯色、棋盘)

### TestYUVImage

YUV 格式图像容器，支持：

- NV12 和 I420 格式
- 分离的 Y、U、V 平面
- 已知模式生成

### AVX2TestRunner

AVX2 专用测试工具类 (`test_utils_avx2.h`)：

- **双实现测试**: 自动运行 AVX2 和 CPU 两种实现
- **性能对比**: 详细的性能分析和加速比计算
- **智能回退**: 在不支持 AVX2 的平台上优雅降级

### PixelTestUtils

像素比较和分析工具：

- 图像比较 (支持容差)
- MSE/PSNR 计算
- RGB 值有效性检查
- 参考 YUV→RGB 转换

### PerformanceTimer

高精度性能计时器：

- 毫秒级精度
- 简单的开始/停止接口

## 性能基准

### 典型性能指标 (1920x1080)

- **NV12→RGB24**: < 10ms (>100 FPS)
- **I420→RGB24**: < 12ms (>80 FPS)
- **RGBA→RGB**: < 3ms (>300 FPS)
- **4 通道重排**: < 5ms (>200 FPS)

### 性能测试包括

- 不同分辨率对比 (VGA 到 4K)
- 内存带宽基准
- AVX2 加速效果
- 吞吐量分析

## 精度要求

### 容差标准

- **颜色重排**: 0 (完全匹配)
- **YUV 转换**: ≤2 (由于舍入误差)
- **PSNR**: >40 dB
- **准确像素**: >95%

## 持续集成

测试套件可以轻松集成到 CI/CD 流水线中：

```yaml
# 示例 GitHub Actions 配置
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

## 扩展测试

### 添加新测试

1. 在适当的测试文件中添加新的 `TEST_F` 函数
2. 或创建新的测试文件并更新 `CMakeLists.txt`
3. 使用现有的测试工具类简化实现

### 自定义基准

在 `test_performance.cpp` 中添加新的性能测试：

```cpp
TEST_F(PerformanceTest, MyCustomBenchmark) {
    benchmarkFunction("My Function", [&]() {
        // 你的代码
    }, iterations);
}
```

## 故障排除

### 常见问题

1. **内存对齐错误**: 确保使用 `TestImage` 类而不是原始指针
2. **性能测试失败**: 在较慢的硬件上调整性能阈值
3. **精度测试失败**: 检查 YUV 输入值是否在有效范围内

### 调试技巧

- 使用 `EXPECT_NEAR` 而不是 `EXPECT_EQ` 进行浮点比较
- 添加详细的失败消息以便调试
- 使用 `--gtest_filter` 运行特定测试

## 贡献指南

在添加新功能到 `ccap_convert.h` 时，请：

1. 添加相应的单元测试
2. 包含边界情况测试
3. 添加性能基准 (对于计算密集型函数)
4. 更新此文档

通过这个全面的测试套件，`ccap_convert.h` 的质量和可靠性得到了充分保障。
