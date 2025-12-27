# ccap-rs - Rust Bindings for CameraCapture

[![Crates.io](https://img.shields.io/crates/v/ccap.svg)](https://crates.io/crates/ccap)
[![Documentation](https://docs.rs/ccap/badge.svg)](https://docs.rs/ccap)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

Safe Rust bindings for [ccap](https://github.com/wysaid/CameraCapture) - A high-performance, lightweight cross-platform camera capture library with hardware-accelerated pixel format conversion.

## Features

- **High Performance**: Hardware-accelerated pixel format conversion with up to 10x speedup (AVX2, Apple Accelerate, NEON)
- **Cross Platform**: Windows (DirectShow), macOS/iOS (AVFoundation), Linux (V4L2)
- **Multiple Formats**: RGB, BGR, YUV (NV12/I420) with automatic conversion
- **Zero Dependencies**: Uses only system frameworks
- **Memory Safe**: Safe Rust API with automatic resource management
- **Async Support**: Optional async/await interface with tokio
- **Thread Safe**: Safe concurrent access to video frames

## Quick Start

Add this to your `Cargo.toml`:

```toml
[dependencies]
ccap = "1.1.0"

# For async support
ccap = { version = "1.1.0", features = ["async"] }
```

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

### Async Usage

```rust
use ccap::r#async::AsyncProvider;

#[tokio::main]
async fn main() -> ccap::Result<()> {
    let provider = AsyncProvider::new().await?;
    let devices = provider.find_device_names().await?;
    
    if !devices.is_empty() {
        provider.open(Some(&devices[0]), true).await?;
        
        // Async frame capture
        if let Some(frame) = provider.grab_frame().await? {
            let info = frame.info()?;
            println!("Async captured: {}x{}", info.width, info.height);
        }
    }
    
    Ok(())
}
```

## Examples

The crate includes several examples:

```bash
# List available cameras
cargo run --example list_cameras

# Capture frames synchronously
cargo run --example capture_frames

# Capture frames asynchronously (requires async feature)
cargo run --features async --example async_capture
```

## Building

### Prerequisites

Before building the Rust bindings, you need to build the ccap C library:

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

## API Documentation

### Core Types

- `Provider`: Main camera capture interface
- `VideoFrame`: Represents a captured video frame
- `DeviceInfo`: Camera device information
- `PixelFormat`: Supported pixel formats (RGB24, BGR24, NV12, I420, etc.)
- `Resolution`: Frame resolution specification

### Error Handling

All operations return `Result<T, CcapError>` for comprehensive error handling:

```rust
match provider.grab_frame_blocking() {
    Ok(Some(frame)) => { /* process frame */ },
    Ok(None) => println!("No frame available"),
    Err(e) => eprintln!("Capture error: {:?}", e),
}
```

### Thread Safety

- `VideoFrame` is `Send + Sync` and can be safely shared between threads
- `Provider` should be used from a single thread or protected with synchronization
- Frame data remains valid until the `VideoFrame` is dropped

## Platform Support

| Platform | Backend | Status |
|----------|---------|---------|
| Windows  | DirectShow | ✅ Supported |
| macOS    | AVFoundation | ✅ Supported |
| iOS      | AVFoundation | ✅ Supported |
| Linux    | V4L2 | ✅ Supported |

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
