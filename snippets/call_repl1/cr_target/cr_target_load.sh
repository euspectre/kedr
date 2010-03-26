#!/bin/sh
module="cr_target"
device="cfake"
mode="664"

# Group: since distributions do it differently, look for wheel or use staff
if grep -q '^staff:' /etc/group; then
    group="staff"
else
    group="wheel"
fi

# Invoke insmod with all arguments we got
# and use a pathname, as insmod doesn't look in . by default
/sbin/insmod ./$module.ko $* || exit 1

# retrieve major number
major=$(awk "\$2==\"$device\" {print \$1}" /proc/devices)
if test "t$major" = "t"; then
	printf "No device found for \"$module\" module\n";
	exit 1;
fi

# Remove stale nodes and replace them, then give gid and perms.

rm -f /dev/${device}[0-1]
mknod /dev/${device}0 c $major 0
mknod /dev/${device}1 c $major 1

ln -sf ${device}0 /dev/${device}

chgrp $group /dev/${device}[0-1] 
chmod $mode  /dev/${device}[0-1]
