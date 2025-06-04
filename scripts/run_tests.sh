#!/bin/bash

# CCAP Unit Tests Runner
# This script builds and runs all ccap unit tests
# Functional tests run in Debug mode, Performance tests run in Release mode
#
# Usage:
#   ./run_tests.sh                    # Run all tests
#   ./run_tests.sh --functional       # Run only functional tests
#   ./run_tests.sh --performance      # Run only performance tests
#   ./run_tests.sh --avx2             # Run only AVX2 performance tests
#   ./run_tests.sh --help             # Show help

set -e # Exit on any error

# Parse command line arguments
RUN_FUNCTIONAL=true
RUN_PERFORMANCE=true
FILTER=""

while [[ $# -gt 0 ]]; do
    case $1 in
    --functional)
        RUN_FUNCTIONAL=true
        RUN_PERFORMANCE=false
        shift
        ;;
    --performance)
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
    --help)
        echo "CCAP Unit Tests Runner"
        echo ""
        echo "Usage:"
        echo "  $0                    # Run all tests"
        echo "  $0 --functional       # Run only functional tests (Debug mode)"
        echo "  $0 --performance      # Run only performance tests (Release mode)"
        echo "  $0 --avx2             # Run only AVX2 performance tests (Release mode)"
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

cd "$(dirname "$0")/.."

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ] || [ ! -d "tests" ]; then
    echo -e "${RED}Error: Please run this script from the ccap root directory${NC}"
    exit 1
fi

# Create build directories if they don't exist
echo -e "${BLUE}Setting up build directories...${NC}"
mkdir -p build/Debug
mkdir -p build/Release

# Initialize results
TEST_RESULT=0
PERF_RESULT=0

# Build Debug version for functional tests
if [ "$RUN_FUNCTIONAL" = true ]; then
    echo ""
    echo -e "${PURPLE}===============================================${NC}"
    echo -e "${BLUE}Building Debug version (for functional tests)${NC}"
    echo -e "${PURPLE}===============================================${NC}"

    cd build/Debug
    echo -e "${BLUE}Configuring CMake (Debug)...${NC}"
    cmake ../.. -DCMAKE_BUILD_TYPE=Debug -DCCAP_BUILD_TESTS=ON

    echo -e "${BLUE}Building Debug project...${NC}"
    cmake --build . --config Debug --parallel $(detectCores)

    echo -e "${BLUE}Building Debug tests...${NC}"
    cmake --build . --config Debug --target ccap_convert_test --parallel $(detectCores)
    cd ../..
fi

# Build Release version for performance tests
if [ "$RUN_PERFORMANCE" = true ]; then
    echo ""
    echo -e "${PURPLE}===============================================${NC}"
    echo -e "${BLUE}Building Release version (for performance tests)${NC}"
    echo -e "${PURPLE}===============================================${NC}"

    cd build/Release
    echo -e "${BLUE}Configuring CMake (Release)...${NC}"
    cmake ../.. -DCMAKE_BUILD_TYPE=Release -DCCAP_BUILD_TESTS=ON

    echo -e "${BLUE}Building Release project...${NC}"
    make -j$(nproc)

    echo -e "${BLUE}Building Release tests...${NC}"
    make -j$(nproc) ccap_performance_test
    cd ../..
fi

# Run unit tests in Debug mode
if [ "$RUN_FUNCTIONAL" = true ]; then
    echo ""
    echo "==============================================="
    echo -e "${GREEN}Running Functional Tests (Debug)${NC}"
    echo "==============================================="

    if [ -f "./build/Debug/tests/ccap_convert_test" ]; then
        echo -e "${YELLOW}Running functional tests in Debug mode...${NC}"
        ./build/Debug/tests/ccap_convert_test --gtest_output=xml:build/Debug/test_results.xml
        TEST_RESULT=$?

        if [ $TEST_RESULT -eq 0 ]; then
            echo -e "${GREEN}✓ Functional tests PASSED${NC}"
        else
            echo -e "${RED}✗ Functional tests FAILED${NC}"
        fi
    else
        echo -e "${RED}Error: ccap_convert_test executable not found in Debug build${NC}"
        TEST_RESULT=1
    fi
fi

# Run performance tests in Release mode
if [ "$RUN_PERFORMANCE" = true ]; then
    echo ""
    echo "==============================================="
    echo -e "${GREEN}Running Performance Tests (Release)${NC}"
    echo "==============================================="

    if [ -f "./build/Release/tests/ccap_performance_test" ]; then
        echo -e "${YELLOW}Running performance benchmarks in Release mode...${NC}"
        echo -e "${BLUE}Note: Release mode provides accurate performance measurements${NC}"
        if [ -n "$FILTER" ]; then
            echo -e "${BLUE}Filter: $FILTER${NC}"
            ./build/Release/tests/ccap_performance_test $FILTER --gtest_output=xml:build/Release/performance_results.xml
        else
            ./build/Release/tests/ccap_performance_test --gtest_output=xml:build/Release/performance_results.xml
        fi
        PERF_RESULT=$?

        if [ $PERF_RESULT -eq 0 ]; then
            echo -e "${GREEN}✓ Performance tests PASSED${NC}"
        else
            echo -e "${RED}✗ Performance tests FAILED${NC}"
        fi
    else
        echo -e "${RED}Error: ccap_performance_test executable not found in Release build${NC}"
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
