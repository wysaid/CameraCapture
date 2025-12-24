# StaticRuntime.cmake
# CMake module for configuring static MSVC runtime linking
#
# PURPOSE:
#   Configures executables to use static MSVC runtime (/MT flag) instead of
#   dynamic runtime (/MD), eliminating the need for VCRUNTIME DLL distribution.
#
# WHEN TO USE:
#   - Building standalone executables for end-user distribution
#   - When you want to eliminate Visual C++ Redistributable dependencies
#   - For CLI tools, utilities, or any self-contained binaries
#
# WHEN NOT TO USE:
#   - When building shared libraries (DLLs) that will be used by other projects
#   - In projects that need to share CRT state across DLL boundaries
#   - When targeting environments with pre-installed VC runtime
#
# IMPORTANT NOTES:
#   - Only affects the target executable, not its dependencies
#   - All static libraries linked to the target should also use /MT for consistency
#   - Mixing /MT and /MD can cause linker warnings or runtime issues
#   - Static linking increases executable size but eliminates runtime dependencies
#
# Function to configure static MSVC runtime for a target
# This eliminates the need for VCRUNTIME DLL when distributing binaries
#
# Usage:
#   ccap_configure_static_msvc_runtime(target_name)
#
# Parameters:
#   target_name - The CMake target to configure (must be an executable or library)
#
function(ccap_configure_static_msvc_runtime target_name)
    if (NOT MSVC)
        return()
    endif ()

    # For CMake 3.15+, use CMAKE_MSVC_RUNTIME_LIBRARY property
    if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.15")
        set_target_properties(${target_name} PROPERTIES
                MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        message(STATUS "ccap: Using static MSVC runtime (/MT) for ${target_name}")
    else ()
        # For older CMake versions (< 3.15), add /MT or /MTd flags directly to the target
        # MSVC uses the last specified runtime flag, so these options override any
        # existing /MD or /MDd flags that might be present in global C/CXX flags
        target_compile_options(${target_name} PRIVATE
                "$<$<CONFIG:Debug>:/MTd>"
                "$<$<NOT:$<CONFIG:Debug>>:/MT>"
        )
        message(STATUS "ccap: Using static MSVC runtime (/MT) via target compile options for ${target_name}")
    endif ()
endfunction()
