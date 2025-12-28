# examples.cmake
cmake_minimum_required(VERSION 3.14)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(DESKTOP_EXAMPLES_DIR ${CMAKE_CURRENT_LIST_DIR}/desktop)

# GLFW detection and setup
set(GLFW_AVAILABLE OFF)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # Check if we're building standalone CLI - if so, use bundled GLFW
    if(CCAP_BUILD_CLI_STANDALONE)
        # For standalone builds, use bundled static GLFW to avoid system dependencies
        set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "Build GLFW example programs" FORCE)
        set(GLFW_BUILD_TESTS OFF CACHE BOOL "Build GLFW test programs" FORCE)
        set(GLFW_BUILD_DOCS OFF CACHE BOOL "Build GLFW documentation" FORCE)
        set(GLFW_INSTALL OFF CACHE BOOL "Generate installation target" FORCE)
        set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)  # Force static GLFW

        add_subdirectory(${DESKTOP_EXAMPLES_DIR}/glfw)
        set(GLFW_AVAILABLE ON)
        set(GLFW_TARGET glfw)
        message(STATUS "ccap: Using bundled GLFW for standalone build")
    else()
        # For non-standalone builds, prefer system GLFW
        if(NOT GLFW_AVAILABLE)
            find_package(glfw3 QUIET)

            if(glfw3_FOUND)
                set(GLFW_AVAILABLE ON)
                set(GLFW_TARGET glfw)
            endif()
        endif()

        if(NOT GLFW_AVAILABLE)
            message(STATUS "ccap: GLFW not found on system. GLFW-dependent examples will be disabled.")
            message(STATUS "ccap: Install GLFW with: sudo apt-get install libglfw3-dev (Ubuntu/Debian) or equivalent")
        endif()
    endif()
else()
    # On non-Linux platforms, use bundled GLFW
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "Build GLFW example programs" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "Build GLFW test programs" FORCE)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "Build GLFW documentation" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "Generate installation target" FORCE)

    add_subdirectory(${DESKTOP_EXAMPLES_DIR}/glfw)
    set(GLFW_AVAILABLE ON)
    set(GLFW_TARGET glfw)
    message(STATUS "ccap: Using bundled GLFW")
endif()

message(STATUS "ccap: GLFW available: ${GLFW_AVAILABLE}")

add_library(common_utils STATIC ${DESKTOP_EXAMPLES_DIR}/utils/helper.cpp)
target_include_directories(common_utils PRIVATE ${DESKTOP_EXAMPLES_DIR}/utils)
target_compile_definitions(common_utils PUBLIC _CRT_SECURE_NO_WARNINGS=1)
target_link_libraries(common_utils PUBLIC ccap)

file(GLOB EXAMPLE_SOURCE ${DESKTOP_EXAMPLES_DIR}/*.cpp ${DESKTOP_EXAMPLES_DIR}/*.c)

foreach(EXAMPLE ${EXAMPLE_SOURCE})
    get_filename_component(EXAMPLE_NAME ${EXAMPLE} NAME)
    string(REGEX REPLACE "\\.(cpp|c)$" "" EXAMPLE_NAME ${EXAMPLE_NAME})

    if(${EXAMPLE_NAME} MATCHES "glfw" AND NOT GLFW_AVAILABLE)
        # Skip GLFW examples if GLFW is not available
        continue()
    endif()

    add_executable(${EXAMPLE_NAME} ${EXAMPLE})
    target_link_libraries(${EXAMPLE_NAME} PRIVATE common_utils)

    if(APPLE)
        target_link_options(${EXAMPLE_NAME} PRIVATE
            "-sectcreate" "__TEXT" "__info_plist" "${CMAKE_CURRENT_LIST_DIR}/Info.plist"
        )
    endif()

    # If NAME contains glfw, link glfw3 and optionally OpenGL
    if(${EXAMPLE_NAME} MATCHES "glfw")
        target_link_libraries(${EXAMPLE_NAME} PRIVATE ${GLFW_TARGET})

        # On Linux, OpenGL functions are loaded dynamically via GLAD
        # GLAD uses function pointers provided by GLFW (glfwGetProcAddress)
        # No additional libraries needed for dynamic loading
        if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
            if(APPLE)
                target_link_libraries(${EXAMPLE_NAME} PRIVATE "-framework OpenGL")
            endif()

            target_include_directories(${EXAMPLE_NAME} PUBLIC ${DESKTOP_EXAMPLES_DIR}/glfw/include)
        endif()

        target_include_directories(${EXAMPLE_NAME} PUBLIC ${DESKTOP_EXAMPLES_DIR})

        message(STATUS "ccap: Add example: ${EXAMPLE_NAME} with GLFW")
    else()
        message(STATUS "ccap: Add example: ${EXAMPLE_NAME}")
    endif()
endforeach()

# Link built-in test video for video examples
set(TEST_VIDEO_PATH "${CMAKE_SOURCE_DIR}/tests/test-data/test.mp4")

if(EXISTS "${TEST_VIDEO_PATH}")
    message(STATUS "ccap: Built-in test video found at ${TEST_VIDEO_PATH}")
    
    foreach(EXAMPLE ${EXAMPLE_SOURCE})
        get_filename_component(EXAMPLE_NAME ${EXAMPLE} NAME)
        string(REGEX REPLACE "\\.(cpp|c)$" "" EXAMPLE_NAME ${EXAMPLE_NAME})
        
        # Check if example name contains "video"
        if(${EXAMPLE_NAME} MATCHES "video")
            # Create a custom command to copy test video to output directory after build
            add_custom_command(
                TARGET ${EXAMPLE_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${TEST_VIDEO_PATH}"
                    "$<TARGET_FILE_DIR:${EXAMPLE_NAME}>/test.mp4"
                COMMENT "Copying test video to ${EXAMPLE_NAME} output directory"
                VERBATIM
            )
            message(STATUS "ccap: Will copy test video for example: ${EXAMPLE_NAME}")
        endif()
    endforeach()
else()
    message(WARNING "ccap: Built-in test video not found at ${TEST_VIDEO_PATH}")
endif()
