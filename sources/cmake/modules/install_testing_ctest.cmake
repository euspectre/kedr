# Infrustructure for create tests, which will be executed using CTest
# after installing.
#
# Tests may be run using "run_tests.sh" script located in the
# <install_dir>, which is passed to ictest_enable_testing() function.
#
# This script effectively execute "ctest" from <install_dir> directory.

get_filename_component(_ictest_module_dir ${CMAKE_CURRENT_LIST_FILE} PATH)

include(CMakeParseArguments)

#  ictest_enable_testing(<install_dir> [COMPONENT <component>])
#
# Activate testing infrastructure.
# Without this function all other functions in this module do nothing.
#
# <install_dir> will be used as base install directory for tests.
#
# Directory(build one) where this function is called is used as
# base build one for tests. When being installed, every test remains
# path relative base binary/install directory.
#
# If COMPONENT options is given, it will be used in install() commands
# for tests.
macro(ictest_enable_testing install_dir)
    cmake_parse_arguments(ictest_enable_testing "" "COMPONENT" "" ${ARGN})
    set(_ictest_install_defs)
    if(ictest_enable_testing_COMPONENT)
        list(APPEND _ictest_install_defs COMPONENT "${ictest_enable_testing_COMPONENT}")
    endif(ictest_enable_testing_COMPONENT)
    
    set(_ictest_enabled "ON")
    set(_ictest_install_dir "${install_dir}")
    set(_ictest_source_dir "${CMAKE_CURRENT_SOURCE_DIR}")
    set(_ictest_binary_dir "${CMAKE_CURRENT_BINARY_DIR}")
    
    _ictest_setup_dir("${_ictest_source_dir}" "${_ictest_binary_dir}")
    configure_file("${_ictest_module_dir}/install_testing_ctest_files/run_tests.sh.in"
        "${CMAKE_CURRENT_BINARY_DIR}/run_tests.sh")
    
    install(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/run_tests.sh"
        DESTINATION ${install_dir}
        ${_ictest_install_defs}
    )
endmacro(ictest_enable_testing install_dir)

#  ictest_add_test(testname <executable> [args...])
#
# Add test with given name. For execute test, next command will be executed:
#  <executable> [args..]
function(ictest_add_test testname executable)
    if(NOT _ictest_enabled)
        return()
    endif(NOT _ictest_enabled)

    ictest_add_current_dir()
    _string_cmake_escape(content "ADD_TEST(${testname}")
    foreach(arg ${executable} ${ARGN})
        _string_cmake_escape(arg_escaped "${arg}")
        set(content "${content} \"${arg_escaped}\"")
    endforeach(arg ${executable} ${ARGN})
    set(content "${content})\n")
    
    _ictest_file_append("${CMAKE_CURRENT_SOURCE_DIR}" "${content}")
endfunction(ictest_add_test testname executable)

#  ictest_add_current_dir()
#
# Make CTest aware about current directory(binary).
#
# Normally, this function is called automatically when test is added
# to this directory or inner one.
#
# Function is required to be called from directory, which added with
# add_subdirectory() command with different source and binary parts.
# In that case function should be called before any inner
# add_subdirectory() command.
function(ictest_add_current_dir)
    if(NOT _ictest_enabled)
        return()
    endif(NOT _ictest_enabled)

    # Collect list of directories which needs setup.
    set(dirs_for_setup)
    # Iterator. After while() it will be equal to the last added dir.
    set(dir "${CMAKE_CURRENT_SOURCE_DIR}") # Iterator
    # Tested value. After while() it will be equal to the binary dir of last added dir.
    get_property(binary_dir DIRECTORY ${dir} PROPERTY ICTEST_BINARY_DIR)
    while(NOT binary_dir)
        # Prepend!
        # That's why we need preliminary while() cycle before foreach() one.
        set(dirs_for_setup "${dir}" ${dirs_for_setup})
        get_property(dir DIRECTORY ${dir} PROPERTY PARENT_DIRECTORY)
        get_property(binary_dir DIRECTORY ${dir} PROPERTY ICTEST_BINARY_DIR)
    endwhile(NOT binary_dir)

    # Setup all directories and adjust parent-child links.
    #
    # On every iteration ${dir} is parent for ${child_dir} and
    # ${binary_dir} is parent's binary dir.
    foreach(child_dir ${dirs_for_setup})
        if(child_dir STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}")
            file(RELATIVE_PATH binary_dir_rel ${binary_dir} "${CMAKE_CURRENT_BINARY_DIR}")
        else(child_dir STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}")
            # Determine child binary dir according to parent one.
            #
            # Here we use assumption that source and binary arguments for
            # corresponded add_subdirectory() command are equal.
            file(RELATIVE_PATH binary_dir_rel "${dir}" "${child_dir}")
        endif(child_dir STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}")

        set(child_binary_dir "${binary_dir}/${binary_dir_rel}")
        _ictest_setup_dir("${child_dir}" "${child_binary_dir}")
        
        _ictest_file_append("${dir}" "SUBDIRS(${binary_dir_rel})\n")
        
        set(dir ${child_dir})
        set(binary_dir ${child_binary_dir})
    endforeach(child_dir ${dirs_for_setup})
endfunction(ictest_add_current_dir)
######################## Internal definitions ##########################
set(_ictest_filename "CTestTestfile.cmake")
set(_ictest_filename_build "CTestConfig.cmake")

define_property(DIRECTORY PROPERTY ICTEST_POS
    BRIEF_DOCS "Current position inside ctest configuration file"
    FULL_DOCS "Current position inside ctest configuration file"
)

define_property(DIRECTORY PROPERTY ICTEST_BINARY_DIR
    BRIEF_DOCS "Corresponded binary directory"
    FULL_DOCS "Corresponded binary directory"
)

# _ictest_setup_dir(dir bindir)
function(_ictest_setup_dir dir bindir)
    set_property(DIRECTORY "${dir}" PROPERTY ICTEST_BINARY_DIR "${bindir}")
    set_property(DIRECTORY "${dir}" PROPERTY ICTEST_POS "0")
    
    # Create CTest configuration file and write header to it.
    _ictest_file_append("${dir}" "# Some header\n")
    
    # Install file created
    file(RELATIVE_PATH bindir_rel ${_ictest_binary_dir} ${bindir})
    install(FILES "${bindir}/${_ictest_filename_build}"
        DESTINATION "${_ictest_install_dir}/${bindir_rel}"
        ${_ictest_install_defs}
        RENAME "${_ictest_filename}"
    )
endfunction(_ictest_setup_dir dir bindir)

function(_ictest_file_append dir content)
    get_property(pos DIRECTORY "${dir}" PROPERTY ICTEST_POS)
    get_property(binary_dir DIRECTORY "${dir}" PROPERTY ICTEST_BINARY_DIR)
    file_update_append("${binary_dir}/${_ictest_filename_build}" "${content}" pos)
    set_property(DIRECTORY "${dir}" PROPERTY ICTEST_POS ${pos})
endfunction(_ictest_file_append dir content)


# Form quoted string for cmake(and ctest).
function(_string_cmake_escape var str)
    string(REPLACE "\\" "\\\\" str "${str}")
    string(REPLACE "\"" "\\\"" str "${str}")
    string(REPLACE "\$" "\\\$" str "${str}")
    
    set("${var}" "${str}" PARENT_SCOPE)
endfunction(_string_cmake_escape var str)
