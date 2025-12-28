#!/bin/bash

# CCAP Unit Tests Runner
# This script builds and runs all ccap unit tests
# Functional tests run in Debug mode, Performance tests run in Release mode
#
# Platform-specific behavior:
# - Windows (MSVC): Uses single build directory, configs specified during build
# - Linux/Mac: Uses separate build/Debug and build/Release directories
#
# Memory Sanitizer (ASAN):
# - Functional tests: ENABLED by default (can be disabled with --no-sanitize)
# - Performance tests: DISABLED by default (can be enabled with --sanitize-all)
# - ASAN helps detect memory errors like buffer overflows, use-after-free, etc.
#
# Usage:
#   ./run_tests.sh                    # Run all tests (functional with ASAN, performance without)
#   ./run_tests.sh --functional       # Run only functional tests (with ASAN)
#   ./run_tests.sh --performance      # Run only performance tests (without ASAN)
#   ./run_tests.sh --video            # Run only video file playback tests (Release mode)
#   ./run_tests.sh --avx2             # Run only AVX2 performance tests
#   ./run_tests.sh --shuffle          # Run only tests with names containing 'shuffle' (case-insensitive) in functional tests
#   ./run_tests.sh --no-sanitize      # Disable ASAN for all tests
#   ./run_tests.sh --sanitize-all     # Enable ASAN for all tests (including performance)
#   ./run_tests.sh --help             # Show help

set -e # Exit on any error

cd "$(dirname "$0")/.."

function isWsl() {
    [[ -d "/mnt/c" ]] || command -v wslpath &>/dev/null
}

