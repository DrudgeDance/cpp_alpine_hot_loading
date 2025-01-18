#!/bin/bash

# Enable debug logging
set -x

# Global variables for user/group management
DEVELOPER_USER="developer"
DEVELOPER_GROUP="developer"

SERVICE_USER="appuser"
SERVICE_GROUP="appgroup"

# Ensure we're running as root or with sudo
if [ "$(id -u)" != "0" ]; then
    echo "This script must be run as root or with sudo"
    exec sudo "$0" "$@"
fi

# Get the workspace directory
WORKSPACE_DIR="/workspaces/alpine_endpoint"
if [ ! -d "$WORKSPACE_DIR" ]; then
    WORKSPACE_DIR="/workspace"
fi

# Change to workspace directory
cd "$WORKSPACE_DIR" || { echo "Failed to change to workspace directory"; exit 1; }
echo "Changed to directory: $(pwd)"

# Create a PID file to track the watcher
echo $$ > /tmp/watch-permissions.pid
chmod 644 /tmp/watch-permissions.pid

# SCRIPT PERMISSIONS
## Function to update script permissions
update_script_permissions() {
    local file="$1"
    
    echo "Updating script permissions for: $file"
    if [[ ! -e "$file" ]]; then
        echo "Warning: File $file does not exist"
        return 1
    fi
    
    # First set ownership
    chown ${DEVELOPER_USER}:${DEVELOPER_GROUP} "$file" || {
        echo "Failed to set ownership for $file"
        return 1
    }
    
    if [[ "$file" == *.sh ]]; then
        echo "Setting executable permissions (700)"
        chmod 700 "$file" || {
            echo "Failed to set permissions for $file"
            return 1
        }
    else
        echo "Setting regular file permissions (644)"
        chmod 644 "$file" || {
            echo "Failed to set permissions for $file"
            return 1
        }
    fi
    
    echo "Successfully updated permissions for $file"
    ls -l "$file"
}

# BINARY PERMISSIONS
## Function to update binary permissions
update_binary_permissions() {
    local file="$1"
    
    if [[ ! -e "$file" ]]; then
        echo "Warning: File $file does not exist"
        return 1
    fi
    
    echo "Updating binary permissions for: $file"
    chmod 755 "$file" && \
    chown ${SERVICE_USER}:${SERVICE_GROUP} "$file" || {
        echo "Failed to set permissions/ownership for $file"
        return 1
    }
    
    echo "Successfully updated permissions for $file"
    ls -l "$file"
}

# Cleanup function
cleanup() {
    echo "Cleaning up..."
    rm -f /tmp/watch-permissions.pid
    exit 0
}

# Set up trap for cleanup
trap cleanup EXIT INT TERM

# INITIAL SETUP
echo "Performing initial setup..."

# Set up scripts directory if it exists
if [ -d "scripts" ]; then
    echo "Setting up scripts directory..."
    chown root:${DEVELOPER_GROUP} scripts/
    chmod 2755 scripts/  # SetGID with r-x for group
    find scripts -type f -name "*.sh" -exec update_script_permissions {} \;
    find scripts -type f -not -name "*.sh" -exec update_script_permissions {} \;
fi

# Set up bin directory if it exists
if [ -d "bin" ]; then
    echo "Setting up bin directory..."
    chown root:${DEVELOPER_GROUP} bin/
    chmod 2775 bin/  # SetGID with rwx for group
    find bin -type f -exec update_binary_permissions {} \;
fi

# WATCHER FOR FILE CHANGES
echo "Starting permission watcher..."
echo "Watcher PID: $$"

# Main watch loop with error handling
inotifywait -m -q -e create,modify,moved_to scripts/ bin/ 2>/dev/null | while read -r directory event filename; do
    echo "$(date): File change detected in ${directory}: ${filename} (Event: ${event})"
    
    # Process the file immediately based on its type and location
    if [[ "$directory" == "scripts/" ]]; then
        if [[ "$filename" == *.sh ]]; then
            echo "Processing shell script: ${directory}${filename}"
            update_script_permissions "${directory}${filename}"
        else
            echo "Processing regular file: ${directory}${filename}"
            update_script_permissions "${directory}${filename}"
        fi
    elif [[ "$directory" == "bin/" ]]; then
        echo "Processing binary file: ${directory}${filename}"
        update_binary_permissions "${directory}${filename}"
    fi
done 