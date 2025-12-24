# ccap-cli.cmake
# CMake configuration for ccap CLI tool

# CLI-specific options (only available when building CLI)
option(CCAP_CLI_WITH_GLFW "Enable GLFW window preview for CLI tool" ON)

message(STATUS "ccap CLI: CCAP_CLI_WITH_GLFW=${CCAP_CLI_WITH_GLFW}")

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
if(MSVC)
    include(cmake/StaticRuntime.cmake)
    ccap_configure_static_msvc_runtime(ccap-cli)
endif()

# Configure static linking for Linux to minimize runtime dependencies
# Link libstdc++ and libgcc statically to avoid version conflicts
# Note: This requires static libraries to be installed (e.g., libstdc++-static on Fedora)
if(UNIX AND NOT APPLE)
    # Check if static libstdc++ is available by testing if we can link with static flags
    include(CheckCXXSourceCompiles)
    set(CMAKE_REQUIRED_FLAGS "-static-libstdc++ -static-libgcc")
    check_cxx_source_compiles(
        "int main() { return 0; }"
        CCAP_HAS_STATIC_LIBSTDCXX
    )
    unset(CMAKE_REQUIRED_FLAGS)
    
    if(CCAP_HAS_STATIC_LIBSTDCXX)
        target_link_options(ccap-cli PRIVATE 
            -static-libgcc 
            -static-libstdc++
        )
        message(STATUS "ccap CLI: Using static libstdc++ and libgcc on Linux")
    else()
        message(STATUS "ccap CLI: Static libstdc++ not available, using dynamic linking")
        message(STATUS "ccap CLI: To enable static linking, install libstdc++-static (Fedora/RHEL) or libstdc++-dev (Debian/Ubuntu)")
    endif()
endif()

# Note: macOS uses static linking for C++ standard library by default
# The CLI only depends on system frameworks which are always available

# Link against ccap library
target_link_libraries(ccap-cli PRIVATE ccap)

# Include directories
target_include_directories(ccap-cli PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CLI_SOURCE_DIR}
)

# Compile definitions
target_compile_definitions(ccap-cli PRIVATE
    _CRT_SECURE_NO_WARNINGS=1
)

# GLFW setup for preview functionality
if(CCAP_CLI_WITH_GLFW)
    set(GLFW_CLI_AVAILABLE OFF)

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        # Try to find system GLFW on Linux
        find_package(glfw3 QUIET)
        if(glfw3_FOUND)
            set(GLFW_CLI_AVAILABLE ON)
            set(GLFW_CLI_TARGET glfw)
            message(STATUS "ccap CLI: Using system GLFW")
        else()
            message(STATUS "ccap CLI: System GLFW not found. Install with: sudo apt-get install libglfw3-dev")
            message(STATUS "ccap CLI: GLFW preview will be disabled")
        endif()
    else()
        # On non-Linux platforms, check if GLFW is already available from examples
        if(TARGET glfw)
            set(GLFW_CLI_AVAILABLE ON)
            set(GLFW_CLI_TARGET glfw)
            message(STATUS "ccap CLI: Using GLFW from examples")
        else()
            # Fetch GLFW ourselves
            set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
            set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
            set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
            set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

            # Use bundled GLFW from examples
            set(GLFW_DIR ${CMAKE_CURRENT_SOURCE_DIR}/examples/desktop/glfw)
            if(EXISTS ${GLFW_DIR}/CMakeLists.txt)
                add_subdirectory(${GLFW_DIR} ${CMAKE_BINARY_DIR}/glfw_cli EXCLUDE_FROM_ALL)
                set(GLFW_CLI_AVAILABLE ON)
                set(GLFW_CLI_TARGET glfw)
                message(STATUS "ccap CLI: Using bundled GLFW from examples")
            else()
                message(STATUS "ccap CLI: GLFW not found. Preview disabled.")
            endif()
        endif()
    endif()

    if(GLFW_CLI_AVAILABLE)
        target_compile_definitions(ccap-cli PRIVATE CCAP_CLI_WITH_GLFW=1)
        target_link_libraries(ccap-cli PRIVATE ${GLFW_CLI_TARGET})

        # Include GLAD and GLFW headers
        target_include_directories(ccap-cli PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/desktop/glad
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/desktop/glfw/include
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/desktop
        )

        # Platform-specific OpenGL linking
        if(APPLE)
            target_link_libraries(ccap-cli PRIVATE "-framework OpenGL")
        elseif(WIN32)
            target_link_libraries(ccap-cli PRIVATE opengl32)
        endif()

        message(STATUS "ccap CLI: GLFW preview enabled")
    else()
        message(STATUS "ccap CLI: GLFW preview disabled")
    endif()
endif()

# MSVC-specific settings
if(MSVC)
    target_compile_options(ccap-cli PRIVATE
        /MP
        /Zc:__cplusplus
        /Zc:preprocessor
        /source-charset:utf-8
        /wd4996
    )
endif()

# Installation
if(CCAP_INSTALL)
    install(TARGETS ccap-cli
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()
