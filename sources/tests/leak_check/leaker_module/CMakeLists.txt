# The module built here leaves memory leaks - to be detected by LeakCheck
set(KMODULE_NAME ${KEDR_TEST_LEAKER_MODULE})

kbuild_add_module(${KMODULE_NAME} 
	"test_module.c"
)

kedr_test_install_module (${KMODULE_NAME})
