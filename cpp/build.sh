#!/bin/bash

# Install dependencies with Conan
conan install . --output-folder=build --build=missing

# Source the Conan environment
cd build
source conanbuild.sh

# Configure CMake using the toolchain file and proper module paths
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_MODULE_PATH=$(pwd) \
    -DCMAKE_PREFIX_PATH=$(pwd)

# Build the project
cmake --build . -j$(nproc)

# Deactivate Conan environment
source deactivate_conanbuild.sh
cd .. 