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
    -DCMAKE_EXE_LINKER_FLAGS="-static" \
    -DCMAKE_FIND_LIBRARY_SUFFIXES=".a" \
    -DBUILD_SHARED_LIBS=OFF \
    -G "Unix Makefiles"

echo "Building..."
cmake --build build

echo "Build complete! Binary is at bin/your_app"

echo -e "\nChecking if binary is statically linked:"
cd bin
if ! ldd your_app &> /dev/null; then
    echo "Success: Binary appears to be fully static!"
    
    echo -e "\nTesting application with appuser permissions:"
    # Run as appuser (using full command instead of alias)
    if ! sudo -u appuser ./your_app; then
        echo "Error: Application crashed when running as appuser!"
        cd ..
        exit 1
    fi
    cd ..
else
    echo "Warning: Binary might not be fully static!"
    cd ..
    exit 1
fi

echo -e "\nAll tests passed successfully!" 