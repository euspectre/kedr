include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
add_executable("fsim_im_test_program" program.c)
target_link_libraries("fsim_im_test_program" "kedr_fsim_indicator_manager_u")

kedr_test_add_target(fsim_im_test_program)