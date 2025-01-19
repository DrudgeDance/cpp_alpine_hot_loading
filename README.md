# Hot-Reloadable Alpine Web Server

A modern C++ web server built for Alpine Linux that demonstrates dynamic plugin loading and hot-reloading capabilities. The server allows endpoints to be modified and recompiled while the server is running, with changes taking effect immediately without restart. Built using Boost.Beast for HTTP handling, it serves as both a practical web server and an example of advanced C++ features like dynamic loading and real-time code updates.

## Dependencies

Core dependencies:
- CMake (>= 3.15)
- Conan (>= 2.0)
- GCC (>= 13)
- Boost 1.84.0
- OpenSSL 3.2.0

## Project Structure

```
.
├── bin/              # Executable outputs
│   └── endpoints/    # Plugin directory
├── build/            # Build artifacts
│   ├── generators/   # CMake and Conan generated files
│   ├── lib/         # Static libraries
│   └── obj/         # Object files
├── include/          # Header files
├── scripts/         
│   ├── build.sh     # Main build script
│   └── rebuild_plugin.sh # Plugin hot-reload script
└── src/             # Source files
```

## Building and Running

The project includes two main build scripts:

1. `build.sh` - Builds the entire project including the server and initial plugins:
```bash
./scripts/build.sh
```

2. `rebuild_plugin.sh` - Rebuilds only the endpoint plugins for hot loading:
```bash
./scripts/rebuild_plugin.sh
```

### Running the Server

From the project root:
```bash
cd bin
./webserver 0.0.0.0 8080 1
```

Arguments:
- Host address (0.0.0.0 to accept connections from any IP)
- Port number (8080 in this example)
- Number of threads (1 in this example)

## Testing Hot Reload Functionality

1. Start the server:
```bash
cd bin
./webserver 0.0.0.0 8080 1
```

2. Test the initial endpoint:
```bash
curl http://localhost:8080/hello
```

3. Modify the endpoint code in `src/plugins/endpoints/HelloEndpoint.cpp`

4. Rebuild only the plugin (keep the server running):
```bash
./scripts/rebuild_plugin.sh
```

5. Test again to see the changes:
```bash
curl http://localhost:8080/hello
```

The server will automatically:
- Detect the new plugin file
- Load the new version
- Unload the old version

## Development Environment

The development environment uses two distinct users for security and deployment testing:

1. `developer` user:
   - Primary development user with sudo privileges
   - Owner of source code and build files
   - Full access to all development operations

2. `appuser` user:
   - Restricted user for testing deployment scenarios
   - Can only execute the compiled binaries
   - Simulates production environment permissions

### File Permissions

- **Binary Files**:
  - Permission: 755 (rwxr-xr-x)
  - Owner: appuser:appgroup
  - Location: bin/

- **Source Files**:
  - Permission: 644 (rw-r--r--)
  - Owner: developer:developer
  - Location: src/, include/

## Troubleshooting

If plugins aren't loading:
- Check the server output for specific error messages
- Verify the plugin file exists in bin/endpoints/
- Ensure the plugin has the correct permissions (755)
- Check that all Perl dependencies are installed for OpenSSL compilation 