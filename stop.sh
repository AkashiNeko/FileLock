#!/bin/bash

MODULE_NAME="filelock"

wait_exit() {
    echo "------------------------"
    echo "Press any key to exit..."
    read -n 1 -s
    exit $1
}

if lsmod | grep -q "$MODULE_NAME"; then
    if sudo rmmod "$MODULE_NAME"; then
        echo "Module unloaded successfully."
        wait_exit 0
    else
        echo "Failed to unload module."
        wait_exit 1
    fi
else
    echo "Module '$MODULE_NAME' is not loaded"
fi
