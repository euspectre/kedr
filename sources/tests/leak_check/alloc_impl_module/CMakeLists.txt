set(KMODULE_NAME ${KEDR_TEST_ALLOC_IMPL_MODULE})

kbuild_add_module(${KMODULE_NAME} 
	"test_module.c"
)

kedr_test_install_module (${KMODULE_NAME})
