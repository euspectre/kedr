# The path where the examples will be installed
set (KEDR_EXAMPLE_PATH "${KEDR_INSTALL_PREFIX_EXAMPLES}")

# Examples need to build kernel modules.
# Here is kernel build directory, which should be used for that purposes.
#
# NB: Hardcoded kernel build directory mainly used in cross-compile mode,
# when examples are disabled.
set(kbuild_dir_make_installed "/lib/modules/\$(KERNEL)/build")


# Most examples need symvers files of installed kernel modules.
#
# As these files are installed into kernel-dependent location,
# use kernel_make_path() for form path suitable for makefiles.
kernel_make_path(KEDR_CORE_MAKE_SYMVERS
    "${KEDR_KERNEL_INSTALL_PREFIX_KSYMVERS}/kedr.symvers"
)
kernel_make_path(KEDR_FAULT_SIMULATION_MAKE_SYMVERS
    "${KEDR_KERNEL_INSTALL_PREFIX_KSYMVERS}/kedr_fault_simulation.symvers"
)
if(KEDR_LEAK_CHECK)
    kernel_make_path(KEDR_LEAK_CHECK_MAKE_SYMVERS
        "${KEDR_KERNEL_INSTALL_PREFIX_KSYMVERS}/kedr_leak_check.symvers"
    )
endif(KEDR_LEAK_CHECK)

if(KEDR_TRACE)
    kernel_make_path(KEDR_TRACE_MAKE_SYMVERS
        "${KEDR_KERNEL_INSTALL_PREFIX_KSYMVERS}/kedr_trace.symvers"
    )
endif(KEDR_TRACE)

# Several examples use 'kedr_gen'.
set(kedr_gen_tool_installed "${KEDR_INSTALL_PREFIX_EXEC_AUX}/kedr_gen")

add_subdirectory(sample_target)
add_subdirectory(sample_indicator)
add_subdirectory(sample_fsim_payload)
add_subdirectory(counters)
if(KEDR_CALLM_PAYLOADS)
    add_subdirectory(custom_payload_callm)
endif(KEDR_CALLM_PAYLOADS)
add_subdirectory(custom_payload_fsim)
add_subdirectory(custom_indicator_fsim)

if(KEDR_LEAK_CHECK)
    add_subdirectory(leak_check)
endif(KEDR_LEAK_CHECK) 
