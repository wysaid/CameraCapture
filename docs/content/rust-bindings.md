# Rust Bindings

ccap provides first-class Rust bindings via the `ccap-rs` crate.

- Crate: [ccap-rs on crates.io](https://crates.io/crates/ccap-rs)
- API docs: [docs.rs/ccap-rs](https://docs.rs/ccap-rs)
- Source in this repo: `bindings/rust/`

> Note: The published crate name is `ccap-rs`, but the library name is `ccap` so you can write `use ccap::Provider;`.

## Quick Start

### Add dependency

```bash
cargo add ccap-rs
```

If you prefer using `ccap` as the crate name in code (recommended):

```toml
[dependencies]
ccap = { package = "ccap-rs", version = "<latest>" }
```

### Minimal example

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

- `build-source` (default): build the underlying C/C++ sources as part of `cargo build`.
  - Best for crates.io users.
- `static-link`: link against a pre-built static library from a CameraCapture checkout.
  - Best for development when you are working on both C/C++ and Rust.
  - If the build script cannot find the repo root automatically, set `CCAP_SOURCE_DIR`.

## Build notes

### Using `build-source` (default)

This is the recommended mode for typical users.

- Requirements: Rust 1.65+, CMake 3.14+.
- The crate builds the C/C++ sources during compilation.

### Using `static-link`

This mode expects a pre-built `ccap` static library under a CameraCapture checkout:

- Build the C/C++ project first (Debug or Release).
- Then build your Rust project with the `static-link` feature.

If needed, set:

- `CCAP_SOURCE_DIR=/path/to/CameraCapture`

## Platform support

Camera capture backends:

- Windows: DirectShow
- macOS/iOS: AVFoundation
- Linux: V4L2

Video file playback support depends on the underlying C/C++ backend (currently Windows/macOS only).

## See also

- [Main ccap Documentation](documentation.md) (English) / [主文档](documentation.zh.md) (中文)
- [C Interface Documentation](c-interface.md) (English) / [C 接口文档](c-interface.zh.md) (中文)
- [CLI Tool Guide](cli.md) (English) / [CLI 工具指南](cli.zh.md) (中文)
