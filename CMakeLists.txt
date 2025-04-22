cmake_minimum_required(VERSION 3.12)

project(CameraCapture LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(NOT MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
endif()

file(GLOB LIB_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/*.h)

if(APPLE)
    file(GLOB LIB_SOURCE_MAC ${CMAKE_CURRENT_SOURCE_DIR}/*.mm)
    list(APPEND LIB_SOURCE ${LIB_SOURCE_MAC})
endif()

add_library(CameraCapture ${LIB_SOURCE})
target_include_directories(CameraCapture PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(CameraCapture PUBLIC "$<IF:$<CONFIG:Debug>,DEBUG=1,NDEBUG=1>")

if(APPLE)
    target_compile_options(CameraCapture PUBLIC -fobjc-arc)
endif()

if(MSVC)
    target_compile_options(CameraCapture PUBLIC
        /source-charset:utf-8
        /MP
        /std:c++17
        /Zc:__cplusplus
        /Zc:preprocessor
        /D_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR=1
    )
else()
    target_compile_options(CameraCapture PUBLIC -std=c++17)
endif()

file(GLOB DEMO_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/demo/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/demo/*.h)

add_executable(CameraDemo ${DEMO_SOURCE})

target_link_libraries(CameraDemo CameraCapture)
