cmake_minimum_required(VERSION 3.14)

option(CCAP_NO_LOG "Disable logging" OFF)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(CCAP_IS_ROOT_PROJECT ON)
else()
    set(CCAP_IS_ROOT_PROJECT OFF)
endif()

option(CCAP_BUILD_EXAMPLES "Build ccap examples" ${CCAP_IS_ROOT_PROJECT})
option(CCAP_BUILD_TESTS "Build ccap unit tests" OFF)

if(CCAP_IS_ROOT_PROJECT)
    set(CMAKE_FETCHCONTENT_BASE_DIR "${CMAKE_SOURCE_DIR}/build" CACHE PATH "FetchContent base dir" FORCE)
endif()

project(ccap LANGUAGES CXX)

# Currently only Windows and macOS are supported; other platforms are not supported yet
if(NOT WIN32 AND NOT APPLE)
    message(FATAL_ERROR "ccap: Unsupported platform, only Windows and macOS are supported by now.")
endif()

if(NOT DEFINED CMAKE_BUILD_TYPE AND NOT MSVC)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
    message(STATUS "CMAKE_BUILD_TYPE not set, defaulting to Release")
elseif(NOT MSVC)
    message(STATUS "CMAKE_BUILD_TYPE is set to ${CMAKE_BUILD_TYPE}")
endif()

if(CCAP_IS_ROOT_PROJECT AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/dev.cmake)
    include(${CMAKE_CURRENT_SOURCE_DIR}/dev.cmake)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum OS X deployment version")

if(NOT MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
endif()

file(GLOB LIB_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h)

if(APPLE)
    file(GLOB LIB_SOURCE_MAC ${CMAKE_CURRENT_SOURCE_DIR}/src/*.mm)
    message(STATUS "ccap: Using Objective-C++ for macOS: ${LIB_SOURCE_MAC}")
    list(APPEND LIB_SOURCE ${LIB_SOURCE_MAC})
endif()

include(CheckCXXCompilerFlag)

# 检查是否支持AVX2
if(MSVC)
    check_cxx_compiler_flag("/arch:AVX2" CCAP_SUPPORTS_AVX2)
else()
    check_cxx_compiler_flag("-mavx2" CCAP_SUPPORTS_AVX2)
endif()

if(CCAP_SUPPORTS_AVX2)
    set(CCAP_CONVERT_AVX2_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/ccap_convert_avx2.cpp)
    set_source_files_properties(${CCAP_CONVERT_AVX2_SRC} PROPERTIES COMPILE_OPTIONS "$<$<CXX_COMPILER_ID:MSVC>:/arch:AVX2>$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-mavx2>")
    list(APPEND LIB_SOURCE ${CCAP_CONVERT_AVX2_SRC})
endif()

add_library(ccap ${LIB_SOURCE})
target_include_directories(ccap PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_definitions(ccap PUBLIC
    $<$<NOT:$<CONFIG:Debug>>:NDEBUG=1>
    $<$<CONFIG:Debug>:DEBUG=1>
    $<$<CONFIG:Release>:NDEBUG=1>
    $<$<CONFIG:RelWithDebInfo>:NDEBUG=1>
    $<$<CONFIG:MinSizeRel>:NDEBUG=1>
)

if(CCAP_NO_LOG)
    target_compile_definitions(ccap PUBLIC CCAP_NO_LOG=1)
    message(STATUS "ccap: Disable logging")
endif()

if(APPLE)
    target_compile_options(ccap PUBLIC -fobjc-arc)
    target_link_libraries(ccap
        "-framework Foundation"
        "-framework AVFoundation"
        "-framework CoreVideo"
        "-framework CoreMedia"
        "-framework Accelerate")
endif()

if(MSVC)
    target_compile_options(ccap PUBLIC
        /MP
        /std:c++17
        /Zc:__cplusplus
        /Zc:preprocessor
    )

    if(CCAP_IS_ROOT_PROJECT)
        target_compile_options(ccap PUBLIC /source-charset:utf-8)
    else()
        target_compile_options(ccap PRIVATE /source-charset:utf-8)
    endif()

else()
    target_compile_options(ccap PUBLIC -std=c++17)
endif()

# ############### Examples ################
message(STATUS "ccap: CCAP_BUILD_EXAMPLES=${CCAP_BUILD_EXAMPLES}")

if(CCAP_BUILD_EXAMPLES)
    include(examples/desktop.cmake)
endif()

# ############### Tests ################
message(STATUS "ccap: CCAP_BUILD_TESTS=${CCAP_BUILD_TESTS}")

if(CCAP_BUILD_TESTS)
    target_compile_definitions(ccap PUBLIC CCAP_BUILD_TESTS=1)
    enable_testing()
    add_subdirectory(tests)
endif()