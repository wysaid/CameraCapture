# CCAP 测试框架

## 概述

基于 Google Test 构建的完整测试套件，涵盖像素格式转换、多后端优化和性能验证。

## 测试结构

- **核心测试**：颜色转换、YUV 处理、平台特性
- **性能测试**：多后端基准测试（CPU/AVX2/Apple Accelerate）
- **调试工具**：详细分析和调试实用程序
- **技术栈**：Google Test 1.14.0、CMake 3.14+、C++17

## 运行测试

### 自动化测试

```bash
# 运行所有测试
./scripts/run_tests.sh

# 特定测试类型
./scripts/run_tests.sh --functional    # 核心功能
./scripts/run_tests.sh --performance   # 性能基准测试
```

### 手动测试

```bash
# 构建测试
mkdir build && cd build
cmake .. -DCCAP_BUILD_TESTS=ON
cmake --build .

# 运行单独的测试可执行文件
./tests/ccap_convert_test          # 核心功能
./tests/ccap_performance_test      # 性能基准测试
```

## 测试覆盖范围

### 核心功能

- 颜色通道重排（RGB/BGR 转换）
- YUV 格式转换（NV12/I420 到 RGB/BGR）
- YUV 单像素转换，支持 BT.601/BT.709 标准
- 平台特性检测（AVX2、Apple Accelerate）

### 性能验证

- CPU、AVX2、Apple Accelerate 多后端比较
- CCAP vs LibYUV 基准测试
- 内存带宽测试
- FPS 和吞吐量分析

### 质量保证

- 像素格式转换高精度验证
- 往返转换完整性测试
- 边界条件处理
- 跨平台兼容性验证

## 性能指标

性能结果因硬件和配置而异。测试套件提供以下基准测试：

- **颜色重排**：优化实现相比 CPU 基线显示显著提升
- **YUV 转换**：性能因操作类型和后端而异
- **精度**：高像素精度和良好的 PSNR 值

## 贡献指南

添加新功能时：

1. 添加相应的单元测试
2. 包含边界情况验证
3. 为计算密集型函数添加性能基准测试
4. 更新文档
