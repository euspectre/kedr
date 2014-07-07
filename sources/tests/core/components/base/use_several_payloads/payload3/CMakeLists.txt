kbuild_add_module(${payload_module3_name}
    "payload.c"
)

kbuild_link_module(${payload_module3_name} ${base_module_name})

kedr_test_install_module(${payload_module3_name})