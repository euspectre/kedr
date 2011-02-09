#!/bin/sh 
########################################################################
# The script prepares the source code of kernel modules that will be 
# used to check if a particular kernel function exists and is usable.
# The source code and the makefile for function 'foo' is created in
# 'probes/foo' subdirectory of the current directory.
########################################################################

if test $# -ne 1; then
    echo "Usage: $0 <top_source_tree_dir>"
    exit 1
fi
########################################################################

WORK_DIR=$(pwd)
SCRIPT_DIR=$(cd `dirname $0` && pwd)
TOP_PROBES_DIR="${WORK_DIR}/probes"
TOP_SRC_DIR=$1

if ! test -d "${TOP_SRC_DIR}"; then
	printf "Directory ${TOP_SRC_DIR} does not exist\n"
	exit 1
fi

rm -rf "${TOP_PROBES_DIR}"
mkdir "${TOP_PROBES_DIR}" || exit 1

TRIGGER_INCLUDES=triggers_include.list

for dd in ${TOP_SRC_DIR}/payloads_callm/*; do
	if test -f "${dd}/${TRIGGER_INCLUDES}"; then
		cd "${dd}"
		if test $? -ne 0; then
			printf "Failed to change dir to ${dd}\n"
			exit 1
		fi
		
		for ff in *.trigger; do
			func_name=$(echo "${ff}" | sed -e 's/\..*$//')
			if ! mkdir "${TOP_PROBES_DIR}/${func_name}"; then
				printf "Failed to create ${TOP_PROBES_DIR}/${func_name}\n"
				exit 1
			fi
			
			cp "${SCRIPT_DIR}/Makefile" "${TOP_PROBES_DIR}/${func_name}"
			if test $? -ne 0; then
				printf "Failed to copy Makefile to ${TOP_PROBES_DIR}/${func_name}/\n"
				exit 1
			fi
			
			cat "${SCRIPT_DIR}/probe_header.c" \
				"${TRIGGER_INCLUDES}" \
				"${SCRIPT_DIR}/probe_body.c" \
				"${ff}" \
				"${SCRIPT_DIR}/probe_footer.c" > "${TOP_PROBES_DIR}/${func_name}/probe.c"
			if test $? -ne 0; then
				printf "Failed to create ${TOP_PROBES_DIR}/${func_name}/probe.c\n"
				exit 1
			fi
		done
		
		cd "${WORK_DIR}" || exit 1
	fi
done
########################################################################

exit 0
