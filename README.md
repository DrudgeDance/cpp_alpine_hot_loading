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

## Development Environment

### User Setup

The development environment uses two distinct users for security and deployment testing:

1. `developer` user:
   - Primary development user with sudo privileges
   - Owner of source code, build files, and development tools
   - Full access to all development operations
   - Default user in the dev container

2. `appuser` user:
   - Restricted user for testing deployment scenarios
   - Can only execute the compiled binaries
   - Simulates production environment permissions
   - Helps identify permission-related issues early

### User Commands

#### Interactive Use (Terminal)
The environment provides convenient aliases for switching between users in the terminal:

```bash
# Run a command as appuser (simulates production environment)
run-as-app ./bin/your_app

# Switch back to developer mode (if needed)
dev-mode

# Check which user you're currently running as
whoami

# Check which user is running your app (Alpine/BusyBox version)
ps -o user,pid,comm | grep "your_app"

# Alternative: see all processes with full details
ps aux | grep "your_app"
```

Note: When using `ps aux | grep "your_app"` after running with `run-as-app` or `sudo -u appuser`, you'll see multiple processes:
- Two `root` processes: These are the `sudo` parent processes that handle the user switching
- One `appuser` process: This is your actual application running as `appuser`
- One process owned by `developer`: This is the `grep` command itself

The `root` processes are normal and expected - they're part of sudo's security model for switching users.

#### In Scripts
When writing shell scripts, use the full commands instead of aliases:

```bash
# Run a command as appuser
sudo -u appuser ./bin/your_app

# Run a command as developer
sudo -u developer ./command
```

### File Permissions

The development environment automatically manages permissions:
- Source files (*.cpp, *.h): owned by developer (644)
- Build directories: owned by developer (755)
- Shell scripts: owned by developer, executable only by developer (700)
- Compiled binaries: owned by appuser, executable by both users (755) 