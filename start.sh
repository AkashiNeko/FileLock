#!/bin/bash

MODULE_NAME="filelock"
DEFAULT_FILES="files.cfg"
DEFAULT_PROCS="procs.cfg"

if lsmod | grep -q "$MODULE_NAME"; then
    echo "Module '$MODULE_NAME' is already loaded, run stop.sh to stop it."
    wait_exit 0
fi

wait_exit() {
    echo "------------------------"
    echo "Press any key to exit..."
    read -n 1 -s
    exit $1
}

join_lines() {
    local filename=$1
    if [ ! -f "$filename" ]; then
        echo "File not found: $filename"
        return 1
    fi
    echo $(cat "$filename" | grep -v '^[[:space:]]*$' | tr '\n' ':' | sed 's/:$//')
}

files=$(join_lines ${1:-$DEFAULT_FILES})
procs=$(join_lines ${2:-$DEFAULT_PROCS})

if [ -z "$files" ] || [ -z "$procs" ]; then
    echo "Error files."
    wait_exit 1
fi

echo "Set protected files: $files"
echo "Set procs whitelist: $procs"
sudo insmod $MODULE_NAME.ko files="$files" procs="$procs"

if [ $? -eq 0 ]; then
    echo "Module loaded successfully."
    wait_exit 0
else
    echo "Failed to load module."
    wait_exit 1
fi