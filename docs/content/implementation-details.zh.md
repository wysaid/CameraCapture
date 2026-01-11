# 实现细节

面向专业开发者的 ccap 内部机制和高级使用模式深入探讨。

## 目录

- [内存管理与视频播放](#内存管理与视频播放)
- [CMake 构建选项](#cmake-构建选项)
- [C 接口](#c-接口)
- [性能优化](#性能优化)

---

## 内存管理与视频播放

理解 ccap 的内存管理，实现高效的相机和视频文件处理。

### 核心概念

| 模式 | 丢帧行为 | 内存 | 适用场景 |
|------|----------|------|----------|
| 相机模式 | 是（保留最新） | 有界 | 直播、实时 |
| 文件模式 | 否（反压） | 有界 | 视频分析、处理 |

**涵盖主题：**
- 相机与文件模式差异
- 反压机制详解
- 缓冲区配置策略
- 播放速度控制
- 内存占用分析

**[阅读完整指南 →](video-memory-management.zh.md)**

### 快速示例

```cpp
// 文件模式：处理所有帧不丢帧
ccap::Provider provider;
provider.open("video.mp4", true);  // 默认 PlaybackSpeed=0

while (auto frame = provider.grab(1000)) {
    // 反压机制确保不丢失帧
    // 内存保持恒定（高清约 25-30MB）
    processFrame(frame);
}
```

---

## CMake 构建选项

为不同部署场景定制 ccap 构建的完整参考。

### 快速预设

**开发构建：**
```bash
cmake -B build -DCCAP_BUILD_EXAMPLES=ON -DCCAP_BUILD_TESTS=ON
```

**生产构建：**
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCCAP_NO_LOG=ON
```

**独立 CLI：**
```bash
cmake -B build -DCCAP_BUILD_CLI_STANDALONE=ON
```

**涵盖主题：**
- 核心库选项
- 平台特定配置
- 开发 vs 生产构建
- 交叉编译策略
- 静态运行时链接
- 常见问题排查

**[阅读完整指南 →](cmake-options.zh.md)**

### 选项参考

| 选项 | 默认值 | 用途 |
|------|--------|------|
| `CCAP_BUILD_SHARED` | OFF | 构建共享库 |
| `CCAP_NO_LOG` | OFF | 禁用日志减小二进制 |
| `CCAP_WIN_NO_DEVICE_VERIFY` | OFF | 修复有问题驱动崩溃 |
| `CCAP_BUILD_CLI_STANDALONE` | OFF | 带静态运行时的便携 CLI |
| `CCAP_FORCE_ARM64` | OFF | 强制 ARM64 编译 |

---

## C 接口

用于语言绑定和传统 C 项目的纯 C99 API。

**[查看完整 C 接口文档 →](c-interface.md)**

### 为什么使用 C 接口？

- ✅ 语言绑定（Python、Go、Rust 等）
- ✅ 嵌入式系统集成
- ✅ 传统 C 代码库兼容
- ✅ 共享库的稳定 ABI

### 示例

```c
#include <ccap_c.h>

int main() {
    CcapProvider* provider = ccap_provider_create();
    
    if (ccap_provider_open(provider, NULL, true)) {
        CcapVideoFrame* frame = ccap_provider_grab(provider, 3000);
        if (frame) {
            CcapVideoFrameInfo info;
            ccap_video_frame_get_info(frame, &info);
            printf("已捕获：%dx%d\n", info.width, info.height);
            ccap_video_frame_release(frame);
        }
    }
    
    ccap_provider_destroy(provider);
    return 0;
}
```

**[阅读完整 C API 文档 →](c-interface.md)**

---

## 性能优化

### 硬件加速

ccap 自动使用平台特定优化：

| 平台 | 技术 | 加速倍数 |
|------|------|----------|
| x86_64 | AVX2 SIMD | ~10x |
| macOS | Apple Accelerate | ~8x |
| ARM64 | NEON SIMD | ~6x |

**像素格式转换性能：**

```
基准测试：1920×1080 RGB → NV12
├── 基准（C）：12.5ms
├── AVX2（x86）：1.2ms（10.4倍）
├── Accelerate（macOS）：1.5ms（8.3倍）
└── NEON（ARM）：2.1ms（6.0倍）
```

### 最大性能提示

1. **尽可能使用原生格式：**
   ```cpp
   // 避免转换开销
   provider.set(ccap::PropertyName::PixelFormat, 
                ccap::PixelFormat::NV12);  // 多数平台原生
   ```

2. **调整缓冲区大小：**
   ```cpp
   // 实时应用更低延迟
   provider.setMaxAvailableFrameSize(1);
   
   // 视频文件更平滑播放
   provider.setMaxAvailableFrameSize(5);
   ```

3. **使用回调模式实现零拷贝：**
   ```cpp
   provider.setFrameCallback([](auto frame) {
       // 立即处理帧，无队列开销
       processFrame(frame);
   });
   ```

### 性能测试

运行性能测试：
```bash
cmake -B build -DCCAP_BUILD_TESTS=ON
cmake --build build
./build/tests/ccap_tests --gtest_filter=*Performance*
```

或使用脚本：
```bash
./scripts/run_tests.sh performance
```

---

## 平台特定说明

### Windows

**DirectShow 后端：**
- 成熟、稳定的 API
- 良好的驱动兼容性
- 已知问题：某些 VR 头盔在枚举时崩溃
  - **解决方案：** 使用 `CCAP_WIN_NO_DEVICE_VERIFY=ON`

**虚拟摄像头：**
- OBS Virtual Camera：✅ 支持
- ManyCam：✅ 支持
- NDI Virtual Input：✅ 支持

### macOS / iOS

**AVFoundation 后端：**
- 现代、高效的 API
- 出色的硬件集成
- 原生格式支持：NV12、BGRA

**相机权限：**
```xml
<!-- Info.plist -->
<key>NSCameraUsageDescription</key>
<string>需要访问相机进行视频捕获</string>
```

### Linux

**V4L2 后端：**
- 直接内核接口
- 低开销
- 广泛的格式支持

**相机访问：**
```bash
# 将用户添加到 video 组
sudo usermod -a -G video $USER

# 验证设备
ls -l /dev/video*
```

---

## 开发工具

### 可用脚本

```bash
# 构建和安装
./scripts/build_and_install.sh

# 格式化所有源文件
./scripts/format_all.sh

# 运行所有测试
./scripts/run_tests.sh

# 运行特定测试类别
./scripts/run_tests.sh functional
./scripts/run_tests.sh performance

# 更新所有文件的版本
./scripts/update_version.sh 1.2.3

# 交叉编译
./scripts/build_arm64.sh              # ARM64 Linux
./scripts/build_arm64_win.sh          # ARM64 Windows
./scripts/build_macos_universal.sh    # macOS 通用二进制
```

---

## 测试

### 测试类别

**功能测试：**
```bash
./scripts/run_tests.sh functional
```

**性能测试：**
```bash
./scripts/run_tests.sh performance
```

**随机测试：**
```bash
./scripts/run_tests.sh shuffle
```

**视频测试**（仅 macOS/Windows）：
```bash
./scripts/run_tests.sh video
```

---

## 需要帮助？

- 📖 [主文档](documentation.zh.md)
- 📖 [CLI 工具指南](cli.zh.md)
- 🦀 [Rust 绑定](rust-bindings.zh.md)
- 🐛 [报告问题](https://github.com/wysaid/CameraCapture/issues)
- 💬 [讨论](https://github.com/wysaid/CameraCapture/discussions)
- 📧 邮箱：wysaid@gmail.com
- 🌐 网站：[ccap.work](https://ccap.work)

---

## 相关资源

- [构建和安装指南](https://github.com/wysaid/CameraCapture/blob/main/BUILD_AND_INSTALL.md)
- [示例程序](https://github.com/wysaid/CameraCapture/tree/main/examples)
- [GitHub 仓库](https://github.com/wysaid/CameraCapture)
- [版本说明](https://github.com/wysaid/CameraCapture/releases)
