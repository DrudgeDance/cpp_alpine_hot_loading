#!/bin/bash

# Source common configuration first
source "$(dirname "$0")/config.sh" || {
    echo "❌ Failed to source config.sh" >&2
    exit 1
}

# Set up logging
setup_logging "$WATCHER_LOG" "$WATCHER_DEBUG_LOG"

# Log startup information
[ -n "$DEBUG" ] && {
    echo "Starting watcher with:"
    echo "  User: $(id)"
    echo "  PWD: $(pwd)"
    echo "  PID: $$"
    echo "  PPID: $PPID"
    echo "  Script: $0"
}

# Ensure we're running as root
ensure_root "$@"

# Check for existing process more thoroughly
# Only look for processes that are actually watching (with inotifywait)
pids=$(pgrep -f "inotifywait.*scripts.*bin" | grep -v $$)
if [ -n "$pids" ]; then
    error "Watcher is already running (PIDs: $pids)"
    exit 1
fi

# Try to acquire lock
acquire_lock || {
    error "Failed to acquire lock"
    exit 1
}

# Change to workspace directory
cd "$WORKSPACE_DIR" || { 
    error "Failed to change to workspace directory: $WORKSPACE_DIR"
    pwd
    ls -la
    exit 1 
}

# Verify directories exist
for dir in scripts bin; do
    if [ ! -d "$dir" ]; then
        error "Required directory not found: $dir"
        pwd
        ls -la
        exit 1
    fi
done

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
        chmod $SCRIPT_PERMS "$file" || {
            error "Failed to set permissions for $file"
            return 1
        }
        [ -n "$DEBUG" ] && success "Set executable permissions ($SCRIPT_PERMS) for $file"
    else
        chmod $FILE_PERMS "$file" || {
            error "Failed to set permissions for $file"
            return 1
        }
        [ -n "$DEBUG" ] && success "Set regular file permissions ($FILE_PERMS) for $file"
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
    
    chmod $BIN_PERMS "$file" && \
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
    kill $INOTIFY_PID 2>/dev/null
    rm -f "$PIPE"
    release_lock
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

# Create a named pipe for inotifywait output
PIPE="/tmp/watch-permissions.pipe"
rm -f "$PIPE"
mkfifo "$PIPE"

# Start inotifywait and redirect its output to the pipe
inotifywait -m -q -e create,modify,moved_to scripts/ bin/ > "$PIPE" 2>/dev/null &
INOTIFY_PID=$!

# Wait a moment to ensure inotifywait started successfully
sleep 1
if ! kill -0 $INOTIFY_PID 2>/dev/null; then
    error "inotifywait failed to start"
    rm -f "$PIPE"
    exit 1
fi

[ -n "$DEBUG" ] && status "inotifywait started successfully (PID: $INOTIFY_PID)"

# Read from the named pipe
while read -r directory event filename; do
    [ -n "$DEBUG" ] && status "Processing $filename in $directory"
    
    if [[ "$directory" == "scripts/" ]]; then
        update_script_permissions "${directory}${filename}"
    elif [[ "$directory" == "bin/" ]]; then
        update_binary_permissions "${directory}${filename}"
    fi
done < "$PIPE"

# If we get here, inotifywait has stopped
error "inotifywait process terminated unexpectedly"
rm -f "$PIPE"
exit 1 