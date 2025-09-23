cmake_minimum_required(VERSION 3.18.1)

project(ccap_android_demo)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required libraries
find_library(log-lib log)
# Try to find camera2ndk library
find_library(camera2ndk-lib camera2ndk)
if(NOT camera2ndk-lib)
    message(WARNING "camera2ndk library not found, some camera features may not be available")
    # Set a placeholder to prevent CMake errors
    set(camera2ndk-lib "")
endif()
find_library(mediandk-lib mediandk)
find_library(jnigraphics-lib jnigraphics)

# Get the absolute path to CCAP project root
get_filename_component(CCAP_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../../" ABSOLUTE)

# Include directories
include_directories(
    ${CCAP_ROOT_DIR}/include
    ${CCAP_ROOT_DIR}/src
)

# CCAP source files  
set(CCAP_SOURCES
    ${CCAP_ROOT_DIR}/src/ccap_c.cpp
    ${CCAP_ROOT_DIR}/src/ccap_convert.cpp
    ${CCAP_ROOT_DIR}/src/ccap_convert_c.cpp
    ${CCAP_ROOT_DIR}/src/ccap_convert_frame.cpp
    ${CCAP_ROOT_DIR}/src/ccap_convert_neon.cpp
    ${CCAP_ROOT_DIR}/src/ccap_core.cpp
    ${CCAP_ROOT_DIR}/src/ccap_imp.cpp
    ${CCAP_ROOT_DIR}/src/ccap_imp_android.cpp
    ${CCAP_ROOT_DIR}/src/ccap_android_utils.cpp
    ${CCAP_ROOT_DIR}/src/ccap_utils.cpp
    ${CCAP_ROOT_DIR}/src/ccap_utils_c.cpp
)

# Demo JNI wrapper source
set(DEMO_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/android_demo_jni.cpp
)

# Create the shared library
add_library(ccap_android_demo SHARED 
    android_demo_jni.cpp
    ${CCAP_SOURCES}
)

# Link libraries
# Link with required libraries
target_link_libraries(ccap_android_demo
    ${log-lib}
    ${android-lib}
    mediandk
)

# Compiler definitions
target_compile_definitions(ccap_android_demo PRIVATE
    __ANDROID__=1
    CCAP_ANDROID=1
)

# Compiler flags
target_compile_options(ccap_android_demo PRIVATE
    -Wall
    -Wextra
    -O2
    -fvisibility=hidden
)

# Enable NEON optimizations for ARM
if(ANDROID_ABI STREQUAL "arm64-v8a" OR ANDROID_ABI STREQUAL "armeabi-v7a")
    target_compile_definitions(ccap_android_demo PRIVATE CCAP_NEON_ENABLED=1)
    if(ANDROID_ABI STREQUAL "armeabi-v7a")
        target_compile_options(ccap_android_demo PRIVATE -mfpu=neon)
    endif()
endif()

# Export symbols for JNI
set_target_properties(ccap_android_demo PROPERTIES
    LINK_FLAGS "-Wl,--export-dynamic"
)