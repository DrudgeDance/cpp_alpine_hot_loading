#!/bin/bash

## Get the workspace directory
WORKSPACE_DIR="/workspaces/alpine_endpoint"

## Change to workspace directory
cd "$WORKSPACE_DIR"

# SCRIPT PERMISSIONS
## Function to update script permissions
update_script_permissions() {
    local file="$1"
    if [[ "$file" == *.sh ]]; then
        sudo chmod 700 "$file"
    else
        sudo chmod 644 "$file"
    fi
}

# BINARY PERMISSIONS
## Function to update binary permissions
update_binary_permissions() {
    local file="$1"
    local service_user="appuser"
    local service_group="appgroup"
    sudo chmod 755 "$file"
    sudo chown ${service_user}:${service_group} "$file"
}

# SCRIPT DIRECTORYPERMISSIONS
## Function to setup and maintain scripts directory
setup_scripts_directory() {
    local developer_user="developer"
    local developer_group="developer"

    ## Set ownership and setgid bit
    sudo chown -R ${developer_user}:${developer_group} scripts/
    sudo chmod 2755 scripts/

    ## Update permissions for existing files
    find scripts -type f -name "*.sh" -exec sudo chmod 700 {} \;
    find scripts -type f -not -name "*.sh" -exec sudo chmod 644 {} \;
}

# BINARY DIRECTORY PERMISSIONS
## Function to setup and maintain bin directory
setup_bin_directory() {
    local developer_user="developer"
    local developer_group="developer"
    
    ## Set directory ownership and permissions
    sudo chown -R ${developer_user}:${developer_group} bin/
    sudo chmod 755 bin/
    
    ## Update permissions for existing files
    find bin -type f -exec update_binary_permissions {} \;
}

# INITIAL SETUP OF SCRIPT AND BINARY DIRECTORIES
## Perform initial setup if directories exist
if [ -d "scripts" ]; then
    echo "Setting up scripts directory..."
    setup_scripts_directory
fi

if [ -d "bin" ]; then
    echo "Setting up bin directory..."
    setup_bin_directory
fi

# WATCHER FOR FILE CHANGES
## Watch both directories for changes
echo "Starting permission watcher..."
while true; do
    inotifywait -q -e create -e modify -e moved_to scripts/ bin/ 2>/dev/null | while read -r directory event filename; do
        echo "File change detected in ${directory}: ${filename}"
        if [[ "$directory" == "scripts/" ]]; then
            update_script_permissions "scripts/$filename"
        elif [[ "$directory" == "bin/" ]]; then
            update_binary_permissions "bin/$filename"
        fi
    done
done 