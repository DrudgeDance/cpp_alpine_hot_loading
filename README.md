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

The development environment uses an advanced permission management system split into two components:

#### Initial Setup
During container creation, `initial-setup.sh` runs once to:
- Set up the base directory structure with correct permissions
- Configure the `scripts` directory with SetGID bit (2755)
- Set initial permissions for all existing files
- Launch the continuous permission watcher

#### Continuous Permission Management
A background watcher (`watch-permissions.sh`) automatically maintains permissions:
- Monitors the `scripts` directory for any changes
- Automatically sets correct permissions when files are:
  - Created
  - Modified
  - Moved into the directory
- Makes shell scripts (*.sh) executable (700) automatically
- Sets appropriate permissions (644) for non-script files

#### Directory Permissions
The environment maintains specific permissions for each directory type:
- `scripts/`: 
  - Directory has SetGID bit (2755)
  - Shell scripts automatically get 700 (rwx------)
  - Other files automatically get 644 (rw-r--r--)
- `src/`: Source files (644) with traversable directories (755)
- `include/`: Header files (644) with traversable directories (755)
- `build/`: Build artifacts (755) owned by developer
- `bin/`: Binaries (755) owned by appuser for deployment testing

#### User Permissions
- `developer` user:
  - Owner of all source code and build files
  - Full sudo privileges for development tasks
  - Can execute all commands and scripts
- `appuser` user:
  - Restricted permissions simulating production
  - Can only execute compiled binaries
  - Used for testing deployment scenarios

This setup ensures:
1. Scripts are always executable without manual intervention
2. Source code and build files are properly protected
3. Compiled binaries can be tested with production-like permissions
4. No manual permission management needed during development
5. Permissions are maintained automatically, even for newly added files 