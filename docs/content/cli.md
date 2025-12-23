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
cmake -B build -DBUILD_CCAP_CLI=ON
cmake --build build
```

### Build with Preview Support (GLFW)

```bash
cmake -B build -DBUILD_CCAP_CLI=ON -DCCAP_CLI_WITH_GLFW=ON
cmake --build build
```

The executable will be located in the `build/` directory (or `build/Debug`, `build/Release` depending on your build configuration).

## Command-Line Reference

### General Options

| Option | Description |
|--------|-------------|
| `-h, --help` | Show help message and exit |
| `-v, --version` | Show version information |
| `--verbose` | Enable verbose logging output |

### Device Enumeration

| Option | Description |
|--------|-------------|
| `-l, --list-devices` | List all available camera devices |
| `-i, --device-info [INDEX]` | Show detailed capabilities for device at INDEX<br>If INDEX is omitted, shows info for all devices |

### Capture Options

| Option | Default | Description |
|--------|---------|-------------|
| `-d, --device INDEX\|NAME` | `0` | Select camera device by index or name |
| `-w, --width WIDTH` | `1280` | Set capture width in pixels |
| `-H, --height HEIGHT` | `720` | Set capture height in pixels |
| `-f, --fps FPS` | `30.0` | Set frame rate |
| `-c, --count COUNT` | `1` | Number of frames to capture |
| `-t, --timeout MS` | `5000` | Capture timeout in milliseconds |
| `-o, --output DIR` | - | Output directory for captured images |
| `--format FORMAT` | - | Output pixel format (see [Supported Formats](#supported-formats)) |
| `--internal-format FORMAT` | - | Camera's internal pixel format |
| `--save-yuv` | - | Save frames as raw YUV data instead of converting to BMP |

### Preview Options

These options are only available when built with GLFW support (`CCAP_CLI_WITH_GLFW=ON`).

| Option | Description |
|--------|-------------|
| `-p, --preview` | Enable real-time preview window |
| `--preview-only` | Show preview without saving frames to disk |

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

### Device Discovery

List all available cameras:
```bash
ccap --list-devices
```

Show detailed information for the first camera:
```bash
ccap --device-info 0
```

Show information for all cameras:
```bash
ccap --device-info
```

### Basic Frame Capture

Capture a single frame from the default camera:
```bash
ccap -d 0 -o ./captures
```

Capture 10 frames at 1080p resolution:
```bash
ccap -d 0 -w 1920 -H 1080 -c 10 -o ./captures
```

Capture using a specific camera by name:
```bash
ccap -d "HD Pro Webcam C920" -c 5 -o ./captures
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
ccap -d 0 --format nv12 --save-yuv -c 5 -o ./yuv_captures
```

Convert a YUV file to BMP image:
```bash
ccap --convert input.yuv \
     --yuv-format nv12 \
     --yuv-width 1920 \
     --yuv-height 1080 \
     --convert-output output.bmp
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

- [Main ccap Documentation](documentation.html) - Overview of the CameraCapture library
- [CMake Build Options](https://github.com/wysaid/CameraCapture/blob/main/docs/CMAKE_OPTIONS.md) - Build configuration details
- [C Interface Documentation](https://github.com/wysaid/CameraCapture/blob/main/docs/C_Interface.md) - C API reference
- [Examples](https://github.com/wysaid/CameraCapture/tree/main/examples) - Code examples using the library
