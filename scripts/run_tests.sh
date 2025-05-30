#!/bin/bash

# CCAP Unit Tests Runner
# This script builds and runs all ccap unit tests

set -e # Exit on any error

echo "==============================================="
echo "CCAP Unit Tests Runner"
echo "==============================================="

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
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

cd build

cd build/Release
echo -e "${BLUE}Configuring CMake (Release)...${NC}"
cmake ../.. -DCMAKE_BUILD_TYPE=Release -DCCAP_BUILD_TESTS=ON

echo -e "${BLUE}Building project...${NC}"
make -j$(nproc)

echo -e "${BLUE}Building tests...${NC}"
make -j$(nproc) ccap_convert_test ccap_performance_test

echo ""
echo "==============================================="
echo -e "${GREEN}Running Unit Tests${NC}"
echo "==============================================="

# Run unit tests
if [ -f "./tests/ccap_convert_test" ]; then
    echo -e "${YELLOW}Running functional tests...${NC}"
    ./tests/ccap_convert_test --gtest_output=xml:test_results.xml
    TEST_RESULT=$?

    if [ $TEST_RESULT -eq 0 ]; then
        echo -e "${GREEN}✓ Functional tests PASSED${NC}"
    else
        echo -e "${RED}✗ Functional tests FAILED${NC}"
    fi
else
    echo -e "${RED}Error: ccap_convert_test executable not found${NC}"
    TEST_RESULT=1
fi

echo ""
echo "==============================================="
echo -e "${GREEN}Running Performance Tests${NC}"
echo "==============================================="

# Run performance tests
if [ -f "./tests/ccap_performance_test" ]; then
    echo -e "${YELLOW}Running performance benchmarks...${NC}"
    ./tests/ccap_performance_test --gtest_output=xml:performance_results.xml
    PERF_RESULT=$?

    if [ $PERF_RESULT -eq 0 ]; then
        echo -e "${GREEN}✓ Performance tests PASSED${NC}"
    else
        echo -e "${RED}✗ Performance tests FAILED${NC}"
    fi
else
    echo -e "${RED}Error: ccap_performance_test executable not found${NC}"
    PERF_RESULT=1
fi

echo ""
echo "==============================================="
echo -e "${GREEN}Test Summary${NC}"
echo "==============================================="

if [ $TEST_RESULT -eq 0 ] && [ $PERF_RESULT -eq 0 ]; then
    echo -e "${GREEN}🎉 All tests PASSED!${NC}"
    exit 0
else
    echo -e "${RED}❌ Some tests FAILED${NC}"
    if [ $TEST_RESULT -ne 0 ]; then
        echo -e "${RED}  - Functional tests failed${NC}"
    fi
    if [ $PERF_RESULT -ne 0 ]; then
        echo -e "${RED}  - Performance tests failed${NC}"
    fi
    exit 1
fi
