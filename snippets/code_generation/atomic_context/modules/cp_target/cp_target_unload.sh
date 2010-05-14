#!/bin/sh
module="cp_target"
device="cfake"

# Invoke rmmod with all arguments we got
/sbin/rmmod $module $* || exit 1

# Remove stale nodes
rm -f /dev/${device} /dev/${device}[0-1] 
