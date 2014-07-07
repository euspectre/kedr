# Useful functions for install programs, kernel modules and other
# components needed for tests.
#
# For simplicity, we install these components to the same relative location,
# where them was in source/binary tree.

#  itesting_init(<base_install_dir>)
#
# Initialize infrustructure, assigning given dir as base install directory
# for all testing components.
function(itesting_init base_install_dir)
    set(_itesting_base_install_dir "${base_install_dir}" PARENT_SCOPE)
    set(_itesting_base_binary_dir "${CMAKE_CURRENT_BINARY_DIR}" PARENT_SCOPE)
endfunction(itesting_init base_install_dir)

#  itesting_path(RESULT_VAR [<path>])
#
# Return install path corresponded to the given <path>.
#
# <path> should be absolute path:
#   1) inside or equal to binary dir, where itesting_init() was called,
#   2) inside or equal to CMAKE_CURRENT_SOURCE_DIR.
#
# If not given, <path> is assumed to be CMAKE_CURRENT_BINARY_DIR.
function(itesting_path RESULT_VAR)
    set(path "${CMAKE_CURRENT_BINARY_DIR}")
    foreach(arg ${ARGN})
        set(path "${arg}")
    endforeach(arg ${ARGN})

    # Firstly assume path to be in binary dir.
    file(RELATIVE_PATH path_relative "${_itesting_base_binary_dir}" "${path}")
    if(path_relative MATCHES "^\\.\\.")
        # First assumption fails.
        file(RELATIVE_PATH path_relative_source "${CMAKE_CURRENT_SOURCE_DIR}" "${path}")
        if(path_relative_source MATCHES "^\\.\\.")
            message(FATAL_ERROR "Path is neither inside testing binary dir nor inside CMAKE_CURRENT_SOURCE_DIR: '${path}'")
        endif(path_relative_source MATCHES "^\\.\\.")
        # Translate current source dir to current binary dir.
        file(RELATIVE_PATH path_relative "${_itesting_base_binary_dir}" "${CMAKE_CURRENT_BINARY_DIR}/${path_relative_source}")
    endif(path_relative MATCHES "^\\.\\.")
    set("${RESULT_VAR}" "${_itesting_base_install_dir}/${path_relative}" PARENT_SCOPE)
endfunction(itesting_path RESULT_VAR)