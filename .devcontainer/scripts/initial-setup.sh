#!/bin/bash

# Function to set up proper permissions for development files
setup_dev_permissions() {
    local developer_user="developer"
    local developer_group="developer"

    # Set permissions for build directories and artifacts
    if [ -d "build" ]; then
        sudo chown -R ${developer_user}:${developer_group} build/
        sudo chmod -R 755 build/
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

    # Start the permissions watcher in the background
    nohup $(dirname "$0")/watch-permissions.sh > /dev/null 2>&1 &
}

# Run setup if in workspace
if [ -d /workspaces/alpine_endpoint ] || [ -d /workspace ]; then
    cd /workspaces/alpine_endpoint 2>/dev/null || cd /workspace
    setup_dev_permissions
fi 