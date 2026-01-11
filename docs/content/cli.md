# ccap CLI Tool

A powerful command-line interface for camera capture operations using the CameraCapture (ccap) library.

## Overview

The `ccap` CLI tool provides a comprehensive command-line interface for working with cameras, including device enumeration, frame capture, format conversion, and optional real-time preview. It's designed for automation, scripting, and quick testing of camera functionality.

## Features

- **Device Discovery**: List all available cameras with detailed capabilities
- **Frame Capture**: Capture frames in various resolutions and pixel formats
- **Format Support**: RGB, BGR, RGBA, BGRA, YUV (NV12, I420, YUYV, UYVY)
- **YUV Operations**: Direct YUV capture and YUV-to-image conversion
- **Real-time Preview**: OpenGL-based preview window (when built with GLFW support)
- **Automation Friendly**: Designed for scripts and CI/CD pipelines
- **Cross-platform**: Windows, macOS, Linux

## Building the CLI Tool

### Basic Build

```bash
cmake -B build -DCCAP_BUILD_CLI=ON
cmake --build build
```

### Build with Preview Support (GLFW)

```bash
cmake -B build -DCCAP_BUILD_CLI=ON -DCCAP_CLI_WITH_GLFW=ON
cmake --build build
```

### Build with Image Format Support (JPG/PNG)

```bash
cmake -B build -DCCAP_BUILD_CLI=ON -DCCAP_CLI_WITH_STB_IMAGE=ON
cmake --build build
```

By default, `CCAP_CLI_WITH_STB_IMAGE` is enabled, providing support for JPG and PNG formats in addition to BMP. To disable this feature and use BMP only:

```bash
cmake -B build -DCCAP_BUILD_CLI=ON -DCCAP_CLI_WITH_STB_IMAGE=OFF
cmake --build build
```

### Optimized Release Build

