#!/bin/bash

# Get the workspace directory
WORKSPACE_DIR="/workspaces/alpine_endpoint"
if [ ! -d "$WORKSPACE_DIR" ]; then
    WORKSPACE_DIR="/workspace"
fi

# Function to update permissions for scripts directory
update_script_permissions() {
    local file="$1"
    if [[ "$file" == *.sh ]]; then
        sudo chmod 700 "$file"
    else
        sudo chmod 644 "$file"
    fi
}

# Change to workspace directory
cd "$WORKSPACE_DIR"

# Watch scripts directory for changes
while true; do
    inotifywait -q -e create -e modify -e moved_to scripts/ 2>/dev/null | while read -r directory event filename; do
        echo "File change detected in scripts directory: $filename"
        update_script_permissions "scripts/$filename"
    done
done 