# Install template files

set(KEDR_TEMPLATES_PATH "${KEDR_INSTALL_PREFIX_TEMPLATES}")

set(templates
    "functions_support.c"
    "payload_callm.c"
    "payload_fsim.c"
    "payload_fsim_add_caller_address.data"
    "fault_indicator.c"
    "fault_indicator.data"
    "fault_indicator_add_caller_address.data")

install(DIRECTORY ${templates} 
        DESTINATION "${KEDR_TEMPLATES_PATH}"
)
