# Common way for create "example", that is set of files which are
# installed "as is" into single directory.
#
# For use example, user should copy all files from this directory
# into another location. After that, building or
# whatever-example-provides may be executed.

# Property is set for every target described example.
# Concrete property's value currently has no special meaning.
define_property(TARGET PROPERTY EXAMPLE_TYPE
    BRIEF_DOCS "Whether given target describes example."
    FULL_DOCS "Whether given target describes example."
)

# List of files which example provides.
define_property(TARGET PROPERTY EXAMPLE_FILES
    BRIEF_DOCS "List of files for 'example' target"
    FULL_DOCS "List of files for 'example' target"
)

# List of source files for example.
define_property(TARGET PROPERTY EXAMPLE_SOURCES
    BRIEF_DOCS "List of sources files for 'example' target"
    FULL_DOCS "List of source files for 'example' target"
)

# Whether example target is imported.
#
# Imported example has no EXAMPLE_SOURCES property,
# but may have EXAMPLE_IMPORTED_LOCATION property set (see below).
define_property(TARGET PROPERTY EXAMPLE_IMPORTED
    BRIEF_DOCS "Whether example target is imported."
    FULL_DOCS "Whether example target is imported."
)

# Directory where imported example is located.
define_property(TARGET PROPERTY EXAMPLE_IMPORTED_LOCATION
    BRIEF_DOCS "Directory where imported example is located."
    FULL_DOCS "Directory where imported example is located."
)

#  example_add(name [EXCLUDE_FROM_ALL] [<file> [SOURCE <source>]] ...)
#
# Add example target as a set of files.
#
# Each example file should be given using its relative path inside
# (abstract) example directory.
#
# By default, origin of each example file <file> is 
# CMAKE_CURRENT_SOURCE_DIR/<file> (if this file exists)
# or
# CMAKE_CURRENT_BINARY_DIR/<file>.
#
# SOURCE option redefine path to origin of example file it immediately
# follows.
#
# By default, example target is build on 'make all' stage, thus
# triggering creation of origins for all example's files.
function(example_add name)
    set(files)
    # List of full names of source files
    set(source_files)
    # Currently processed example file. This file is already added to 'files' list.
    set(curent_file)
    # Whether previous arg is 'SOURCE'.
    set(source_keyword)
    # This variable will be cleared when EXCLUDE_FROM_ALL encountered.
    set(ALL_option "ALL")
    foreach(arg ${ARGN})
        if(source_keyword)
            if(arg STREQUAL "EXCLUDE_FROM_ALL" OR arg STREQUAL "SOURCE")
                message(FATAL_ERROR "Instead of parameter for SOURCE option ${arg} keyword is encountered.")
            endif()
            to_abs_path(source_file ${arg})
            list(APPEND source_files ${source_file})
            set(current_file)
            set(source_keyword)
        elseif(arg STREQUAL "SOURCE")
            if(NOT current_file)
                message(FATAL_ERROR "SOURCE option without previous file listed.")
            endif(NOT current_file)
            set(source_keyword "TRUE")
        elseif(arg STREQUAL "EXCLUDE_FROM_ALL")
            set(ALL_option)
        else()
            if(current_file)
                to_abs_path(source_file ${current_file})
                list(APPEND source_files ${source_file})
                set(current_file)
            endif(current_file)
            set(current_file ${arg})
            list(APPEND files ${current_file})
        endif()
    endforeach(arg)
    if(current_file)
        to_abs_path(source_file ${current_file})
        list(APPEND source_files ${source_file})
        set(current_file)
    endif(current_file)

    add_custom_target(${name} ${ALL_option}
        DEPENDS ${source_files}
    )
    set_property(TARGET ${name} PROPERTY EXAMPLE_TYPE "example")
    set_property(TARGET ${name} PROPERTY EXAMPLE_IMPORTED "FALSE")
    set_property(TARGET ${name} PROPERTY EXAMPLE_FILES ${files})
    set_property(TARGET ${name} PROPERTY EXAMPLE_SOURCES ${source_files})
endfunction(example_add)

