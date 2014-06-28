kbuild_add_module(${verificator_module_name}
    "verificator.c"
)
kbuild_link_module(${verificator_module_name} ${base_module_name})

kedr_test_add_target(${verificator_module_name})