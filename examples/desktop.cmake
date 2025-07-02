# examples.cmake
cmake_minimum_required(VERSION 3.14)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(DESKTOP_EXAMPLES_DIR ${CMAKE_CURRENT_LIST_DIR}/desktop)

# GLFW detection and setup
set(GLFW_AVAILABLE FALSE)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if(NOT GLFW_AVAILABLE)
        find_package(glfw3 QUIET)

        if(glfw3_FOUND)
            find_package(OpenGL REQUIRED)
            set(GLFW_AVAILABLE TRUE)
            set(GLFW_TARGET glfw)
            message(STATUS "ccap: Using system-installed GLFW via CMake")
        endif()
    endif()

    if(NOT GLFW_AVAILABLE)
        message(STATUS "ccap: GLFW not found on system. GLFW-dependent examples will be disabled.")
        message(STATUS "ccap: Install GLFW with: sudo apt-get install libglfw3-dev (Ubuntu/Debian) or equivalent")
    endif()
else()
    # On non-Linux platforms, use bundled GLFW
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "Build GLFW example programs" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "Build GLFW test programs" FORCE)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "Build GLFW documentation" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "Generate installation target" FORCE)

    add_subdirectory(${DESKTOP_EXAMPLES_DIR}/glfw)
    set(GLFW_AVAILABLE TRUE)
    set(GLFW_TARGET glfw)
    message(STATUS "ccap: Using bundled GLFW")
endif()

file(GLOB EXAMPLE_SOURCE ${DESKTOP_EXAMPLES_DIR}/*.cpp ${DESKTOP_EXAMPLES_DIR}/*.c)

foreach(EXAMPLE ${EXAMPLE_SOURCE})
    if(${EXAMPLE_NAME} MATCHES "glfw" AND NOT GLFW_AVAILABLE)
        # Skip GLFW examples if GLFW is not available
        continue()
    endif()

    get_filename_component(EXAMPLE_NAME ${EXAMPLE} NAME)
    string(REGEX REPLACE "\\.(cpp|c)$" "" EXAMPLE_NAME ${EXAMPLE_NAME})

    add_executable(${EXAMPLE_NAME} ${EXAMPLE})
    target_link_libraries(${EXAMPLE_NAME} PRIVATE ccap)

    if(APPLE)
        target_link_options(${EXAMPLE_NAME} PRIVATE
            "-sectcreate" "__TEXT" "__info_plist" "${CMAKE_CURRENT_LIST_DIR}/Info.plist"
        )
    endif()

    # If NAME contains glfw, link glfw3 and OpenGL, etc.
    if(${EXAMPLE_NAME} MATCHES "glfw")
        target_link_libraries(${EXAMPLE_NAME} PRIVATE
            ${GLFW_TARGET}
        )

        if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            # On Linux, link OpenGL libraries
            target_link_libraries(${EXAMPLE_NAME} PRIVATE
                OpenGL::GL
            )
        else()
            if(APPLE)
                target_link_libraries(${EXAMPLE_NAME} PRIVATE
                    "-framework OpenGL"
                )
            endif()

            target_include_directories(${EXAMPLE_NAME} PUBLIC
                ${DESKTOP_EXAMPLES_DIR}/glfw/include
            )
        endif()

        target_include_directories(${EXAMPLE_NAME} PUBLIC
            ${DESKTOP_EXAMPLES_DIR}
        )

        message(STATUS "ccap: Add example: ${EXAMPLE_NAME} with GLFW")
    else()
        message(STATUS "ccap: Add example: ${EXAMPLE_NAME}")
    endif()
endforeach()
