# Hot-Reloadable Web Server

A modern C++ web server that demonstrates dynamic plugin loading and hot-reloading capabilities. The server allows endpoints to be modified and recompiled while the server is running, with changes taking effect immediately without restart. Built using Boost.Beast for HTTP handling and Linux's inotify for efficient filesystem monitoring, it serves as both a practical web server and an example of advanced C++ features like dynamic loading and real-time code updates.

## Dependencies

Core dependencies:
- Boost 1.84.0 (for Asio networking library and filesystem)
- CMake 3.15+ (build system)
- Conan 2.x (package manager)
- GCC 11+ (C++17 support)

Transitive dependencies (automatically handled by Conan):
- OpenSSL (required by Boost, though not directly used)
- zlib (compression, required by Boost)
- bzip2 (compression, required by Boost)

## Building the Project

The project includes two main build scripts:

1. `build.sh` - Builds the entire project including the server and initial plugins:
```bash
./build.sh
```

2. `rebuild_plugin.sh` - Rebuilds only the endpoint plugins for hot loading:
```bash
./rebuild_plugin.sh
```

For a manual build process:
```bash
rm -rf build
mkdir build
cd build
conan install .. --output-folder . --build=missing
source conanbuild.sh
cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

## Running the Server

From the project root:
```bash
cd bin
./webserver 0.0.0.0 8080 1
```

The arguments are:
- Host address (0.0.0.0 to accept connections from any IP)
- Port number (8080 in this example)
- Number of threads (1 in this example)

## Testing Hot Reload Functionality

1. Start the server:
```bash
cd bin
./webserver 0.0.0.0 8080 1
```

2. In a separate terminal, test the initial endpoint:
```bash
curl http://localhost:8080/hello
```

3. Modify the endpoint code in `src/plugins/endpoints/HelloEndpoint.cpp`. For example:
```cpp
response_.body() = "Hello, Hot Reload! Build: " + std::to_string(BUILD_NUMBER);
```

4. Rebuild only the plugin (keep the server running):
```bash
./rebuild_plugin.sh
```
This script will:
- Increment the build counter
- Rebuild only the plugin files
- Add a timestamp to the plugin filename
- Copy the new plugin to the monitored directory

5. Test again to see the changes:
```bash
curl http://localhost:8080/hello
```

The response will show:
- Updated message
- New build number
- New build timestamp
- Current timestamp

The server automatically:
- Detects the new plugin file
- Creates a backup of the current plugin
- Loads the new version
- Unloads the old version
- Maintains backups for rollback if needed

## Plugin System Features

The hot-reload system provides:
- Automatic plugin detection and loading
- Backup management (keeps recent versions)
- Rollback capability if a new version fails
- Build number tracking
- Timestamp-based versioning
- Efficient file monitoring using inotify

## Development

To develop new endpoints:
1. Create a new .cpp file in src/plugins/endpoints/
2. Implement the Plugin interface
3. Build using `rebuild_plugin.sh`
4. The endpoint will be automatically detected and loaded

## Troubleshooting

If you encounter "accept: Invalid argument" errors:
- This is normal during high-frequency rebuilds
- The server implements a retry mechanism with backoff
- The errors should not affect functionality

For build issues:
- Ensure all Perl dependencies are installed (perl-FindBin, perl-IPC-Cmd, perl-Digest-SHA)
- Check that OpenSSL development libraries are available
- Verify Conan has installed all required dependencies 

If plugins aren't loading:
- Check the server output for specific error messages
- Verify the plugin file exists in bin/endpoints/
- Ensure the plugin has the correct permissions
- Look for backup files if a restore is needed 