# ccap-cli.cmake
# CMake configuration for ccap CLI tool

# Smart detection for GLFW on Linux: auto-disable if it can't be built
# Windows and macOS have built-in dependencies, so they don't need this check
if (UNIX AND NOT APPLE AND NOT DEFINED CCAP_CLI_WITH_GLFW)
    # User hasn't explicitly set CCAP_CLI_WITH_GLFW
    # Try to configure GLFW to see if Linux dependencies (Wayland/X11) are available
    message(STATUS "ccap CLI: Auto-detecting GLFW availability by attempting to configure it...")
    
    set(GLFW_DIR ${CMAKE_CURRENT_SOURCE_DIR}/examples/desktop/glfw)
    if (EXISTS ${GLFW_DIR}/CMakeLists.txt)
        # Save current policy settings
        cmake_policy(PUSH)
        
        # Prepare GLFW configuration
        set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
        set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
        
        # Try to add GLFW, suppress errors temporarily
        set(_GLFW_TEST_BUILD_DIR ${CMAKE_BINARY_DIR}/glfw_cli_test)
        
        # Use execute_process to test GLFW configuration in isolation
        execute_process(
            COMMAND ${CMAKE_COMMAND} ${GLFW_DIR}
                    -B ${_GLFW_TEST_BUILD_DIR}
                    -DGLFW_BUILD_EXAMPLES=OFF
                    -DGLFW_BUILD_TESTS=OFF
                    -DGLFW_BUILD_DOCS=OFF
                    -DGLFW_INSTALL=OFF
            RESULT_VARIABLE GLFW_CONFIG_RESULT
            OUTPUT_QUIET
            ERROR_QUIET
        )
        
        cmake_policy(POP)
        
        # Clean up test build
        file(REMOVE_RECURSE ${_GLFW_TEST_BUILD_DIR})
        
        if (GLFW_CONFIG_RESULT EQUAL 0)
            set(CCAP_CLI_WITH_GLFW ON CACHE BOOL "Enable GLFW window preview for CLI tool" FORCE)
            message(STATUS "ccap CLI: GLFW configured successfully, enabling preview support")
        else()
            set(CCAP_CLI_WITH_GLFW OFF CACHE BOOL "Enable GLFW window preview for CLI tool" FORCE)
            message(STATUS "ccap CLI: GLFW configuration failed, disabling preview support")
            message(STATUS "ccap CLI: Install X11: libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev")
            message(STATUS "ccap CLI: Or Wayland: libwayland-dev libxkbcommon-dev wayland-protocols")
        endif()
    else()
        set(CCAP_CLI_WITH_GLFW OFF CACHE BOOL "Enable GLFW window preview for CLI tool" FORCE)
        message(STATUS "ccap CLI: GLFW source not found, disabling preview support")
    endif()
endif()

# CLI-specific options (only available when building CLI)
option(CCAP_CLI_WITH_GLFW "Enable GLFW window preview for CLI tool" ON)
option(CCAP_CLI_WITH_STB_IMAGE "Enable stb_image_write for JPG/PNG support in CLI tool" ON)

message(STATUS "ccap CLI: CCAP_CLI_WITH_GLFW=${CCAP_CLI_WITH_GLFW}")
message(STATUS "ccap CLI: CCAP_CLI_WITH_STB_IMAGE=${CCAP_CLI_WITH_STB_IMAGE}")

set(CLI_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/cli)

# CLI source files
set(CLI_SOURCES
        ${CLI_SOURCE_DIR}/ccap_cli.cpp
)

# Create CLI executable
add_executable(ccap-cli ${CLI_SOURCES})

# Set output name to 'ccap' for the CLI tool
set_target_properties(ccap-cli PROPERTIES
        OUTPUT_NAME "ccap"
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
)

# Configure static runtime linking for MSVC (use /MT instead of /MD)
# This ensures the CLI tool doesn't require MSVC runtime DLLs to be installed
# Only applied when CCAP_BUILD_CLI_STANDALONE is enabled
if (CCAP_BUILD_CLI_STANDALONE AND MSVC)
    include(cmake/StaticRuntime.cmake)
    ccap_configure_static_msvc_runtime(ccap-cli)
    message(STATUS "ccap CLI: Using static MSVC runtime (/MT)")
