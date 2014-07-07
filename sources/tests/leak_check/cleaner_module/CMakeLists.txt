# The module built here cleans up memory leaks left by the leaker module.
# In addition, this allows to check how LeakCheck detects "unallocated"
# frees.
set(KMODULE_NAME ${KEDR_TEST_CLEANER_MODULE})

kbuild_add_module(${KMODULE_NAME} 
    "test_module.c"
)

kedr_test_install_module (${KMODULE_NAME})
