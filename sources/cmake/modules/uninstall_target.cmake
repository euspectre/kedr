# Add 'uninstall' target to the project.
#
# By default, only files which has been installed are removed at 'uninstall'
# stage.
#
# Function add_uninstall_dir() may be used for add directories to be
# removed.
get_filename_component(uninstall_target_this_dir ${CMAKE_CURRENT_LIST_FILE} PATH)

configure_file(
    "${uninstall_target_this_dir}/uninstall_target_files/cmake_uninstall.sh.in"
    "${CMAKE_BINARY_DIR}/cmake_uninstall.sh"
    IMMEDIATE @ONLY
)

configure_file(
    "${uninstall_target_this_dir}/uninstall_target_files/cmake_uninstall_dirs.sh.in"
    "${CMAKE_BINARY_DIR}/cmake_uninstall_dirs.sh"
    IMMEDIATE @ONLY
)


add_custom_target (uninstall_files
    COMMAND "/bin/sh" "${CMAKE_BINARY_DIR}/cmake_uninstall.sh"
)

# Create empty directories list
FILE(WRITE "${CMAKE_BINARY_DIR}/install_dirs_manifest.txt")

# Add directory(ies) for uninstall
function(add_uninstall_dir dir)
    foreach(d ${dir} ${ARGN})
        FILE(APPEND "${CMAKE_BINARY_DIR}/install_dirs_manifest.txt" "${d}\n")
    endforeach(d ${dir} ${ARGN})
endfunction(add_uninstall_dir dir)

add_custom_target (uninstall_dirs
    COMMAND "/bin/sh" "${CMAKE_BINARY_DIR}/cmake_uninstall_dirs.sh"
)


add_custom_target (uninstall)

add_dependencies (uninstall uninstall_files uninstall_dirs)
# During uninstall process, the files should be removed first, then 
# the directories.
add_dependencies (uninstall_dirs uninstall_files)