endif ()

# Configure static linking for Linux to minimize runtime dependencies
# Only applied when CCAP_BUILD_CLI_STANDALONE is enabled
if (CCAP_BUILD_CLI_STANDALONE AND UNIX AND NOT APPLE)
    # Check if static libstdc++ is available
    include(CheckCXXSourceCompiles)
    set(CMAKE_REQUIRED_FLAGS "-static-libstdc++ -static-libgcc")
    check_cxx_source_compiles(
            "int main() { return 0; }"
            CCAP_HAS_STATIC_LIBSTDCXX
    )
    unset(CMAKE_REQUIRED_FLAGS)
    
    if (CCAP_HAS_STATIC_LIBSTDCXX)
        target_link_options(ccap-cli PRIVATE
                -static-libgcc
                -static-libstdc++
        )
        message(STATUS "ccap CLI: Using static libstdc++ and libgcc")
    else ()
        message(STATUS "ccap CLI: Static libraries not available, using dynamic linking")
    endif ()
endif ()

# Link against ccap library
target_link_libraries(ccap-cli PRIVATE ccap)

# Include directories
target_include_directories(ccap-cli PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CLI_SOURCE_DIR}
)

# stb_image_write setup for JPG/PNG support
if (CCAP_CLI_WITH_STB_IMAGE)
    target_compile_definitions(ccap-cli PRIVATE CCAP_CLI_WITH_STB_IMAGE=1)
    # Use stb_image_write.h from examples/desktop/glfw/deps directory
    target_include_directories(ccap-cli PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/examples/desktop/glfw/deps
    )
    message(STATUS "ccap CLI: stb_image_write enabled (JPG/PNG support)")
else ()
    message(STATUS "ccap CLI: stb_image_write disabled (BMP only)")
endif ()

# Compile definitions
target_compile_definitions(ccap-cli PRIVATE
        _CRT_SECURE_NO_WARNINGS=1
)