function isWindows() {
    if isWsl; then
        # 当当前路径以 /mnt/ 开头时，认为是 Windows, 否则不是
        if [[ "$(pwd)" == /mnt/* ]]; then
            return 0
        else
            return 1
        fi
    fi

    [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" ]] || [[ -n "$WINDIR" ]]
}

function detectCores() {
    if [[ -n "$NUMBER_OF_PROCESSORS" ]]; then
        echo $NUMBER_OF_PROCESSORS
    else
        getconf _NPROCESSORS_ONLN || nproc || echo 1
    fi
}

if isWindows && isWsl; then
    # Check if this is a Windows mount point or native WSL Linux
    echo "You're using WSL with Windows mount point. Switching to Git Bash for Windows tests!" >&2
    GIT_BASH_PATH_WIN=$(/mnt/c/Windows/system32/cmd.exe /C "where bash.exe" | grep -i Git | head -n 1 | tr -d '\n\r')
    GIT_BASH_PATH_WSL=$(wslpath -u "$GIT_BASH_PATH_WIN")
    echo "== GIT_BASH_PATH_WIN=$GIT_BASH_PATH_WIN"
    echo "== GIT_BASH_PATH_WSL=$GIT_BASH_PATH_WSL"
    if [[ -f "$GIT_BASH_PATH_WSL" ]]; then
        THIS_BASE_NAME=$(basename "$0")
        "$GIT_BASH_PATH_WSL" "$THIS_BASE_NAME" $@
        exit $?
    else
        echo "Git Bash not found, please install Git Bash!" >&2
        exit 1
    fi
fi

# Parse command line arguments
RUN_ALL=false
RUN_FUNCTIONAL=true
RUN_PERFORMANCE=true
RUN_VIDEO=false
SKIP_BUILD=false
EXIT_WHEN_FAILED=false
GTEST_FAIL_FAST_PARAM=""
FILTER=""
SHUFFLE_ONLY=false
# Memory sanitizer flags: auto-detect based on test type, can be overridden
SANITIZE_FUNCTIONAL="auto"  # Default: enabled for functional tests
SANITIZE_PERFORMANCE="auto" # Default: disabled for performance tests

while [[ $# -gt 0 ]]; do
    case $1 in
    -a | --all)
        RUN_ALL=true
        RUN_FUNCTIONAL=true
        RUN_PERFORMANCE=true
        RUN_VIDEO=true
        shift
        ;;
    -f | --functional)
        RUN_FUNCTIONAL=true
        RUN_PERFORMANCE=false
        shift
        ;;
    -p | --performance)
        RUN_FUNCTIONAL=false
        RUN_PERFORMANCE=true
        shift
        ;;
    --avx2)
        RUN_FUNCTIONAL=false
        RUN_PERFORMANCE=true
        FILTER="--gtest_filter=*AVX2*"
        shift
        ;;
    --video)
        # Video file playback tests only (Release mode)
        RUN_FUNCTIONAL=false
        RUN_PERFORMANCE=false
        RUN_VIDEO=true
        shift
        ;;
    --shuffle)
        # Functional only, and later restrict to tests containing 'shuffle' (case-insensitive)
        RUN_FUNCTIONAL=true
        RUN_PERFORMANCE=false
        SHUFFLE_ONLY=true
        shift
        ;;
    --skip-build)
        SKIP_BUILD=true
        shift
        ;;
    --exit-when-failed)
        EXIT_WHEN_FAILED=true
        GTEST_FAIL_FAST_PARAM="--gtest_fail_fast"
        shift
        ;;
    --no-sanitize)
        SANITIZE_FUNCTIONAL="no"
        SANITIZE_PERFORMANCE="no"
        shift
        ;;
    --sanitize-all)
        SANITIZE_FUNCTIONAL="yes"
        SANITIZE_PERFORMANCE="yes"
        shift
        ;;
    --help)
        echo "CCAP Unit Tests Runner"
        echo ""
        echo "Usage:"
        echo "  $0                    # Run all tests (functional with ASAN, performance without)"
        echo "  $0 --all              # Run all tests including video file playback tests"
        echo "  $0 --functional       # Run only functional tests (Debug mode, with ASAN)"
        echo "  $0 --performance      # Run only performance tests (Release mode, without ASAN)"
        echo "  $0 --video            # Run only video file playback tests (Release mode)"
        echo "  $0 --avx2             # Run only AVX2 performance tests (Release mode)"
        echo "  $0 --shuffle          # Run only tests whose names contain '*shuffle*' or '*Shuffle*' in functional tests"
        echo "  $0 --skip-build       # Skip build step, run tests only"
        echo "  $0 --exit-when-failed # Stop at first test failure (gtest fail fast mode)"
        echo "  $0 --no-sanitize      # Disable AddressSanitizer (ASAN) for all tests"
        echo "  $0 --sanitize-all     # Enable AddressSanitizer (ASAN) for all tests (including performance)"
        echo "  $0 --help             # Show this help"
        echo ""
        echo "Memory Sanitizer (ASAN):"
        echo "  - Functional tests: ENABLED by default (detects memory errors)"
        echo "  - Performance tests: DISABLED by default (would affect performance measurements)"
        echo "  - Use --no-sanitize to disable ASAN completely"
        echo "  - Use --sanitize-all to enable ASAN for performance tests too"
        echo ""
        echo "Note: Performance tests are automatically run in Release mode for accurate results"
        exit 0
        ;;
    *)
        echo "Unknown option: $1"
        echo "Use --help for usage information"
        exit 1
        ;;
    esac
done

echo "==============================================="
echo "CCAP Unit Tests Runner"
echo "==============================================="

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
NC='\033[0m' # No Color

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ] || [ ! -d "tests" ]; then
    echo -e "${RED}Error: Please run this script from the ccap root directory, cwd: $(pwd)"
    exit 1
fi

# Remove dev.cmake if it exists (tests should not depend on local dev settings)
if [ -f "dev.cmake" ]; then
    echo -e "${YELLOW}⚠️  Removing dev.cmake to ensure clean test environment${NC}"
    rm -f dev.cmake
fi

# Create build directories if they don't exist
echo -e "${BLUE}Setting up build directories...${NC}"
if isWindows; then
    # On Windows with MSVC, use single build directory
    mkdir -p build
else
    # On Linux/Mac, use separate directories for Debug/Release
    mkdir -p build/Debug
    mkdir -p build/Release
fi

# Initialize results
TEST_RESULT=0
PERF_RESULT=0
VIDEO_RESULT=0

# Determine ASAN usage
# Functional tests: default enabled, Performance tests: default disabled
USE_ASAN_FUNCTIONAL=false
USE_ASAN_PERFORMANCE=false

if [ "$SANITIZE_FUNCTIONAL" = "auto" ]; then
    USE_ASAN_FUNCTIONAL=true # Default: enable ASAN for functional tests
elif [ "$SANITIZE_FUNCTIONAL" = "yes" ]; then
    USE_ASAN_FUNCTIONAL=true
fi

if [ "$SANITIZE_PERFORMANCE" = "auto" ]; then
    USE_ASAN_PERFORMANCE=false # Default: disable ASAN for performance tests
elif [ "$SANITIZE_PERFORMANCE" = "yes" ]; then
    USE_ASAN_PERFORMANCE=true
fi

# Function to check if ASAN is supported
function checkAsanSupport() {
    # ASAN is well supported on Linux and macOS with GCC/Clang
    # Windows MSVC support is limited and not used here
    if isWindows; then
        return 1 # Disable ASAN on Windows for now
    fi
    return 0
}

# Check ASAN support
ASAN_SUPPORTED=false
if checkAsanSupport; then
    ASAN_SUPPORTED=true
fi

# Disable ASAN if not supported
if [ "$ASAN_SUPPORTED" = false ]; then
    if [ "$USE_ASAN_FUNCTIONAL" = true ] || [ "$USE_ASAN_PERFORMANCE" = true ]; then
        echo -e "${YELLOW}⚠ AddressSanitizer not supported on this platform, disabling ASAN${NC}"
        USE_ASAN_FUNCTIONAL=false
        USE_ASAN_PERFORMANCE=false
    fi
fi

# Build Debug version for functional tests
if [ "$RUN_FUNCTIONAL" = true ]; then
    echo ""
    echo -e "${PURPLE}===============================================${NC}"
    if [ "$SKIP_BUILD" = true ]; then
        echo -e "${BLUE}Skipping build, using existing Debug binaries${NC}"
    else
        echo -e "${BLUE}Building Debug version (for functional tests)${NC}"
        if [ "$USE_ASAN_FUNCTIONAL" = true ]; then
            echo -e "${GREEN}🛡️  AddressSanitizer (ASAN) ENABLED for memory error detection${NC}"
        fi
    fi
    echo -e "${PURPLE}===============================================${NC}"

    if [ "$SKIP_BUILD" = false ]; then
        # Prepare ASAN flags if enabled
        ASAN_FLAGS=""
        if [ "$USE_ASAN_FUNCTIONAL" = true ]; then
            ASAN_FLAGS="-DCMAKE_CXX_FLAGS=\"-fsanitize=address -g\" -DCMAKE_C_FLAGS=\"-fsanitize=address -g\" -DCMAKE_EXE_LINKER_FLAGS=\"-fsanitize=address\" -DCMAKE_SHARED_LINKER_FLAGS=\"-fsanitize=address\""
        fi

        if isWindows; then
            # Windows MSVC: use single build directory, specify config during build
            cd build
            echo -e "${BLUE}Configuring CMake (Windows MSVC)...${NC}"
            eval cmake .. -DCCAP_BUILD_TESTS=ON -DCCAP_BUILD_CLI=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5 $ASAN_FLAGS

            echo -e "${BLUE}Building Debug project...${NC}"
            cmake --build . --config Debug --parallel $(detectCores)

            echo -e "${BLUE}Building Debug tests...${NC}"
            cmake --build . --config Debug --target ccap_convert_test --parallel $(detectCores)
            cmake --build . --config Debug --target ccap_guid_test --parallel $(detectCores)
            cd ..
        else
            # Linux/Mac: use separate Debug directory
            cd build/Debug
            echo -e "${BLUE}Configuring CMake (Debug)...${NC}"
            eval cmake ../.. -DCMAKE_BUILD_TYPE=Debug -DCCAP_BUILD_TESTS=ON -DCCAP_BUILD_CLI=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5 $ASAN_FLAGS

            echo -e "${BLUE}Building Debug project...${NC}"
            cmake --build . --config Debug --parallel $(detectCores)

            echo -e "${BLUE}Building Debug tests...${NC}"
            cmake --build . --config Debug --target ccap_convert_test --parallel $(detectCores)
            cd ../..
        fi
    fi
fi

# Build Release version for performance tests or video tests
if [ "$RUN_PERFORMANCE" = true ] || [ "$RUN_VIDEO" = true ]; then
    echo ""
    echo -e "${PURPLE}===============================================${NC}"
    if [ "$SKIP_BUILD" = true ]; then
        echo -e "${BLUE}Skipping build, using existing Release binaries${NC}"
    else
        if [ "$RUN_VIDEO" = true ]; then
            echo -e "${BLUE}Building Release version (for video file playback tests)${NC}"
        else
            echo -e "${BLUE}Building Release version (for performance tests)${NC}"
        fi
        if [ "$USE_ASAN_PERFORMANCE" = true ]; then
            echo -e "${GREEN}🛡️  AddressSanitizer (ASAN) ENABLED${NC}"
            echo -e "${YELLOW}⚠  Note: ASAN affects performance measurements${NC}"
        fi
    fi
    echo -e "${PURPLE}===============================================${NC}"

    if [ "$SKIP_BUILD" = false ]; then
        # Prepare ASAN flags if enabled
        ASAN_FLAGS=""
        if [ "$USE_ASAN_PERFORMANCE" = true ]; then
            ASAN_FLAGS="-DCMAKE_CXX_FLAGS=\"-fsanitize=address -g\" -DCMAKE_C_FLAGS=\"-fsanitize=address -g\" -DCMAKE_EXE_LINKER_FLAGS=\"-fsanitize=address\" -DCMAKE_SHARED_LINKER_FLAGS=\"-fsanitize=address\""
        fi

        if isWindows; then
            # Windows MSVC: use single build directory, specify config during build
            cd build
            # Only configure if not already configured
            if [ ! -f "CMakeCache.txt" ]; then
                echo -e "${BLUE}Configuring CMake (Windows MSVC)...${NC}"
                eval cmake .. -DCCAP_BUILD_TESTS=ON -DCCAP_BUILD_CLI=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5 $ASAN_FLAGS
            fi

            echo -e "${BLUE}Building Release project...${NC}"
            cmake --build . --config Release --parallel $(detectCores)

            echo -e "${BLUE}Building Release tests...${NC}"
            cmake --build . --config Release --target ccap_performance_test --parallel $(detectCores)
            cd ..
        else
            # Linux/Mac: use separate Release directory
            cd build/Release
            echo -e "${BLUE}Configuring CMake (Release)...${NC}"
            eval cmake ../.. -DCMAKE_BUILD_TYPE=Release -DCCAP_BUILD_TESTS=ON -DCCAP_BUILD_CLI=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5 $ASAN_FLAGS

            echo -e "${BLUE}Building Release project...${NC}"
            cmake --build . --config Release --parallel $(detectCores)

            echo -e "${BLUE}Building Release tests...${NC}"
            if [ "$RUN_PERFORMANCE" = true ]; then
                cmake --build . --config Release --target ccap_performance_test --parallel $(detectCores)
            fi
            if [ "$RUN_VIDEO" = true ]; then
                cmake --build . --config Release --target ccap_file_playback_test --parallel $(detectCores)
            fi
            cd ../..
        fi
    fi
fi

# Run unit tests in Debug mode
if [ "$RUN_FUNCTIONAL" = true ]; then
    echo ""
    echo "==============================================="
    echo -e "${GREEN}Running Functional Tests (Debug)${NC}"
    echo "===============================================" # Determine test executable path based on platform

    pushd ./build

    if isWindows; then
        TEST_EXECUTABLE="./tests/Debug/ccap_convert_test.exe"
        GUID_TEST_EXECUTABLE="./tests/Debug/ccap_guid_test.exe"
    else
        TEST_EXECUTABLE="./Debug/tests/ccap_convert_test"
    fi

    if [ -f "$TEST_EXECUTABLE" ]; then
        echo -e "${YELLOW}Running functional tests in Debug mode...${NC}"

        # Set ASAN options if enabled
        if [ "$USE_ASAN_FUNCTIONAL" = true ]; then
            echo -e "${GREEN}🛡️  Running with AddressSanitizer enabled${NC}"
            # Disable memory leak detection in ASAN (can cause false positives in tests)
            # Enable detailed error reporting
            export ASAN_OPTIONS="detect_leaks=0:halt_on_error=1:allocator_may_return_null=1"
        fi

        if [ "$SHUFFLE_ONLY" = true ]; then
            echo -e "${BLUE}Filtering tests to names containing '*shuffle*' or '*Shuffle*'...${NC}"
            "$TEST_EXECUTABLE" --gtest_filter='*shuffle*:*Shuffle*:*SHUFFLE*' $GTEST_FAIL_FAST_PARAM --gtest_output=xml:test_results_debug.xml
        else
            "$TEST_EXECUTABLE" $GTEST_FAIL_FAST_PARAM --gtest_output=xml:test_results_debug.xml
        fi

        TEST_RESULT=$?

        if [ $TEST_RESULT -eq 0 ]; then
            echo -e "${GREEN}✓ Functional tests PASSED${NC}"
        else
            echo -e "${RED}✗ Functional tests FAILED${NC}"
            if [ "$EXIT_WHEN_FAILED" = true ]; then
                echo -e "${RED}❌ Exiting due to --exit-when-failed flag${NC}"
                exit 1
            fi
        fi
    else
        echo -e "${RED}Error: ccap_convert_test executable not found at $TEST_EXECUTABLE${NC}"
        TEST_RESULT=1
    fi

    # Run GUID verification tests (Windows only)
    if isWindows && [ -f "$GUID_TEST_EXECUTABLE" ]; then
        echo ""
        echo -e "${YELLOW}Running GUID verification tests (Windows only)...${NC}"
        "$GUID_TEST_EXECUTABLE" $GTEST_FAIL_FAST_PARAM --gtest_output=xml:guid_test_results_debug.xml
        GUID_TEST_RESULT=$?

        if [ $GUID_TEST_RESULT -eq 0 ]; then
            echo -e "${GREEN}✓ GUID verification tests PASSED${NC}"
        else
            echo -e "${RED}✗ GUID verification tests FAILED${NC}"
            TEST_RESULT=1
            if [ "$EXIT_WHEN_FAILED" = true ]; then
                echo -e "${RED}❌ Exiting due to --exit-when-failed flag${NC}"
                exit 1
            fi
        fi
    fi

    popd
fi

# Run performance tests in Release mode
if [ "$RUN_PERFORMANCE" = true ]; then
    echo ""
    echo "==============================================="
    echo -e "${GREEN}Running Performance Tests (Release)${NC}"
    echo "===============================================" # Determine test executable path based on platform
    if isWindows; then
        PERF_EXECUTABLE="./build/tests/Release/ccap_performance_test.exe"
    else
        PERF_EXECUTABLE="./build/Release/tests/ccap_performance_test"
    fi

    if [ -f "$PERF_EXECUTABLE" ]; then
        echo -e "${YELLOW}Running performance benchmarks in Release mode...${NC}"
        echo -e "${BLUE}Note: Release mode provides accurate performance measurements${NC}"

        # Set ASAN options if enabled
        if [ "$USE_ASAN_PERFORMANCE" = true ]; then
            echo -e "${GREEN}🛡️  Running with AddressSanitizer enabled${NC}"
            echo -e "${YELLOW}⚠  Performance results may be affected by ASAN overhead${NC}"
            export ASAN_OPTIONS="detect_leaks=0:halt_on_error=1:allocator_may_return_null=1"
        fi

        if [ -n "$FILTER" ]; then
            echo -e "${BLUE}Filter: $FILTER${NC}"
            "$PERF_EXECUTABLE" $FILTER $GTEST_FAIL_FAST_PARAM --gtest_output=xml:build/performance_results_release.xml
        else
            "$PERF_EXECUTABLE" $GTEST_FAIL_FAST_PARAM --gtest_output=xml:build/performance_results_release.xml
        fi

        PERF_RESULT=$?

        if [ $PERF_RESULT -eq 0 ]; then
            echo -e "${GREEN}✓ Performance tests PASSED${NC}"
        else
            echo -e "${RED}✗ Performance tests FAILED${NC}"
            if [ "$EXIT_WHEN_FAILED" = true ]; then
                echo -e "${RED}❌ Exiting due to --exit-when-failed flag${NC}"
                exit 1
            fi
        fi
    else
        echo -e "${RED}Error: ccap_performance_test executable not found at $PERF_EXECUTABLE${NC}"
        if [ "$EXIT_WHEN_FAILED" = true ]; then
            echo -e "${RED}❌ Exiting due to --exit-when-failed flag${NC}"
            exit 1
        fi
        PERF_RESULT=1
    fi
fi

# Run video file playback tests in Release mode
if [ "$RUN_VIDEO" = true ]; then
    echo ""
    echo "==============================================="
    echo -e "${GREEN}Running Video File Playback Tests (Release)${NC}"
    echo "==============================================="

    # Verify built-in test video exists
    if [ ! -f "tests/test-data/test.mp4" ]; then
        echo -e "${RED}✗ Built-in test video not found at tests/test-data/test.mp4${NC}"
        echo -e "${YELLOW}⚠ This should not happen - the test video is included in the repository${NC}"
        VIDEO_RESULT=1
    fi
    echo ""

    # Determine test executable path based on platform
    if isWindows; then
        VIDEO_EXECUTABLE="./build/tests/Release/ccap_file_playback_test.exe"
    else
        VIDEO_EXECUTABLE="./build/Release/tests/ccap_file_playback_test"
    fi

    if [ -f "$VIDEO_EXECUTABLE" ]; then
        echo -e "${YELLOW}Running video file playback tests in Release mode...${NC}"

        "$VIDEO_EXECUTABLE" $GTEST_FAIL_FAST_PARAM --gtest_output=xml:build/video_test_results_release.xml

        VIDEO_RESULT=$?

        if [ $VIDEO_RESULT -eq 0 ]; then
            echo -e "${GREEN}✓ Video file playback tests PASSED${NC}"
        else
            echo -e "${RED}✗ Video file playback tests FAILED${NC}"
            if [ "$EXIT_WHEN_FAILED" = true ]; then
                echo -e "${RED}❌ Exiting due to --exit-when-failed flag${NC}"
                exit 1
            fi
        fi
    else
        echo -e "${RED}Error: ccap_file_playback_test executable not found at $VIDEO_EXECUTABLE${NC}"
        echo -e "${YELLOW}Note: Video file playback may not be supported on this platform${NC}"
        echo -e "${YELLOW}      (only available on Windows and macOS with CCAP_ENABLE_FILE_PLAYBACK=ON)${NC}"
        if [ "$EXIT_WHEN_FAILED" = true ]; then
            echo -e "${RED}❌ Exiting due to --exit-when-failed flag${NC}"
            exit 1
        fi
        VIDEO_RESULT=1
    fi
fi

echo ""
echo "==============================================="
echo -e "${GREEN}Test Summary${NC}"
echo "==============================================="

# Only check results for tests that were actually run
OVERALL_RESULT=0

if [ "$RUN_FUNCTIONAL" = true ] && [ $TEST_RESULT -ne 0 ]; then
    OVERALL_RESULT=1
fi

if [ "$RUN_PERFORMANCE" = true ] && [ $PERF_RESULT -ne 0 ]; then
    OVERALL_RESULT=1
fi

if [ "$RUN_VIDEO" = true ] && [ $VIDEO_RESULT -ne 0 ]; then
    OVERALL_RESULT=1
fi

if [ $OVERALL_RESULT -eq 0 ]; then
    echo -e "${GREEN}🎉 All tests PASSED!${NC}"

    # Show what was tested
    if [ "$RUN_FUNCTIONAL" = true ]; then
        echo -e "${GREEN}  ✓ Functional tests (Debug mode)${NC}"
    fi
    if [ "$RUN_PERFORMANCE" = true ]; then
        if [ -n "$FILTER" ]; then
            echo -e "${GREEN}  ✓ AVX2 Performance tests (Release mode)${NC}"
        else
            echo -e "${GREEN}  ✓ Performance tests (Release mode)${NC}"
        fi
    fi
    if [ "$RUN_VIDEO" = true ]; then
        echo -e "${GREEN}  ✓ Video file playback tests (Release mode)${NC}"
    fi

    exit 0
else
    echo -e "${RED}❌ Some tests FAILED${NC}"
    if [ "$RUN_FUNCTIONAL" = true ] && [ $TEST_RESULT -ne 0 ]; then
        echo -e "${RED}  - Functional tests failed${NC}"
    fi
    if [ "$RUN_PERFORMANCE" = true ] && [ $PERF_RESULT -ne 0 ]; then
        echo -e "${RED}  - Performance tests failed${NC}"
    fi
    if [ "$RUN_VIDEO" = true ] && [ $VIDEO_RESULT -ne 0 ]; then
        echo -e "${RED}  - Video file playback tests failed${NC}"
    fi
    exit 1
fi
