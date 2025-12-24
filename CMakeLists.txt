cmake_minimum_required(VERSION 3.14)

# Set CMAKE_POLICY_VERSION_MINIMUM if not already defined to avoid compatibility issues
# with newer CMake versions when fetching dependencies like GoogleTest
if(NOT DEFINED CMAKE_POLICY_VERSION_MINIMUM)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
endif()

option(CCAP_NO_LOG "Disable logging" OFF)
option(CCAP_BUILD_SHARED "Build ccap as shared library" OFF)
option(CCAP_WIN_NO_DEVICE_VERIFY "Skip device verification on Windows (for buggy camera drivers)" OFF)
option(BUILD_CCAP_CLI "Build ccap CLI tool" OFF)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(CCAP_IS_ROOT_PROJECT ON)
else()
    set(CCAP_IS_ROOT_PROJECT OFF)
endif()

option(CCAP_BUILD_EXAMPLES "Build ccap examples" ${CCAP_IS_ROOT_PROJECT})
option(CCAP_BUILD_TESTS "Build ccap unit tests" OFF)

# Auto-enable CLI when building tests
if(CCAP_BUILD_TESTS AND NOT BUILD_CCAP_CLI)
    message(STATUS "ccap: Enabling BUILD_CCAP_CLI for tests")
    set(BUILD_CCAP_CLI ON CACHE BOOL "Build ccap CLI tool (auto-enabled for tests)" FORCE)
endif()

if(CCAP_IS_ROOT_PROJECT)
    set(CMAKE_FETCHCONTENT_BASE_DIR "${CMAKE_SOURCE_DIR}/build" CACHE PATH "FetchContent base dir" FORCE)
endif()