# example_install(TARGETS example_targets ... [EXPORT <export_name>]
#    DESTINATION <dir>
#    [CONFIGURATIONS configurations ... ]
#    [COMPONENT component]
#    [FILE_PERMISSIONS permissions ...]
#    [REGEX <regex> [PERMISSIONS permissions ...]][...]
# )
#
# Install example.
# All files belonging to example target(s), listed in TARGETS,
# are installed into directory given by DESTINATION option.
#
# Iff EXPORT option is given, <export_name> will be imported example target,
# binded with install directory as its EXAMPLE_IMPORTED_LOCATION property.
#
# One can control premissions of files installed using REGEX option
# followed by PERMISSIONS option. Regex is applied to filenames, as them
# listed in example_add() command.
#
# FILE_PERMISSIONS option is applied to files, which matches to none of
# regular expressions given by REGEX options.
#
# [NB] The command's arguments syntax is based on one of install(DIRECTORY)
# command, but all example's files are installed in any case.
# Permissions on directories are currently not supported.
function(example_install mode)
    if(NOT mode STREQUAL "TARGETS")
        message(FATAL_ERROR "Only TARGETS mode is currently supported")
    endif(NOT mode STREQUAL "TARGETS")
    
    # Parse REGEX sections first.
    #
    # For every REGEX section 'example_install_SECTION${i}' is used
    # as prefix. 'i' is counted from 0.
    
    # Number of REGEX sections, it also use for assign prefix for new
    # section encountered.
    set(n_sections 0)
    # Prefix for current section, empty if no current section.
    set(section_prefix)
    # Arguments before first section.
    set(global_args)
    
    foreach(arg ${ARGN})
        if(arg STREQUAL "REGEX")
            # Found new REGEX section.
            set(section_prefix "example_install_SECTION${n_sections}")
            math(EXPR n_sections "${n_sections}+1")
            # Clear values for new section.
            set(${section_prefix}_REGEX)
            set(${section_prefix}_PERMISSIONS)
            set(current_section_keyword "REGEX")
        elseif(section_prefix)
            # Continue REGEX section.
            if(arg STREQUAL "PERMISSIONS")
                if(current_section_keyword STREQUAL "REGEX")
                    message(FATAL_ERROR "REGEX option with no argument.")
                endif(current_section_keyword STREQUAL "REGEX")
                set(current_section_keyword "PERMISSIONS")
            elseif(current_section_keyword STREQUAL "REGEX")
                set("${section_prefix}_REGEX" "${arg}")
                set(current_section_keyword)
            elseif(current_section_keyword STREQUAL "PERMISSIONS")
                list(APPEND "${section_prefix}_PERMISSIONS" "${arg}")
            else(arg STREQUAL "PERMISSIONS")
                message(FATAL_ERROR "Unknown keyword inside REGEX section: \"${arg}\"")
            endif(arg STREQUAL "PERMISSIONS")
        else(arg STREQUAL "REGEX")
            # No REGEX section is currently found.
            list(APPEND global_args ${arg})
        endif(arg STREQUAL "REGEX")
    endforeach(arg ${ARGN})
    
    # Now parse global arguments
    cmake_parse_arguments(example_install "" "EXPORT;DESTINATION;COMPONENT" "CONFIGURATIONS;FILE_PERMISSIONS" ${global_args})
    
    if(NOT example_install_DESTINATION)
        message(FATAL_ERROR "DESTINATION option is missed, but required.")
    endif(NOT example_install_DESTINATION)

    if(example_install_EXPORT)
        set(export_files)
    endif(example_install_EXPORT)
    
    set(install_options)
    if(example_install_COMPONENT)
        list(APPEND install_options COMPONENT ${example_install_COMPONENT})
    endif(example_install_COMPONENT)
    
    if(example_install_CONFIGURATIONS)
        list(APPEND install_options CONFIGURATIONS ${example_install_CONFIGURATIONS})
    endif(example_install_CONFIGURATIONS)
    
    if(example_install_FILE_PERMISSIONS)
        set(default_file_permissions_option "PERMISSIONS" ${example_install_FILE_PERMISSIONS})
    else(example_install_FILE_PERMISSIONS)
        set(default_file_permissions_option)
    endif(example_install_FILE_PERMISSIONS)
    
    foreach(target ${example_install_UNPARSED_ARGUMENTS})
        get_property(example_sources TARGET ${target} PROPERTY EXAMPLE_SOURCES)
        get_property(example_files TARGET ${target} PROPERTY EXAMPLE_FILES)
        list(LENGTH example_files n_files)
        set(index 0)
        while(index LESS ${n_files})
            list(GET example_sources ${index} example_source)
            list(GET example_files ${index} example_file)
            # Choose permissions for install file
            set(file_permissions_option ${default_file_permissions_option})
            set(regex_index 0)
            while(regex_index LESS ${n_sections})
                set(section_prefix "example_install_SECTION${regex_index}")
                if(example_file MATCHES "${${section_prefix}_REGEX}")
                    if("${section_prefix}_PERMISSIONS")
                        set(file_permissions_option PERMISSIONS ${${section_prefix}_PERMISSIONS})
                    else("${section_prefix}_PERMISSIONS")
                        set(file_permissions_option)
                    endif("${section_prefix}_PERMISSIONS")
                    # Stop search on first matching.
                    break()
                endif(example_file MATCHES "${${section_prefix}_REGEX}")
                math(EXPR regex_index "${regex_index}+1")
            endwhile(regex_index LESS ${n_sections})
            install(FILES ${example_source}
                DESTINATION ${example_install_DESTINATION}
                ${file_permissions_option}
                ${install_options}
                RENAME ${example_file}
            )
            math(EXPR index "${index} + 1")
            if(example_install_EXPORT)
                list(APPEND export_files ${example_file})
            endif(example_install_EXPORT)
        endwhile(index LESS ${n_files})
    endforeach(target ${example_install_UNPARSED_ARGUMENTS})

    if(example_install_EXPORT)
        add_custom_target(${example_install_EXPORT})

        set_property(TARGET ${example_install_EXPORT} PROPERTY EXAMPLE_TYPE "example")
        set_property(TARGET ${example_install_EXPORT} PROPERTY EXAMPLE_IMPORTED "TRUE")
        set_property(TARGET ${example_install_EXPORT} PROPERTY EXAMPLE_FILES ${export_files})
        set_property(TARGET ${example_install_EXPORT} PROPERTY EXAMPLE_IMPORTED_LOCATION ${example_install_DESTINATION})
    endif(example_install_EXPORT)
endfunction(example_install)