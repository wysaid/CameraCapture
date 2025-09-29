# Release Workflow Optimization Summary

This document summarizes the changes made to optimize the release workflow by adding dynamic library packages.

## Changes Made

### 1. Updated Build Matrix
- **Before**: 4 build configurations (static libraries only)
  - macOS Universal (Intel + Apple Silicon)
  - Windows MSVC x64  
  - Linux x86_64
  - Linux ARM64

- **After**: 8 build configurations (static + dynamic libraries)
  - 4 static library builds (existing, unchanged)
  - 4 dynamic library builds (new)

### 2. Updated CMake Configuration
- Added `CCAP_BUILD_SHARED` flag configuration based on matrix `shared` parameter
- Static builds: `-DCCAP_BUILD_SHARED=OFF`
- Dynamic builds: `-DCCAP_BUILD_SHARED=ON`

### 3. Updated Library Packaging Logic
- **Static builds**: Copy only static libraries (.a/.lib)
- **Dynamic builds**: Copy only dynamic libraries (.so/.dll/.dylib)
- Windows: Proper handling of import libraries for DLLs
- Conditional copying based on build type

### 4. Updated Release Artifacts
- **Static packages**: Original naming (e.g., `ccap-linux-x86_64.tar.gz`)
- **Dynamic packages**: New naming with `-dynamic` suffix (e.g., `ccap-linux-x86_64-dynamic.tar.gz`)

### 5. Updated Documentation
- Release notes now clearly separate static vs dynamic packages
- Usage instructions for both static and dynamic linking
- Updated package contents descriptions

## Package Output

The release workflow now produces **8 packages** instead of 4:

**Static Library Packages:**
1. `ccap-macos-universal.tar.gz`
2. `ccap-msvc-x86_64.zip`  
3. `ccap-linux-x86_64.tar.gz`
4. `ccap-linux-arm64.tar.gz`

**Dynamic Library Packages:**
5. `ccap-macos-universal-dynamic.tar.gz`
6. `ccap-msvc-x86_64-dynamic.zip`
7. `ccap-linux-x86_64-dynamic.tar.gz`
8. `ccap-linux-arm64-dynamic.tar.gz`

## Technical Details

### Library Files by Platform
- **Windows Static**: `ccap.lib`, `ccapd.lib`
- **Windows Dynamic**: `ccap.dll` + `ccap_import.lib`, `ccapd.dll` + `ccapd_import.lib`
- **macOS Static**: `libccap.a` (Universal)
- **macOS Dynamic**: `libccap.dylib` (Universal)
- **Linux Static**: `libccap.a`
- **Linux Dynamic**: `libccap.so`

### Minimal Changes Philosophy
The implementation follows the minimal changes approach:
- No changes to existing static build configurations
- No changes to CMake files or build system
- Leverages existing `CCAP_BUILD_SHARED` option
- Maintains backward compatibility
- Doubles package output without breaking existing workflow

## Testing
- Verified both static and dynamic builds work on Linux
- Validated YAML syntax
- Confirmed CMake configurations are correct
- Package naming follows consistent pattern

The workflow is now ready to produce both static and dynamic library packages for all supported platforms.