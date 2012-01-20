#!/bin/sh
# Usage: lookup_kernel_function.sh <function_name> [map_file]

MAP_FILE=/proc/kallsyms
SYMVERS_FILE=/lib/modules/$(uname -r)/build/Module.symvers
SYMVERS_GREP_EXPR="[[:xdigit:]]+[[:space:]]+$1[[:space:]]+vmlinux[[:space:]]+EXPORT"

# TODO: Allow specifying Module.symvers file instead of 'map_file'
# as the argument to this script.

if test -n "$2"; then
	MAP_FILE="$2"
else
# Check system-wide Module.symvers first if it is present.
# It is more reliable as Module.symvers should list only exported
# symbols while in System.map/kallsysms internal symbols can also
# be marked with "T".
	if test -f "${SYMVERS_FILE}"; then
		grep -E "${SYMVERS_GREP_EXPR}" "${SYMVERS_FILE}" > /dev/null
		RESULT=$?
		if test ${RESULT} -lt 2; then
			exit ${RESULT}
		fi
	fi
fi

if ! test -f "${MAP_FILE}"; then
	printf "Symbol map file ${MAP_FILE} does not exist\n" 
	exit 2
fi

grep -E "^[[:xdigit:]]+[[:space:]]+T[[:space:]]+$1$" "${MAP_FILE}" > /dev/null
