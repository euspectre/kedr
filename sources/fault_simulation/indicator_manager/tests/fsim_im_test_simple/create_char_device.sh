#!/bin/sh

#./create_char_device.sh device

# Create and bind (using mknod) /dev/$device file with device "$device".
# Set minor device number to 0, permissions to 644 and choose some owner group.

if test $# -ne 1; then
    printf "Usage: ./create_char_device.sh device"
    exit 1
fi

device="$1"
mode="664"

# Group: since distributions do it differently, look for wheel or use staff
if grep -q '^staff:' /etc/group; then
    group="staff"
else
    group="wheel"
fi

# retrieve major number
major=$(awk "\$2==\"$device\" {print \$1}" /proc/devices)
if test "t$major" = "t"; then
	printf "Device '%s' wasn't found.\n" "$device"
	exit 1
fi

# Remove old device node, create new, set gid and permissions.

rm -f /dev/${device}0
mknod /dev/${device}0 c $major 0

ln -sf ${device}0 /dev/${device}

chgrp $group /dev/${device}0
chmod $mode  /dev/${device}0
