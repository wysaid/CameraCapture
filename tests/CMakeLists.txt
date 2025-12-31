cmake_minimum_required(VERSION 3.14)

# Google Test
include(FetchContent)

# Fix GoogleTest pthread issues on MinGW
# MinGW on Windows uses native threading, not pthread
# Setting this BEFORE FetchContent prevents GoogleTest from searching for Threads package
if(MINGW)
    set(gtest_disable_pthreads ON CACHE BOOL "" FORCE)
    message(STATUS "MinGW detected: Disabling pthread for GoogleTest")
endif()

FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG release-1.11.0
)

message(STATUS "Fetching Google Test archive...")

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Fix compile definitions for tests on MinGW
# GoogleTest sets GTEST_HAS_PTHREAD=1 by default on MinGW, which causes AutoHandle errors
if(MINGW)
    # Remove GTEST_HAS_PTHREAD=1 from all googletest targets and set it to 0
    foreach(target gtest gtest_main gmock gmock_main)
        if(TARGET ${target})
            # Get current compile definitions
            get_target_property(compile_defs ${target} INTERFACE_COMPILE_DEFINITIONS)
            if(compile_defs)
                # Create a new list without GTEST_HAS_PTHREAD
                set(new_defs "")
                foreach(def IN LISTS compile_defs)
                    if(NOT def MATCHES "GTEST_HAS_PTHREAD")
                        list(APPEND new_defs ${def})
                    endif()
                endforeach()
                # Add GTEST_HAS_PTHREAD=0
                list(APPEND new_defs "GTEST_HAS_PTHREAD=0")
                # Replace the property
                set_property(TARGET ${target} PROPERTY INTERFACE_COMPILE_DEFINITIONS ${new_defs})
            else()
                # If no definitions exist, just set GTEST_HAS_PTHREAD=0
                set_property(TARGET ${target} PROPERTY INTERFACE_COMPILE_DEFINITIONS "GTEST_HAS_PTHREAD=0")
            endif()
        endif()
    endforeach()
    message(STATUS "MinGW: Overridden GTEST_HAS_PTHREAD to 0 for test compilation")
endif()

