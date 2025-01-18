#!/bin/bash

# Source common configuration
source "$(dirname "$0")/config.sh"

# Function to set up proper permissions for development files
setup_dev_permissions() {
    echo "Setting up development permissions..."

    # Set permissions for build directories and artifacts
    if [ -d "build" ]; then
        echo "Setting up build directory..."
        sudo chown -R ${DEVELOPER_USER}:${DEVELOPER_GROUP} build/ || echo "Failed to set build ownership"
        sudo chmod -R 755 build/ || echo "Failed to set build permissions"
    fi

    # Set source code permissions
    if [ -d "src" ]; then
        echo "Setting up source directory..."
        sudo chown -R ${DEVELOPER_USER}:${DEVELOPER_GROUP} src/ || echo "Failed to set src ownership"
        sudo find src -type f -exec sudo chmod 644 {} \; || echo "Failed to set src file permissions"
        sudo find src -type d -exec sudo chmod 755 {} \; || echo "Failed to set src directory permissions"
    fi

    # Set include permissions
    if [ -d "include" ]; then
        echo "Setting up include directory..."
        sudo chown -R ${DEVELOPER_USER}:${DEVELOPER_GROUP} include/ || echo "Failed to set include ownership"
        sudo find include -type f -exec sudo chmod 644 {} \; || echo "Failed to set include file permissions"
        sudo find include -type d -exec sudo chmod 755 {} \; || echo "Failed to set include directory permissions"
    fi

    # Start the permissions watcher in the background with nohup and proper redirection
    echo "Starting permissions watcher..."
    sudo chmod +x "$(dirname "$0")/watch-permissions.sh" || echo "Failed to make watcher executable"
    nohup sudo -u ${DEVELOPER_USER} "$(dirname "$0")/watch-permissions.sh" > /tmp/watch-permissions.log 2>&1 &
    local watcher_pid=$!
    echo "Watcher started with PID: ${watcher_pid}"
    
    # Verify watcher is running (BusyBox compatible)
    sleep 1  # Give the process a moment to start
    if ps | grep -v grep | grep -q "watch-permissions.sh"; then
        echo "Watcher successfully started"
        # Set up initial permissions for scripts directory if it exists
        if [ -d "scripts" ]; then
            echo "Setting up initial scripts directory permissions..."
            sudo chown root:${DEVELOPER_GROUP} scripts/
            sudo chmod 2755 scripts/  # SetGID with r-x for group
            sudo find scripts -type f -name "*.sh" -exec chmod 700 {} \;
            sudo find scripts -type f -not -name "*.sh" -exec chmod 644 {} \;
        fi
    else
        echo "Warning: Watcher may not have started properly"
    fi
}

# Run setup if in workspace
if [ -d /workspaces/alpine_endpoint ] || [ -d /workspace ]; then
    cd /workspaces/alpine_endpoint 2>/dev/null || cd /workspace
    setup_dev_permissions
fi 