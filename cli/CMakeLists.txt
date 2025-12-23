# cli/CMakeLists.txt
# CMake configuration for ccap CLI tool
cmake_minimum_required(VERSION 3.14)

# CLI-specific options (only available when building CLI)
option(CCAP_CLI_WITH_GLFW "Enable GLFW window preview for CLI tool" ON)
option(CCAP_CLI_WITH_WUFFS "Enable wuffs library for additional image format support" OFF)

message(STATUS "ccap CLI: CCAP_CLI_WITH_GLFW=${CCAP_CLI_WITH_GLFW}")
message(STATUS "ccap CLI: CCAP_CLI_WITH_WUFFS=${CCAP_CLI_WITH_WUFFS}")

set(CLI_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# CLI source files
set(CLI_SOURCES
    ${CLI_SOURCE_DIR}/ccap_cli.cpp
)

# Add wuffs source if enabled
if(CCAP_CLI_WITH_WUFFS)
    list(APPEND CLI_SOURCES ${CLI_SOURCE_DIR}/ccap_cli_wuffs.cpp)
endif()

# Create CLI executable
add_executable(ccap_tool ${CLI_SOURCES})

# Set output name to 'ccap' for the CLI tool
set_target_properties(ccap_tool PROPERTIES
    OUTPUT_NAME "ccap"
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Link against ccap library
target_link_libraries(ccap_tool PRIVATE ccap)

# Include directories
target_include_directories(ccap_tool PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CLI_SOURCE_DIR}
)

# Compile definitions
target_compile_definitions(ccap_tool PRIVATE
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
            set(GLFW_DIR ${CMAKE_SOURCE_DIR}/examples/desktop/glfw)
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
        target_compile_definitions(ccap_tool PRIVATE CCAP_CLI_WITH_GLFW=1)
        target_link_libraries(ccap_tool PRIVATE ${GLFW_CLI_TARGET})

        # Include GLAD and GLFW headers
        target_include_directories(ccap_tool PRIVATE
            ${CMAKE_SOURCE_DIR}/examples/desktop/glad
            ${CMAKE_SOURCE_DIR}/examples/desktop/glfw/include
            ${CMAKE_SOURCE_DIR}/examples/desktop
        )

        # Platform-specific OpenGL linking
        if(APPLE)
            target_link_libraries(ccap_tool PRIVATE "-framework OpenGL")
        elseif(WIN32)
            target_link_libraries(ccap_tool PRIVATE opengl32)
        endif()

        message(STATUS "ccap CLI: GLFW preview enabled")
    else()
        message(STATUS "ccap CLI: GLFW preview disabled")
    endif()
endif()

# Wuffs setup for additional image format support
if(CCAP_CLI_WITH_WUFFS)
    include(FetchContent)

    message(STATUS "ccap CLI: Fetching wuffs library...")

    FetchContent_Declare(
        wuffs
        GIT_REPOSITORY https://github.com/google/wuffs.git
        GIT_TAG v0.4.0-alpha.8
        EXCLUDE_FROM_ALL
    )

    FetchContent_MakeAvailable(wuffs)

    target_compile_definitions(ccap_tool PRIVATE CCAP_CLI_WITH_WUFFS=1)
    target_include_directories(ccap_tool PRIVATE ${wuffs_SOURCE_DIR}/release/c)

    message(STATUS "ccap CLI: wuffs library enabled for additional image formats")
endif()

# MSVC-specific settings
if(MSVC)
    target_compile_options(ccap_tool PRIVATE
        /MP
        /std:c++17
        /Zc:__cplusplus
        /Zc:preprocessor
        /source-charset:utf-8
        /wd4996
    )
else()
    target_compile_options(ccap_tool PRIVATE
        -std=c++17
    )
endif()

# Installation
if(CCAP_INSTALL)
    install(TARGETS ccap_tool
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()
