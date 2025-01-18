#!/bin/bash

# Debug logging only if DEBUG is set
if [ -n "$DEBUG" ]; then
    set -x
    exec 1> >(tee -a "/tmp/watch-permissions-debug.log") 2>&1
else
    # Cleaner output for normal operation
    exec 1> >(tee -a "/tmp/watch-permissions.log") 2>&1
fi

# Check if we're being run through sudo
if [ -z "$SUDO_COMMAND" ]; then
    exec sudo "$0" "$@"
fi

# Source common configuration
source "$(dirname "$0")/config.sh"

# Function to print status messages
status() {
    echo "→ $1"
}

# Function to print success messages
success() {
    echo "✓ $1"
}

# Function to print error messages
error() {
    echo -e "❌ $1\n\n" >&2
}

# Get the workspace directory
WORKSPACE_DIR="/workspaces/alpine_endpoint"
if [ ! -d "$WORKSPACE_DIR" ]; then
    WORKSPACE_DIR="/workspace"
fi

# Change to workspace directory
cd "$WORKSPACE_DIR" || { error "Failed to change to workspace directory"; exit 1; }

# Create a PID file to track the watcher
echo $$ > /tmp/watch-permissions.pid
chmod 644 /tmp/watch-permissions.pid

# SCRIPT PERMISSIONS
## Function to update script permissions
update_script_permissions() {
    local file="$1"
    
    if [[ ! -e "$file" ]]; then
        error "File not found: $file"
        return 1
    fi
    
    # First set ownership
    chown ${DEVELOPER_USER}:${DEVELOPER_GROUP} "$file" || {
        error "Failed to set ownership for $file"
        return 1
    }
    
    if [[ "$file" == *.sh ]]; then
        chmod 700 "$file" || {
            error "Failed to set permissions for $file"
            return 1
        }
        [ -n "$DEBUG" ] && success "Set executable permissions (700) for $file"
    else
        chmod 644 "$file" || {
            error "Failed to set permissions for $file"
            return 1
        }
        [ -n "$DEBUG" ] && success "Set regular file permissions (644) for $file"
    fi
    
    [ -n "$DEBUG" ] && ls -l "$file"
    return 0
}

# BINARY PERMISSIONS
## Function to update binary permissions
update_binary_permissions() {
    local file="$1"
    
    if [[ ! -e "$file" ]]; then
        error "File not found: $file"
        return 1
    fi
    
    chmod 755 "$file" && \
    chown ${SERVICE_USER}:${SERVICE_GROUP} "$file" || {
        error "Failed to set permissions/ownership for $file"
        return 1
    }
    
    [ -n "$DEBUG" ] && {
        success "Updated permissions for $file"
        ls -l "$file"
    }
    return 0
}

# Cleanup function
cleanup() {
    [ -n "$DEBUG" ] && status "Cleaning up..."
    rm -f /tmp/watch-permissions.pid
    kill -TERM 0
    exit 0
}

# Set up trap for cleanup
trap cleanup SIGTERM SIGINT EXIT

# WATCHER FOR FILE CHANGES
status "Starting permission watcher..."

# Check if inotifywait is available
if ! command -v inotifywait >/dev/null 2>&1; then
    error "inotifywait not found. Please install inotify-tools."
    exit 1
fi

# Main watch loop with error handling
[ -n "$DEBUG" ] && status "Watching for file changes..."
inotifywait -m -q -e create,modify,moved_to scripts/ bin/ 2>/dev/null | while read -r directory event filename; do
    [ -n "$DEBUG" ] && status "Processing $filename in $directory"
    
    if [[ "$directory" == "scripts/" ]]; then
        if [[ "$filename" == *.sh ]]; then
            update_script_permissions "${directory}${filename}"
        else
            update_script_permissions "${directory}${filename}"
        fi
    elif [[ "$directory" == "bin/" ]]; then
        update_binary_permissions "${directory}${filename}"
    fi
done 