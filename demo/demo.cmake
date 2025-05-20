cmake_minimum_required(VERSION 3.14)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

file(GLOB DEMO_SOURCE ${CMAKE_CURRENT_LIST_DIR}/*.cpp)

foreach(DEMO ${DEMO_SOURCE})
    get_filename_component(DEMO_NAME ${DEMO} NAME)
    string(REPLACE ".cpp" "" DEMO_NAME ${DEMO_NAME})

    add_executable(${DEMO_NAME} ${DEMO})
    target_link_libraries(${DEMO_NAME} PRIVATE ccap)

    # If NAME contains glfw, link glfw3 and OpenGL, etc.
    if(${DEMO_NAME} MATCHES "glfw")
        target_link_libraries(${DEMO_NAME} PRIVATE
            ccap
            glfw
        )

        if(APPLE)
            target_link_libraries(${DEMO_NAME} PRIVATE
                "-framework OpenGL"
            )
        endif()

        target_include_directories(${DEMO_NAME} PUBLIC
            ${CMAKE_CURRENT_LIST_DIR}
            ${CMAKE_CURRENT_LIST_DIR}/glfw/include
        )

        message(STATUS "ccap: Add demo: ${DEMO_NAME} with glfw3")
    else()
        message(STATUS "ccap: Add demo: ${DEMO_NAME}")
    endif()
endforeach()