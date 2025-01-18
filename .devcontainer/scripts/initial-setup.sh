#!/bin/bash

# Function to set up proper permissions for development files
setup_dev_permissions() {
    local developer_user="developer"
    local developer_group="developer"
    local service_user="appuser"
    local service_group="appgroup"

    # Set up scripts directory with setgid bit
    if [ -d "scripts" ]; then
        # Set ownership
        sudo chown -R ${developer_user}:${developer_group} scripts/
        # Set setgid bit on directory (2755 = rwxr-sr-x)
        sudo chmod 2755 scripts/
        # Make all current .sh files executable
        find scripts -type f -name "*.sh" -exec sudo chmod 700 {} \;
        
        # Start the permissions watcher in the background
        .devcontainer/scripts/watch-permissions.sh &
    fi

    # Set permissions for build directories and artifacts
    if [ -d "build" ]; then
        sudo chown -R ${developer_user}:${developer_group} build/
        sudo chmod -R 755 build/
    fi

    # Set permissions for binary output directories
    if [ -d "bin" ]; then
        # Developer owns the directory
        sudo chown -R ${developer_user}:${developer_group} bin/
        # But executables should be runnable by appuser
        find bin -type f -executable -exec sudo chown ${service_user}:${service_group} {} \; -exec sudo chmod 755 {} \;
    fi

    # Set source code permissions
    if [ -d "src" ]; then
        sudo chown -R ${developer_user}:${developer_group} src/
        sudo chmod -R 644 src/
        find src -type d -exec sudo chmod 755 {} \;
    fi

    # Set include permissions
    if [ -d "include" ]; then
        sudo chown -R ${developer_user}:${developer_group} include/
        sudo chmod -R 644 include/
        find include -type d -exec sudo chmod 755 {} \;
    fi
}

alias setup-permissions='setup_dev_permissions'

# Run it once when container starts if we're in the workspace
if [ -d /workspace ]; then
    cd /workspace && setup_dev_permissions
fi 