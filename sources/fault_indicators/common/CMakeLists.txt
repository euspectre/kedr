#name of module created
set(kmodule_name "kedr_fsim_indicator_common")

create_indicator(${kmodule_name})

kedr_install_kmodule(${kmodule_name})

kedr_test_add_subdirectory(tests)
