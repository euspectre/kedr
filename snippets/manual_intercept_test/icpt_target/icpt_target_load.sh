#!/bin/sh
module="icpt_target"
device="icpt_target"
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
major=$(awk "\$2==\"$module\" {print \$1}" /proc/devices)
if test "t$major" = "t"; then
	printf "No device found for \"$module\" module\n";
	exit 1;
fi

# Remove stale nodes and replace them, then give gid and perms.

rm -f /dev/${device}
mknod /dev/${device} c $major 0

chgrp $group /dev/${device}
chmod $mode  /dev/${device}
