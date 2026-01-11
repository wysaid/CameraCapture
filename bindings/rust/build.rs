use std::env;
use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;

fn file_contains_bytes(path: &Path, needle: &[u8]) -> bool {
    let Ok(data) = fs::read(path) else {
        return false;
    };
    if needle.is_empty() {
        return false;
    }
    data.windows(needle.len()).any(|w| w == needle)
}

fn clang_resource_dir() -> Option<PathBuf> {
    // Prefer clang in PATH.
    if let Ok(out) = Command::new("clang").arg("--print-resource-dir").output() {
        if out.status.success() {
            let s = String::from_utf8_lossy(&out.stdout);
            let p = s.trim();
            if !p.is_empty() {
                return Some(PathBuf::from(p));
            }
        }
    }

    // Fallback to xcrun on macOS.
    if let Ok(out) = Command::new("xcrun")
        .args(["--sdk", "macosx", "--find", "clang"])
        .output()
    {
        if out.status.success() {
            let clang_path = String::from_utf8_lossy(&out.stdout);
            let clang_path = clang_path.trim();
            if !clang_path.is_empty() {
                if let Ok(out2) = Command::new(clang_path)
                    .arg("--print-resource-dir")
                    .output()
                {
                    if out2.status.success() {
                        let s = String::from_utf8_lossy(&out2.stdout);
                        let p = s.trim();
                        if !p.is_empty() {
                            return Some(PathBuf::from(p));
                        }
                    }
                }
            }
        }
    }

    None
}

fn looks_like_ccap_root(dir: &Path) -> bool {
    dir.join("include/ccap_c.h").exists() && dir.join("src/ccap_core.cpp").exists()
}

fn find_ccap_root_from(start: &Path) -> Option<PathBuf> {
    // Walk up a reasonable number of parents to find the repo root.
    // This fixes cases like `cargo publish --dry-run` where the manifest dir
    // becomes: <repo>/bindings/rust/target/package/<crate>-<ver>
    let mut cur = Some(start);
    for _ in 0..16 {
        let dir = cur?;
        if looks_like_ccap_root(dir) {
            return Some(dir.to_path_buf());
        }
        cur = dir.parent();
    }
    None
}

