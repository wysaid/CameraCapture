cmake_minimum_required(VERSION 3.15)
project(test_package LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ccap CONFIG REQUIRED)

add_executable(${PROJECT_NAME} src/test_package.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE ccap::ccap)
