# Support for paradigm "one user part for many kernels".
#
# NB: Before use macros defined here, 'Kbuild' should be configured.
#
# All project's deliverables are divided into two groups.
# Deliverable in the first group, defined under "if(USER_PART)",
# is common for all kernels.
# Deliverable in the second group, defined under "if(KERNEL_PART)",
# is built and installed per-kernel.
#
# By default, both groups are compiled together, and resulted single
# installation work with only one kernel (for which it has been configured).
#
# For work with many kernels, user part should be configured with
#    -DUSER_PART_ONLY=ON
# cmake option.
# Kernel part should be configured with
#   -DKERNEL_PART_ONLY=ON
# cmake option into different
# build directories for every kernel.
#
# Note, that common definitions should be passed both to user part
# and kernel ones.
#
# Usually, per-kernel components are installed to directory,
# contained kernel version. So common components may chose per-kernel
# ones using version of currently run kernel.

# Environment variable used for refer to kernel version in deliverables.
# By default, it is "KERNEL", but it may be redefined by the project.
if(NOT multi_kernel_KERNEL_VAR)
    set(multi_kernel_KERNEL_VAR "KERNEL")
endif(NOT multi_kernel_KERNEL_VAR)

option(USER_PART_ONLY "Build only user-space part of the project" OFF)
option(KERNEL_PART_ONLY "Build only kernel-space part of the project" OFF)

mark_as_advanced(USER_PART_ONLY KERNEL_PART_ONLY)

if(USER_PART_ONLY AND KERNEL_PART_ONLY)
    message(FATAL_ERROR "At most one of KERNEL_PART_ONLY and USER_PART_ONLY can be specified")
endif(USER_PART_ONLY AND KERNEL_PART_ONLY)

# Setup macros defining each part.
if(NOT USER_PART_ONLY)
    set(KERNEL_PART ON)
endif(NOT USER_PART_ONLY)
if(NOT KERNEL_PART_ONLY)
    set(USER_PART ON)
endif(NOT KERNEL_PART_ONLY)

# Definition of ${multi_kernel_KERNEL_VAR} variable in shell and make scripts.
if(KERNEL_PART)
    set(multi_kernel_KERNEL_VAR_SHELL_DEFINITION "${multi_kernel_KERNEL_VAR}=${Kbuild_VERSION_STRING}")
    set(multi_kernel_KERNEL_VAR_MAKE_DEFINITION "${multi_kernel_KERNEL_VAR} := ${Kbuild_VERSION_STRING}")
else(KERNEL_PART)
    set(multi_kernel_KERNEL_VAR_SHELL_DEFINITION "${multi_kernel_KERNEL_VAR}=`uname -r`")
    set(multi_kernel_KERNEL_VAR_MAKE_DEFINITION "${multi_kernel_KERNEL_VAR} := \$(shell uname -r)")
endif(KERNEL_PART)

# Form kernel-dependent path from pattern.
# Result is usable in shell scripts.
#
# Replace %kernel% substring with '${multi_kernel_KERNEL_VAR}'
# variable reference.
macro(kernel_shell_path RESULT_VAR pattern)
    # Double escaping, otherwise does not work
    kernel_path("\\\${${multi_kernel_KERNEL_VAR}}" ${RESULT_VAR} ${pattern})
endmacro(kernel_shell_path RESULT_VAR pattern)

# Form kernel-dependent path from pattern.
# Result is usable in make scripts.
#
# Replace %kernel% substring with '${multi_kernel_KERNEL_VAR}'
# variable reference.
macro(kernel_make_path RESULT_VAR pattern)
    kernel_path("$(${multi_kernel_KERNEL_VAR})" ${RESULT_VAR} ${pattern})
endmacro(kernel_make_path RESULT_VAR pattern)

# Form kernel-dependent path from pattern.
# Result is constant string.
# May be used only in KERNEL_PART.
#
# Replace %kernel% substring with value of '${Kbuild_VERSION_STRING}'
# variable.
macro(kernel_part_path RESULT_VAR pattern)
    if(NOT KERNEL_PART)
        message(FATAL_ERROR "kernel_part_path macro is usable only in KERNEL_PART.")
    endif(NOT KERNEL_PART)
    kernel_path("${Kbuild_VERSION_STRING}" ${RESULT_VAR} ${pattern})
endmacro(kernel_part_path RESULT_VAR pattern)
