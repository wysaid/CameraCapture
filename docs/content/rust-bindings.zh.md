# Rust 绑定

ccap 提供了完善的 Rust bindings，对应 crates.io 上的 `ccap-rs` crate。

- Crate: [ccap-rs](https://crates.io/crates/ccap-rs)
- API 文档: [docs.rs/ccap-rs](https://docs.rs/ccap-rs)
- 仓库内源码：`bindings/rust/`

> 说明：发布到 crates.io 的 crate 名称是 `ccap-rs`，但库名保持为 `ccap`，因此你可以直接 `use ccap::Provider;`。

## 快速开始

### 添加依赖

```bash
cargo add ccap-rs
```

如果你希望在代码中使用 `ccap` 作为 crate 名称（推荐）：

```toml
[dependencies]
ccap = { package = "ccap-rs", version = "<latest>" }
```

### 最小示例

```rust
use ccap::{Provider, Result};

fn main() -> Result<()> {
    let mut provider = Provider::new()?;
    let devices = provider.find_device_names()?;

    if let Some(device) = devices.first() {
        provider.open_device(Some(device), true)?;
        if let Some(frame) = provider.grab_frame(3000)? {
            let info = frame.info()?;
            println!("{}x{} {:?}", info.width, info.height, info.pixel_format);
        }
    }

    Ok(())
}
```

## Feature flags

- `build-source`（默认）：在 `cargo build` 时自动编译底层 C/C++ 源码。
  - 更适合 crates.io 用户。
- `static-link`：链接到 CameraCapture 源码仓库下预先编译好的静态库。
  - 更适合开发阶段（同时改 C/C++ 与 Rust）。
  - 若构建脚本无法自动定位仓库根目录，可设置 `CCAP_SOURCE_DIR`。


## 构建说明

### 使用 `build-source`（默认）

这是普通用户的推荐模式。

- 依赖：Rust 1.65+、CMake 3.14+。
- crate 会在编译时构建底层 C/C++ 源码。

### 使用 `static-link`

该模式要求你在 CameraCapture 源码仓库中先编译出 `ccap` 静态库：

- 先构建 C/C++ 项目（Debug 或 Release）。
- 再在 Rust 项目中启用 `static-link` feature 进行构建。

必要时设置：

- `CCAP_SOURCE_DIR=/path/to/CameraCapture`

## 平台支持

相机捕获后端：

- Windows：DirectShow
- macOS/iOS：AVFoundation
- Linux：V4L2

视频文件播放是否可用取决于底层 C/C++ 后端（目前 Windows/macOS 支持，Linux 暂不支持）。

## 另请参阅

- [主文档](documentation.zh.md)
- [C 接口文档](c-interface.zh.md)
- [CLI 工具指南](cli.zh.md)
