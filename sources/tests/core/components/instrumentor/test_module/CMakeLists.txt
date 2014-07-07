kbuild_add_module(${test_module_name}
    "test_module.c"
)
kbuild_link_module(${test_module_name} ${instrumentor_module_name})

kedr_test_install_module(${test_module_name})