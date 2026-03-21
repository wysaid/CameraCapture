# ccap Installation Notes

This file supports the publishable `ccap` skill.

## Preferred Order

1. Reuse an existing `ccap` installation if present.
2. On macOS with Homebrew available, prefer Homebrew.
3. If a local compiler and CMake toolchain are available, build from source.
4. Only if no suitable build toolchain exists, check whether a matching GitHub Release binary exists.

## Reuse Existing Install

```bash
command -v ccap
ccap --version
```

## Homebrew On macOS

```bash
brew tap wysaid/ccap
brew install ccap
ccap --version
```

Use this when the host is macOS and the task only needs the CLI.

## Build From Source

Repository install shortcut:

```bash
./scripts/build_and_install.sh
```

CLI-focused build:

```bash
cmake -B build -DCCAP_BUILD_CLI=ON
cmake --build build
./build/ccap --version
```

Release-style CLI build:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCCAP_BUILD_CLI=ON
cmake --build build
./build/ccap --version
```

## GitHub Release Binary Fallback

Only use this path when:

- no suitable local build toolchain is available
- network access is available
- a matching asset exists for the current OS and architecture

Rules:

- Verify the asset name before download.
- Do not invent URLs or filenames.
- If no matching release asset exists, fall back to source build instead of guessing.

## Notes

- Homebrew installation is documented for macOS.
- Source build is the broadest fallback and should be preferred over speculative binary downloads.
- On Windows, repository documentation expects source build/install to run in Git Bash for the provided script path.