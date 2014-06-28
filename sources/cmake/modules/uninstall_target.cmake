# NOTE: Should be included from the top-level CMakeLists.txt.
set(this_dir "${CMAKE_SOURCE_DIR}/cmake/modules")

configure_file(
    "${this_dir}/uninstall_target_files/cmake_uninstall.cmake.in"
    "${CMAKE_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY
)

configure_file(
    "${this_dir}/uninstall_target_files/cmake_uninstall_dirs.cmake.in"
    "${CMAKE_BINARY_DIR}/cmake_uninstall_dirs.cmake"
    IMMEDIATE @ONLY
)


add_custom_target (uninstall_files
    "${CMAKE_COMMAND}" -P "${CMAKE_BINARY_DIR}/cmake_uninstall.cmake"
)

# Create empty directories list
FILE(WRITE "${CMAKE_BINARY_DIR}/uninstall_dirs.txt")

# Add directory(ies) for uninstall
function(add_uninstall_dir dir)
    foreach(d ${dir} ${ARGN})
        FILE(APPEND "${CMAKE_BINARY_DIR}/uninstall_dirs.txt" "${d}\n")
    endforeach(d ${dir} ${ARGN})
endfunction(add_uninstall_dir dir)

add_custom_target (uninstall_dirs
    "${CMAKE_COMMAND}" -P "${CMAKE_BINARY_DIR}/cmake_uninstall_dirs.cmake"
)


add_custom_target (uninstall)

add_dependencies (uninstall uninstall_files uninstall_dirs)
# During uninstall process, the files should be removed first, then 
# the directories.
add_dependencies (uninstall_dirs uninstall_files)