if (NOT DEFINED LIBYUV_REPO_URL)
    if (DEFINED ENV{LIBYUV_REPO_URL})
        set(LIBYUV_REPO_URL $ENV{LIBYUV_REPO_URL})
    else ()
        set(LIBYUV_REPO_URL https://github.com/lemenkov/libyuv)
    endif ()
endif ()

message(STATUS "Fetching libYUV from repository: ${LIBYUV_REPO_URL}")

# LibYUV for performance comparison
FetchContent_Declare(
        libyuv
        GIT_REPOSITORY ${LIBYUV_REPO_URL}
        GIT_TAG b7d97d5f3f8f897b88872b6935e4c996b955bc1f
)

# Set libyuv options to minimize dependencies
set(UNIT_TEST OFF CACHE BOOL "" FORCE)
set(JPEG_FOUND OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(libyuv)

# Add UTF-8 encoding support for libyuv if using MSVC
if (MSVC)
    if (TARGET yuv)
        target_compile_options(yuv PRIVATE /utf-8 /wd4819)
    endif()
    if (TARGET yuv_internal)
        target_compile_options(yuv_internal PRIVATE /utf-8 /wd4819)
    endif()
    # Add for all object libraries as well
    get_property(libyuv_targets DIRECTORY ${libyuv_SOURCE_DIR} PROPERTY BUILDSYSTEM_TARGETS)
    foreach(target ${libyuv_targets})
        if (TARGET ${target})
            target_compile_options(${target} PRIVATE /utf-8 /wd4819)
        endif()
    endforeach()
endif()

# Common test utilities
add_library(ccap_test_utils
        test_utils.cpp
        test_utils.h
)

target_link_libraries(ccap_test_utils
        PUBLIC
        ccap
        gtest
        gtest_main
        gmock
        gmock_main
)

target_include_directories(ccap_test_utils PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Add MSVC specific compile options for test utils
if (MSVC)
    target_compile_options(ccap_test_utils PRIVATE
            /MP
            /std:c++17
            /Zc:__cplusplus
            /source-charset:utf-8
            /bigobj
            /wd4996
            /D_CRT_SECURE_NO_WARNINGS
    )
endif ()

# Test executables - Refactored and simplified
add_executable(
        ccap_convert_test
        test_accuracy.cpp
        test_color_conversions.cpp
        test_yuv_conversions.cpp
        test_platform_features.cpp
        test_frame_conversions.cpp
        test_boundary_conditions.cpp
        test_grab_timeout.cpp
)

target_link_libraries(
        ccap_convert_test
        ccap_test_utils
        gtest_main
)

# Add src directory to include path for internal API testing
target_include_directories(
        ccap_convert_test
        PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

# GUID definition verification test (Windows only)
# This test verifies that locally defined GUIDs match strmiids.lib values
if (WIN32)
    add_executable(
            ccap_guid_test
            test_guid_definitions.cpp
    )

    target_link_libraries(
            ccap_guid_test
            gtest
            gtest_main
            strmiids  # Link against strmiids.lib to get reference GUID values
            ole32     # For CoCreateInstance, CoInitializeEx
    )

    set_target_properties(ccap_guid_test PROPERTIES
            CXX_STANDARD 17
            CXX_STANDARD_REQUIRED ON
    )

    if (MSVC)
        target_compile_options(ccap_guid_test PRIVATE
                /MP
                /std:c++17
                /Zc:__cplusplus
                /source-charset:utf-8
                /bigobj
                /wd4996
                /D_CRT_SECURE_NO_WARNINGS
        )
    endif ()
endif ()

# Performance test executable - should be compiled in Release mode
add_executable(
        ccap_performance_test
        test_performance.cpp
)

target_link_libraries(
        ccap_performance_test
        ccap_test_utils
        gtest_main
        yuv
)

# Add LibYUV include directory for performance test
target_include_directories(ccap_performance_test PRIVATE ${libyuv_SOURCE_DIR}/include)

# CLI test executable - integration tests that invoke ccap-cli command
if (CCAP_BUILD_CLI)
    add_executable(
            ccap_cli_test
            test_ccap_cli.cpp
    )

    target_link_libraries(
            ccap_cli_test
            PRIVATE
            ccap_test_utils
            gtest
            gmock
    )
    # Test depends on ccap-cli being built
    add_dependencies(ccap_cli_test ccap-cli)

    set_target_properties(ccap_cli_test PROPERTIES
            CXX_STANDARD 17
            CXX_STANDARD_REQUIRED ON
    )

    target_compile_definitions(ccap_cli_test PRIVATE
            $<$<CONFIG:Debug>:DEBUG>
            $<$<CONFIG:Release>:NDEBUG>
            $<$<AND:$<NOT:$<BOOL:${MSVC}>>,$<NOT:$<BOOL:${MINGW}>>>:GTEST_HAS_PTHREAD=1>
    )

    if (MSVC)
        target_compile_options(ccap_cli_test PRIVATE
                /MP
                /std:c++17
                /Zc:__cplusplus
                /Zc:preprocessor
                /source-charset:utf-8
                /bigobj
                /wd4996
                /D_CRT_SECURE_NO_WARNINGS
        )
    else ()
        target_compile_options(ccap_cli_test PRIVATE
                -std=c++17
        )
    endif ()
endif ()

# File playback test executable - tests video file playback functionality
# Only build if file playback is enabled (Windows/macOS only)
if (CCAP_ENABLE_FILE_PLAYBACK)
    add_executable(
            ccap_file_playback_test
            test_file_playback.cpp
    )

    target_link_libraries(
            ccap_file_playback_test
            PRIVATE
            ccap_test_utils
            gtest
            gmock
    )

    set_target_properties(ccap_file_playback_test PROPERTIES
            CXX_STANDARD 17
            CXX_STANDARD_REQUIRED ON
    )

    target_compile_definitions(ccap_file_playback_test PRIVATE
            $<$<CONFIG:Debug>:DEBUG>
            $<$<CONFIG:Release>:NDEBUG>
            $<$<AND:$<NOT:$<BOOL:${MSVC}>>,$<NOT:$<BOOL:${MINGW}>>>:GTEST_HAS_PTHREAD=1>
    )

    if (MSVC)
        target_compile_options(ccap_file_playback_test PRIVATE
                /MP
                /std:c++17
                /Zc:__cplusplus
                /Zc:preprocessor
                /source-charset:utf-8
                /bigobj
                /wd4996
                /D_CRT_SECURE_NO_WARNINGS
        )
    else ()
        target_compile_options(ccap_file_playback_test PRIVATE
                -std=c++17
        )
    endif ()

    # Memory safety test executable - tests frame dropping behavior and memory limits
    add_executable(
            ccap_memory_safety_test
            test_memory_safety.cpp
    )

    target_link_libraries(
            ccap_memory_safety_test
            PRIVATE
            ccap_test_utils
            gtest
            gmock
    )

    set_target_properties(ccap_memory_safety_test PROPERTIES
            CXX_STANDARD 17
            CXX_STANDARD_REQUIRED ON
    )

    target_compile_definitions(ccap_memory_safety_test PRIVATE
            $<$<CONFIG:Debug>:DEBUG>
            $<$<CONFIG:Release>:NDEBUG>
            $<$<AND:$<NOT:$<BOOL:${MSVC}>>,$<NOT:$<BOOL:${MINGW}>>>:GTEST_HAS_PTHREAD=1>
    )

    if (MSVC)
        target_compile_options(ccap_memory_safety_test PRIVATE
                /MP
                /std:c++17
                /Zc:__cplusplus
                /Zc:preprocessor
                /source-charset:utf-8
                /bigobj
                /wd4996
                /D_CRT_SECURE_NO_WARNINGS
        )
    else ()
        target_compile_options(ccap_memory_safety_test PRIVATE
                -std=c++17
        )
    endif ()
    
    message(STATUS "ccap: File playback tests enabled")
else ()
    message(STATUS "ccap: File playback tests disabled (CCAP_ENABLE_FILE_PLAYBACK is OFF)")
endif ()

# Enable testing before any test registration
enable_testing()

# Register tests with CTest
include(GoogleTest)
# When cross-compiling (e.g., building ARM64 on x86_64), avoid discovering tests
# at configure/build time because it would attempt to execute the target binary
# for test listing on the host. We'll run tests explicitly (e.g., via QEMU) in CI.
if (NOT CMAKE_CROSSCOMPILING)
    # Defer discovery to test time to avoid executing the binary during configure
    gtest_discover_tests(ccap_convert_test DISCOVERY_MODE PRE_TEST)
    gtest_discover_tests(ccap_performance_test DISCOVERY_MODE PRE_TEST)
    if (WIN32)
        gtest_discover_tests(ccap_guid_test DISCOVERY_MODE PRE_TEST)
    endif ()
    if (CCAP_BUILD_CLI)
        gtest_discover_tests(ccap_cli_test DISCOVERY_MODE PRE_TEST)
    endif ()
    if (CCAP_ENABLE_FILE_PLAYBACK)
        gtest_discover_tests(ccap_file_playback_test DISCOVERY_MODE PRE_TEST)
        gtest_discover_tests(ccap_memory_safety_test DISCOVERY_MODE PRE_TEST)
    endif ()
else ()
    message(STATUS "CMAKE_CROSSCOMPILING is ON: skipping GoogleTest discovery at configure time.")
endif ()

# Set test properties
set_target_properties(ccap_convert_test PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
)

# Set performance test properties - optimize for Release builds
set_target_properties(ccap_performance_test PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
)

# Add compile definitions for tests
target_compile_definitions(ccap_convert_test PRIVATE
        $<$<CONFIG:Debug>:DEBUG> # Define DEBUG macro for Debug builds
        $<$<CONFIG:Release>:NDEBUG> # Define NDEBUG macro for Release builds
        $<$<AND:$<NOT:$<BOOL:${MSVC}>>,$<NOT:$<BOOL:${MINGW}>>>:GTEST_HAS_PTHREAD=1> # Enable pthread for non-MSVC/non-MinGW compilers
)

target_compile_definitions(ccap_performance_test PRIVATE
        $<$<CONFIG:Debug>:DEBUG> # Define DEBUG macro for Debug builds
        $<$<CONFIG:Release>:NDEBUG> # Define NDEBUG macro for Release builds
        $<$<AND:$<NOT:$<BOOL:${MSVC}>>,$<NOT:$<BOOL:${MINGW}>>>:GTEST_HAS_PTHREAD=1> # Enable pthread for non-MSVC/non-MinGW compilers
)

if (MSVC)
    target_compile_options(ccap_convert_test PRIVATE
            /MP
            /std:c++17
            /Zc:__cplusplus
            /Zc:preprocessor
            /source-charset:utf-8
            /bigobj # Allow big object files
            /wd4996 # Disable deprecated function warnings
            /D_CRT_SECURE_NO_WARNINGS
    )

    target_compile_options(ccap_performance_test PRIVATE
            /MP
            /std:c++17
            /Zc:__cplusplus
            /Zc:preprocessor
            /source-charset:utf-8
            /bigobj # Allow big object files
            /wd4996 # Disable deprecated function warnings
            /D_CRT_SECURE_NO_WARNINGS
    )

else ()
    target_compile_options(ccap_convert_test PRIVATE
            -std=c++17
    )
    target_compile_options(ccap_performance_test PRIVATE
            -std=c++17
            -O3
    )
endif ()
