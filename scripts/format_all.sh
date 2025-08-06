#!/usr/bin/env bash

cd "$(dirname "$0")/.."

# Run clang-format on src, tests, and examples directories
# Exclude the examples/desktop/glfw and examples/desktop/glad directories
find src examples \
    -type f \
    \( -name "*.c" -o -name "*.cpp" -o -name "*.cc" -o -name "*.cxx" -o -name "*.h" -o -name "*.hpp" -o -name "*.hxx" \) \
    -not -path "examples/desktop/glfw/*" \
    -not -path "examples/desktop/glad/*" \
    -exec clang-format -i {} +

echo "Code formatting completed!"
