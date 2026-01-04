use std::env;
use std::path::PathBuf;

fn main() {
    // Tell cargo to look for shared libraries in the specified directory
    let manifest_dir = env::var("CARGO_MANIFEST_DIR").unwrap();
    let manifest_path = PathBuf::from(&manifest_dir);

    // Locate ccap root:
    // 1. Check for local "native" directory (Packaged/Crates.io mode)
    // 2. Fallback to "../../" (Repo/Git mode)
    let (ccap_root, is_packaged) = if manifest_path.join("native").exists() {
        (manifest_path.join("native"), true)
    } else {
        (
            manifest_path
                .parent()
                .and_then(|p| p.parent())
                .expect("Cargo manifest must be at least 2 directories deep (bindings/rust/)")
                .to_path_buf(),
            false,
        )
    };

    // Check if we should build from source or link against pre-built library
    let build_from_source = env::var("CARGO_FEATURE_BUILD_SOURCE").is_ok();

    if build_from_source {
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

    // Tell cargo to invalidate the built crate whenever the wrapper changes
    println!("cargo:rerun-if-changed=wrapper.h");
    // Use ccap_root for include paths to work in both packaged and repo modes
    if !is_packaged {
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
