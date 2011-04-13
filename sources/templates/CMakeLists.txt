# Install template files

set(KEDR_TEMPLATES_PATH "${KEDR_INSTALL_PREFIX_TEMPLATES}")

install(DIRECTORY  "functions_support.c" "payload_callm.c" "payload_fsim.c" "fault_indicator.c" "fault_indicator.data"
        DESTINATION "${KEDR_TEMPLATES_PATH}"
)