For production use, build in Release mode for optimal performance and minimal size:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCCAP_BUILD_CLI=ON
cmake --build build
```

**Optimizations automatically enabled in Release builds:**
- Link Time Optimization (LTO/IPO) for better performance (+20%) and smaller size
- Symbol stripping for minimal binary size (~25% reduction)
- Compiler optimizations (-O3)

**Expected binary sizes:**
- Debug: ~6MB (with debug symbols)
- Release: ~1.8MB (optimized, stripped)
- MinSizeRel: ~1.5MB (size-optimized, slight performance trade-off)

The executable will be located in the `build/` directory (or `build/Debug`, `build/Release` depending on your build configuration).

### Static Runtime Linking

**Windows (MSVC)**: The CLI tool uses static runtime linking (`/MT` flag) to eliminate the dependency on VCRUNTIME DLL, allowing single-file distribution without requiring Visual C++ Redistributables.

**Linux**: Attempts to statically link libstdc++ and libgcc when available. Falls back to dynamic linking if not available (e.g., Fedora without `libstdc++-static` package). The binary still depends on glibc and may not work on systems with older glibc versions.


## Command-Line Reference

### General Options

| Option | Description |
|--------|-------------|
| `-h, --help` | Show help message and exit |
| `-v, --version` | Show version information |
| `--verbose` | Enable verbose logging output |
| `--timeout SECONDS` | Program timeout: auto-exit after N seconds |
| `--timeout-exit-code CODE` | Exit code when timeout occurs (default: 0) |

### Device Enumeration

| Option | Description |
|--------|-------------|
| `-l, --list-devices` | List all available camera devices |
| `-I, --device-info [INDEX]` | Show detailed capabilities for device at INDEX<br>If INDEX is omitted, shows info for all devices |

### Input Options

| Option | Default | Description |
|--------|---------|-------------|
| `-i, --input SOURCE` | - | Input source: video file path, device index, or device name |
| `-d, --device INDEX\|NAME` | `0` | Select camera device by index or name |

### Capture Options (Camera Mode)

| Option | Default | Description |
|--------|---------|-------------|
| `-w, --width WIDTH` | `1280` | Set capture width in pixels |
| `-H, --height HEIGHT` | `720` | Set capture height in pixels |
| `-f, --fps FPS` | `30.0` | Set frame rate (supports floating point, e.g., 29.97)<br>**Camera mode:** Sets the camera's capture frame rate<br>**Video mode:** Calculates playback speed (e.g., video is 15fps, --fps 30 → 2.0x speed)<br>**Note:** Cannot be used with `--speed` |
| `-c, --count COUNT` | - | Number of frames to capture, then exit |
| `-t, --grab-timeout MS` | `5000` | Timeout for grabbing a single frame in milliseconds |
| `--format, --output-format` | - | Output pixel format (see [Supported Formats](#supported-formats)) |
| `--internal-format FORMAT` | - | Camera's internal pixel format (camera mode only) |

### Save Options

| Option | Default | Description |
|--------|---------|-------------|
| `-o, --output DIR` | - | Output directory for captured images |
| `-s, --save` | - | Enable saving captured frames to output directory |
| `--save-yuv` | - | Save frames as raw YUV data (auto-enables --save, auto-sets --internal-format nv12) |
| `--save-format FORMAT` | `jpg` | Image format: `jpg`, `png`, `bmp` (auto-enables --save) |
| `--save-jpg` | - | Save as JPEG format (shortcut for `--save-format jpg`) |
| `--save-png` | - | Save as PNG format (shortcut for `--save-format png`) |
| `--save-bmp` | - | Save as BMP format (shortcut for `--save-format bmp`) |
| `--image-format FORMAT` | `jpg` | Image output format: `jpg`, `png`, `bmp` (requires `CCAP_CLI_WITH_STB_IMAGE=ON` for jpg/png) |
| `--jpeg-quality QUALITY` | `90` | JPEG quality (1-100, only for JPG format) |

### Preview Options

These options are only available when built with GLFW support (`CCAP_CLI_WITH_GLFW=ON`).

| Option | Description |
|--------|-------------|
| `-p, --preview` | Enable real-time preview window |
| `--preview-only` | Same as --preview (kept for compatibility) |

### Video Playback Options

| Option | Description |
|--------|-------------|
| `--loop[=N]` | Loop video playback. Omit N for infinite loop, or specify N for exact number of loops |
| `--speed SPEED` | Playback speed multiplier<br>`0.0` = no frame rate control (process as fast as possible)<br>`1.0` = normal speed (match video's original frame rate)<br>`>1.0` = speed up (e.g., 2.0 = 2x speed)<br>`<1.0` = slow down (e.g., 0.5 = half speed)<br>Default: `0.0` for capture mode, `1.0` for preview mode<br>**Note:** Cannot be used with `-f/--fps` |

**Note:** `--loop` and `-c` are mutually exclusive. Use `-c` to limit captured frames, or `--loop` for video looping, but not both.

### Format Conversion

| Option | Description |
|--------|-------------|
| `--convert INPUT` | Convert a YUV file to BMP image |
| `--yuv-format FORMAT` | Specify YUV input format (see [YUV Formats](#yuv-formats)) |
| `--yuv-width WIDTH` | Width of YUV input file |
| `--yuv-height HEIGHT` | Height of YUV input file |
| `--convert-output FILE` | Output file path for converted image |

## Supported Formats

### RGB/BGR Formats

- `rgb24` - 24-bit RGB (8 bits per channel)
- `bgr24` - 24-bit BGR (8 bits per channel)
- `rgba32` - 32-bit RGBA (8 bits per channel + alpha)
- `bgra32` - 32-bit BGRA (8 bits per channel + alpha)

### YUV Formats

- `nv12` - YUV 4:2:0 semi-planar (UV interleaved)
- `nv12f` - YUV 4:2:0 semi-planar (full range)
- `i420` - YUV 4:2:0 planar (separate U and V planes)
- `i420f` - YUV 4:2:0 planar (full range)
- `yuyv` - YUV 4:2:2 packed (YUYV order)
- `yuyvf` - YUV 4:2:2 packed (YUYV order, full range)
- `uyvy` - YUV 4:2:2 packed (UYVY order)
- `uyvyf` - YUV 4:2:2 packed (UYVY order, full range)

## Usage Examples

### Device Discovery and Information

List all available cameras:
```bash
ccap --list-devices
```

Show detailed information for the first camera:
```bash
ccap --device-info 0
# or simply:
ccap -d 0
```

Show information for all cameras:
```bash
ccap --device-info
```

Print video file information:
```bash
ccap -i /path/to/video.mp4
```

### Basic Frame Capture

Capture 5 frames from the default camera:
```bash
ccap -d 0 -c 5 -o ./captures
```

Capture 10 frames at 1080p resolution:
```bash
ccap -d 0 -w 1920 -H 1080 -c 10 -o ./captures
```

Capture using a specific camera by name:
```bash
ccap -d "HD Pro Webcam C920" -c 5 -o ./captures
```

### Save Options

Save frames as JPEG (default):
```bash
ccap -d 0 -c 5 -o ./captures --save-jpg
```

Save frames as PNG:
```bash
ccap -d 0 -c 5 -o ./captures --save-png
```

Save frames as BMP (always available):
```bash
ccap -d 0 -c 5 -o ./captures --save-bmp
```

Save with custom JPEG quality:
```bash
ccap -d 0 -c 5 -o ./captures --save-format jpg --jpeg-quality 95
```

### Timeout and Automated Exit

Preview for 60 seconds then exit:
```bash
ccap -d 0 --preview --timeout 60
```

Capture with timeout and custom exit code:
```bash
ccap -d 0 --preview --timeout 30 --timeout-exit-code 100
```

### Video Playback

Extract 30 frames from a video file:
```bash
ccap -i /path/to/video.mp4 -c 30 -o ./frames
```

Loop video playback indefinitely:
```bash
ccap -i /path/to/video.mp4 --preview --loop
```

Loop video 5 times:
```bash
ccap -i /path/to/video.mp4 --preview --loop=5
```

Preview video at 2x speed:
```bash
ccap -i /path/to/video.mp4 --preview --speed 2.0
```

Extract frames at maximum speed (no frame rate control):
```bash
ccap -i /path/to/video.mp4 -c 100 -o ./frames --speed 0.0
```

Preview video at half speed (slow motion):
```bash
ccap -i /path/to/video.mp4 --preview --speed 0.5
```

Control playback using --fps (automatically calculates speed):
```bash
# Video is 30fps, play at 60fps (2x speed)
ccap -i /path/to/video.mp4 --preview --fps 60

