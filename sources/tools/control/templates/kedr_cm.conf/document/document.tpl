on_load mkdir -p "<$debugfs_mount_point$>"
# Mount debugfs for enabling tracepoints in the payloads
on_load mount debugfs -t debugfs "<$debugfs_mount_point$>"
# For rollback in case of error while loading some of payload
on_unload mount | grep "<$debugfs_mount_point$>"; if test $? -eq 0; then umount "<$debugfs_mount_point$>"; fi
#Load payloads
<$payload_elem : join(\n)$>
# After tracepoints are enabled for all payloads, unmount debugfs
on_load umount "<$debugfs_mount_point$>"