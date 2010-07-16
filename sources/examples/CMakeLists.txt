# The path where the examples will be installed
set (KEDR_EXAMPLE_PATH "${KEDR_INSTALL_PREFIX_EXAMPLES}")

# TODO: provide instructions to install the examples

#
# [NB] Note there is no trailing '/' in the name of the directory. This is
# important. If it were present, the contents of the directory would be 
# installed to the specified path rather than the directory
# itself.
#
# install (DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/example1"
#    DESTINATION "${KEDR_EXAMPLE_PATH}")

#######################################################################

add_subdirectory(sample_target)