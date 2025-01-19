#!/bin/bash
set -e

# Change to project root directory
cd "$(dirname "$0")/.."

echo "Cleaning build artifacts..."
rm -rf build/CMakeFiles
mkdir -p build

# Initialize counter if it doesn't exist
if [ ! -f build_counter.txt ]; then
    echo "1" > build_counter.txt
fi

echo "Current build counter value:"
BUILD_NUM=$(cat build_counter.txt)
echo $BUILD_NUM

echo "Installing dependencies with conan..."
conan install . \
    --output-folder=build \
    --build=missing \
    -s build_type=Release \
    -g "CMakeDeps"

echo "Rebuilding plugin..."
cmake -B build -S . \
    -DCMAKE_TOOLCHAIN_FILE=build/generators/conan_toolchain.cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DBUILD_NUMBER=$BUILD_NUM \
    -G "Unix Makefiles"

# Wait for CMake to generate files
sleep 1

# Get the most recent plugin target name
PLUGIN_TARGET=$(find build/CMakeFiles -maxdepth 1 -type d -name 'hello_endpoint_*' | sort -r | head -n1 | xargs basename | sed 's/\.dir$//')

if [ -z "$PLUGIN_TARGET" ]; then
    echo "Error: Could not find plugin target"
    exit 1
fi

echo "Building target: $PLUGIN_TARGET"
cmake --build build --target "$PLUGIN_TARGET" -j$(nproc)

# Only increment counter after successful build
echo $((BUILD_NUM + 1)) > build_counter.txt

echo "New build counter value:"
cat build_counter.txt

echo "Forcing filesystem sync..."
sync

echo "Setting permissions..."
chmod 755 bin/endpoints/libhello_endpoint_*.so

echo "Forcing another sync..."
sync

echo "Plugin rebuilt and synced"
ls -l bin/endpoints/libhello_endpoint_*.so