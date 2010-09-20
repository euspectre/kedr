#!/bin/sh

#./create_char_device.sh device

# Remove file, creating by ./create_char_device.sh device.

if test $# -ne 1; then
    printf "Usage: ./delete_char_device.sh device"
    exit 1
fi

device="$1"

# Remove the device node
rm -f /dev/${device} /dev/${device}0