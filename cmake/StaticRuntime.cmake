# StaticRuntime.cmake
# CMake module for configuring static MSVC runtime linking

# Function to configure static MSVC runtime for a target
# This eliminates the need for VCRUNTIME DLL when distributing binaries
#
# Usage:
#   ccap_configure_static_msvc_runtime(target_name)
#
function(ccap_configure_static_msvc_runtime target_name)
    if(NOT MSVC)
        return()
    endif()

    # For CMake 3.15+, use CMAKE_MSVC_RUNTIME_LIBRARY property
    if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.15")
        set_target_properties(${target_name} PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        message(STATUS "ccap: Using static MSVC runtime (/MT) for ${target_name}")
    else()
        # For older CMake versions, manually replace /MD with /MT
        # Note: These are global flags that affect all targets built after this point
        foreach(flag CMAKE_CXX_FLAGS_RELEASE CMAKE_CXX_FLAGS_MINSIZEREL
                     CMAKE_CXX_FLAGS_RELWITHDEBINFO CMAKE_CXX_FLAGS_DEBUG
                     CMAKE_C_FLAGS_RELEASE CMAKE_C_FLAGS_MINSIZEREL
                     CMAKE_C_FLAGS_RELWITHDEBINFO CMAKE_C_FLAGS_DEBUG)
            if(${flag} MATCHES "/MD")
                string(REGEX REPLACE "/MD" "/MT" ${flag} "${${flag}}")
                set(${flag} "${${flag}}" PARENT_SCOPE)
            endif()
            if(${flag} MATCHES "/MDd")
                string(REGEX REPLACE "/MDd" "/MTd" ${flag} "${${flag}}")
                set(${flag} "${${flag}}" PARENT_SCOPE)
            endif()
        endforeach()
        message(STATUS "ccap: Using static MSVC runtime (/MT) via flag replacement for ${target_name}")
    endif()
endfunction()