fn main() {
    // Re-run build script when the build script itself changes.
    println!("cargo:rerun-if-changed=build.rs");
    // Re-run when wrapper changes (bindgen input).
    println!("cargo:rerun-if-changed=wrapper.h");
    // Allow users to override the source checkout location.
    println!("cargo:rerun-if-env-changed=CCAP_SOURCE_DIR");
    // Allow users to opt out ASan runtime auto-link (for static-link + ASan prebuilt libs).
    println!("cargo:rerun-if-env-changed=CCAP_RUST_NO_ASAN_LINK");

    // Tell cargo to look for shared libraries in the specified directory
    let manifest_dir = env::var("CARGO_MANIFEST_DIR").unwrap();
    let manifest_path = PathBuf::from(&manifest_dir);

    // Check if we should build from source or link against pre-built library.
    // NOTE: We treat `build-source` and `static-link` differently regarding source root:
    // - build-source should prefer vendored ./native for crates.io friendliness.
    // - static-link should prefer the repo root / CCAP_SOURCE_DIR so it can find build/Debug|Release.
    let build_from_source = env::var("CARGO_FEATURE_BUILD_SOURCE").is_ok();
    let static_link = env::var("CARGO_FEATURE_STATIC_LINK").is_ok();

    // Locate ccap root.
    // build-source path (distribution): prefer ./native for crates.io.
    // static-link path (development): prefer repo root / CCAP_SOURCE_DIR for build artifacts.
    let (ccap_root, _is_packaged) = if build_from_source {
        // 1) Vendored sources under ./native (ideal for crates.io)
        if manifest_path.join("native").exists() {
            (manifest_path.join("native"), true)
        } else if let Some(root) = find_ccap_root_from(&manifest_path) {
            // 2) Search parent dirs for CameraCapture repo root (works for git checkout
            //    and for `cargo publish --dry-run` which builds from target/package)
            (root, false)
        } else if let Ok(root) = env::var("CCAP_SOURCE_DIR") {
            // 3) Allow override via CCAP_SOURCE_DIR
            let root = PathBuf::from(root);
            if looks_like_ccap_root(&root) {
                (root, false)
            } else {
                panic!(
                    "CCAP_SOURCE_DIR is set but does not look like CameraCapture root: {}",
                    root.display()
                );
            }
        } else {
            // Keep a placeholder; if build-source is enabled we'll error with a clear message.
            (manifest_path.clone(), false)
        }
    } else {
        // Dev/static-link mode: even if ./native exists, we still prefer the repo root so we can
        // link against pre-built build/Debug|Release artifacts.
        if let Some(root) = find_ccap_root_from(&manifest_path) {
            (root, false)
        } else if let Ok(root) = env::var("CCAP_SOURCE_DIR") {
            let root = PathBuf::from(root);
            if looks_like_ccap_root(&root) {
                (root, false)
            } else {
                panic!(
                    "CCAP_SOURCE_DIR is set but does not look like CameraCapture root: {}",
                    root.display()
                );
            }
        } else if static_link {
            panic!(
                "static-link feature is enabled, but CameraCapture repo root was not found.\n\
\
Tried (in order):\n\
  - searching parent directories for include/ccap_c.h and src/ccap_core.cpp\n\
  - CCAP_SOURCE_DIR environment variable\n\
\
Please set CCAP_SOURCE_DIR to a CameraCapture checkout (with build/Debug|Release built)."
            );
        } else {
            // Fallback placeholder.
            (manifest_path.clone(), false)
        }
    };

    if build_from_source {
        if !looks_like_ccap_root(&ccap_root) {
            panic!(
                "build-source feature is enabled, but CameraCapture sources were not found.\n\
\
Tried (in order):\n\
  - ./native (vendored) under the crate root\n\
  - searching parent directories for include/ccap_c.h and src/ccap_core.cpp\n\
  - CCAP_SOURCE_DIR environment variable\n\
\
Please vendor the sources into bindings/rust/native/, or set CCAP_SOURCE_DIR to a CameraCapture checkout." 
            );
        }

        // Build from source using cc crate
        let mut build = cc::Build::new();

        // Add source files (excluding SIMD-specific files)
        build
            .file(ccap_root.join("src/ccap_core.cpp"))
            .file(ccap_root.join("src/ccap_utils.cpp"))
            .file(ccap_root.join("src/ccap_convert.cpp"))
            .file(ccap_root.join("src/ccap_convert_frame.cpp"))
            .file(ccap_root.join("src/ccap_imp.cpp"))
            .file(ccap_root.join("src/ccap_c.cpp"))
            .file(ccap_root.join("src/ccap_utils_c.cpp"))
            .file(ccap_root.join("src/ccap_convert_c.cpp"));

        // Platform specific sources
        #[cfg(target_os = "macos")]
        {
            build
                .file(ccap_root.join("src/ccap_imp_apple.mm"))
                .file(ccap_root.join("src/ccap_convert_apple.cpp"))
                .file(ccap_root.join("src/ccap_file_reader_apple.mm"));
        }

        #[cfg(target_os = "linux")]
        {
            build.file(ccap_root.join("src/ccap_imp_linux.cpp"));
        }

        #[cfg(target_os = "windows")]
        {
            build
                .file(ccap_root.join("src/ccap_imp_windows.cpp"))
                .file(ccap_root.join("src/ccap_file_reader_windows.cpp"));
        }

        // Include directories
        build
            .include(ccap_root.join("include"))
            .include(ccap_root.join("src"));

        // Compiler flags
        build.cpp(true).std("c++17"); // Use C++17

        // Enable file playback support
        build.define("CCAP_ENABLE_FILE_PLAYBACK", "1");

        #[cfg(target_os = "macos")]
        {
            build.flag("-fobjc-arc"); // Enable ARC for Objective-C++
        }

        // Compile
        build.compile("ccap");

        // Build SIMD-specific files separately with appropriate flags
        #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
        {
            let mut avx2_build = cc::Build::new();
            avx2_build
                .file(ccap_root.join("src/ccap_convert_avx2.cpp"))
                .include(ccap_root.join("include"))
                .include(ccap_root.join("src"))
                .cpp(true)
                .std("c++17");

            // Only add SIMD flags on non-MSVC compilers
            if !avx2_build.get_compiler().is_like_msvc() {
                avx2_build.flag("-mavx2").flag("-mfma");
            } else {
                // MSVC uses /arch:AVX2
                avx2_build.flag("/arch:AVX2");
            }

            avx2_build.compile("ccap_avx2");
        }

        // Always build neon file for hasNEON() symbol
        // On non-ARM architectures, ENABLE_NEON_IMP will be 0 and function returns false
        {
            let mut neon_build = cc::Build::new();
            neon_build
                .file(ccap_root.join("src/ccap_convert_neon.cpp"))
                .include(ccap_root.join("include"))
                .include(ccap_root.join("src"))
                .cpp(true)
                .std("c++17");

            // Only add NEON flags on aarch64
            #[cfg(target_arch = "aarch64")]
            {
                // NEON is always available on aarch64, no special flags needed
            }

            neon_build.compile("ccap_neon");
        }

        println!("cargo:warning=Building ccap from source...");
    } else {
        // Link against pre-built library (Development mode)
        // Determine build profile
        let profile = env::var("PROFILE").unwrap_or_else(|_| "debug".to_string());
        let build_type = if profile == "release" {
            "Release"
        } else {
            "Debug"
        };

        // If the prebuilt static library was compiled with AddressSanitizer (ASan), we must link
        // the ASan runtime as well. The repo's default functional test build enables ASan for
        // Debug builds (see scripts/run_tests.sh), so this situation is expected.
        //
        // We detect this by scanning the archive bytes for common ASan symbols.
        let target_os = env::var("CARGO_CFG_TARGET_OS").unwrap_or_default();
        if env::var("CCAP_RUST_NO_ASAN_LINK").is_err() && (target_os == "macos" || target_os == "linux") {
            let archive_path = ccap_root
                .join("build")
                .join(build_type)
                .join("libccap.a");

            let asan_instrumented = file_contains_bytes(&archive_path, b"___asan_init")
                || file_contains_bytes(&archive_path, b"__asan_init");

            if asan_instrumented {
                // rustc links with `-nodefaultlibs` which can prevent clang from automatically
                // adding the ASan runtime, even if `-fsanitize=address` is present.
                // We therefore explicitly link the runtime.
                println!("cargo:rustc-link-arg=-fsanitize=address");

                if target_os == "linux" {
                    // Requires libasan (e.g. Ubuntu: libasan6) to be installed.
                    println!("cargo:rustc-link-lib=asan");
                }

                if target_os == "macos" {
                    // Prefer the ASan runtime shipped with the active clang toolchain.
                    if let Some(resource_dir) = clang_resource_dir() {
                        let runtime_dir = resource_dir.join("lib").join("darwin");
                        let dylib = runtime_dir.join("libclang_rt.asan_osx_dynamic.dylib");
                        if dylib.exists() {
                            println!("cargo:rustc-link-search=native={}", runtime_dir.display());
                            // Ensure the runtime dylib can be found at execution time.
                            println!("cargo:rustc-link-arg=-Wl,-rpath,{}", runtime_dir.display());
                        }
                    }
                    println!("cargo:rustc-link-lib=dylib=clang_rt.asan_osx_dynamic");
                }

                println!(
                    "cargo:warning=Prebuilt {} appears to be ASan-instrumented; linking ASan runtime. Set CCAP_RUST_NO_ASAN_LINK=1 to disable.",
                    archive_path.display()
                );
            }
        }

        // Add the ccap library search path
        // Try specific build type first, then fallback to others
        println!(
            "cargo:rustc-link-search=native={}/build/{}",
            ccap_root.display(),
            build_type
        );
        println!(
            "cargo:rustc-link-search=native={}/build/Debug",
            ccap_root.display()
        );
        println!(
            "cargo:rustc-link-search=native={}/build/Release",
            ccap_root.display()
        );

        // Link to ccap library
        // Note: On MSVC, we always link to the Release version (ccap.lib)
        // to avoid CRT mismatch issues, since Rust uses the release CRT
        // even in debug builds by default
        println!("cargo:rustc-link-lib=static=ccap");

        println!("cargo:warning=Linking against pre-built ccap library (dev mode)...");
    }

    // Platform-specific linking (Common for both modes)
    #[cfg(target_os = "macos")]
    {
        println!("cargo:rustc-link-lib=framework=Foundation");
        println!("cargo:rustc-link-lib=framework=AVFoundation");
        println!("cargo:rustc-link-lib=framework=CoreMedia");
        println!("cargo:rustc-link-lib=framework=CoreVideo");
        println!("cargo:rustc-link-lib=framework=Accelerate");
        println!("cargo:rustc-link-lib=System");
        println!("cargo:rustc-link-lib=c++");
    }

    #[cfg(target_os = "linux")]
    {
        // v4l2 might not be available on all systems
        // println!("cargo:rustc-link-lib=v4l2");
        println!("cargo:rustc-link-lib=stdc++");
    }

    #[cfg(target_os = "windows")]
    {
        println!("cargo:rustc-link-lib=strmiids");
        println!("cargo:rustc-link-lib=ole32");
        println!("cargo:rustc-link-lib=oleaut32");
        // Media Foundation libraries for video file playback
        println!("cargo:rustc-link-lib=mfplat");
        println!("cargo:rustc-link-lib=mfreadwrite");
        println!("cargo:rustc-link-lib=mfuuid");
    }

    // Use ccap_root for include paths to work in both packaged and repo modes.
    println!(
        "cargo:rerun-if-changed={}/include/ccap_c.h",
        ccap_root.display()
    );
    println!(
        "cargo:rerun-if-changed={}/include/ccap_utils_c.h",
        ccap_root.display()
    );
    println!(
        "cargo:rerun-if-changed={}/include/ccap_convert_c.h",
        ccap_root.display()
    );

    // If we're compiling from source, also re-run when the vendored/source files change.
    if build_from_source {
        println!(
            "cargo:rerun-if-changed={}/src/ccap_core.cpp",
            ccap_root.display()
        );
        println!(
            "cargo:rerun-if-changed={}/src/ccap_utils.cpp",
            ccap_root.display()
        );
        println!(
            "cargo:rerun-if-changed={}/src/ccap_convert.cpp",
            ccap_root.display()
        );
        println!(
            "cargo:rerun-if-changed={}/src/ccap_convert_frame.cpp",
            ccap_root.display()
        );
        println!(
            "cargo:rerun-if-changed={}/src/ccap_imp.cpp",
            ccap_root.display()
        );
        println!(
            "cargo:rerun-if-changed={}/src/ccap_c.cpp",
            ccap_root.display()
        );
        println!(
            "cargo:rerun-if-changed={}/src/ccap_utils_c.cpp",
            ccap_root.display()
        );
        println!(
            "cargo:rerun-if-changed={}/src/ccap_convert_c.cpp",
            ccap_root.display()
        );
    }

    // Generate bindings
    let bindings = bindgen::Builder::default()
        .header("wrapper.h")
        .clang_arg(format!("-I{}/include", ccap_root.display()))
        .parse_callbacks(Box::new(bindgen::CargoCallbacks))
        .allowlist_function("ccap_.*")
        .allowlist_type("Ccap.*")
        .allowlist_var("CCAP_.*")
        .derive_default(true)
        .derive_debug(true)
        .derive_partialeq(true)
        .derive_eq(true)
        .generate()
        .expect("Unable to generate bindings");

    // Write the bindings to the $OUT_DIR/bindings.rs file.
    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());
    bindings
        .write_to_file(out_path.join("bindings.rs"))
        .expect("Couldn't write bindings!");
}
