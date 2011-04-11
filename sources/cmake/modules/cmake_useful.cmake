#Create rule for obtain one file by copying another one
function(rule_copy_file target_file source_file)
    add_custom_command(OUTPUT ${target_file}
                    COMMAND cp -p ${source_file} ${target_file}
                    DEPENDS ${source_file}
                    )
endfunction(rule_copy_file target_file source_file)

#Create rule for obtain file in binary tree by copiing it from source tree
function(rule_copy_source rel_source_file)
    rule_copy_file(${CMAKE_CURRENT_BINARY_DIR}/${rel_source_file} ${CMAKE_CURRENT_SOURCE_DIR}/${rel_source_file})
endfunction(rule_copy_source rel_source_file)

# to_abs_path(output_var path [...])
#
# Convert relative path of file to absolute path:
# use path in source tree, if file already exist there.
# otherwise use path in binary tree.
# If initial path already absolute, return it.
function(to_abs_path output_var)
    set(result)
    foreach(path ${ARGN})
        string(REGEX MATCH "^/" _is_abs_path ${path})
        if(_is_abs_path)
            list(APPEND result ${path})
        else(_is_abs_path)
            file(GLOB to_abs_path_file 
                "${CMAKE_CURRENT_SOURCE_DIR}/${path}"
            )
            if(NOT to_abs_path_file)
                set (to_abs_path_file "${CMAKE_CURRENT_BINARY_DIR}/${path}")
            endif(NOT to_abs_path_file)
            list(APPEND result ${to_abs_path_file})
        endif(_is_abs_path)
    endforeach(path ${ARGN})
    set("${output_var}" ${result} PARENT_SCOPE)
endfunction(to_abs_path output_var path)

#is_path_inside_dir(output_var dir path)
#
# Set output_var to true if path is absolute path inside given directory.
# (!) path should be absolute.
macro(is_path_inside_dir output_var dir path)
    file(RELATIVE_PATH _rel_path ${dir} ${path})
    string(REGEX MATCH "^\\.\\." _is_not_inside_dir ${_rel_path})
    if(_is_not_inside_dir)
        set(${output_var} "FALSE")
    else(_is_not_inside_dir)
        set(${output_var} "TRUE")
    endif(_is_not_inside_dir)
endmacro(is_path_inside_dir output_var dir path)

########################################################################
# Test-related macros
########################################################################

# When we are building KEDR for another system (cross-build), testing is
# disabled. This is because the tests need the build tree.
# In the future, the tests could be prepared that need only the installed 
# components of KEDR. It could be a separate test suite.

# This macro enables testing support and performs other initialization tasks.
# It should be used in the top-level CMakeLists.txt file before 
# add_subdirectory () calls.
macro (kedr_test_init)
	if (NOT CMAKE_CROSSCOMPILING)
	    enable_testing ()
	    add_custom_target (check 
	        COMMAND ${CMAKE_CTEST_COMMAND}
	    )
	    add_custom_target (build_tests)
	    add_dependencies (check build_tests)
	endif (NOT CMAKE_CROSSCOMPILING)
endmacro (kedr_test_init)

# Use this macro to specify an additional target to be built before the tests
# are executed.
macro (kedr_test_add_target target_name)
	if (NOT CMAKE_CROSSCOMPILING)
	    set_target_properties (${target_name}
	        PROPERTIES EXCLUDE_FROM_ALL true
	    )
	    add_dependencies (build_tests ${target_name})
	endif (NOT CMAKE_CROSSCOMPILING)
endmacro (kedr_test_add_target target_name)

# This function adds a test script (a Bash script, actually) to the set of
# tests for the package. The script may reside in current source or binary 
# directory (the source directory is searched first).
function (kedr_test_add_script test_name script_file)
	if (NOT CMAKE_CROSSCOMPILING)
	    set (TEST_SCRIPT_FILE)
	    to_abs_path (TEST_SCRIPT_FILE ${script_file})
	        
	    add_test (${test_name}
	        /bin/bash ${TEST_SCRIPT_FILE} ${ARGN}
	    )
	endif (NOT CMAKE_CROSSCOMPILING)
endfunction (kedr_test_add_script)

# Use this macro instead of add_subdirectory() for the subtrees related to 
# testing of the package.

# We could use other kedr_*test* macros to disable the tests when 
# cross-building, but the rules of Kbuild system (concerning .symvers,
# etc.) still need to be disabled explicitly. So it is more reliable to 
# just turn off each add_subdirectory(tests) in this case.
macro (kedr_test_add_subdirectory subdir)
	if (NOT CMAKE_CROSSCOMPILING)
		add_subdirectory(${subdir})
	endif (NOT CMAKE_CROSSCOMPILING)
endmacro (kedr_test_add_subdirectory subdir)
########################################################################
