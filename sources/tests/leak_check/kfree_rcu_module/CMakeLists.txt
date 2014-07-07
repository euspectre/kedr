# This module uses kfree_rcu() to free an RCU-protected structure
set(KMODULE_NAME ${KEDR_TEST_KFREE_RCU_MODULE})

kbuild_add_module(${KMODULE_NAME} 
	"test_module.c"
)

kedr_test_install_module (${KMODULE_NAME})
