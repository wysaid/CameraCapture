# ccap 命令行工具

基于 CameraCapture (ccap) 库的强大命令行界面工具。

## 概述

`ccap` 命令行工具提供了全面的相机操作命令行界面,包括设备枚举、帧捕获、格式转换和可选的实时预览。它专为自动化、脚本编程和快速测试相机功能而设计。

## 功能特性

- **设备发现**: 列出所有可用相机及其详细功能
- **帧捕获**: 以各种分辨率和像素格式捕获帧
- **格式支持**: RGB、BGR、RGBA、BGRA、YUV (NV12、I420、YUYV、UYVY)
- **YUV 操作**: 直接 YUV 捕获和 YUV 转图像转换
- **实时预览**: 基于 OpenGL 的预览窗口(需 GLFW 支持)
- **自动化友好**: 专为脚本和 CI/CD 流水线设计
- **跨平台**: Windows、macOS、Linux

## 构建 CLI 工具

### 基本构建

```bash
cmake -B build -DBUILD_CCAP_CLI=ON
cmake --build build
```

### 启用预览支持构建 (GLFW)

```bash
cmake -B build -DBUILD_CCAP_CLI=ON -DCCAP_CLI_WITH_GLFW=ON
cmake --build build
```

可执行文件将位于 `build/` 目录(或 `build/Debug`、`build/Release`,取决于您的构建配置)。

### 分发与静态链接

CLI 工具配置为最小化运行时依赖：

- **Windows (MSVC)**: 使用 `/MT` 标志进行静态运行时链接，无需安装 VCRUNTIME DLL
- **Linux**: 尝试静态链接 libstdc++ 和 libgcc（如果可用）。如果未安装静态库，则回退到动态链接。要在 Fedora/RHEL 上启用静态链接，请安装 `libstdc++-static`；在 Debian/Ubuntu 上，静态库通常包含在默认开发包中。
- **macOS**: 使用默认的静态 C++ 标准库链接，仅依赖系统框架

当静态链接成功时，CLI 工具可以作为独立可执行文件分发，仅需最少的依赖项（在 Linux 上仅依赖 libc 和 libm 等系统库）。

验证已构建 CLI 工具的依赖项：

```bash
# Linux
ldd ccap

# macOS
otool -L ccap

# Windows (PowerShell)
dumpbin /dependents ccap.exe
```


## 命令行参考

### 通用选项

| 选项 | 描述 |
|-----|------|
| `-h, --help` | 显示帮助信息并退出 |
| `-v, --version` | 显示版本信息 |
| `--verbose` | 启用详细日志输出 |

### 设备枚举

| 选项 | 描述 |
|-----|------|
| `-l, --list-devices` | 列出所有可用的相机设备 |
| `-i, --device-info [INDEX]` | 显示指定索引设备的详细功能<br>如果省略 INDEX,则显示所有设备的信息 |

### 捕获选项

