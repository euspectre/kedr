# This module uses kfree_rcu() to free an RCU-protected structure
set(KMODULE_NAME ${KEDR_TEST_KFREE_RCU_MODULE})

kbuild_include_directories (
	"${CMAKE_CURRENT_SOURCE_DIR}"
	"${CMAKE_CURRENT_SOURCE_DIR}/.."
)
kbuild_add_module(${KMODULE_NAME} 
	"test_module.c"
)

kedr_test_add_target (${KMODULE_NAME})
