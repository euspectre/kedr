#!/bin/sh
module="icpt_target"
device="icpt_target"

# Invoke rmmod with all arguments we got
/sbin/rmmod $module $* || exit 1

# Remove stale nodes
rm -f /dev/${device}