| 选项 | 默认值 | 描述 |
|-----|--------|------|
| `-d, --device INDEX\|NAME` | `0` | 通过索引或名称选择相机设备 |
| `-w, --width WIDTH` | `1280` | 设置捕获宽度(像素) |
| `-H, --height HEIGHT` | `720` | 设置捕获高度(像素) |
| `-f, --fps FPS` | `30.0` | 设置帧率 |
| `-c, --count COUNT` | `1` | 要捕获的帧数 |
| `-t, --timeout MS` | `5000` | 捕获超时时间(毫秒) |
| `-o, --output DIR` | - | 捕获图像的输出目录 |
| `--format FORMAT` | - | 输出像素格式(参见[支持的格式](#支持的格式)) |
| `--internal-format FORMAT` | - | 相机的内部像素格式 |
| `--save-yuv` | - | 将帧保存为原始 YUV 数据而不转换为 BMP |

### 预览选项

这些选项仅在启用 GLFW 支持(`CCAP_CLI_WITH_GLFW=ON`)构建时可用。

| 选项 | 描述 |
|-----|------|
| `-p, --preview` | 启用实时预览窗口 |
| `--preview-only` | 仅显示预览,不保存帧到磁盘 |

### 格式转换

| 选项 | 描述 |
|-----|------|
| `--convert INPUT` | 将 YUV 文件转换为 BMP 图像 |
| `--yuv-format FORMAT` | 指定 YUV 输入格式(参见[YUV 格式](#yuv-格式)) |
| `--yuv-width WIDTH` | YUV 输入文件的宽度 |
| `--yuv-height HEIGHT` | YUV 输入文件的高度 |
| `--convert-output FILE` | 转换后图像的输出文件路径 |

## 支持的格式

### RGB/BGR 格式

- `rgb24` - 24位 RGB (每通道 8 位)
- `bgr24` - 24位 BGR (每通道 8 位)
- `rgba32` - 32位 RGBA (每通道 8 位 + alpha)
- `bgra32` - 32位 BGRA (每通道 8 位 + alpha)

### YUV 格式

- `nv12` - YUV 4:2:0 半平面 (UV 交错)
- `nv12f` - YUV 4:2:0 半平面 (全范围)
- `i420` - YUV 4:2:0 平面 (独立的 U 和 V 平面)
- `i420f` - YUV 4:2:0 平面 (全范围)
- `yuyv` - YUV 4:2:2 打包 (YUYV 顺序)
- `yuyvf` - YUV 4:2:2 打包 (YUYV 顺序,全范围)
- `uyvy` - YUV 4:2:2 打包 (UYVY 顺序)
- `uyvyf` - YUV 4:2:2 打包 (UYVY 顺序,全范围)

## 使用示例

### 设备发现

列出所有可用相机:
```bash
ccap --list-devices
```

显示第一个相机的详细信息:
```bash
ccap --device-info 0
```

显示所有相机的信息:
```bash
ccap --device-info
```

### 基本帧捕获

从默认相机捕获单帧:
```bash
ccap -d 0 -o ./captures
```

以 1080p 分辨率捕获 10 帧:
```bash
ccap -d 0 -w 1920 -H 1080 -c 10 -o ./captures
```

使用特定相机名称捕获:
```bash
ccap -d "HD Pro Webcam C920" -c 5 -o ./captures
```

### 特定格式捕获

以 BGR24 格式捕获帧:
```bash
ccap -d 0 --format bgr24 -c 5 -o ./captures
```

使用特定的内部相机格式捕获(以提高性能):
```bash
ccap -d 0 --internal-format nv12 --format bgr24 -c 5 -o ./captures
```

### YUV 操作

将帧保存为原始 YUV 数据:
```bash
ccap -d 0 --format nv12 --save-yuv -c 5 -o ./yuv_captures
```

将 YUV 文件转换为 BMP 图像:
```bash
ccap --convert input.yuv \
     --yuv-format nv12 \
     --yuv-width 1920 \
     --yuv-height 1080 \
     --convert-output output.bmp
```

### 预览模式

实时预览相机画面(需要 GLFW):
```bash
ccap -d 0 --preview
```

仅预览,不保存帧:
```bash
ccap -d 0 --preview-only
```

在捕获帧的同时预览:
```bash
ccap -d 0 -w 1920 -H 1080 -c 10 -o ./captures --preview
```

### 高级用法

使用自定义超时和高帧率捕获:
```bash
ccap -d 0 -w 1920 -H 1080 -f 60 -c 100 -t 10000 -o ./captures
```

启用详细日志用于调试:
```bash
ccap --verbose -d 0 -c 5 -o ./captures
```

## 输出文件

### 图像文件 (BMP)

在 RGB/BGR 格式捕获或将 YUV 转换为图像时,文件保存为:
```
<output_dir>/capture_<时间戳>_<分辨率>_<帧索引>.bmp
```

示例: `captures/capture_20231224_153045_1920x1080_0.bmp`

### YUV 文件

使用 `--save-yuv` 时,原始 YUV 数据保存为:
```
<output_dir>/capture_<时间戳>_<分辨率>_<帧索引>.<格式>.yuv
```

示例: `yuv_captures/capture_20231224_153045_1920x1080_0.NV12.yuv`

这些文件包含原始平面或打包 YUV 数据,没有任何容器格式。

## 错误处理

CLI 工具返回以下退出代码:

| 退出代码 | 含义 |
|---------|------|
| `0` | 成功 |
| `1` | 无效参数或用法错误 |
| `2` | 未找到相机设备 |
| `3` | 相机打开/启动失败 |
| `4` | 帧捕获失败 |
| `5` | 文件 I/O 错误 |

## 集成示例

### Bash 脚本

```bash
#!/bin/bash
# 从所有可用相机捕获帧

# 列出设备
ccap --list-devices

# 从每个相机捕获
for i in 0 1 2; do
    mkdir -p "camera_${i}_captures"
    ccap -d "$i" -w 1920 -H 1080 -c 5 -o "camera_${i}_captures" || true
done
```

### Python 集成

```python
import subprocess
import json

# 运行 ccap 并捕获输出
result = subprocess.run(
    ['ccap', '--list-devices'],
    capture_output=True,
    text=True
)

if result.returncode == 0:
    print("找到的设备:")
    print(result.stdout)
    
    # 捕获帧
    subprocess.run([
        'ccap', '-d', '0', 
        '-w', '1920', '-H', '1080',
        '-c', '10', 
        '-o', './captures'
    ])
```

### Makefile 集成

```makefile
.PHONY: capture test-camera

capture:
	mkdir -p captures
	ccap -d 0 -w 1920 -H 1080 -c 5 -o ./captures

test-camera:
	ccap --list-devices
	ccap --device-info 0
```

## 性能提示

1. **使用内部格式**: 指定 `--internal-format` 以匹配相机的原生格式,以获得更好的性能
2. **直接 YUV 捕获**: 当需要 YUV 数据时使用 `--save-yuv`,避免不必要的转换
3. **更大的超时**: 对于高分辨率捕获或慢速相机,增加 `--timeout`
4. **硬件加速**: 库会在可用时自动使用硬件加速(AVX2、NEON、Apple Accelerate)

## 故障排除

### 未找到设备

```bash
ccap --list-devices
# 返回: No camera devices found
```

**解决方案:**
- 确保相机已连接并被操作系统识别
- 在 Linux 上,检查是否有 `/dev/video*` 设备的权限
- 尝试使用 `--verbose` 运行以获取更多详细信息

### 权限被拒绝(Linux)

```bash
# 将用户添加到 video 组
sudo usermod -a -G video $USER
# 然后注销并重新登录
```

### 捕获超时

```bash
ccap -d 0 -c 1 -o ./captures
# 错误: Frame capture timeout
```

**解决方案:**
- 增加超时时间: `--timeout 10000`
- 检查相机是否正被其他应用程序使用
- 尝试不同的分辨率: `-w 640 -H 480`

### 不支持的格式

```bash
ccap -d 0 --format xyz
# 错误: Unknown format
```

**解决方案:** 使用[支持的格式](#支持的格式)之一

## 另请参阅

- [主 ccap 文档](documentation.html) - CameraCapture 库概述
- [CMake 构建选项](https://github.com/wysaid/CameraCapture/blob/main/docs/CMAKE_OPTIONS.md) - 构建配置详情
- [C 接口文档](https://github.com/wysaid/CameraCapture/blob/main/docs/C_Interface.md) - C API 参考
- [示例](https://github.com/wysaid/CameraCapture/tree/main/examples) - 使用库的代码示例
