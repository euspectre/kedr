# This module is same as one in examples/sample_target,
# but it is built for testing purposes.

set(source_dir "${CMAKE_SOURCE_DIR}/examples/sample_target")

rule_copy_file("${CMAKE_CURRENT_BINARY_DIR}/cfake.c"
    "${source_dir}/cfake.c")

kbuild_include_directories("${source_dir}")

kbuild_add_module("kedr_sample_target"
    "cfake.c"
    "${source_dir}/cfake.h"
)

kedr_test_install_module("kedr_sample_target")

# Note that load/unload script is installed to per-kernel directory
# in the kernel part.
itesting_path(this_install_path)
install(PROGRAMS "${source_dir}/kedr_sample_target"
    DESTINATION ${this_install_path}/${Kbuild_VERSION_STRING}
)