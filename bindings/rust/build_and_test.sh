#!/bin/bash

# Rust bindings build and test script for ccap

set -e

echo "ccap Rust Bindings - Build and Test Script"
echo "==========================================="

# Get the script directory and project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
RUST_DIR="$SCRIPT_DIR"

echo "Project root: $PROJECT_ROOT"
echo "Rust bindings: $RUST_DIR"

# Build the C library first if needed
echo ""
echo "Step 1: Checking ccap C library..."
if [ ! -f "$PROJECT_ROOT/build/Debug/libccap.a" ] && [ ! -f "$PROJECT_ROOT/build/Release/libccap.a" ]; then
    echo "ccap C library not found. Building..."
    cd "$PROJECT_ROOT"

    if [ ! -d "build" ]; then
        mkdir -p build/Debug
        cd build/Debug
        cmake ../.. -DCMAKE_BUILD_TYPE=Debug
        make -j$(nproc 2>/dev/null || echo 4)
    else
        echo "Build directory exists, assuming library is built"
    fi

    cd "$RUST_DIR"
else
    echo "ccap C library found"
fi

# Build Rust bindings
echo ""
echo "Step 2: Building Rust bindings..."
cd "$RUST_DIR"

# Clean previous build
cargo clean

# Build with default features
echo "Building with default features..."
cargo build

# Build with async features
echo "Building with async features..."
cargo build --features async

# Run tests
echo ""
echo "Step 3: Running tests..."
cargo test

# Build examples
echo ""
echo "Step 4: Building examples..."
cargo build --examples
cargo build --features async --examples

# Try to run basic example
echo ""
echo "Step 5: Testing basic functionality..."
echo "Running camera discovery test..."
if cargo run --example list_cameras; then
    echo "✅ Camera discovery test passed"
else
    echo "⚠️  Camera discovery test failed (this may be normal if no cameras are available)"
fi

echo ""
echo "✅ All Rust binding builds completed successfully!"
echo ""
echo "Usage examples:"
echo "  cargo run --example list_cameras"
echo "  cargo run --example capture_frames"
echo "  cargo run --features async --example async_capture"
echo ""
echo "To use in your project, add to Cargo.toml:"
echo '  ccap = { path = "'$RUST_DIR'" }'
echo '  # or with async support:'
echo '  ccap = { path = "'$RUST_DIR'", features = ["async"] }'
