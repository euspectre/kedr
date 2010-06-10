# The path where the examples will be installed
set (KEDR_EXAMPLE_PATH "share/${KEDR_PACKAGE_NAME}-${KEDR_VERSION_MAJOR}.${KEDR_VERSION_MINOR}/examples")

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
