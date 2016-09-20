set(PLUGIN_TARGET ${PLUGIN_NAME}-shared)
set(PLUGIN_SOURCES
	i13n.cpp
	classes.cpp
	i13n.h
	common_includes.h
	${CMAKE_CURRENT_BINARY_DIR}/config.h
)

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/config.h.in"
	"${CMAKE_CURRENT_BINARY_DIR}/config.h"
	@ONLY
)

# -fvisibility=hidden, the CMake way
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

# -Wno-literal-suffix - workaround for the issue with -std=c++11 and
# the GCC headers: "invalid suffix on literal <...>"
add_definitions(-Wall -Wextra -fno-rtti -std=c++11 -Wno-literal-suffix)

# Find the directory with the header files for GCC plugins
execute_process(
	COMMAND ${CMAKE_C_COMPILER} -print-file-name=plugin
	OUTPUT_VARIABLE plugin_dir
	OUTPUT_STRIP_TRAILING_WHITESPACE
)
if (NOT plugin_dir)
	message(FATAL_ERROR "Failed to find GCC plugin directory")
endif ()

include_directories(
	"${CMAKE_CURRENT_BINARY_DIR}"
	"${CMAKE_CURRENT_SOURCE_DIR}"
	"${plugin_dir}/include"
)

add_library(${PLUGIN_NAME} MODULE ${PLUGIN_SOURCES})
set_target_properties(${PLUGIN_NAME} PROPERTIES
	PREFIX ""
)

install(TARGETS ${PLUGIN_NAME}
	DESTINATION "${KEDR_LIBDIR}"
)
