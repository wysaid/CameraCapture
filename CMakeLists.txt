cmake_minimum_required(VERSION 3.10)

option(CCAP_NO_LOG "Disable logging" OFF)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(CCAP_IS_ROOT_PROJECT ON)
else()
    set(CCAP_IS_ROOT_PROJECT OFF)
endif()

option(CCAP_BUILD_DEMO "Build ccap demo" ${CCAP_IS_ROOT_PROJECT})

if(WIN32)
    option(ENABLE_LIBYUV "Enable libyuv" ${CCAP_IS_ROOT_PROJECT})
endif()

project(CameraCapture LANGUAGES CXX)

if(WIN32 AND ENABLE_LIBYUV)
    if(NOT DEFINED LIBYUV_DIR OR NOT EXISTS "${LIBYUV_DIR}/README.md")
        message(STATUS "ccap: Fetching libyuv from GitHub...")
        include(${CMAKE_CURRENT_SOURCE_DIR}/deps/libyuv.cmake)
    endif()

    if(ENABLE_LIBYUV)
        set(UNIT_TEST OFF CACHE BOOL "Disable libyuv unit tests" FORCE)
        add_subdirectory(${LIBYUV_DIR})
        set(YUV_INCLUDE_DIRS ${LIBYUV_DIR}/include)
        message(STATUS "ccap: Using libyuv from ${LIBYUV_DIR}")
    endif()
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

add_library(CameraCapture ${LIB_SOURCE})
target_include_directories(CameraCapture PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(CameraCapture PUBLIC "$<IF:$<CONFIG:Debug>,DEBUG=1,NDEBUG=1>")

if(CCAP_NO_LOG)
    target_compile_definitions(CameraCapture PUBLIC CCAP_NO_LOG=1)
    message(STATUS "ccap: Disable logging")
endif()

if(APPLE)
    target_compile_options(CameraCapture PUBLIC -fobjc-arc)
    target_link_libraries(CameraCapture
        "-framework Foundation"
        "-framework AVFoundation"
        "-framework CoreVideo"
        "-framework CoreMedia"
        "-framework Accelerate")
endif()

if(MSVC)
    target_compile_options(CameraCapture PUBLIC
        /source-charset:utf-8
        /MP
        /std:c++17
        /Zc:__cplusplus
        /Zc:preprocessor
        /D_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR=1
        /D_CRT_SECURE_NO_WARNINGS=1
        /DNOMINMAX=1
    )
else()
    target_compile_options(CameraCapture PUBLIC -std=c++17)
endif()

if(WIN32 AND ENABLE_LIBYUV)
    target_include_directories(CameraCapture PRIVATE ${YUV_INCLUDE_DIRS})
    target_link_libraries(CameraCapture yuv)
    target_compile_definitions(CameraCapture PRIVATE ENABLE_LIBYUV=1)
endif()

if(CCAP_BUILD_DEMO)
    message(STATUS "ccap: CCAP_BUILD_DEMO=${CCAP_BUILD_DEMO}")
    file(GLOB DEMO_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/demo/*.cpp)

    foreach(DEMO ${DEMO_SOURCE})
        get_filename_component(DEMO_NAME ${DEMO} NAME)

        # 去掉文件名中的后缀
        string(REPLACE ".cpp" "" DEMO_NAME ${DEMO_NAME})

        message(STATUS "ccap: Add demo: ${DEMO_NAME}")

        add_executable(${DEMO_NAME} ${DEMO})
        target_include_directories(${DEMO_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(${DEMO_NAME} CameraCapture)
    endforeach()
endif()