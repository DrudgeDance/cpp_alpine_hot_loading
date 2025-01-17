# Alpine Endpoint

A statically linked C++ application built with CMake and Conan.

## Prerequisites

- CMake (>= 3.15)
- Conan (>= 2.0)
- GCC (>= 13)
- Make

## Project Structure

```
.
├── bin/              # Executable outputs
├── build/            # Build artifacts
│   ├── generators/   # CMake and Conan generated files
│   ├── lib/         # Static libraries
│   └── obj/         # Object files
├── include/          # Header files
├── scripts/         
│   └── build.sh     # Build script
└── src/             # Source files
```

## Building

The project uses Conan for dependency management and CMake for building. All dependencies are statically linked.

### Quick Start

```bash
# Make build script executable
chmod +x scripts/build.sh

# Build the project
./scripts/build.sh
```

The resulting binary will be placed in the `bin` directory.

### Manual Build Steps

If you prefer to build manually, follow these steps:

1. Set up build directories:
```bash
rm -rf build bin
mkdir -p build/generators build/lib build/obj bin
```

2. Install dependencies with Conan:
```bash
conan install . \
    --output-folder=build \
    --build=missing \
    -s build_type=Release \
    -g "CMakeDeps"
```

3. Configure with CMake:
```bash
cmake -B build -S . \
    -DCMAKE_TOOLCHAIN_FILE=build/generators/conan_toolchain.cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DCMAKE_EXE_LINKER_FLAGS="-static" \
    -DCMAKE_FIND_LIBRARY_SUFFIXES=".a" \
    -DBUILD_SHARED_LIBS=OFF \
    -G "Unix Makefiles"
```

4. Build:
```bash
cmake --build build
```

The binary will be output to `bin/your_app`.

## Dependencies

- Boost 1.83.0 (static linking)
  - system component
  - headers

## Static Linking

The application is configured for full static linking, including:
- Static linking of all dependencies
- Static linking of C++ runtime (libstdc++)
- Static linking of GCC runtime (libgcc)

You can verify static linking with:
```bash
ldd bin/your_app
```
If the command fails, it indicates successful static linking. 