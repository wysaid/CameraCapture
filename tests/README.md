# CCAP 单元测试说明

## 概述

这个目录包含了 `ccap_convert.h` 的全面单元测试套件，使用 Google Test 框架构建。测试覆盖了所有的像素格式转换功能，包括正确性验证、性能基准测试和边界情况处理。

## 测试架构

### 测试框架

- **测试框架**: Google Test (gtest)
- **构建系统**: CMake
- **C++ 标准**: C++17
- **内存对齐**: 支持 32 字节对齐，用于 SIMD 优化测试

### 测试文件结构

```
tests/
├── CMakeLists.txt                    # CMake 配置文件
├── test_utils.h                      # 测试工具类定义
├── test_utils.cpp                    # 测试工具类实现
├── test_convert_yuv_rgb.cpp          # YUV 到 RGB 单像素转换测试
├── test_convert_color_shuffle.cpp    # 颜色通道重排测试
├── test_convert_yuv_format.cpp       # YUV 格式转换测试 (NV12/I420)
├── test_convert_comprehensive.cpp    # 综合功能测试
├── test_accuracy.cpp                 # 精度测试和参考实现对比
└── test_performance.cpp              # 性能基准测试
```

## 测试覆盖范围

### 1. 颜色通道重排 (Color Shuffle)

- ✅ 4→4 通道重排 (`colorShuffle4To4`)
- ✅ 3→4 通道重排 (`colorShuffle3To4`) - 添加 Alpha 通道
- ✅ 4→3 通道重排 (`colorShuffle4To3`) - 移除 Alpha 通道
- ✅ 3→3 通道重排 (`colorShuffle3To3`)
- ✅ 预定义转换函数 (`rgbaToRgb`, `rgbToBgra` 等)
- ✅ 函数别名验证
- ✅ 往返转换测试 (确保数据完整性)

### 2. YUV 单像素转换

- ✅ BT.601 视频范围 (`yuv2rgb601v`)
- ✅ BT.709 视频范围 (`yuv2rgb709v`)
- ✅ BT.601 全范围 (`yuv2rgb601f`)
- ✅ BT.709 全范围 (`yuv2rgb709f`)
- ✅ 边界值测试 (0, 255, 16, 235 等)
- ✅ 已知测试值验证

### 3. YUV 格式转换

- ✅ NV12 → RGB24/BGR24
- ✅ NV12 → RGBA32/BGRA32
- ✅ I420 → RGB24/BGR24
- ✅ I420 → RGBA32/BGRA32
- ✅ 不同色彩空间标准 (BT.601/BT.709)
- ✅ 不同范围 (视频范围/全范围)
- ✅ Alpha 通道正确性验证

### 4. 精度测试

- ✅ 与参考实现对比
- ✅ MSE 和 PSNR 计算
- ✅ 统计精度分析
- ✅ NV12 和 I420 一致性测试
- ✅ RGB/BGR 关系验证

### 5. 性能测试

- ✅ 1080p 转换性能基准
- ✅ 不同分辨率性能对比
- ✅ 内存带宽测试
- ✅ AVX2 检测和加速验证
- ✅ 吞吐量和 FPS 计算

### 6. 边界和错误情况

- ✅ 最小/最大图像尺寸
- ✅ 内存对齐测试
- ✅ 垂直翻转支持 (负高度)
- ✅ 边界 YUV 值处理
- ✅ 像素值范围验证

## 运行测试

### 方法 1: 使用测试脚本 (推荐)

```bash
# 在 ccap 根目录下运行
./run_tests.sh
```

这个脚本会：

- 自动配置和构建项目
- 运行所有功能测试
- 运行性能基准测试
- 生成 XML 测试报告
- 显示彩色输出和测试摘要

### 方法 2: 手动构建和运行

```bash
# 创建构建目录
mkdir -p build
cd build

# 配置 CMake
cmake .. -DCMAKE_BUILD_TYPE=Debug

# 构建测试
make -j$(nproc) ccap_convert_test ccap_performance_test

# 运行功能测试
./ccap_convert_test

# 运行性能测试
./ccap_performance_test
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
