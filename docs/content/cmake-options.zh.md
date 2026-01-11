# CMake 构建选项

ccap 构建定制的完整参考。

## 快速开始预设

常见构建配置：

### 默认构建（静态库）
```bash
cmake -B build
cmake --build build
```

### 共享库构建
```bash
cmake -B build -DCCAP_BUILD_SHARED=ON
cmake --build build
```

### 开发构建（所有功能）
```bash
cmake -B build \
  -DCCAP_BUILD_EXAMPLES=ON \
  -DCCAP_BUILD_TESTS=ON \
  -DCCAP_BUILD_CLI=ON
cmake --build build
```

### 生产构建（优化）
```bash
cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCCAP_NO_LOG=ON \
  -DCCAP_INSTALL=ON
cmake --build build
cmake --install build --prefix /usr/local
```

### 独立 CLI（无运行时依赖）
```bash
cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCCAP_BUILD_CLI_STANDALONE=ON
cmake --build build
```

## 核心选项

### `CCAP_BUILD_SHARED`
**构建为共享库而非静态库**

- **类型**：布尔值（ON/OFF）
- **默认值**：`OFF`
- **用法**：`-DCCAP_BUILD_SHARED=ON`

**适用场景：**
- 多个可执行文件共享库
- 插件或动态加载场景
- 减少多个应用的总体二进制大小

### `CCAP_NO_LOG`
**禁用所有日志以减小二进制大小**

- **类型**：布尔值（ON/OFF）
- **默认值**：`OFF`
- **用法**：`-DCCAP_NO_LOG=ON`

**优点：**
- 减少二进制大小约 5-10%
- 轻微性能提升
- 生产环境控制台输出更清晰

**推荐：** 仅用于生产构建。

### `CCAP_WIN_NO_DEVICE_VERIFY`（仅 Windows）
**枚举时跳过设备验证**

- **类型**：布尔值（ON/OFF）
- **默认值**：`OFF`
- **用法**：`-DCCAP_WIN_NO_DEVICE_VERIFY=ON`

**解决的问题：**
某些有问题的相机驱动程序（如 VR 头盔、故障摄像头）在枚举设备期间调用 `IMoniker::BindToObject()` 时会崩溃。

**何时启用：**
- 设备枚举时遇到崩溃
- 使用 VR 头盔或异常捕获设备
- 默认启用安全，兼容性更好