# Video is 30fps, play at 15fps (0.5x speed)
ccap -i /path/to/video.mp4 --preview --fps 15
```

### Format-Specific Capture

Capture frames in BGR24 format:
```bash
ccap -d 0 --format bgr24 -c 5 -o ./captures
```

Capture with specific internal camera format (for performance):
```bash
ccap -d 0 --internal-format nv12 --format bgr24 -c 5 -o ./captures
```

### YUV Operations

Save frames as raw YUV data:
```bash
ccap -d 0 --save-yuv -c 5 -o ./yuv_captures
```

Convert a YUV file to BMP image:
```bash
ccap --convert input.yuv \
     --yuv-format nv12 \
     --yuv-width 1920 \
     --yuv-height 1080 \
     --convert-output output.bmp
```

Convert a YUV file to JPEG (requires CCAP_CLI_WITH_STB_IMAGE=ON):
```bash
ccap --convert input.yuv \
     --yuv-format nv12 \
     --yuv-width 1920 \
     --yuv-height 1080 \
     --convert-output output.jpg \
     --image-format jpg \
     --jpeg-quality 90
```

Convert a YUV file to PNG:
```bash
ccap --convert input.yuv \
     --yuv-format nv12 \
     --yuv-width 1920 \
     --yuv-height 1080 \
     --convert-output output.png \
     --image-format png
