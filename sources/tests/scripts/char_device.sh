#!/bin/sh

#./char_device.sh device 'create'|'delete'

#
# 'create': create and bind (using mknod) /dev/$device file with device "$device".
# Set minor device number to 0, permissions to 644 and choose some owner group.
#
# 'delete': remove file, /dev/$device.
#

if test $# -ne 2; then
    printf "Usage: ./char_device.sh device create|delete"
    exit 1
fi


device="$1"
action="$2"

case ${action} in
create)
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
    ;;
delete)
    rm -f /dev/${device} /dev/${device}0
    ;;
*)
    printf "%s\n" "Incorrect action '$action' to execute, should be 'create' or 'delete'."
esac


