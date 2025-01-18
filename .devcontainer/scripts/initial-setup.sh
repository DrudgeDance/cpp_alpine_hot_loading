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

    # Create and configure /workspaces
    setup_directory "/workspaces" "$WORKSPACE_OWNER" "$WORKSPACE_GROUP" "$WORKSPACE_PERMS" "$WORKSPACE_PERMS"

    # Create and configure project directory
    setup_directory "$WORKSPACE_DIR" "$PROJECT_OWNER" "$PROJECT_GROUP" "$PROJECT_PERMS" "$PROJECT_PERMS"

    # Create necessary directories
    status "Creating directory structure"
    for dir in "${REQUIRED_DIRS[@]}"; do
        mkdir -p "$WORKSPACE_DIR/$dir" || error "Failed to create $dir"
    done
    success "Directory structure created"

    # Set up build directory with proper permissions
    status "Configuring build directory"
    setup_directory "$WORKSPACE_DIR/build" "$BUILD_OWNER" "$BUILD_GROUP" "$BUILD_DIR_PERMS" "$BUILD_FILE_PERMS"

    # Set up standard directories with proper permissions
    setup_directory "$WORKSPACE_DIR/src" "$DEVELOPER_USER" "$DEVELOPER_GROUP" "$DIR_PERMS" "$FILE_PERMS"
    setup_directory "$WORKSPACE_DIR/include" "$DEVELOPER_USER" "$DEVELOPER_GROUP" "$DIR_PERMS" "$FILE_PERMS"
    
    # Special handling for scripts directory
    if [ -d "$WORKSPACE_DIR/scripts" ]; then
        status "Configuring scripts directory"
        chown root:${DEVELOPER_GROUP} "$WORKSPACE_DIR/scripts" && \
        chmod $SCRIPT_DIR_PERMS "$WORKSPACE_DIR/scripts" && \
        find "$WORKSPACE_DIR/scripts" -type f -name "*.sh" -exec chmod $SCRIPT_PERMS {} \; && \
        find "$WORKSPACE_DIR/scripts" -type f -not -name "*.sh" -exec chmod $FILE_PERMS {} \; && \
        chown -R ${DEVELOPER_USER}:${DEVELOPER_GROUP} "$WORKSPACE_DIR/scripts/"* && \
        success "Scripts directory configured" || error "Failed to configure scripts directory"
    fi

    # Special handling for bin directory
    if [ -d "$WORKSPACE_DIR/bin" ]; then
        status "Configuring bin directory"
        chown root:${DEVELOPER_GROUP} "$WORKSPACE_DIR/bin" && \
        chmod $BIN_DIR_PERMS "$WORKSPACE_DIR/bin" && \
        find "$WORKSPACE_DIR/bin" -type f -exec chmod $BIN_PERMS {} \; && \
        find "$WORKSPACE_DIR/bin" -type f -exec chown ${SERVICE_USER}:${SERVICE_GROUP} {} \; && \
        success "Bin directory configured" || error "Failed to configure bin directory"
    fi

    # Kill any existing watchers
    status "Managing permission watcher"
    kill_watchers
    
    # Verify all watchers are stopped
    if check_watcher_running; then
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
    
    # Give it a moment to start and check if it's running
    sleep 2
    
    # Check for the actual inotifywait process
    if ! check_watcher_running; then
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