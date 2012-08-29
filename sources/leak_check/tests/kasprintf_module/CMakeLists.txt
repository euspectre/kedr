# This module uses kasprintf() and kvasprintf(). 
set(KMODULE_NAME ${KEDR_TEST_KASPRINTF_MODULE})

kbuild_include_directories (
	"${CMAKE_CURRENT_SOURCE_DIR}"
	"${CMAKE_CURRENT_SOURCE_DIR}/.."
)
kbuild_add_module(${KMODULE_NAME} 
	"test_module.c"
)

kedr_test_add_target (${KMODULE_NAME})
