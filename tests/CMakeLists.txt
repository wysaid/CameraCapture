cmake_minimum_required(VERSION 3.14)

# Google Test
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/release-1.11.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP ON
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

if(NOT DEFINED LIBYUV_REPO_URL)
    if(DEFINED ENV{LIBYUV_REPO_URL})
        set(LIBYUV_REPO_URL $ENV{LIBYUV_REPO_URL})
    else()
        set(LIBYUV_REPO_URL https://github.com/lemenkov/libyuv)
    endif()
endif()

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
if(MSVC)
    target_compile_options(ccap_test_utils PRIVATE
        /MP
        /std:c++17
        /Zc:__cplusplus
        /bigobj
        /wd4996
        /D_CRT_SECURE_NO_WARNINGS
    )
endif()

# Test executables - Refactored and simplified
add_executable(
    ccap_convert_test
    test_accuracy.cpp
    test_color_conversions.cpp
    test_yuv_conversions.cpp
    test_platform_features.cpp
)

target_link_libraries(
    ccap_convert_test
    ccap_test_utils
    gtest_main
)

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

# Register tests with CTest
include(GoogleTest)
gtest_discover_tests(ccap_convert_test)
gtest_discover_tests(ccap_performance_test)

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
    $<$<NOT:$<BOOL:${MSVC}>>:GTEST_HAS_PTHREAD=1> # Enable pthread for non-MSVC compilers
)

target_compile_definitions(ccap_performance_test PRIVATE
    $<$<CONFIG:Debug>:DEBUG> # Define DEBUG macro for Debug builds
    $<$<CONFIG:Release>:NDEBUG> # Define NDEBUG macro for Release builds
    $<$<NOT:$<BOOL:${MSVC}>>:GTEST_HAS_PTHREAD=1> # Enable pthread for non-MSVC compilers
)

# Enable testing
enable_testing()

if(MSVC)
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

else()
    target_compile_options(ccap_convert_test PRIVATE
        -std=c++17
    )
    target_compile_options(ccap_performance_test PRIVATE
        -std=c++17
        -O3
    )
endif()
