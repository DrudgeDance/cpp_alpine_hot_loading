#!/bin/bash

# Workspace configuration
WORKSPACE_DIR="/workspaces/alpine_endpoint"

# User and group configurations
DEVELOPER_USER="developer"
DEVELOPER_GROUP="developer"
DEVELOPER_UID=1000
DEVELOPER_GID=1000

SERVICE_USER="appuser"
SERVICE_GROUP="appgroup"
SERVICE_UID=2000
SERVICE_GID=2000

# Directory ownership
WORKSPACE_OWNER="root"
WORKSPACE_GROUP="root"
WORKSPACE_PERMS=755

PROJECT_OWNER="$DEVELOPER_USER"
PROJECT_GROUP="$DEVELOPER_GROUP"
PROJECT_PERMS=755

# Build directory permissions
BUILD_OWNER="$DEVELOPER_USER"
BUILD_GROUP="$DEVELOPER_GROUP"
BUILD_DIR_PERMS=755
BUILD_FILE_PERMS=644

# Directory structure
REQUIRED_DIRS=(
    "build"
    "src"
    "scripts"
    "bin"
    "include"
)

# Paths to exclude from permission changes
EXCLUDED_PATHS=(
    ".git"
    ".github"
    ".vscode"
    "node_modules"
    "build/CMakeFiles"     # CMake internal files
    "build/_deps"          # CMake dependencies
    "build/Testing"        # CMake test outputs
)

# Common file paths
LOCKFILE="/tmp/watch-permissions.lock"
PIPE="/tmp/watch-permissions.pipe"
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

# Watcher configuration
WATCH_DIRS=("scripts/" "bin/")  # Directories to watch
WATCH_EVENTS="create,modify,moved_to"  # Events to watch for

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

# Check if a path should be excluded
is_excluded() {
    local check_path="$1"
    for excluded in "${EXCLUDED_PATHS[@]}"; do
        if [[ "$check_path" == *"/$excluded"* ]] || [[ "$check_path" == *"/$excluded/" ]]; then
            return 0
        fi
    done
    return 1
}

# Common directory setup function
setup_directory() {
    local dir="$1"
    local owner="$2"
    local group="$3"
    local dir_perms="$4"
    local file_perms="$5"
    
    if [ ! -d "$dir" ]; then
        mkdir -p "$dir" || {
            error "Failed to create directory: $dir"
            return 1
        }
    fi
    
    status "Configuring $dir directory"
    
    # Set ownership and permissions on the directory itself
    chown "$owner:$group" "$dir" || error "Failed to set ownership on $dir"
    chmod "$dir_perms" "$dir" || error "Failed to set permissions on $dir"
    
    # Process subdirectories and files
    find "$dir" -mindepth 1 | while read -r path; do
        # Skip excluded paths
        if is_excluded "$path"; then
            [ -n "$DEBUG" ] && echo "Skipping excluded path: $path"
            continue
        fi
        
        if [ -d "$path" ]; then
            chmod "$dir_perms" "$path" 2>/dev/null || true
            chown "$owner:$group" "$path" 2>/dev/null || true
        else
            chmod "$file_perms" "$path" 2>/dev/null || true
            chown "$owner:$group" "$path" 2>/dev/null || true
        fi
    done
    
    success "$dir directory configured"
    return 0
}

# Ensure root privileges
ensure_root() {
    if [ "$(id -u)" != "0" ]; then
        exec sudo "$0" "$@"
    fi
}

# Process management functions
kill_watchers() {
    pkill -f "watch-permissions.sh" 2>/dev/null || true
    pkill -f "inotifywait.*scripts.*bin" 2>/dev/null || true
    rm -f "$LOCKFILE" 2>/dev/null || true
    rm -f "$PIPE" 2>/dev/null || true
    sleep 1
}

check_watcher_running() {
    pgrep -f "inotifywait.*scripts.*bin" >/dev/null
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