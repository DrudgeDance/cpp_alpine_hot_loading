#!/bin/bash

# Make all .sh files executable automatically
find_and_chmod_sh() {
    find . -type f -name "*.sh" -exec chmod +x {} \;
}
alias make-sh-exec='find_and_chmod_sh'

# Run it once when container starts
if [ -d /workspace ]; then
    cd /workspace && find_and_chmod_sh
fi 