# Detect libc type (glibc vs musl)
if (UNIX AND NOT APPLE)
    # Try to detect musl vs glibc
    execute_process(
        COMMAND ${CMAKE_C_COMPILER} -dumpmachine
        OUTPUT_VARIABLE TARGET_TRIPLE
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if (TARGET_TRIPLE MATCHES "musl")
        set(CCAP_CLI_LIBC_TYPE "musl")
    else()
        # Check for glibc by trying to compile a test program
        file(WRITE ${CMAKE_BINARY_DIR}/check_glibc.c "#include <features.h>\n#ifndef __GLIBC__\n#error not glibc\n#endif\nint main(){return 0;}\n")
        try_compile(HAS_GLIBC
            ${CMAKE_BINARY_DIR}
            SOURCES ${CMAKE_BINARY_DIR}/check_glibc.c
        )
        if (HAS_GLIBC)
            set(CCAP_CLI_LIBC_TYPE "glibc")
        else()
            set(CCAP_CLI_LIBC_TYPE "unknown")
        endif()
    endif()
    message(STATUS "ccap CLI: Detected libc type: ${CCAP_CLI_LIBC_TYPE}")
endif()

# Add build information for --version output
target_compile_definitions(ccap-cli PRIVATE
        CCAP_CLI_BUILD_PLATFORM="${CMAKE_SYSTEM_NAME}"
        CCAP_CLI_BUILD_ARCH="${CMAKE_SYSTEM_PROCESSOR}"
        CCAP_CLI_BUILD_COMPILER="${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}"
        CCAP_CLI_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
)

# Add libc type for Linux builds
if (UNIX AND NOT APPLE AND DEFINED CCAP_CLI_LIBC_TYPE)
    target_compile_definitions(ccap-cli PRIVATE
        CCAP_CLI_LIBC_TYPE="${CCAP_CLI_LIBC_TYPE}"
    )
endif()

# Add static linking information
if (CCAP_BUILD_CLI_STANDALONE)
    if (MSVC)
        target_compile_definitions(ccap-cli PRIVATE CCAP_CLI_STATIC_RUNTIME=1)
    elseif (UNIX AND NOT APPLE)
        if (CCAP_HAS_STATIC_LIBSTDCXX)
            target_compile_definitions(ccap-cli PRIVATE CCAP_CLI_STATIC_RUNTIME=1)
        else ()
            target_compile_definitions(ccap-cli PRIVATE CCAP_CLI_STATIC_RUNTIME=0)
        endif ()
    endif ()
else ()
    target_compile_definitions(ccap-cli PRIVATE CCAP_CLI_STATIC_RUNTIME=0)
endif ()

# Add STB_IMAGE compilation info
if (CCAP_CLI_WITH_STB_IMAGE)
    target_compile_definitions(ccap-cli PRIVATE CCAP_CLI_WITH_STB_IMAGE_INFO=1)
else ()
    target_compile_definitions(ccap-cli PRIVATE CCAP_CLI_WITH_STB_IMAGE_INFO=0)
endif ()

# Optimization: Enable LTO (Link Time Optimization) for Release builds
if (CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
    
    if (ipo_supported)
        set_target_properties(ccap-cli PROPERTIES
            INTERPROCEDURAL_OPTIMIZATION TRUE
        )
        message(STATUS "ccap CLI: LTO/IPO enabled for ${CMAKE_BUILD_TYPE} build")
    else ()
        message(STATUS "ccap CLI: LTO/IPO not supported: ${ipo_error}")
    endif ()
endif ()

# Optimization: Strip symbols in Release builds (non-MSVC)
if (NOT MSVC)
    if (CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
        target_link_options(ccap-cli PRIVATE -s)
        message(STATUS "ccap CLI: Symbol stripping enabled for ${CMAKE_BUILD_TYPE} build")
    endif ()
endif ()

# GLFW setup for preview functionality
if (CCAP_CLI_WITH_GLFW)
    set(GLFW_CLI_AVAILABLE OFF)

    # Check if GLFW target is already available (from examples or parent scope)
    if (TARGET glfw)
        set(GLFW_CLI_AVAILABLE ON)
        set(GLFW_CLI_TARGET glfw)
        message(STATUS "ccap CLI: Using bundled GLFW from examples")
    else ()
        # GLFW not available, build from bundled source
        # Optimize GLFW build: disable unnecessary features to reduce binary size
        set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
        set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
        
        # Use bundled GLFW from examples
        set(GLFW_DIR ${CMAKE_CURRENT_SOURCE_DIR}/examples/desktop/glfw)
        if (EXISTS ${GLFW_DIR}/CMakeLists.txt)
            add_subdirectory(${GLFW_DIR} ${CMAKE_BINARY_DIR}/glfw_cli EXCLUDE_FROM_ALL)
            set(GLFW_CLI_AVAILABLE ON)
            set(GLFW_CLI_TARGET glfw)
            message(STATUS "ccap CLI: Built bundled GLFW from source")
        else ()
            message(WARNING "ccap CLI: GLFW source not found. Preview disabled.")
        endif ()
    endif ()

    if (GLFW_CLI_AVAILABLE)
        target_compile_definitions(ccap-cli PRIVATE CCAP_CLI_WITH_GLFW=1)
        target_link_libraries(ccap-cli PRIVATE ${GLFW_CLI_TARGET})

        # Include GLAD and GLFW headers
        target_include_directories(ccap-cli PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/examples/desktop/glad
                ${CMAKE_CURRENT_SOURCE_DIR}/examples/desktop/glfw/include
                ${CMAKE_CURRENT_SOURCE_DIR}/examples/desktop
        )

        # Platform-specific OpenGL linking
        if (APPLE)
            target_link_libraries(ccap-cli PRIVATE "-framework OpenGL")
        elseif (WIN32)
            target_link_libraries(ccap-cli PRIVATE opengl32)
        endif ()

        message(STATUS "ccap CLI: GLFW preview enabled")
    else ()
        message(STATUS "ccap CLI: GLFW preview disabled")
    endif ()
endif ()

# MSVC-specific settings
if (MSVC)
    target_compile_options(ccap-cli PRIVATE
            /MP
            /Zc:__cplusplus
            /Zc:preprocessor
            /source-charset:utf-8
            /wd4996
    )
endif ()

# Installation
if (CCAP_INSTALL)
    install(TARGETS ccap-cli
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif ()