# Read version from header file
set(CCAP_CONFIG_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/include/ccap_config.h")
if(EXISTS "${CCAP_CONFIG_HEADER}")
    file(READ "${CCAP_CONFIG_HEADER}" ver_h)
    string(REGEX MATCH "#define CCAP_VERSION_MAJOR ([0-9]+)" _ "${ver_h}")
    set(ver_major ${CMAKE_MATCH_1})
    string(REGEX MATCH "#define CCAP_VERSION_MINOR ([0-9]+)" _ "${ver_h}")
    set(ver_minor ${CMAKE_MATCH_1})
    string(REGEX MATCH "#define CCAP_VERSION_PATCH ([0-9]+)" _ "${ver_h}")
    set(ver_patch ${CMAKE_MATCH_1})

    if("${ver_major}" STREQUAL "" OR "${ver_minor}" STREQUAL "" OR "${ver_patch}" STREQUAL "")
        set(PROJECT_VERSION "0.0.9999")
        message(WARNING "Could not parse version from ${CCAP_CONFIG_HEADER}, using default version ${PROJECT_VERSION}")
    else()
        set(PROJECT_VERSION "${ver_major}.${ver_minor}.${ver_patch}")
    endif()
else()
    set(PROJECT_VERSION "0.0.9999")
    message(WARNING "${CCAP_CONFIG_HEADER} not found, using default version ${PROJECT_VERSION}")
endif()

project(ccap VERSION ${PROJECT_VERSION} LANGUAGES C CXX)

# Installation options
option(CCAP_INSTALL "Generate installation target" ${CCAP_IS_ROOT_PROJECT})

if(NOT DEFINED CMAKE_BUILD_TYPE AND NOT MSVC)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
    message(STATUS "CMAKE_BUILD_TYPE not set, defaulting to Release")
elseif(NOT MSVC)
    message(STATUS "CMAKE_BUILD_TYPE is set to ${CMAKE_BUILD_TYPE}")
endif()

# Load local development settings if available (ignored by git)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/dev.cmake)
    message(STATUS "ccap: Loading dev.cmake for local development settings")
    include(${CMAKE_CURRENT_SOURCE_DIR}/dev.cmake)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum OS X deployment version")

# Unified ARM64 force compilation option, supports both macOS and Windows
option(CCAP_FORCE_ARM64 "Force ARM64 architecture compilation" OFF)

if(CCAP_FORCE_ARM64)
    if(APPLE)
        set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Build architectures for Mac OS X" FORCE)
        message(STATUS "ccap: Forcing ARM64 architecture compilation for macOS")
    elseif(WIN32)
        set(CMAKE_GENERATOR_PLATFORM "ARM64" CACHE STRING "Generator platform for Windows" FORCE)
        message(STATUS "ccap: Forcing ARM64 architecture compilation for Windows")
    endif()
endif()

# Detect ARM64 architecture and set corresponding compile flags
if(CMAKE_SYSTEM_PROCESSOR MATCHES "[Aa][Rr][Mm]64|[Aa][Aa][Rr][Cc]h64" OR
   CMAKE_GENERATOR_PLATFORM MATCHES "[Aa][Rr][Mm]64" OR
   CMAKE_OSX_ARCHITECTURES MATCHES "[Aa][Rr][Mm]64" OR
   CCAP_FORCE_ARM64)
    
    message(STATUS "ccap: Building for ARM64 architecture")
    
    # Windows ARM64 specific settings
    if(WIN32 AND MSVC)
        # MSVC automatically enables NEON on ARM64, add preprocessor definition
        add_compile_definitions(_M_ARM64=1)
        message(STATUS "ccap: NEON support enabled for Windows ARM64")
    endif()
    
    # macOS ARM64 specific settings
    if(APPLE)
        # Ensure a valid deployment target for macOS arm64
        if(CMAKE_OSX_DEPLOYMENT_TARGET VERSION_LESS "11.0")
            set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum OS X deployment version" FORCE)
            message(STATUS "ccap: Bumping macOS deployment target to 11.0 for ARM64")
        endif()
        message(STATUS "ccap: NEON support enabled for macOS ARM64")
    endif()
endif()



file(GLOB LIB_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h)

if(APPLE)
    file(GLOB LIB_SOURCE_MAC ${CMAKE_CURRENT_SOURCE_DIR}/src/*.mm)
    message(STATUS "ccap: Using Objective-C++ for macOS: ${LIB_SOURCE_MAC}")
    list(APPEND LIB_SOURCE ${LIB_SOURCE_MAC})
endif()

include(CheckCXXCompilerFlag)

# Check if AVX2 is supported
if(MSVC)
    check_cxx_compiler_flag("/arch:AVX2" CCAP_SUPPORTS_AVX2)
else()
    check_cxx_compiler_flag("-mavx2" CCAP_SUPPORTS_AVX2)
endif()

if(CCAP_SUPPORTS_AVX2)
    set(CCAP_CONVERT_AVX2_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/ccap_convert_avx2.cpp)
    # Set compile options for AVX2 source file
    if(MSVC)
        set_source_files_properties(${CCAP_CONVERT_AVX2_SRC} PROPERTIES COMPILE_OPTIONS "/arch:AVX2")
    else()
        # GCC/Clang/MinGW need these options to enable AVX2
        set_source_files_properties(${CCAP_CONVERT_AVX2_SRC} PROPERTIES COMPILE_OPTIONS "-mavx2;-mfma")
    endif()
    list(APPEND LIB_SOURCE ${CCAP_CONVERT_AVX2_SRC})
    message(STATUS "ccap: AVX2 support enabled for ${CCAP_CONVERT_AVX2_SRC}")
else()
    message(STATUS "ccap: AVX2 support not available")
endif()

if(CCAP_BUILD_SHARED)
    add_library(ccap SHARED ${LIB_SOURCE})
    message(STATUS "ccap: Building as shared library")
else()
    add_library(ccap STATIC ${LIB_SOURCE})
    message(STATUS "ccap: Building as static library")
endif()

# Set library output name with debug suffix for MSVC
if(MSVC)
    set_target_properties(ccap PROPERTIES
        OUTPUT_NAME "ccap"
        DEBUG_POSTFIX "d"
    )
    
    # Configure static MSVC runtime for CLI tool to avoid runtime dependencies
    # This only applies when building the CLI tool with static library
    # For shared library builds or library-only builds, default runtime linking is used
    if(BUILD_CCAP_CLI AND NOT CCAP_BUILD_SHARED)
        include(cmake/StaticRuntime.cmake)
        ccap_configure_static_msvc_runtime(ccap)
    endif()
endif()

target_include_directories(ccap PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
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

if(CCAP_WIN_NO_DEVICE_VERIFY)
    target_compile_definitions(ccap PRIVATE CCAP_WIN_NO_DEVICE_VERIFY=1)
    message(STATUS "ccap: Skip device verification on Windows (for buggy camera drivers)")
endif()

# Configure shared library export definitions
if(CCAP_BUILD_SHARED)
    target_compile_definitions(ccap PUBLIC CCAP_SHARED=1)
    if(WIN32)
        target_compile_definitions(ccap PRIVATE CCAP_BUILDING_DLL=1)
    endif()
endif()

if(APPLE)
    target_compile_options(ccap PRIVATE -fobjc-arc)
    target_link_libraries(ccap PUBLIC
        "-framework Foundation"
        "-framework AVFoundation"
        "-framework CoreVideo"
        "-framework CoreMedia"
        "-framework Accelerate")

    # Set pkg-config private libs for macOS
    set(PKG_CONFIG_LIBS_PRIVATE "Libs.private: -framework Foundation -framework AVFoundation -framework CoreVideo -framework CoreMedia -framework Accelerate")
elseif(UNIX AND NOT APPLE AND NOT WIN32)
    # Linux – link pthread for std::thread support
    find_package(Threads REQUIRED)
    target_link_libraries(ccap PUBLIC Threads::Threads)

    # Propagate to pkg-config for consumers
    set(PKG_CONFIG_LIBS_PRIVATE "Libs.private: -lpthread")
else()
    # Windows (including MinGW) or other platforms
    set(PKG_CONFIG_LIBS_PRIVATE "")
endif()

if(MSVC)
    target_compile_options(ccap PRIVATE
        /MP
        $<$<COMPILE_LANGUAGE:CXX>:/std:c++17>
        $<$<COMPILE_LANGUAGE:CXX>:/Zc:__cplusplus>
        $<$<COMPILE_LANGUAGE:CXX>:/Zc:preprocessor>
    )

    target_compile_options(ccap PRIVATE /source-charset:utf-8)

else()
    target_compile_options(ccap PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-std=c++17>)
endif()

# ############### Examples ################
message(STATUS "ccap: CCAP_BUILD_EXAMPLES=${CCAP_BUILD_EXAMPLES}")

if(CCAP_BUILD_EXAMPLES)
    include(examples/desktop.cmake)
endif()

# ############### CLI Tool ################
message(STATUS "ccap: BUILD_CCAP_CLI=${BUILD_CCAP_CLI}")

if(BUILD_CCAP_CLI)
    include(cli/ccap-cli.cmake)
endif()

# ############### Tests ################
message(STATUS "ccap: CCAP_BUILD_TESTS=${CCAP_BUILD_TESTS}")

if(CCAP_BUILD_TESTS)
    target_compile_definitions(ccap PUBLIC CCAP_BUILD_TESTS=1)
    enable_testing()
    add_subdirectory(tests)
endif()

# ############### Installation ################
if(CCAP_INSTALL)
    message(STATUS "ccap: Installing enabled")

    # Include necessary modules for installation
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    # Set installation directories
    set(CCAP_INSTALL_LIBDIR ${CMAKE_INSTALL_LIBDIR})
    set(CCAP_INSTALL_INCLUDEDIR ${CMAKE_INSTALL_INCLUDEDIR})
    set(CCAP_INSTALL_CMAKEDIR ${CMAKE_INSTALL_LIBDIR}/cmake/ccap)

    # Install the library
    install(TARGETS ccap
        EXPORT ccapTargets
        LIBRARY DESTINATION ${CCAP_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CCAP_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CCAP_INSTALL_INCLUDEDIR}
    )

    # Install header files
    install(DIRECTORY include/
        DESTINATION ${CCAP_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
    )

    # Create and install package configuration files
    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ccapConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/ccapConfig.cmake"
        INSTALL_DESTINATION ${CCAP_INSTALL_CMAKEDIR}
        PATH_VARS CCAP_INSTALL_INCLUDEDIR CCAP_INSTALL_LIBDIR
    )

    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/ccapConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    # Install the configuration files
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/ccapConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/ccapConfigVersion.cmake"
        DESTINATION ${CCAP_INSTALL_CMAKEDIR}
    )

    # Install the targets file
    install(EXPORT ccapTargets
        FILE ccapTargets.cmake
        NAMESPACE ccap::
        DESTINATION ${CCAP_INSTALL_CMAKEDIR}
    )

    # Generate pkg-config file
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ccap.pc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/ccap.pc"
        @ONLY
    )

    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/ccap.pc"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
    )

    message(STATUS "ccap: Installation paths:")
    message(STATUS "  Libraries: ${CMAKE_INSTALL_PREFIX}/${CCAP_INSTALL_LIBDIR}")
    message(STATUS "  Headers: ${CMAKE_INSTALL_PREFIX}/${CCAP_INSTALL_INCLUDEDIR}")
    message(STATUS "  CMake: ${CMAKE_INSTALL_PREFIX}/${CCAP_INSTALL_CMAKEDIR}")
endif()