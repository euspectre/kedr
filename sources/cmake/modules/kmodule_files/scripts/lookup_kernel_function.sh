#!/bin/sh
# Usage: lookup_kernel_function.sh <function_name> [map_file]

MAP_FILE=/proc/kallsyms
if test -n "$2"; then
	MAP_FILE="$2"
fi

if ! test -f "${MAP_FILE}"; then
	printf "Symbol map file ${MAP_FILE} does not exist\n" 
	exit 2
fi

grep -E "^[[:xdigit:]]+[[:space:]]+T[[:space:]]+$1$" "${MAP_FILE}" > /dev/null
