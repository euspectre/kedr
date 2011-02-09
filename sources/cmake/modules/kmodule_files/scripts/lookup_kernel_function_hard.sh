#!/bin/sh 
########################################################################
# The script checks if the specified function exists in the kernel and
# can be called by a module.
# 
# exit code: 0 - found, 1 - not found, >1 - error(s) occured
#
# prepare_function_lookup.sh must be called in this directory first 
# to prepare the source code of the probe modules.
########################################################################

if test $# -ne 1; then
    echo "Usage: $0 <function>"
    exit 2
fi
########################################################################

WORK_DIR=$(pwd)
SCRIPT_DIR=$(cd `dirname $0` && pwd)
TOP_PROBES_DIR="${WORK_DIR}/probes"

FUNC_NAME=$1
if test "t${FUNC_NAME}" = "t"; then
	printf "Name of the function is empty\n"
	exit 2
fi

# First look through /proc/kallsyms. If the function is listed there,
# this script additionally tries to compile a probe module that calls it
# to verify it is usable.
grep -E "^[[:xdigit:]]+[[:space:]]+T[[:space:]]+${FUNC_NAME}$" /proc/kallsyms > /dev/null
RESULT_CODE=$?
if test ${RESULT_CODE} -ne 0; then
	# not found or error occured
	exit ${RESULT_CODE}
fi

if ! test -d "${TOP_PROBES_DIR}/${FUNC_NAME}"; then
	printf "Directory ${TOP_PROBES_DIR}/${FUNC_NAME} does not exist\n"
	exit 2
fi

make -C "${TOP_PROBES_DIR}/${FUNC_NAME}" clean > /dev/null 2>&1
make -C "${TOP_PROBES_DIR}/${FUNC_NAME}" > /dev/null 2>&1
if test $? -ne 0; then
	exit 1
fi

########################################################################
# The function has been found and it can be called.
exit 0
