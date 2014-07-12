set(KEDR_TEST_LEAKER_MODULE "kedr_test_leaker")
set(KEDR_TEST_CLEANER_MODULE "kedr_test_cleaner")

set(KEDR_TEST_DIR "${KEDR_TEST_PREFIX_TEMP_SESSION}/leak_check")

if(KERNEL_PART)
    kbuild_include_directories ("${CMAKE_CURRENT_SOURCE_DIR}")
endif(KERNEL_PART)

if(USER_PART)
    kedr_module_ref(KEDR_LC_COMMON_MM_REF "kedr_lc_common_mm")

    # '@ONLY' is essential when doing substitutions in the shell scripts. 
    # Without it, CMake would replace "${...}" too, which is usually not what 
    # you want.
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/test_basics.sh.in"
        "${CMAKE_CURRENT_BINARY_DIR}/test_basics.sh"
        @ONLY
    )

    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/leak_check_test.conf.in"
        "${CMAKE_CURRENT_BINARY_DIR}/leak_check_test.conf"
        @ONLY
    )
    
    kedr_test_install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/leak_check_test.conf"
        "${CMAKE_CURRENT_SOURCE_DIR}/check_addresses.awk"
        "${CMAKE_CURRENT_SOURCE_DIR}/check_summary.awk"
    )


    kedr_test_install(PROGRAMS "test_basics.sh")

    kedr_test_add_script_shared(leak_check.01 
        test_basics.sh "both"
    )

    kedr_test_add_script_shared(leak_check.02 
        test_basics.sh "leaker" 4
    )

    kedr_test_add_script_shared(leak_check.03 
        test_basics.sh "cleaner" 4
    )
endif(USER_PART)

if(KERNEL_PART)
    add_subdirectory(leaker_module)
    add_subdirectory(cleaner_module)
endif(KERNEL_PART)

# We need to add test in user part, but condition for test's existence
# is checked only in kernel part.
#
# Currently add such tests only when both user and kernel parts are built
# together.
if (HAVE_KFREE_RCU AND USER_PART AND KERNEL_PART)
    set(KEDR_TEST_KFREE_RCU_MODULE "kedr_test_kfree_rcu")
    
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/test_kfree_rcu.sh.in"
        "${CMAKE_CURRENT_BINARY_DIR}/test_kfree_rcu.sh"
        @ONLY
    )
    
    kedr_test_add_script(leak_check.kfree_rcu.01
        test_kfree_rcu.sh
    )
    
    add_subdirectory(kfree_rcu_module)
endif (HAVE_KFREE_RCU AND USER_PART AND KERNEL_PART)

if (DEFINED KEDR_FUNC_USED_kasprintf AND DEFINED KEDR_FUNC_USED_kvasprintf
    AND USER_PART AND KERNEL_PART
)
    # Test the processing of kasprintf() and kvasprintf(). The former 
    # function has a variable argument list so we currently can not 
    # test interception of the calls to it in a usual way.
    set(KEDR_TEST_KASPRINTF_MODULE "kedr_test_kasprintf")
    
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/test_kasprintf.sh.in"
        "${CMAKE_CURRENT_BINARY_DIR}/test_kasprintf.sh"
        @ONLY
    )
    
    kedr_test_add_script(leak_check.kasprintf.01
        test_kasprintf.sh
    )    
    
    add_subdirectory(kasprintf_module)
endif (DEFINED KEDR_FUNC_USED_kasprintf AND DEFINED KEDR_FUNC_USED_kvasprintf
 AND USER_PART AND KERNEL_PART
)

set(KEDR_TEST_ALLOC_IMPL_MODULE "kedr_test_alloc_impl")

if(USER_PART)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/test_alloc_impl.sh.in"
        "${CMAKE_CURRENT_BINARY_DIR}/test_alloc_impl.sh"
        @ONLY
    )

    kedr_test_add_script(leak_check.alloc_impl.01
        test_alloc_impl.sh
    )
endif(USER_PART)

if(KERNEL_PART)
    add_subdirectory(alloc_impl_module)
endif(KERNEL_PART)

if(USER_PART)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/test_flush.sh.in"
        "${CMAKE_CURRENT_BINARY_DIR}/test_flush.sh"
        @ONLY
    )

    kedr_test_add_script(leak_check.flush.01
        test_flush.sh
    )

    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/test_clear.sh.in"
        "${CMAKE_CURRENT_BINARY_DIR}/test_clear.sh"
        @ONLY
    )

    kedr_test_add_script(leak_check.clear.01
        test_clear.sh
    )
endif(USER_PART)