# Android CMake build configuration for ccap library
# This file is designed to be used from a build directory

cmake_minimum_required(VERSION 3.18.1)

project(ccap_android)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set source directory
set(CCAP_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../src)
set(CCAP_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../include)

# Find the Android log library
find_library(log-lib log)

# Find Android camera2ndk library (API 24+)
find_library(camera2ndk-lib camera2ndk)

# Find Android mediandk library (API 21+)
find_library(mediandk-lib mediandk)

# Fallback: for older API levels or if libraries are not found
if(NOT camera2ndk-lib)
    message(WARNING "camera2ndk library not found, some camera features may not be available")
    set(camera2ndk-lib "")
endif()

if(NOT mediandk-lib)
    message(WARNING "mediandk library not found, some media features may not be available")
    set(mediandk-lib "")
endif()

# Include directories
include_directories(
    ${CCAP_INCLUDE_DIR}
    ${CCAP_SRC_DIR}
)

# Source files for Android
set(CCAP_ANDROID_SOURCES
    ${CCAP_SRC_DIR}/ccap_c.cpp
    ${CCAP_SRC_DIR}/ccap_convert.cpp
    ${CCAP_SRC_DIR}/ccap_convert_c.cpp
    ${CCAP_SRC_DIR}/ccap_convert_frame.cpp
    ${CCAP_SRC_DIR}/ccap_convert_neon.cpp
    ${CCAP_SRC_DIR}/ccap_core.cpp
    ${CCAP_SRC_DIR}/ccap_imp.cpp
    ${CCAP_SRC_DIR}/ccap_imp_android.cpp  # Android-specific implementation
    ${CCAP_SRC_DIR}/ccap_utils.cpp
    ${CCAP_SRC_DIR}/ccap_utils_c.cpp
    ${CCAP_SRC_DIR}/ccap_android_utils.cpp
)

# Create the library
add_library(ccap_android SHARED ${CCAP_ANDROID_SOURCES})

# Link libraries
target_link_libraries(ccap_android
    ${log-lib}
    android
    jnigraphics
)

# Add camera2ndk and mediandk if available
if(camera2ndk-lib)
    target_link_libraries(ccap_android ${camera2ndk-lib})
    target_compile_definitions(ccap_android PRIVATE CCAP_HAS_CAMERA2NDK=1)
endif()

if(mediandk-lib)
    target_link_libraries(ccap_android ${mediandk-lib})
    target_compile_definitions(ccap_android PRIVATE CCAP_HAS_MEDIANDK=1)
endif()

# Compiler definitions for Android
target_compile_definitions(ccap_android PRIVATE
    __ANDROID__=1
    CCAP_ANDROID=1
)

# Compiler flags
target_compile_options(ccap_android PRIVATE
    -Wall
    -Wextra
    -O2
    -fvisibility=hidden
)

# Enable NEON optimizations for ARM
if(ANDROID_ABI STREQUAL "arm64-v8a" OR ANDROID_ABI STREQUAL "armeabi-v7a")
    target_compile_definitions(ccap_android PRIVATE CCAP_NEON_ENABLED=1)
    if(ANDROID_ABI STREQUAL "armeabi-v7a")
        target_compile_options(ccap_android PRIVATE -mfpu=neon)
    endif()
endif()

# Export native symbols for JNI
set_target_properties(ccap_android PROPERTIES
    LINK_FLAGS "-Wl,--export-dynamic"
)