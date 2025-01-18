#!/bin/bash

# Debug logging only if DEBUG is set
if [ -n "$DEBUG" ]; then
    set -x
    exec 1> >(tee -a "/tmp/initial-setup-debug.log") 2>&1
else
    # Cleaner output for normal operation
    exec 1> >(tee -a "/tmp/initial-setup.log") 2>&1
fi

# Check if we're being run through sudo
if [ -z "$SUDO_COMMAND" ] && [ "$(id -u)" != "0" ]; then
    exec sudo "$0" "$@"
fi

# Source common configuration
source "$(dirname "$0")/config.sh"

# Function to print status messages
status() {
    echo -e "\n$1"
}

# Function to print success messages
success() {
    echo -e "✅ $1\n"
}

# Function to print error messages
error() {
    echo -e "❌ $1\n" >&2
}

# Function to set up proper permissions for development files
setup_dev_permissions() {
    status "Setting up development environment..."

    # Set permissions for build directories and artifacts
    if [ -d "build" ]; then
        status "Configuring build directory"
        chown -R ${DEVELOPER_USER}:${DEVELOPER_GROUP} build/ && \
        chmod -R 755 build/ && \
        success "Build directory configured" || error "Failed to configure build directory"
    fi

    # Set source code permissions
    if [ -d "src" ]; then
        status "Configuring source directory"
        chown -R ${DEVELOPER_USER}:${DEVELOPER_GROUP} src/ && \
        find src -type f -exec chmod 644 {} \; && \
        find src -type d -exec chmod 755 {} \; && \
        success "Source directory configured" || error "Failed to configure source directory"
    fi

    # Set include permissions
    if [ -d "include" ]; then
        status "Configuring include directory"
        chown -R ${DEVELOPER_USER}:${DEVELOPER_GROUP} include/ && \
        find include -type f -exec chmod 644 {} \; && \
        find include -type d -exec chmod 755 {} \; && \
        success "Include directory configured" || error "Failed to configure include directory"
    fi

    # Set up scripts directory if it exists
    if [ -d "scripts" ]; then
        status "Configuring scripts directory"
        chown root:${DEVELOPER_GROUP} scripts/ && \
        chmod 2755 scripts/ && \
        find scripts -type f -name "*.sh" -exec chmod 700 {} \; && \
        find scripts -type f -not -name "*.sh" -exec chmod 644 {} \; && \
        chown -R ${DEVELOPER_USER}:${DEVELOPER_GROUP} scripts/* && \
        success "Scripts directory configured" || error "Failed to configure scripts directory"
    fi

    # Set up bin directory if it exists
    if [ -d "bin" ]; then
        status "Configuring bin directory"
        chown root:${DEVELOPER_GROUP} bin/ && \
        chmod 2775 bin/ && \
        find bin -type f -exec chmod 755 {} \; && \
        find bin -type f -exec chown ${SERVICE_USER}:${SERVICE_GROUP} {} \; && \
        success "Bin directory configured" || error "Failed to configure bin directory"
    fi

    # Kill any existing watchers
    status "Managing permission watcher"
    if [ -f /tmp/watch-permissions.pid ]; then
        local old_pid=$(cat /tmp/watch-permissions.pid)
        if [ -n "$old_pid" ]; then
            kill -TERM "$old_pid" 2>/dev/null || true
            rm -f /tmp/watch-permissions.pid
        fi
    fi
    pkill -f watch-permissions.sh 2>/dev/null || true
    sleep 1

    # Start the permissions watcher in the background
    local watcher_script="$(dirname "$0")/watch-permissions.sh"
    chmod +x "$watcher_script" || {
        error "Failed to make watcher executable"
        return 1
    }
    
    # Start watcher directly (it will handle sudo itself)
    setsid "$watcher_script" > /tmp/watch-permissions.log 2>&1 &
    local watcher_pid=$!
    
    # Verify watcher is running (BusyBox compatible)
    sleep 2  # Give the process a moment to start
    
    if ps | grep -v grep | grep -q "watch-permissions.sh"; then
        success "Permission watcher started successfully"
        if [ -n "$DEBUG" ]; then
            echo "Last 10 lines of watcher log:"
            tail -n 10 /tmp/watch-permissions.log
        fi
        return 0
    else
        error "Permission watcher failed to start"
        if [ -n "$DEBUG" ]; then
            echo "Watcher log contents:"
            cat /tmp/watch-permissions.log
            echo "Process list:"
            ps aux
        fi
        return 1
    fi
}

# Run setup if in workspace
if [ -d /workspaces/alpine_endpoint ] || [ -d /workspace ]; then
    cd /workspaces/alpine_endpoint 2>/dev/null || cd /workspace
    setup_dev_permissions
    exit $?
fi 