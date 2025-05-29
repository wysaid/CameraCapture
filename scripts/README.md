# 构建脚本说明

这个目录包含了 ccap 项目的各种构建和测试脚本。

**所有脚本都支持从任意目录用绝对路径执行！**

## 主要脚本

### 构建脚本

- **`build.sh`** - 通用构建脚本，支持多种架构
  ```bash
  # 构建原生架构版本（推荐）
  /path/to/ccap/scripts/build.sh native Debug
  
  # 构建ARM64版本
  /path/to/ccap/scripts/build.sh arm64 Debug
  
  # 构建x86_64版本
  /path/to/ccap/scripts/build.sh x86_64 Debug
  
  # 构建通用版本（两种架构都构建）
  /path/to/ccap/scripts/build.sh universal Release
  ```

- **`build_arm64.sh`** - 专门构建ARM64版本
  ```bash
  /path/to/ccap/scripts/build_arm64.sh Debug
  ```

- **`build_x86_64.sh`** - 专门构建x86_64版本
  ```bash
  /path/to/ccap/scripts/build_x86_64.sh Debug
  ```

### 测试脚本

- **`test_arch.sh`** - 架构检测测试
  ```bash
  /path/to/ccap/scripts/test_arch.sh
  ```
# 构建脚本说明

这个目录包含了 ccap 项目的各种构建和测试脚本。

**重要：所有脚本都支持从任意目录用绝对路径执行！**

## 主要脚本

### 构建脚本

- **`build.sh`** - 通用构建脚本，支持多种架构

```bash
# 构建原生架构版本（推荐）
/path/to/ccap/scripts/build.sh native Debug

# 构建ARM64版本
/path/to/ccap/scripts/build.sh arm64 Debug

# 构建x86_64版本
/path/to/ccap/scripts/build.sh x86_64 Debug

# 构建通用版本（两种架构都构建）
/path/to/ccap/scripts/build.sh universal Release
```

- **`build_arm64.sh`** - 专门构建ARM64版本

```bash
/path/to/ccap/scripts/build_arm64.sh Debug
```

- **`build_x86_64.sh`** - 专门构建x86_64版本

```bash
/path/to/ccap/scripts/build_x86_64.sh Debug
```

### 测试脚本

- **`test_arch.sh`** - 架构检测测试

```bash
/path/to/ccap/scripts/test_arch.sh
```

这个脚本会检测当前系统和编译器的架构支持情况，帮助调试NEON等特性的检测问题。

- **`verify_neon.sh`** - NEON支持验证

```bash
/path/to/ccap/scripts/verify_neon.sh
```

这个脚本专门验证NEON指令集的编译时和运行时检测，展示ccap在不同架构上的NEON支持状态。

## 路径处理技术

所有脚本都使用以下技术确保可以从任何目录执行：

```bash
# 获取脚本所在的真实目录路径
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
```

这意味着你可以：

- 在项目根目录执行：`./scripts/build.sh`
- 在任何其他目录执行：`/absolute/path/to/ccap/scripts/build.sh`
- 通过符号链接执行脚本

## 使用建议

1. **日常开发**：使用 `build.sh native Debug`
2. **测试特定架构**：使用对应的专门脚本
3. **发布构建**：使用 `build.sh universal Release`
4. **调试架构问题**：先运行 `test_arch.sh`

## 目录结构

```
scripts/
├── README.md           # 本文件
├── build.sh           # 通用构建脚本
├── build_arm64.sh     # ARM64专用构建脚本
├── build_x86_64.sh    # x86_64专用构建脚本
├── test_arch.sh       # 架构检测测试脚本
└── verify_neon.sh     # NEON支持验证脚本
```

## 注意事项

- 在Intel Mac上编译ARM64版本的二进制文件无法直接运行，但可以用于交叉编译
- 在Apple Silicon Mac上编译x86_64版本需要Rosetta 2支持
- 所有脚本都支持Debug和Release两种构建类型
- 现在已修正CMakeLists.txt，只有显式请求时才强制ARM64架构
