#!/bin/bash

# Common workspace directory detection
WORKSPACE_DIR="/workspaces/alpine_endpoint"
if [ ! -d "$WORKSPACE_DIR" ]; then
    WORKSPACE_DIR="/workspace"
fi

# User and group configurations
DEVELOPER_USER="developer"
DEVELOPER_GROUP="developer"
SERVICE_USER="appuser"
SERVICE_GROUP="appgroup"

# Common file paths
LOCKFILE="/tmp/watch-permissions.lock"
WATCHER_LOG="/tmp/watch-permissions.log"
WATCHER_DEBUG_LOG="/tmp/watch-permissions-debug.log"
SETUP_LOG="/tmp/initial-setup.log"
SETUP_DEBUG_LOG="/tmp/initial-setup-debug.log"

# Directory permissions
SCRIPT_DIR_PERMS=2755  # SGID for scripts directory
BIN_DIR_PERMS=2775     # SGID for bin directory
DIR_PERMS=755         # Standard directory permissions
FILE_PERMS=644        # Standard file permissions
SCRIPT_PERMS=700      # Executable script permissions
BIN_PERMS=755         # Binary permissions

# Common message functions
status() {
    echo -e "\n→ $1"
}

success() {
    echo -e "✅ $1"
}

error() {
    echo -e "❌ $1\n\n" >&2
}

# Debug logging setup
setup_logging() {
    local log_file="$1"
    local debug_log_file="$2"
    
    if [ -n "$DEBUG" ]; then
        set -x
        exec 1> >(tee -a "$debug_log_file") 2>&1
    else
        exec 1> >(tee -a "$log_file") 2>&1
    fi
}

# Common directory setup function
setup_directory() {
    local dir="$1"
    local owner="$2"
    local group="$3"
    local dir_perms="$4"
    local file_perms="$5"
    
    if [ ! -d "$dir" ]; then
        return 0  # Skip if directory doesn't exist
    fi
    
    status "Configuring $dir directory"
    chown "$owner:$group" "$dir" && \
    chmod "$dir_perms" "$dir" && \
    find "$dir" -type f -exec chmod "$file_perms" {} \; && \
    find "$dir" -type d -exec chmod "$dir_perms" {} \; && \
    success "$dir directory configured" || {
        error "Failed to configure $dir directory"
        return 1
    }
}

# Ensure root privileges
ensure_root() {
    if [ "$(id -u)" != "0" ]; then
        exec sudo "$0" "$@"
    fi
}

# Common process management
acquire_lock() {
    exec 9>"$LOCKFILE"
    if ! flock -n 9; then
        error "Another instance is already running"
        exit 1
    fi
}

release_lock() {
    flock -u 9
    rm -f "$LOCKFILE"
} 