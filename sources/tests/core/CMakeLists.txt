########################################################################
# Add the tests here and set their properties if necessary.
#
# [Naming conventions]
# Name of each test should be unique and should have the following format:
#   <namespace>[.group].<numeric_id>
# For global tests, <namespace> is the name of the package ("kedr").
# For local tests, <namespace> is the name of the respective component of
#   KEDR (like "core", etc.).
# The numeric ID should contain at least two digits. It is not required for
# the IDs to go in any particular order.
# 
# A name of a test must not be a substring of a name of some other test.
# For example, using "aaa.01" and "aaa.011" together is not allowed but
# it is acceptable to use "aaa.001" and "aaa.011" together.
########################################################################


# Paths to directories(source and binary), where KEDR core module was built.
set (CORE_SOURCE_DIR "${CMAKE_SOURCE_DIR}/core")
set (CORE_BINARY_DIR "${CMAKE_BINARY_DIR}/core")


add_subdirectory(simple)
add_subdirectory(payload_api)
add_subdirectory(components)

