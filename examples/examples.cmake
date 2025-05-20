# examples.cmake
cmake_minimum_required(VERSION 3.14)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

file(GLOB EXAMPLE_SOURCE ${CMAKE_CURRENT_LIST_DIR}/*.cpp)

foreach(EXAMPLE ${EXAMPLE_SOURCE})
    get_filename_component(EXAMPLE_NAME ${EXAMPLE} NAME)
    string(REPLACE ".cpp" "" EXAMPLE_NAME ${EXAMPLE_NAME})

    add_executable(${EXAMPLE_NAME} ${EXAMPLE})
    target_link_libraries(${EXAMPLE_NAME} PRIVATE ccap)

    # If NAME contains glfw, link glfw3 and OpenGL, etc.
    if(${EXAMPLE_NAME} MATCHES "glfw")
        target_link_libraries(${EXAMPLE_NAME} PRIVATE
            glfw
        )

        if(APPLE)
            target_link_libraries(${EXAMPLE_NAME} PRIVATE
                "-framework OpenGL"
            )
        endif()

        target_include_directories(${EXAMPLE_NAME} PUBLIC
            ${CMAKE_CURRENT_LIST_DIR}
            ${CMAKE_CURRENT_LIST_DIR}/glfw/include
        )

        message(STATUS "ccap: Add example: ${EXAMPLE_NAME} with glfw3")
    else()
        message(STATUS "ccap: Add example: ${EXAMPLE_NAME}")
    endif()
endforeach()
