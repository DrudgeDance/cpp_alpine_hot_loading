#!/bin/bash
set -e

# Change to project root directory
cd "$(dirname "$0")/.."

echo "Installing dependencies with conan..."
conan install . --output-folder=build/build/Release --build=missing

echo "Configuring with CMake..."
cmake -B build -S . \
    -DCMAKE_TOOLCHAIN_FILE=build/build/Release/generators/conan_toolchain.cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

echo "Building..."
cmake --build build

echo "Build complete! Binary is at build/your_app"

echo -e "\nChecking if binary is statically linked:"
if ldd build/your_app &> /dev/null; then
    echo "Warning: Binary might not be fully static!"
else
    echo "Success: Binary appears to be fully static!"
fi

cd build && ./your_app 