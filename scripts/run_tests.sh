#!/bin/bash

# CCAP Unit Tests Runner
# This script builds and runs all ccap unit tests
# Functional tests run in Debug mode, Performance tests run in Release mode
#
# Platform-specific behavior:
# - Windows (MSVC): Uses single build directory, configs specified during build
# - Linux/Mac: Uses separate build/Debug and build/Release directories
#
# Usage:
#   ./run_tests.sh                    # Run all tests
#   ./run_tests.sh --functional       # Run only functional tests
#   ./run_tests.sh --performance      # Run only performance tests
#   ./run_tests.sh --avx2             # Run only AVX2 performance tests
#   ./run_tests.sh --shuffle          # Run only tests with names containing 'shuffle' (case-insensitive) in functional tests
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
RUN_FUNCTIONAL=true
RUN_PERFORMANCE=true
SKIP_BUILD=false
EXIT_WHEN_FAILED=false
GTEST_FAIL_FAST_PARAM=""
FILTER=""
SHUFFLE_ONLY=false

while [[ $# -gt 0 ]]; do
    case $1 in
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
    --help)
        echo "CCAP Unit Tests Runner"
        echo ""
        echo "Usage:"
        echo "  $0                    # Run all tests"
        echo "  $0 --functional       # Run only functional tests (Debug mode)"
        echo "  $0 --performance      # Run only performance tests (Release mode)"
        echo "  $0 --avx2             # Run only AVX2 performance tests (Release mode)"
        echo "  $0 --shuffle          # Run only tests whose names contain '*shuffle*' or '*Shuffle*' in functional tests"
        echo "  $0 --skip-build       # Skip build step, run tests only"
        echo "  $0 --exit-when-failed # Stop at first test failure (gtest fail fast mode)"
        echo "  $0 --help             # Show this help"
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

# Build Debug version for functional tests
if [ "$RUN_FUNCTIONAL" = true ]; then
    echo ""
    echo -e "${PURPLE}===============================================${NC}"
    if [ "$SKIP_BUILD" = true ]; then
        echo -e "${BLUE}Skipping build, using existing Debug binaries${NC}"
    else
        echo -e "${BLUE}Building Debug version (for functional tests)${NC}"
    fi
    echo -e "${PURPLE}===============================================${NC}"

    if [ "$SKIP_BUILD" = false ]; then
        if isWindows; then
            # Windows MSVC: use single build directory, specify config during build
            cd build
            echo -e "${BLUE}Configuring CMake (Windows MSVC)...${NC}"
            cmake .. -DCCAP_BUILD_TESTS=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5

            echo -e "${BLUE}Building Debug project...${NC}"
            cmake --build . --config Debug --parallel $(detectCores)

            echo -e "${BLUE}Building Debug tests...${NC}"
            cmake --build . --config Debug --target ccap_convert_test --parallel $(detectCores)
            cd ..
        else
            # Linux/Mac: use separate Debug directory
            cd build/Debug
            echo -e "${BLUE}Configuring CMake (Debug)...${NC}"
            cmake ../.. -DCMAKE_BUILD_TYPE=Debug -DCCAP_BUILD_TESTS=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5

            echo -e "${BLUE}Building Debug project...${NC}"
            cmake --build . --config Debug --parallel $(detectCores)

            echo -e "${BLUE}Building Debug tests...${NC}"
            cmake --build . --config Debug --target ccap_convert_test --parallel $(detectCores)
            cd ../..
        fi
    fi
fi

# Build Release version for performance tests
if [ "$RUN_PERFORMANCE" = true ]; then
    echo ""
    echo -e "${PURPLE}===============================================${NC}"
    if [ "$SKIP_BUILD" = true ]; then
        echo -e "${BLUE}Skipping build, using existing Release binaries${NC}"
    else
        echo -e "${BLUE}Building Release version (for performance tests)${NC}"
    fi
    echo -e "${PURPLE}===============================================${NC}"

    if [ "$SKIP_BUILD" = false ]; then
        if isWindows; then
            # Windows MSVC: use single build directory, specify config during build
            cd build
            # Only configure if not already configured
            if [ ! -f "CMakeCache.txt" ]; then
                echo -e "${BLUE}Configuring CMake (Windows MSVC)...${NC}"
                cmake .. -DCCAP_BUILD_TESTS=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5
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
            cmake ../.. -DCMAKE_BUILD_TYPE=Release -DCCAP_BUILD_TESTS=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5

            echo -e "${BLUE}Building Release project...${NC}"
            cmake --build . --config Release --parallel $(detectCores)

            echo -e "${BLUE}Building Release tests...${NC}"
            cmake --build . --config Release --target ccap_performance_test --parallel $(detectCores)
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
    else
        TEST_EXECUTABLE="./Debug/tests/ccap_convert_test"
    fi

    if [ -f "$TEST_EXECUTABLE" ]; then
        echo -e "${YELLOW}Running functional tests in Debug mode...${NC}"

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

if [ $OVERALL_RESULT -eq 0 ]; then
    echo -e "${GREEN}🎉 All tests PASSED!${NC}"

    # Show what was tested
    if [ "$RUN_FUNCTIONAL" = true ] && [ "$RUN_PERFORMANCE" = true ]; then
        echo -e "${GREEN}  ✓ Functional tests (Debug mode)${NC}"
        echo -e "${GREEN}  ✓ Performance tests (Release mode)${NC}"
    elif [ "$RUN_FUNCTIONAL" = true ]; then
        echo -e "${GREEN}  ✓ Functional tests (Debug mode)${NC}"
    elif [ "$RUN_PERFORMANCE" = true ]; then
        if [ -n "$FILTER" ]; then
            echo -e "${GREEN}  ✓ AVX2 Performance tests (Release mode)${NC}"
        else
            echo -e "${GREEN}  ✓ Performance tests (Release mode)${NC}"
        fi
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
    exit 1
fi