**相关：** [Issue #26](https://github.com/wysaid/CameraCapture/issues/26)

### `CCAP_BUILD_CLI_STANDALONE`
**构建带静态运行时的独立 CLI**

- **类型**：布尔值（ON/OFF）
- **默认值**：`OFF`
- **用法**：`-DCCAP_BUILD_CLI_STANDALONE=ON`

**功能：**
- 自动启用 `CCAP_BUILD_CLI=ON`
- **Windows**：使用 `/MT` 标志（静态 MSVC 运行时）
  - 无需 VCRUNTIME DLL
  - 可移植可执行文件
- **Linux**：静态链接 libstdc++ 和 libgcc
  - 跨发行版兼容性更好

**要求：**
- 必须使用 `CCAP_BUILD_SHARED=OFF`
- 建议 Release 构建以减小二进制大小

## 开发选项

### `CCAP_BUILD_EXAMPLES`
**构建示例程序**

- **默认值**：`ON`（作为根项目），`OFF`（作为子目录）
- **用法**：`-DCCAP_BUILD_EXAMPLES=ON`

包括：
- `0-print_camera` - 列出可用相机
- `1-minimal_example` - 基本捕获
- `2-capture_grab` - 同步捕获
- `3-capture_callback` - 异步捕获
- `4-example_with_glfw` - OpenGL 实时预览
- `5-play_video` - 视频文件播放（macOS/Windows）

### `CCAP_BUILD_TESTS`
**构建单元测试**

- **默认值**：`OFF`
- **用法**：`-DCCAP_BUILD_TESTS=ON`

**运行测试：**
```bash
cmake -B build -DCCAP_BUILD_TESTS=ON
cmake --build build
cd build && ctest --output-on-failure
```

### `CCAP_BUILD_CLI`
**构建命令行工具**

- **默认值**：`OFF`
- **用法**：`-DCCAP_BUILD_CLI=ON`

构建 `ccap` CLI 工具，功能包括：
- 列出相机设备
- 捕获图像
- 实时预览
- 视频文件处理

参见 [CLI 文档](cli.zh.md) 了解使用详情。

## 高级场景

### 构建通用二进制（macOS）

```bash
cmake -B build \
  -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

或使用脚本：
```bash
./scripts/build_macos_universal.sh
```

### ARM64 交叉编译（Windows）

```bash
cmake -B build -G "Visual Studio 17 2022" -A ARM64
cmake --build build --config Release
```

或：
```bash
./scripts/build_arm64_win.sh
```

### 最小尺寸构建

```bash
cmake -B build \
  -DCMAKE_BUILD_TYPE=MinSizeRel \
  -DCCAP_NO_LOG=ON \
  -DCCAP_BUILD_SHARED=OFF
cmake --build build
```

## 常见问题

### "找不到 ccap 包"
```bash
# 解决方案：设置 CMAKE_PREFIX_PATH
cmake -B build -DCMAKE_PREFIX_PATH=/usr/local
```

### 运行时找不到共享库
```bash
# Linux 解决方案：
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH

# macOS 解决方案：
export DYLD_LIBRARY_PATH=/usr/local/lib:$DYLD_LIBRARY_PATH
```

### Linux 上"无法打开相机"
```bash
# 确保用户有相机访问权限
sudo usermod -a -G video $USER
# 注销并重新登录
```

## 平台特定说明

### Windows
- 推荐 MSVC 2019+
- 也支持 MinGW 和 Clang
- 使用开发者命令提示符以获得最佳结果

### macOS
- 需要 Xcode 11+
- 推荐使用 Homebrew 安装依赖
- 分发可能需要代码签名

### Linux
- 需要 V4L2 头文件（通常在 linux-headers 中）
- pthread 自动链接
- 相机权限可能需要配置

## 使用 dev.cmake 进行自定义构建

创建 `cmake/dev.cmake`（git 忽略）以覆盖默认值：

```cmake
# cmake/dev.cmake - 自定义开发设置

# 强制共享库
set(CCAP_BUILD_SHARED ON CACHE BOOL "" FORCE)

# 启用所有功能
set(CCAP_BUILD_EXAMPLES ON CACHE BOOL "" FORCE)
set(CCAP_BUILD_TESTS ON CACHE BOOL "" FORCE)
set(CCAP_BUILD_CLI ON CACHE BOOL "" FORCE)
```

参见 `cmake/dev.cmake.example` 获取更多示例。

## 快速参考表

| 选项 | 默认值 | 用途 | 何时启用 |
|------|--------|------|----------|
| `CCAP_BUILD_SHARED` | OFF | 共享库 | 多应用、插件 |
| `CCAP_NO_LOG` | OFF | 禁用日志 | 生产构建 |
| `CCAP_WIN_NO_DEVICE_VERIFY` | OFF | 跳过设备检查 | 有问题的驱动 |
| `CCAP_BUILD_EXAMPLES` | ON/OFF* | 示例程序 | 学习、测试 |
| `CCAP_BUILD_TESTS` | OFF | 单元测试 | 开发、CI |
| `CCAP_BUILD_CLI` | OFF | CLI 工具 | 自动化、脚本 |
| `CCAP_BUILD_CLI_STANDALONE` | OFF | 便携 CLI | 分发 |
| `CCAP_FORCE_ARM64` | OFF | ARM 编译 | ARM 设备 |

*取决于 ccap 是否为根项目

## 需要帮助？

- 📖 [完整文档](documentation.zh.md)
- 🦀 [Rust 绑定](rust-bindings.zh.md)
- 🐛 [报告问题](https://github.com/wysaid/CameraCapture/issues)
- 💬 [讨论](https://github.com/wysaid/CameraCapture/discussions)
- 📧 邮箱：wysaid@gmail.com
