# Install template files

set(KEDR_TEMPLATES_PATH "${KEDR_INSTALL_PREFIX_TEMPLATES}")

set(templates
    "functions_support.c"
    "payload_fsim.c"
    "payload_fsim_add_caller_address.data"
    "fault_indicator.c"
    "fault_indicator.data"
    "fault_indicator_add_caller_address.data"
    "payload_leak_check.c")

if(KEDR_CALLM_PAYLOADS)
    list(APPEND templates "payload_callm.c")
endif(KEDR_CALLM_PAYLOADS)

install(DIRECTORY ${templates} 
        DESTINATION "${KEDR_TEMPLATES_PATH}"
)
