#!/bin/sh

# Usage:
# kedr_capture_trace_until_done.sh output_file [<debug-fs-mount-point>]
#
# 'output_file' - file to which trace should be output
# <debug-fs-mount-point> current point(one of), where debugfs is mounted.
# if not stated, <debug-fs-mount-point> assumed to be "/sys/kernel/debug"

if test $# -lt 1; then
    printf "Usage:\n\n\t%s output_file [<debug-fs-mount-point>]\n\n" "kedr_capture_trace_until_done.sh"
    exit 1
fi

rel_trace_pipe="tracing/trace_pipe"


out_file=$1

if test $# -eq 1; then
    debugfs_mount_point="/sys/kernel/debug"
else
    debugfs_mount_point="$2"
fi

# Verify mount point
if test ! -d "${debugfs_mount_point}"; then
    printf "debugfs mount point '%s' is not exist.\n" "${debugfs_mount_point}"
    printf "Please specify correct directory, where debugfs is mounted, by the second parameter.\n"
    exit 1
fi

trace_pipe="${debugfs_mount_point}/${rel_trace_pipe}"
if test ! -e "${trace_pipe}"; then
    printf "Tracing file is not exist(may be incorrect mount point of debugfs is specified?).\n"
    exit 1
fi

end_session_regexp="target_session_ends"
# Verify output file
if test -f "${out_file}"; then
    start_session_regexp="target_session_starts"

    awk_script="
        BEGIN {result=\"no\"}
        /${start_session_regexp}/ {result=\"no\"; next}
        /${end_session_regexp}/ {result=\"yes\"; next}
        END {print result}"

    is_trace_already_complete=`awk "${awk_script}" "${out_file}"`
    if test "${is_trace_already_complete}" = "yes"; then
        exit 0
    fi
else
    printf "" > ${out_file}
    if test $? -ne 0; then
        printf "Cannot create output file '%s'\n" "${out_file}"
        exit 1
    fi
fi

sed -e "/$end_session_regexp/ q" < "${trace_pipe}" >> "${out_file}"