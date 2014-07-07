# This module uses kasprintf() and kvasprintf(). 
set(KMODULE_NAME ${KEDR_TEST_KASPRINTF_MODULE})

kbuild_add_module(${KMODULE_NAME} 
	"test_module.c"
)

kedr_test_install_module (${KMODULE_NAME})
