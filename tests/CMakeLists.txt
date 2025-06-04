cmake_minimum_required(VERSION 3.14)

# Google Test
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP ON
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Common test utilities
add_library(ccap_test_utils
    test_utils.cpp
    test_utils.h
    test_utils_avx2.h
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

# Test executables
add_executable(
    ccap_convert_test
    test_convert_yuv_rgb.cpp
    test_convert_color_shuffle.cpp
    test_convert_yuv_format.cpp
    test_convert_comprehensive.cpp
    test_convert_platform_features.cpp
    test_dual_implementation.cpp
    test_accuracy.cpp
    test_debug_simple.cpp
)

# Performance benchmarks (separate executable)
add_executable(
    ccap_performance_test
    test_performance.cpp
)

# Debug test executable
add_executable(
    ccap_debug_detailed_test
    test_debug_detailed.cpp
)

# Debug shuffle map test
add_executable(
    ccap_debug_shuffle_test
    test_debug_shuffle_map.cpp
)

target_link_libraries(
    ccap_convert_test
    ccap_test_utils
    gtest_main
)

target_link_libraries(
    ccap_performance_test
    ccap_test_utils
    gtest_main
)

target_link_libraries(
    ccap_debug_detailed_test
    ccap_test_utils
    gtest_main
)

target_link_libraries(
    ccap_debug_shuffle_test
    ccap_test_utils
    gtest_main
)

# Register tests with CTest
include(GoogleTest)
gtest_discover_tests(ccap_convert_test)
gtest_discover_tests(ccap_performance_test)

# Set test properties
set_target_properties(ccap_convert_test PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

set_target_properties(ccap_performance_test PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Add compile definitions for tests
target_compile_definitions(ccap_convert_test PRIVATE
    GTEST_HAS_PTHREAD=1
)

target_compile_definitions(ccap_performance_test PRIVATE
    GTEST_HAS_PTHREAD=1
    CCAP_ENABLE_PERFORMANCE_TESTS=1
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
    )
else()
    target_compile_options(ccap_convert_test PRIVATE -std=c++17)
endif()
