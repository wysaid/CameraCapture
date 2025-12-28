# Build Scripts Documentation

This directory contains various build and test scripts for the ccap project.

**All scripts support execution from any directory using absolute paths!**

## Main Scripts

### Build Scripts

- **`build.sh`** - Universal build script supporting multiple architectures

  ```bash
  # Build native architecture version (recommended)
  /path/to/ccap/scripts/build.sh native Debug
  
  # Build ARM64 version
  /path/to/ccap/scripts/build.sh arm64 Debug
  
  # Build x86_64 version
  /path/to/ccap/scripts/build.sh x86_64 Debug
  
  # Build universal version (both architectures)
  /path/to/ccap/scripts/build.sh universal Release
  ```

- **`build_arm64.sh`** - Dedicated ARM64 build script

  ```bash
  /path/to/ccap/scripts/build_arm64.sh Debug
  ```

- **`build_x86_64.sh`** - Dedicated x86_64 build script

  ```bash
  /path/to/ccap/scripts/build_x86_64.sh Debug
  ```

### Test Scripts

- **`test_arch.sh`** - Architecture detection test

  ```bash
  /path/to/ccap/scripts/test_arch.sh
  ```

### Windows Build

- **`build.sh`** - Universal build script supporting multiple architectures

```bash
# Build native architecture version (recommended)
/path/to/ccap/scripts/build.sh native Debug

# Build ARM64 version
/path/to/ccap/scripts/build.sh arm64 Debug

# Build x86_64 version
/path/to/ccap/scripts/build.sh x86_64 Debug

# Build universal version (both architectures)
/path/to/ccap/scripts/build.sh universal Release
```

- **`build_arm64.sh`** - Dedicated ARM64 build script

```bash
/path/to/ccap/scripts/build_arm64.sh Debug
```

- **`build_x86_64.sh`** - Dedicated x86_64 build script

```bash
/path/to/ccap/scripts/build_x86_64.sh Debug
```

### Test Scripts

- **`run_tests.sh`** - Comprehensive test runner for all ccap tests

  ```bash
  # Run all tests (functional + performance)
  /path/to/ccap/scripts/run_tests.sh
  
  # Run only functional tests (with AddressSanitizer)
  /path/to/ccap/scripts/run_tests.sh --functional
  
  # Run only performance tests (without ASAN)
  /path/to/ccap/scripts/run_tests.sh --performance
  
  # Run video file playback tests
  /path/to/ccap/scripts/run_tests.sh --video
  ```
  
  Video playback tests use a built-in test video (tests/test-data/test.mp4) included in the repository.

- **`test_arch.sh`** - Architecture detection test

  ```bash
  /path/to/ccap/scripts/test_arch.sh
  ```

This script detects the current system and compiler architecture support, helping debug NEON feature detection issues.

- **`verify_neon.sh`** - NEON support verification

```bash
/path/to/ccap/scripts/verify_neon.sh
```

This script specifically verifies compile-time and runtime detection of NEON instruction set, demonstrating ccap's NEON support status on different architectures.

## Path Handling Technique

All scripts use the following technique to ensure they can be executed from any directory:

```bash
# Get the real directory path where the script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
```

This means you can:

- Execute from project root: `./scripts/build.sh`
- Execute from any other directory: `/absolute/path/to/ccap/scripts/build.sh`
- Execute via symbolic links

## Usage Recommendations

1. **Daily Development**: Use `build.sh native Debug`
2. **Test Specific Architecture**: Use the corresponding dedicated script
3. **Release Build**: Use `build.sh universal Release`
4. **Debug Architecture Issues**: Run `test_arch.sh` first

## Directory Structure

```
scripts/
├── README.md           # This file
├── build.sh           # Universal build script
├── build_arm64.sh     # ARM64-specific build script
├── build_x86_64.sh    # x86_64-specific build script
├── test_arch.sh       # Architecture detection test script
└── verify_neon.sh     # NEON support verification script
```

## Notes

- ARM64 binaries compiled on Intel Mac cannot run directly but can be used for cross-compilation
- Compiling x86_64 version on Apple Silicon Mac requires Rosetta 2 support
- All scripts support both Debug and Release build types
- CMakeLists.txt has been fixed to only force ARM64 architecture when explicitly requested
