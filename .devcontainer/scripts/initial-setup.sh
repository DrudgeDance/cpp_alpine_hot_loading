#!/bin/bash

# Source common configuration first
source "$(dirname "$0")/config.sh"

# Set up logging
setup_logging "$SETUP_LOG" "$SETUP_DEBUG_LOG"

# Ensure we're running as root
ensure_root "$@"

# Function to set up proper permissions for development files
setup_dev_permissions() {
    status "Setting up development environment..."

    # Create necessary directories if they don't exist
    status "Creating directory structure"
    mkdir -p build/generators build/lib build/obj || error "Failed to create build directories"
    mkdir -p src || error "Failed to create source directory"
    mkdir -p scripts || error "Failed to create scripts directory"
    mkdir -p bin || error "Failed to create bin directory"
    success "Directory structure created"

    # Set up each directory with proper permissions
    setup_directory "build" "$DEVELOPER_USER" "$DEVELOPER_GROUP" "$DIR_PERMS" "$FILE_PERMS"
    setup_directory "src" "$DEVELOPER_USER" "$DEVELOPER_GROUP" "$DIR_PERMS" "$FILE_PERMS"
    setup_directory "include" "$DEVELOPER_USER" "$DEVELOPER_GROUP" "$DIR_PERMS" "$FILE_PERMS"
    
    # Special handling for scripts directory
    if [ -d "scripts" ]; then
        status "Configuring scripts directory"
        chown root:${DEVELOPER_GROUP} scripts/ && \
        chmod $SCRIPT_DIR_PERMS scripts/ && \
        find scripts -type f -name "*.sh" -exec chmod $SCRIPT_PERMS {} \; && \
        find scripts -type f -not -name "*.sh" -exec chmod $FILE_PERMS {} \; && \
        chown -R ${DEVELOPER_USER}:${DEVELOPER_GROUP} scripts/* && \
        success "Scripts directory configured" || error "Failed to configure scripts directory"
    fi

    # Special handling for bin directory
    if [ -d "bin" ]; then
        status "Configuring bin directory"
        chown root:${DEVELOPER_GROUP} bin/ && \
        chmod $BIN_DIR_PERMS bin/ && \
        find bin -type f -exec chmod $BIN_PERMS {} \; && \
        find bin -type f -exec chown ${SERVICE_USER}:${SERVICE_GROUP} {} \; && \
        success "Bin directory configured" || error "Failed to configure bin directory"
    fi

    # Kill any existing watchers more thoroughly
    status "Managing permission watcher"
    # Kill both the watcher script and inotifywait processes
    pkill -f "watch-permissions.sh" 2>/dev/null || true
    pkill -f "inotifywait.*watch-permissions" 2>/dev/null || true
    pkill -f "inotifywait.*scripts.*bin" 2>/dev/null || true
    rm -f "$LOCKFILE" 2>/dev/null || true
    
    # Verify all watchers are stopped
    sleep 1
    if pgrep -f "inotifywait" >/dev/null; then
        error "Failed to stop existing watchers"
        ps aux | grep -E "watch-permissions|inotifywait"
        return 1
    fi

    # Start the permissions watcher in the background
    status "Starting permission watcher"
    local watcher_script="$(dirname "$0")/watch-permissions.sh"
    
    # Verify watcher script exists
    if [ ! -f "$watcher_script" ]; then
        error "Watcher script not found at: $watcher_script"
        return 1
    fi
    
    # Make watcher executable
    chmod +x "$watcher_script" || {
        error "Failed to make watcher executable"
        return 1
    }
    
    # Start watcher with debug output
    if [ -n "$DEBUG" ]; then
        status "Starting watcher in debug mode"
        DEBUG=1 setsid "$watcher_script" > "$WATCHER_LOG" 2>&1 &
    else
        setsid "$watcher_script" > "$WATCHER_LOG" 2>&1 &
    fi
    local watcher_pid=$!
    
    # Give it a moment to start and check if it's running
    sleep 2
    
    # Check for the actual inotifywait process
    if ! pgrep -f "inotifywait.*scripts.*bin" >/dev/null; then
        error "Watcher process failed to start inotifywait"
        echo "Last 50 lines of watcher log:"
        tail -n 50 "$WATCHER_LOG"
        return 1
    fi
    
    # Success - watcher is running
    success "Permission watcher started successfully"
    if [ -n "$DEBUG" ]; then
        echo "Last 10 lines of watcher log:"
        tail -n 10 "$WATCHER_LOG"
        echo "Process list:"
        ps aux | grep -E "watch-permissions|inotifywait"
    fi
    return 0
}

# Run setup if in workspace
cd "$WORKSPACE_DIR" 2>/dev/null || { error "Failed to change to workspace directory"; exit 1; }
setup_dev_permissions
exit $? 