```

### Preview Mode

Preview camera feed in real-time (requires GLFW):
```bash
ccap -d 0 --preview
```

Preview only, without saving frames:
```bash
ccap -d 0 --preview-only
```

Preview while capturing frames:
```bash
ccap -d 0 -w 1920 -H 1080 -c 10 -o ./captures --preview
```

### Advanced Usage

Capture with custom timeout and high frame rate:
```bash
ccap -d 0 -w 1920 -H 1080 -f 60 -c 100 -t 10000 -o ./captures
```

Verbose logging for debugging:
```bash
ccap --verbose -d 0 -c 5 -o ./captures
```

## Output Files

### Image Files (BMP)

When capturing in RGB/BGR formats or converting YUV to images, files are saved as:
```
<output_dir>/capture_<timestamp>_<resolution>_<frame_index>.bmp
```

Example: `captures/capture_20231224_153045_1920x1080_0.bmp`

### YUV Files

When using `--save-yuv`, raw YUV data is saved as:
```
<output_dir>/capture_<timestamp>_<resolution>_<frame_index>.<format>.yuv
```

Example: `yuv_captures/capture_20231224_153045_1920x1080_0.NV12.yuv`

These files contain raw planar or packed YUV data without any container format.

## Error Handling

The CLI tool returns the following exit codes:

| Exit Code | Meaning |
|-----------|---------|
| `0` | Success |
| `1` | Invalid arguments or usage error |
| `2` | No camera devices found |
| `3` | Camera open/start failed |
| `4` | Frame capture failed |
| `5` | File I/O error |

## Integration Examples

### Bash Script

```bash
#!/bin/bash
# Capture frames from all available cameras

# List devices
ccap --list-devices

# Capture from each camera
for i in 0 1 2; do
    mkdir -p "camera_${i}_captures"
    ccap -d "$i" -w 1920 -H 1080 -c 5 -o "camera_${i}_captures" || true
done
```

### Python Integration

```python
import subprocess
import json

# Run ccap and capture output
result = subprocess.run(
    ['ccap', '--list-devices'],
    capture_output=True,
    text=True
)

if result.returncode == 0:
    print("Devices found:")
    print(result.stdout)
    
    # Capture frames
    subprocess.run([
        'ccap', '-d', '0', 
        '-w', '1920', '-H', '1080',
        '-c', '10', 
        '-o', './captures'
    ])
```

### Makefile Integration

```makefile
.PHONY: capture test-camera

capture:
	mkdir -p captures
	ccap -d 0 -w 1920 -H 1080 -c 5 -o ./captures

test-camera:
	ccap --list-devices
	ccap --device-info 0
```

## Performance Tips

1. **Use internal format**: Specify `--internal-format` to match the camera's native format for better performance
2. **Direct YUV capture**: Use `--save-yuv` when you need YUV data to avoid unnecessary conversions
3. **Larger timeouts**: Increase `--timeout` for high-resolution captures or slow cameras
4. **Hardware acceleration**: The library automatically uses hardware acceleration (AVX2, NEON, Apple Accelerate) when available

## Troubleshooting

### No devices found

```bash
ccap --list-devices
# Returns: No camera devices found
```

**Solutions:**
- Ensure camera is connected and recognized by the OS
- On Linux, check if you have permissions for `/dev/video*` devices
- Try running with `--verbose` for more details

### Permission denied (Linux)

```bash
# Add your user to the video group
sudo usermod -a -G video $USER
# Then log out and back in
```

### Capture timeout

```bash
ccap -d 0 -c 1 -o ./captures
# Error: Frame capture timeout
```

**Solutions:**
- Increase timeout: `--timeout 10000`
- Check if camera is in use by another application
- Try a different resolution: `-w 640 -H 480`

### Format not supported

```bash
ccap -d 0 --format xyz
# Error: Unknown format
```

**Solution:** Use one of the [supported formats](#supported-formats)

## See Also

- [Main ccap Documentation](documentation.md) - Overview of the CameraCapture library
- [CMake Build Options](cmake-options.md) - Build configuration details
- [C Interface Documentation](c-interface.md) - C API reference
- [Rust Bindings](rust-bindings.md) - Rust crate usage and build notes
- [Examples](https://github.com/wysaid/CameraCapture/tree/main/examples) - Code examples using the library
