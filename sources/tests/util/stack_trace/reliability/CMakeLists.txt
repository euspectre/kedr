# Test module
set(KMODULE_NAME "test_stack_trace")

if(USER_PART)
    # '@ONLY' is essential when doing substitutions in the shell scripts. 
    # Without it, CMake would replace "${...}" too, which is usually not what 
    # you want.
    configure_file (
      "${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in"
      "${CMAKE_CURRENT_BINARY_DIR}/test.sh"
      @ONLY
    )

    kedr_test_add_script (stack_trace.01 
        test.sh
    )
endif(USER_PART)

if(KERNEL_PART)
    rule_copy_file("stack_trace.c"
        "${CMAKE_SOURCE_DIR}/util/stack_trace/stack_trace.c") 

    kbuild_add_module(${KMODULE_NAME} 
        "test_module.c"
        "stack_trace.c"
    )

    kedr_test_install_module (${KMODULE_NAME})
endif(KERNEL_PART)
