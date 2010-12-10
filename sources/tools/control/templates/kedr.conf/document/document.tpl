<$if concat(payload.name)$># Mount debugfs for enabling trace events in the payloads
on_load mkdir -p "<$debugfs_mount_point$>"
on_load mount debugfs -t debugfs "<$debugfs_mount_point$>"
# For rollback in case of error while loading some of payload
on_unload mount | grep "<$debugfs_mount_point$>"; if test $? -eq 0; then umount "<$debugfs_mount_point$>"; fi
<$if concat(payload_is_callm)$>
#Load call monitor payloads
<$payload_elem_callm : join()$><$endif$><$if concat(payload.is_fsim)$>
#Load module for fault simulation support
module <$fault_simulation_module$>

#Load fault simulation payloads
<$payload_elem_fsim : join()$><$endif$>
# After trace events are enabled for all payloads, unmount debugfs
on_load umount "<$debugfs_mount_point$>"
<$endif$>