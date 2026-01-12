# ccap-rs (CameraCapture / ccap Rust Bindings)

[![Crates.io](https://img.shields.io/crates/v/ccap-rs.svg)](https://crates.io/crates/ccap-rs)
[![Documentation](https://docs.rs/ccap-rs/badge.svg)](https://docs.rs/ccap-rs)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

Safe Rust bindings for [CameraCapture (ccap)](https://github.com/wysaid/CameraCapture) — a high-performance, lightweight, cross-platform **webcam/camera capture** library with **hardware-accelerated pixel format conversion** (Windows DirectShow, macOS/iOS AVFoundation, Linux V4L2).

> Note: The published *package* name on crates.io is `ccap-rs`, but the *crate name in code* is `ccap`.

## Features

- **High Performance**: Hardware-accelerated pixel format conversion with up to 10x speedup (AVX2, Apple Accelerate, NEON)
- **Cross Platform**: Windows (DirectShow), macOS/iOS (AVFoundation), Linux (V4L2)
- **Multiple Formats**: RGB, BGR, YUV (NV12/I420) with automatic conversion
- **Zero Dependencies**: Uses only system frameworks
- **Memory Safe**: Safe Rust API with automatic resource management

## Quick Start

### Add dependency

Option A (simple):

```bash
cargo add ccap-rs
```

Option B (recommended): keep the crate name as `ccap` in your code:

```toml
[dependencies]
ccap = { package = "ccap-rs", version = "<latest>" }
```

> Tip: Replace `<latest>` with the latest version shown on <https://crates.io/crates/ccap-rs>

### Basic Usage

```rust
use ccap::{Provider, Result};

fn main() -> Result<()> {
    // Create a camera provider
    let mut provider = Provider::new()?;
    
    // Find available cameras
    let devices = provider.find_device_names()?;
    println!("Found {} cameras:", devices.len());
    for (i, device) in devices.iter().enumerate() {
        println!("  [{}] {}", i, device);
    }
    
    // Open the first camera
    if !devices.is_empty() {
        provider.open_device(Some(&devices[0]), true)?;
        println!("Camera opened successfully!");
        
        // Capture a frame (with 3 second timeout)
        if let Some(frame) = provider.grab_frame(3000)? {
            let info = frame.info()?;
            println!("Captured frame: {}x{}, format: {:?}", 
                     info.width, info.height, info.pixel_format);
            
            // Access frame data
            let data = frame.data()?;
            println!("Frame data size: {} bytes", data.len());
        }
    }
    
    Ok(())
}
```

## Examples

The crate includes several examples:

```bash
# List available cameras and their info
cargo run --example print_camera

# Minimal capture example
cargo run --example minimal_example

# Capture frames using grab mode
cargo run --example capture_grab

# Capture frames using callback mode
cargo run --example capture_callback
```

## Building

### Feature Modes

This crate supports two build modes:

- **Distribution mode (default):** `build-source` — Builds the native C/C++ implementation via the `cc` crate (intended for crates.io users).
- **Development mode:** `static-link` — Links against a pre-built native library from a CameraCapture checkout (e.g. `build/Debug/libccap.a`) (intended for developing this repository).

### Prerequisites

If you are using **development mode** (`static-link`), you need to build the native library first:

```bash
# From the root of the CameraCapture project
./scripts/build_and_install.sh
```

Or manually:

```bash
mkdir -p build/Debug
cd build/Debug
cmake ../.. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
```

### Building the Rust Crate

```bash
# From bindings/rust directory
cargo build
cargo test
```

### Using development mode (`static-link`)

```bash
# Link against pre-built build/Debug or build/Release from the repo
cargo build --no-default-features --features static-link
cargo test  --no-default-features --features static-link
```

#### AddressSanitizer (ASan) and `static-link`

The CameraCapture repo's test scripts may build the native library with **ASan enabled** (e.g. Debug functional tests).
An ASan-instrumented `libccap.a` requires the ASan runtime at link/run time.

- When using `static-link`, `build.rs` will **only** link the ASan runtime **if it detects** ASan symbols inside the prebuilt `libccap.a`.
- This does **not** affect the default crates.io build (`build-source`).
- You can disable the auto-link behavior by setting `CCAP_RUST_NO_ASAN_LINK=1`.

## Feature flags

- `build-source` (default): build the C/C++ ccap sources during `cargo build` (best for crates.io usage).
- `static-link`: link against a pre-built static library from a CameraCapture checkout (best for development). If you use this mode, make sure you have built the C/C++ project first, and set `CCAP_SOURCE_DIR` when needed.

## Platform notes

- Camera capture: Windows (DirectShow), macOS/iOS (AVFoundation), Linux (V4L2)
- Video file playback support depends on the underlying C/C++ library backend (currently Windows/macOS only).

## API Documentation

### Core Types

- `Provider`: Main camera capture interface
- `VideoFrame`: Represents a captured video frame
- `DeviceInfo`: Camera device information
- `PixelFormat`: Supported pixel formats (RGB24, BGR24, NV12, I420, etc.)
- `Resolution`: Frame resolution specification

### Error Handling

All operations return `Result<T, CcapError>` for comprehensive error handling.
For frame capture, `grab_frame(timeout_ms)` returns `Result<Option<VideoFrame>, CcapError>`:

```rust
match provider.grab_frame(3000) {  // 3 second timeout
    Ok(Some(frame)) => { /* process frame */ },
    Ok(None) => println!("No frame available"),
    Err(e) => eprintln!("Capture error: {:?}", e),
}
```

### Thread Safety

- `VideoFrame` implements `Send` so frames can be moved across threads (e.g. processed/dropped on a worker thread)
- `Provider` implements `Send` but the underlying C++ API is **not thread-safe**
  - Use `Provider` from a single thread, or wrap it with `Arc<Mutex<Provider>>` for multi-threaded access
- Frame data remains valid until the `VideoFrame` is dropped

> **Note**: The C++ layer's thread-safety is based on code inspection. If you encounter issues with cross-thread frame usage, please report them.

## Platform Support

| Platform | Backend      | Status       |
| -------- | ------------ | ------------ |
| Windows  | DirectShow   | ✅ Supported |
| macOS    | AVFoundation | ✅ Supported |
| iOS      | AVFoundation | ✅ Supported |
| Linux    | V4L2         | ✅ Supported |

## System Requirements

- Rust 1.65+
- CMake 3.14+
- Platform-specific camera frameworks (automatically linked)

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## License

This project is licensed under the MIT License - see the [LICENSE](../../LICENSE) file for details.

## Related Projects

- [ccap (C/C++)](https://github.com/wysaid/CameraCapture) - The underlying C/C++ library
- [OpenCV](https://opencv.org/) - Alternative computer vision library with camera support
