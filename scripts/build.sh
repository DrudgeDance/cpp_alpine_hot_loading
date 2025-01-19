#!/bin/bash
set -e

# Change to project root directory
cd "$(dirname "$0")/.."

# Clean directories
rm -rf build bin
mkdir -p build/generators build/lib build/obj bin

echo "Installing dependencies with conan..."
conan install . \
    --output-folder=build \
    --build=missing \
    -s build_type=Release \
    -g "CMakeDeps"

echo "Configuring with CMake..."
cmake -B build -S . \
    -DCMAKE_TOOLCHAIN_FILE=build/generators/conan_toolchain.cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -G "Unix Makefiles"

echo "Building..."
cmake --build build -j$(nproc)

echo "Build complete! Binary is at bin/webserver"

cd .. 