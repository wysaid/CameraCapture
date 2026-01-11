#!/usr/bin/env bash

# Test ccap-rs crate from crates.io
#
# Usage:
#   scripts/test_remote_crate.sh [version]
#
# Examples:
#   scripts/test_remote_crate.sh                    # Test latest version
#   scripts/test_remote_crate.sh 1.5.0-test.xxx    # Test specific version

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

VERSION="${1:-latest}"
TEST_DIR="${PROJECT_ROOT}/build/rust_crate_verification"

log() {
    printf "\033[0;32m[TEST]\033[0m %s\n" "$*"
}

warn() {
    printf "\033[0;33m[WARN]\033[0m %s\n" "$*" >&2
}

error() {
    printf "\033[0;31m[ERROR]\033[0m %s\n" "$*" >&2
}

die() {
    error "$*"
    exit 1
}

cleanup() {
    if [[ -d "${TEST_DIR}" ]]; then
        log "Cleaning up test directory: ${TEST_DIR}"
        rm -rf "${TEST_DIR}"
    fi
}

# Note: cleanup is disabled to preserve test results (captured images, etc.)
# Uncomment the line below to enable automatic cleanup on exit
# trap cleanup EXIT

main() {
    log "Testing ccap-rs from crates.io"

    # If version is "latest", fetch the actual latest version from crates.io
    if [[ "${VERSION}" == "latest" ]]; then
        log "Fetching latest version from crates.io..."
        local latest_version
        latest_version=$(cargo search ccap-rs --limit 1 2>/dev/null | grep -oE '[0-9]+\.[0-9]+\.[0-9]+(-[a-zA-Z0-9.]+)?' | head -1 || echo "")

        if [[ -n "${latest_version}" ]]; then
            VERSION="${latest_version}"
            log "Found latest version: ${VERSION}"
        else
            die "Failed to fetch latest version from crates.io"
        fi
    fi

    log "Version: ${VERSION}"
    log "Test directory: ${TEST_DIR}"

    # Clean up any existing test directory
    rm -rf "${TEST_DIR}"
    mkdir -p "${TEST_DIR}"

    # Create test project
    log "Creating test project..."
    cd "${TEST_DIR}"
    cargo init --bin ccap-test --quiet

    cd ccap-test

    # Add dependency to Cargo.toml
    log "Adding ccap-rs dependency (version: ${VERSION})..."
    python3 - "${VERSION}" <<'PY'
import re
import sys
from pathlib import Path

version = sys.argv[1]
path = Path("Cargo.toml")
text = path.read_text(encoding="utf-8")

dep_line = f'ccap-rs = "{version}"\n'

# Find the [dependencies] section (until the next [section] or EOF)
m = re.search(r'(?ms)^\[dependencies\]\s*\n(.*?)(^\[|\Z)', text)

if m:
    body = m.group(1)
    # Replace existing ccap-rs entry if present; otherwise insert at top of the section.
    if re.search(r'(?m)^ccap-rs\s*=\s*"[^"]*"\s*$', body):
        body2 = re.sub(r'(?m)^ccap-rs\s*=\s*"[^"]*"\s*$', dep_line.rstrip("\n"), body, count=1)
    else:
        body2 = dep_line + body
    text = text[: m.start(1)] + body2 + text[m.end(1) :]
else:
    # No [dependencies] section - append one.
    if not text.endswith("\n"):
        text += "\n"
    text += "\n[dependencies]\n" + dep_line

path.write_text(text, encoding="utf-8")
PY

    # Debug: Show Cargo.toml content
    if [[ "${CCAP_TEST_DEBUG:-0}" == "1" ]]; then
        log "Debug: Cargo.toml content:"
        cat Cargo.toml
    fi

    # Get the actual version being used
    log "Running cargo update..."
    if ! cargo update --quiet 2>&1; then
        warn "cargo update had warnings/errors, continuing..."
    fi

    local actual_version
    actual_version=$(cargo metadata --format-version=1 --no-deps 2>/dev/null |
        python3 -c "import sys, json; data = json.load(sys.stdin); pkg = next((p for p in data.get('packages', []) if p.get('name') == 'ccap-rs'), None); print(pkg['version'] if pkg else 'unknown')" 2>/dev/null || echo "unknown")

    log "Testing ccap-rs version: ${actual_version}"

    # Create test code
    log "Creating test code..."

    # Copy test video file if it exists (for video playback test)
    local test_video="${PROJECT_ROOT}/tests/test-data/test.mp4"
    if [[ -f "${test_video}" ]]; then
        log "Copying test video file..."
        mkdir -p tests/test-data
        cp "${test_video}" tests/test-data/
    fi

    cat >src/main.rs <<'RUST_CODE'
use ccap::{Provider, PixelFormat, Convert, Utils};


fn main() {
    println!("=== ccap-rs Remote Crate Verification ===\n");

    // Test 1: Create provider and list available devices
    println!("Test 1: Listing camera devices");
    let has_devices = match Provider::new() {
        Ok(provider) => {
            match provider.find_device_names() {
                Ok(devices) => {
                    if devices.is_empty() {
                        println!("  ⚠️  No camera devices found (this is OK in CI/headless environments)");
                        false
                    } else {
                        println!("  ✓ Found {} device(s):", devices.len());
                        for (i, device) in devices.iter().enumerate() {
                            println!("    {}. {}", i + 1, device);
                        }
                        true
                    }
                }
                Err(e) => {
                    eprintln!("  ✗ Error listing devices: {}", e);
                    std::process::exit(1);
                }
            }
        }
        Err(e) => {
            eprintln!("  ✗ Error creating provider: {}", e);
            std::process::exit(1);
        }
    };
    println!();

    // Test 1.5: Capture image from camera if available
    if has_devices {
        println!("Test 1.5: Capturing image from camera");
        match Provider::new() {
            Ok(mut provider) => {
                // Open first device by name
                match provider.find_device_names() {
                    Ok(devices) if !devices.is_empty() => {
                        // Open device with the device name
                        match provider.open_device(Some(&devices[0]), false) {
                            Ok(_) => {
                                // Start capture
                                if let Err(e) = provider.start() {
                                    eprintln!("  ⚠️  Failed to start camera: {}", e);
                                } else {
                                    // Wait a bit for camera to initialize
                                    std::thread::sleep(std::time::Duration::from_millis(500));
                                    
                                    // Try to grab a frame
                                    match provider.grab_frame(2000) {
                                        Ok(Some(frame)) => {
                                            let width = frame.width();
                                            let height = frame.height();
                                            let format = frame.pixel_format();
                                            
                                            println!("  ✓ Captured frame: {}x{} {:?}", width, height, format);
                                            
                                            // Save frame as BMP using ccap's built-in function
                                            let filename = "captured_frame";
                                            match Utils::save_frame_as_bmp(&frame, filename) {
                                                Ok(_) => {
                                                    println!("  ✓ Saved captured frame to {}.bmp", filename);
                                                }
                                                Err(e) => {
                                                    eprintln!("  ⚠️  Failed to save frame: {}", e);
                                                }
                                            }
                                        }
                                        Ok(None) => {
                                            eprintln!("  ⚠️  Failed to grab frame: timeout");
                                        }
                                        Err(e) => {
                                            eprintln!("  ⚠️  Failed to grab frame: {}", e);
                                        }
                                    }
                                    
                                    // Stop camera
                                    let _ = provider.stop();
                                }
                            }
                            Err(e) => {
                                eprintln!("  ⚠️  Failed to open camera: {}", e);
                            }
                        }
                    }
                    _ => {}
                }
            }
            Err(e) => {
                eprintln!("  ⚠️  Failed to create provider: {}", e);
            }
        }
        println!();
    }

    // Test 2: Test pixel format conversions with Convert trait
    println!("Test 2: Testing color conversion functions");
    
    // Create test data (16x16 NV12 image)
    let width = 16;
    let height = 16;
    let y_size = width * height;
    let uv_size = width * height / 2;
    
    let y_plane = vec![128u8; y_size];  // Gray Y plane
    let uv_plane = vec![128u8; uv_size]; // Neutral UV plane
    
    // For simple test without padding: stride equals width
    let y_stride = width;
    let uv_stride = width;
    
    // Try NV12 to RGB24 conversion
    match Convert::nv12_to_rgb24(&y_plane, y_stride, &uv_plane, uv_stride, width as u32, height as u32) {
        Ok(rgb_data) => {
            let expected_size = width * height * 3;
            if rgb_data.len() == expected_size {
                println!("  ✓ NV12 to RGB24 conversion successful ({} bytes)", rgb_data.len());
            } else {
                eprintln!("  ✗ Unexpected output size: {} (expected {})", rgb_data.len(), expected_size);
                std::process::exit(1);
            }
        }
        Err(e) => {
            eprintln!("  ✗ Conversion error: {}", e);
            std::process::exit(1);
        }
    }
    
    // Try NV12 to BGR24 conversion
    match Convert::nv12_to_bgr24(&y_plane, y_stride, &uv_plane, uv_stride, width as u32, height as u32) {
        Ok(bgr_data) => {
            println!("  ✓ NV12 to BGR24 conversion successful ({} bytes)", bgr_data.len());
        }
        Err(e) => {
            eprintln!("  ✗ Conversion error: {}", e);
            std::process::exit(1);
        }
    }
    println!();

    // Test 3: Test PixelFormat enum
    println!("Test 3: Testing PixelFormat enum");
    let formats = vec![
        PixelFormat::Nv12,
        PixelFormat::I420,
        PixelFormat::Rgb24,
        PixelFormat::Bgr24,
        PixelFormat::Rgba32,
        PixelFormat::Bgra32,
    ];
    
    for format in formats {
        println!("  ✓ PixelFormat::{:?} available", format);
    }
    println!();

    // Test 4: Video file playback (Windows/macOS only)
    #[cfg(any(target_os = "windows", target_os = "macos"))]
    {
        println!("Test 4: Testing video file playback");
        
        // Test video file path
        let video_path = "tests/test-data/test.mp4";
        
        if std::path::Path::new(video_path).exists() {
            match Provider::new() {
                Ok(mut provider) => {
                    // Open video file using open_device
                    match provider.open_device(Some(video_path), false) {
                        Ok(_) => {
                            if let Err(e) = provider.start() {
                                eprintln!("  ⚠️  Failed to start video: {}", e);
                            } else {
                                println!("  ✓ Video opened successfully: {}", video_path);
                                
                                // Read a few frames
                                let mut frame_count = 0;
                                for i in 0..5 {
                                    match provider.grab_frame(1000) {
                                        Ok(Some(frame)) => {
                                            frame_count += 1;
                                            if i == 0 {
                                                println!("  ✓ Read frame {}: {}x{} {:?}", 
                                                    frame_count, frame.width(), frame.height(), frame.pixel_format());
                                            }
                                        }
                                        Ok(None) => break,
                                        Err(_) => break,
                                    }
                                }
                                
                                if frame_count > 0 {
                                    println!("  ✓ Successfully read {} frame(s) from video", frame_count);
                                } else {
                                    eprintln!("  ⚠️  No frames could be read from video");
                                }
                                
                                let _ = provider.stop();
                            }
                        }
                        Err(e) => {
                            eprintln!("  ⚠️  Failed to open video file: {}", e);
                        }
                    }
                }
                Err(e) => {
                    eprintln!("  ⚠️  Failed to create provider: {}", e);
                }
            }
        } else {
            println!("  ⚠️  Test video file not found at {}, skipping video playback test", video_path);
        }
        println!();
    }

    println!("=== All Tests Passed ✓ ===");
}
RUST_CODE

    # Build the test
    log "Building test project..."
    if ! cargo build --quiet 2>&1; then
        error "Build failed"
        return 1
    fi

    log "Build successful"

    # Run the test
    log "Running tests..."
    echo ""
    if cargo run --quiet; then
        echo ""
        log "✓ All tests passed successfully"
        log ""
        log "Test artifacts saved in: ${TEST_DIR}/ccap-test/"
        log "  - Captured image: captured_frame.bmp (if camera was available)"
        log "  - Test binary: target/debug/ccap-test"
        log ""
        log "To clean up test directory manually, run:"
        log "  rm -rf ${TEST_DIR}"
        return 0
    else
        echo ""
        error "✗ Tests failed"
        return 1
    fi
}

main "$@"
