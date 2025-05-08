cmake_minimum_required(VERSION 3.12)

project(CameraCapture LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    option(CCAP_BUILD_DEMO "Build ccap demo" ON)
else()
    option(CCAP_BUILD_DEMO "Build ccap demo" OFF)
endif()

if(NOT MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
endif()

file(GLOB LIB_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h)

if(APPLE)
    file(GLOB LIB_SOURCE_MAC ${CMAKE_CURRENT_SOURCE_DIR}/src/*.mm)
    message(STATUS "Using Objective-C++ for macOS: ${LIB_SOURCE_MAC}")
    list(APPEND LIB_SOURCE ${LIB_SOURCE_MAC})
endif()

add_library(CameraCapture ${LIB_SOURCE})
target_include_directories(CameraCapture PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(CameraCapture PUBLIC "$<IF:$<CONFIG:Debug>,DEBUG=1,NDEBUG=1>")

if(APPLE)
    target_compile_options(CameraCapture PUBLIC -fobjc-arc)
    target_link_libraries(CameraCapture
        "-framework Foundation"
        "-framework AVFoundation"
        "-framework CoreVideo"
        "-framework CoreMedia")
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
    )
else()
    target_compile_options(CameraCapture PUBLIC -std=c++17)
endif()

if(CCAP_BUILD_DEMO)
    file(GLOB DEMO_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/demo/*.cpp)

    foreach(DEMO ${DEMO_SOURCE})
        get_filename_component(DEMO_NAME ${DEMO} NAME)

        # 去掉文件名中的后缀
        string(REPLACE ".cpp" "" DEMO_NAME ${DEMO_NAME})

        message(STATUS "Add demo: ${DEMO_NAME}")

        add_executable(${DEMO_NAME} ${DEMO})
        target_include_directories(${DEMO_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(${DEMO_NAME} CameraCapture)
    endforeach()
endif()