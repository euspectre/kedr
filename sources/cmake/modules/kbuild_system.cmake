# Provide way for create kernel modules, and perform other actions on them
# in a way similar to standard cmake executables and libraries processing.
#
# NB: At the end of the whole configuration stage
#   kbuild_finalize_linking()
# should be executed.
#
# Should be included after:
#  find_package(Kbuild)
#
# Cached variables(ADVANCED), which affects on definitions below:
#  CROSS_COMPILE - corresponded parameter for 'make' to build kernel objects.
#  KBUILD_C_FLAGS - Additional kbuild flags for all builds
#  KBUILD_C_FLAGS{DEBUG|RELEASE|RELWITHDEBINFO|MINSIZEREL} - per-configuration flags.

include(cmake_useful)

# Compute path to the auxiliary files.
get_filename_component(kbuild_this_module_dir ${CMAKE_CURRENT_LIST_FILE} PATH)
set(kbuild_aux_dir "${kbuild_this_module_dir}/kbuild_system_files")

# _declare_per_build_vars(<variable> <doc-pattern>)
#
# Per-build type definitions for given <variable>.
# 
# Create CACHE STRING variables <variable>_{DEBUG|RELEASE|RELWITHDEBINFO|MINSIZEREL}
# with documentation string <doc-pattern> where %build% is replaced
# with corresponded build type description.
#
# Default value for variable <variable>_<type> is taken from <variable>_<type>_init.
macro(_declare_per_build_vars variable doc_pattern)
    set(_build_type)
    foreach(t
        RELEASE "release"
        DEBUG "debug"
        RELWITHDEBINFO "Release With Debug Info"
        MINSIZEREL "release minsize"
    )
        if(_build_type)
            string(REPLACE "%build%" "${t}" _docstring ${doc_pattern})
            set("${variable}_${_build_type}" "${${variable}_${_build_type}_INIT}"
                CACHE STRING "${_docstring}")
            set(_build_type)
        else(_build_type)
            set(_build_type "${t}")
        endif(_build_type)
    endforeach(t)
endmacro(_declare_per_build_vars variable doc_pattern)


# Default compiler flags.
#
# These flags are used directly when configuring 'Kbuild', so them should be
# expressed as single-string, where different flags are joinged using ' '.
set(KBUILD_C_FLAGS "" CACHE STRING
    "Compiler flags used by Kbuild system during all builds."
)

# Additional default compiler flags per build type.
set(KBUILD_C_FLAGS_DEBUG_INIT "-g")
set(KBUILD_C_FLAGS_RELWITHDEBINFO_INIT "-g")

_declare_per_build_vars(KBUILD_C_FLAGS
    "Compiler flags used by Kbuild system during %build% builds."
)
mark_as_advanced(
    KBUILD_C_FLAGS
    KBUILD_C_FLAGS_DEBUG
    KBUILD_C_FLAGS_RELEASE
    KBUILD_C_FLAGS_RELWITHDEBINFO
    KBUILD_C_FLAGS_MINSIZEREL
)


# Additional definitions which are passed directly to 'make' for
# kbuild process.
#
# Unlike to compiler flags, these ones are expressed using cmake list.
set(KBUILD_MAKE_FLAGS "" CACHE STRING
    "Make flags used by Kbuild system during all builds."
)

_declare_per_build_vars(KBUILD_MAKE_FLAGS
    "Make flags used by Kbuild system during %build% builds."
)

mark_as_advanced(
    KBUILD_MAKE_FLAGS
    KBUILD_MAKE_FLAGS_DEBUG
    KBUILD_MAKE_FLAGS_RELEASE
    KBUILD_MAKE_FLAGS_RELWITHDEBINFO
    KBUILD_MAKE_FLAGS_MINSIZEREL
)


# Per-directory tracking for kbuild compiler flags.
#
# Unlike to standard COMPILE_FLAGS, these flags do not include values
# set for parent directories.
# (There is no generic "fill-with-parent's values" mechanism exists
# in cmake).
#
# So, this property has a little sence for the user.
# It exists only for make effect of kbuild_add_definitions() to 
# cross "function", "foreach" and other non-directory scopes.
define_property(DIRECTORY PROPERTY KBUILD_COMPILE_FLAGS
    BRIEF_DOCS "Compiler flags used by Kbuild system added in this directory."
    FULL_DOCS "Compiler flags used by Kbuild system added in this directory."
)

# Per-directory tracking for kbuild include directories.
#
# Unlike to standard INCLUDE_DIRECTORIES, these ones do not include values
# set for parent directories.
# (There is no generic "fill-with-parent's values" mechanism exists
# in cmake).
#
# So, this property has a little sence for the user.
# It exists only for make effect of kbuild_add_definitions() to 
# cross "function", "foreach" and other non-directory scopes.
define_property(DIRECTORY PROPERTY KBUILD_INCLUDE_DIRECTORIES
    BRIEF_DOCS "Include directories used by Kbuild system; added in this directory."
    FULL_DOCS "Include directories used by Kbuild system; added in this directory."
)

# Parameters below are set externally only in try_compile() for subproject,
# which include this file.
# No need to cache them as try_compile() project is not configured by the user.

# Real top-level source directory.
if(NOT KBUILD_REAL_SOURCE_DIR)
    set(KBUILD_REAL_SOURCE_DIR ${CMAKE_SOURCE_DIR})
endif(NOT KBUILD_REAL_SOURCE_DIR)
# Real top-level binary directory.
if(NOT KBUILD_REAL_BINARY_DIR)
    set(KBUILD_REAL_BINARY_DIR ${CMAKE_BINARY_DIR})
endif(NOT KBUILD_REAL_BINARY_DIR)

set(CROSS_COMPILE "" CACHE STRING "Cross compilation prefix for build linux kernel objects.")
mark_as_advanced(CROSS_COMPILE)

# These flags are passed to the 'make' when compile kernel module.
set(kbuild_additional_make_flags)
# These CMake flags will be passed to try_compile() subproject.
set(kbuild_try_compile_flags
    "-DKBUILD_REAL_SOURCE_DIR=${KBUILD_REAL_SOURCE_DIR}"
    "-DKBUILD_REAL_BINARY_DIR=${KBUILD_REAL_BINARY_DIR}"
)

# As ARCH and CROSS_COMPILE are passed to submake only when non-empty.
if(ARCH)
    list(APPEND kbuild_additional_make_flags "ARCH=${ARCH}")
    list(APPEND kbuild_try_compile_flags "-DARCH=${ARCH}")
endif(ARCH)
if(CROSS_COMPILE)
    list(APPEND kbuild_additional_make_flags "CROSS_COMPILE=${CROSS_COMPILE}")
    list(APPEND kbuild_try_compile_flags "-DCROSS_COMPILE=${CROSS_COMPILE}")
endif(CROSS_COMPILE)


# Property prefixed with 'KMODULE_' is readonly for outer use,
# unless explicitely noted in its description.

# List of targets created with kmodule_add_module() without
# 'IMPORTED' option.
#
# This list is traversed in kmodule_finalize_linking() for
# add targets which generate imported symbols list.
#
# Note, that this list does not contain all defined kernel modules,
# so property normally shouldn't be used by outer code.
define_property(GLOBAL PROPERTY KMODULE_TARGETS
    BRIEF_DOCS "List of kernel module targets configured for build"
    FULL_DOCS "List of kernel module targets configured for build"
)
set_property(GLOBAL PROPERTY KMODULE_TARGETS)

# Property is set for every target described kernel module.
# Concrete property's value currently has no special meaning.
define_property(TARGET PROPERTY KMODULE_TYPE
    BRIEF_DOCS "Whether given target describes kernel module."
    FULL_DOCS "Whether given target describes kernel module."
)

# Absolute filename of .ko file which is built.
# NOTE: Imported targets use
#  KMODULE_IMPORTED_MODULE_LOCATION
# property instead.
define_property(TARGET PROPERTY KMODULE_MODULE_LOCATION
    BRIEF_DOCS "Location of the built kernel module."
    FULL_DOCS "Location of the built kernel module."
)

# Name of the given module, like "ext4" or "kedr".
define_property(TARGET PROPERTY KMODULE_MODULE_NAME
    BRIEF_DOCS "Name of the kernel module."
    FULL_DOCS "Name of the kernel module."
)


# Absolute filename of Module.symvers file which is built.
# NOTE: Imported targets use
#  KMODULE_IMPORTED_SYMVERS_LOCATION
# property instead.
define_property(TARGET PROPERTY KMODULE_SYMVERS_LOCATION
    BRIEF_DOCS "Location of the symvers file of the kernel module."
    FULL_DOCS "Location of the symvers file of the kernel module."
)

# CMAKE_CURRENT_BINARY_DIR at the moment, when kbuild_add_module() is issued.
#
# This property is used for determine, whether kbuild_link_module()
# command may be implemented as
#   add_custom_command(... APPEND)
# instead of
#   add_custom_target()
define_property(TARGET PROPERTY KMODULE_BINARY_DIR
    BRIEF_DOCS "CMAKE_CURRENT_BINARY_DIR where module is built."
    FULL_DOCS "CMAKE_CURRENT_BINARY_DIR where module is built."
)

# List of symvers files, added by kbuild_link_module(), when it is called
# from directory differed from one where module is build.
#
# This is internal property for implement linking mechanism.
define_property(TARGET PROPERTY KMODULE_FAR_SYMVERS
    BRIEF_DOCS "'Far' symvers files this module depends on."
    FULL_DOCS "'Far' symvers files this module depends on."
)

# List of targets, from which this module should indirectly depend on.
# These targets are added by kbuild_link_module(), when it is called
# from directory differed from one where module is build.
#
# This is internal property for implement linking mechanism.
define_property(TARGET PROPERTY KMODULE_FAR_DEPEND_TARGETS
    BRIEF_DOCS "'Far' targets this module depends on."
    FULL_DOCS "'Far' targets this module depends on."
)



# Property is "TRUE" for every target described imported kernel module,
# "FALSE" for other targets described kernel module.
define_property(TARGET PROPERTY KMODULE_IMPORTED
    BRIEF_DOCS "Whether given target describes imported kernel module."
    FULL_DOCS "Whether given target describes imported kernel module."
)

# Absolute filename of .ko file which is imported.
# Property may be set after
#  kbuild_add_module(... IMPORTED)
# for allow loading of given module or perform other actions required
# kernel module itself.
#
# Analogue for KMODULE_MODULE_LOCATION property of normal(non-exported)
# kernel module target.
define_property(TARGET PROPERTY KMODULE_IMPORTED_MODULE_LOCATION
    BRIEF_DOCS "Location of the imported kernel module."
    FULL_DOCS "Location of the imported kernel module."
)

# Absolute filename of Module.symvers file for imported kernel module.
# Property may be set after
#  kbuild_add_module(... IMPORTED)
# for allow linking with given module or perform other actions required
# its symbols.
#
# Analogue for KMODULE_MODULE_SYMVERS_LOCATION property of normal(non-exported)
# kernel module target.
define_property(TARGET PROPERTY KMODULE_IMPORTED_SYMVERS_LOCATION
    BRIEF_DOCS "Location of the symvers file for imported kernel module."
    FULL_DOCS "Location of the symvers file for imported kernel module."
)

# Helpers for simple extract some kernel module properties

# Return location of the module, determined by the property
# KMODULE_MODULE_LOCATION or KMODULE_IMPORTED_MODULE_LOCATION for
# imported target. In the last case the property is checked for being set.
function(kbuild_get_module_location RESULT_VAR name)
    if(NOT TARGET ${name})
        message(FATAL_ERROR "\"${name}\" is not really a target.")
    endif(NOT TARGET ${name})
    get_property(kmodule_type TARGET ${name} PROPERTY KMODULE_TYPE)
    if(NOT kmodule_type)
        message(FATAL_ERROR "\"${name}\" is not really a target for kernel module.")
    endif(NOT kmodule_type)
    get_property(kmodule_imported TARGET ${name} PROPERTY KMODULE_IMPORTED)
    if(kmodule_imported)
        get_property(module_location TARGET ${name} PROPERTY KMODULE_IMPORTED_MODULE_LOCATION)
        if(NOT module_location)
            message(FATAL_ERROR "target \"${name}\" for imported module has no property KMODULE_IMPORTED_MODULE_LOCATION set.")
        endif(NOT module_location)
    else(kmodule_imported)
        get_property(module_location TARGET ${name} PROPERTY KMODULE_MODULE_LOCATION)
    endif(kmodule_imported)
    set(${RESULT_VAR} ${module_location} PARENT_SCOPE)
endfunction(kbuild_get_module_location RESULT_VAR module)

# Return location of the symvers file for the module, determined by the
# property KMODULE_SYMVERS_LOCATION or KMODULE_IMPORTED_SYMVERS_LOCATION
# for imported target. In the last case the property is checked for being set.
function(kbuild_get_symvers_location RESULT_VAR name)
    if(NOT TARGET ${name})
        message(FATAL_ERROR "\"${name}\" is not really a target.")
    endif(NOT TARGET ${name})
    get_property(kmodule_type TARGET ${name} PROPERTY KMODULE_TYPE)
    if(NOT kmodule_type)
        message(FATAL_ERROR "\"${name}\" is not really a target for kernel module.")
    endif(NOT kmodule_type)
    get_property(kmodule_imported TARGET ${name} PROPERTY KMODULE_IMPORTED)
    if(kmodule_imported)
        get_property(symvers_location TARGET ${name} PROPERTY KMODULE_IMPORTED_SYMVERS_LOCATION)
        if(NOT symvers_location)
            message(FATAL_ERROR "target \"${name}\" for imported module has no property KMODULE_IMPORTED_SYMVERS_LOCATION set.")
        endif(NOT symvers_location)
    else(kmodule_imported)
        get_property(symvers_location TARGET ${name} PROPERTY KMODULE_SYMVERS_LOCATION)
    endif(kmodule_imported)
    set(${RESULT_VAR} ${symvers_location} PARENT_SCOPE)
endfunction(kbuild_get_symvers_location RESULT_VAR module)


# Constants for internal filenames.
set(_kbuild_symvers "Module.symvers")
set(_kbuild_symvers_imported_near "Module.symvers_imported_near")
set(_kbuild_symvers_imported_far "Module.symvers_imported_far")


# Helper for the building kernel module.
#
# copy_source_to_binary_dir(<source> <new_source_var>)
#
# Make sure that given source file is inside current binary dir.
# That place is writable and has some "uniqeness" garantee for generate
# auxiliary files during build process.
#
# If <source> already inside current binary dir, do nothing and
# set <new_source_var> variable to <source> itself.
#
# Otherwise create rule for copy source into "nice" place inside
# current binary dir and set <new_source_var> pointed to that place.
function(copy_source_to_binary_dir source new_source_var)
    is_path_inside_dir(is_in_current_binary ${CMAKE_CURRENT_BINARY_DIR} "${source}")
    if(is_in_current_binary)
        # Source is already placed where we want.
        set(${new_source_var} "${source}" PARENT_SCOPE)
    else(is_in_current_binary)
        # Base directory from which count relative source path.
        # By default, it is root directory.
        set(base_dir "/")
        # Try "nicer" base directories.
        foreach(d
            ${KBUILD_REAL_BINARY_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${KBUILD_REAL_SOURCE_DIR}
            )
            is_path_inside_dir(is_in_d ${d} "${source}")
            if(is_in_d)
                set(base_dir ${d})
                break()
            endif(is_in_d)
        endforeach(d)
		# Copy source into same relativ dir, but inside current binary one.
        file(RELATIVE_PATH source_rel "${base_dir}" "${source}")
		set(new_source "${CMAKE_CURRENT_BINARY_DIR}/${source_rel}")
        get_filename_component(new_source_dir ${new_source} PATH)
        file(MAKE_DIRECTORY "${new_source_dir}")
		rule_copy_file("${new_source}" "${source}")
        # Return path to the new source.
        set(${new_source_var} "${new_source}" PARENT_SCOPE)
    endif(is_in_current_binary)
endfunction(copy_source_to_binary_dir source new_source_var)

# kbuild_add_module(<name> [EXCLUDE_FROM_ALL] [MODULE_NAME <module_name>] [<sources> ...])
#
# Build kernel module from <sources>, analogue of add_library().
#
# Source files are divided into two categories:
# -Object sources
# -Other sourses
#
# Object sources are those sources, which may be used in building kernel
# module externally.
# Follow types of object sources are supported now:
# .c: a file with the code in C language;
# .S: a file with the code in assembly language;
# .o_shipped: shipped file in binary format,
#             does not require additional preprocessing.
# 
# Other sources are treated as only prerequisite of building process.
#
#
# kbuild_add_module(<name> [MODULE_NAME <module_name>] IMPORTED)
#
# Create target, corresponded to the imported kernel module.
# In that case KMODULE_IMPORTED_* properties should be set manually if needed.
#
# In either case, if MODULE_NAME option is given, it determine name
# of the kernel module. Otherwise <name> itself is used.
function(kbuild_add_module name)
    cmake_parse_arguments(kbuild_add_module "IMPORTED;EXCLUDE_FROM_ALL" "MODULE_NAME" "" ${ARGN})
    if(kbuild_add_module_MODULE_NAME)
        set(module_name "${kbuild_add_module_MODULE_NAME}")
    else(kbuild_add_module_MODULE_NAME)
        set(module_name "${name}")
    endif(kbuild_add_module_MODULE_NAME)
    
    string(LENGTH ${module_name} module_name_len)
    if(module_name_len GREATER 55)
        # Name of the kernel module should fit into array of size (64-sizeof(unsigned long)).
        # On 64-bit systems(most restricted) this is 56.
        # Even 56 length is not good, as it is not include null character.
        # Without it, old versions of rmmod failed to unload module.
        message(SEND_ERROR "Kernel module name exceeds 55 characters: '${module_name}'")
    endif(module_name_len GREATER 55)
    
    if(kbuild_add_module_IMPORTED)
        # Creation of IMPORTED target is simple.
        add_custom_target(${name})
        set_property(TARGET ${name} PROPERTY KMODULE_TYPE "kmodule")
        set_property(TARGET ${name} PROPERTY KMODULE_IMPORTED "TRUE")
        set_property(TARGET ${name} PROPERTY KMODULE_MODULE_NAME "${module_name}")
        return()
    endif(kbuild_add_module_IMPORTED)

    if(kbuild_add_module_EXCLUDE_FROM_ALL)
        set(all_arg)
    else(kbuild_add_module_EXCLUDE_FROM_ALL)
        set(all_arg "ALL")
    endif(kbuild_add_module_EXCLUDE_FROM_ALL)
    
    # List of all source files, which are given.
    set(sources ${kbuild_add_module_UNPARSED_ARGUMENTS})

    # Sources with absolute paths
    to_abs_path(sources_abs ${sources})
    # list of files from which module building is depended
    set(depend_files)
    # Sources of "c" type, but without extension.
    # Used for clean files, and for out-of-source builds do not create
    # files in source tree.
    set(c_sources_noext_abs)
	# The sources with the code in assembly
	set(asm_sources_noext_abs)
	# Sources of "o_shipped" type, but without extension
	set(shipped_sources_noext_abs)
    # Categorize sources
    foreach(source_abs ${sources_abs})
        get_filename_component(ext ${source_abs} EXT)
        if(ext STREQUAL ".c" OR ext STREQUAL ".S" OR ext STREQUAL ".o_shipped")
			# Real sources
			# Move source into binary tree, if needed
			copy_source_to_binary_dir("${source_abs}" source_abs)
            
            get_filename_component(source_noext "${source_abs}" NAME_WE)
            get_filename_component(source_dir "${source_abs}" PATH)
            set(source_noext_abs "${source_dir}/${source_noext}")
            if(ext STREQUAL ".c")
				# c-source
				list(APPEND c_sources_noext_abs ${source_noext_abs})
			elseif(ext STREQUAL ".S")
				# asm source
				list(APPEND asm_sources_noext_abs ${source_noext_abs})
			elseif(ext STREQUAL ".o_shipped")
				# shipped-source
				list(APPEND shipped_sources_noext_abs ${source_noext_abs})
			endif(ext STREQUAL ".c")
		endif(ext STREQUAL ".c" OR ext STREQUAL ".S" OR ext STREQUAL ".o_shipped")
		# In any case, add file to depend list
        list(APPEND depend_files ${source_abs})
    endforeach(source_abs ${sources_abs})

    # Object sources relative to current binary dir
    # (for $(module)-y :=)
    set(obj_sources_noext_rel)
    foreach(obj_sources_noext_abs
            ${c_sources_noext_abs} ${asm_sources_noext_abs} ${shipped_sources_noext_abs})
        file(RELATIVE_PATH obj_source_noext_rel
            ${CMAKE_CURRENT_BINARY_DIR} ${obj_sources_noext_abs})
        list(APPEND obj_sources_noext_rel ${obj_source_noext_rel})
    endforeach(obj_sources_noext_abs)

    if(NOT obj_sources_noext_rel)
        message(FATAL_ERROR "List of object files for module ${name} is empty.")
    endif(NOT obj_sources_noext_rel)
    # Detect, if build simple - source object name coincide with module name
    if(obj_sources_noext_rel STREQUAL ${name})
        set(is_build_simple "TRUE")
    else(obj_sources_noext_rel STREQUAL ${name})
        #Detect, if only one of source object names coincide with module name.
        #This situation is incorrect for kbuild system.
        list(FIND obj_sources_noext_rel ${name} is_objects_contain_name)
        if(is_objects_contain_name GREATER -1)
            message(FATAL_ERROR "Module should be built "
            "either from only one object with same name, "
            "or from objects with names different from the name of the module")
        endif(is_objects_contain_name GREATER -1)
        set(is_build_simple "FALSE")
    endif(obj_sources_noext_rel STREQUAL ${name})

    # Build 'kbuild' file - object sources string
    if(is_build_simple)
        set(obj_src_string "")
    else(is_build_simple)
        set(obj_src_string "${name}-y :=")
        foreach(obj ${obj_sources_noext_rel})
            set(obj_src_string "${obj_src_string} ${obj}.o")
        endforeach(obj ${obj_sources_noext_rel})
    endif(is_build_simple)

    # Build kbuild file - compiler flags
    _kbuild_get_compile_flags(ccflags)
    # Append include directories definitions to the flags
    _kbuild_get_include_directories(include_dirs)

    foreach(dir ${include_dirs})
        _string_join(" " ccflags "${ccflags}" "-I${dir}")
    endforeach(dir ${include_dirs})

    # Configure kbuild file
    configure_file(${kbuild_this_module_dir}/kbuild_system_files/Kbuild.in
                    ${CMAKE_CURRENT_BINARY_DIR}/Kbuild
                    )
    
    # Target for create module.
    add_custom_target(${name} ALL
        DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/${module_name}.ko"
                "${CMAKE_CURRENT_BINARY_DIR}/${_kbuild_symvers}"
    )

	# Create .cmd files for 'shipped' sources.
    # Gcc does not create them automatically for some reason.
	if(shipped_sources_noext_abs)
		set(cmd_create_command)
		foreach(shipped_source_noext_abs ${shipped_source_noext_abs})
			get_filename_component(shipped_dir ${shipped_source_noext_abs} PATH)
            get_filename_component(shipped_name ${shipped_source_noext_abs} NAME)
			list(APPEND cmd_create_command
				COMMAND printf "cmd_%s.o := cp -p %s.o_shipped %s.o\\n"
					"${shipped_source_noext_abs}"
					"${shipped_source_noext_abs}"
					"${shipped_source_noext_abs}"
					> "${shipped_dir}/.${shipped_name}.o.cmd")
		endforeach(shipped_source_noext_abs ${shipped_source_noext_abs})
	endif(shipped_sources_noext_abs)
    
    # User-defined parameters for 'make'
    set(make_flags ${KBUILD_MAKE_FLAGS})
    _get_per_build_var(make_flags_per_build KBUILD_MAKE_FLAGS)
    list(APPEND make_flags ${make_flags_per_build})
    
    # Rule for create module(and symvers file).
    add_custom_command(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${module_name}.ko"
            "${CMAKE_CURRENT_BINARY_DIR}/${_kbuild_symvers}"
        COMMAND $(MAKE) ${make_flags} ${kbuild_additional_make_flags} 
            -C ${Kbuild_BUILD_DIR} M=${CMAKE_CURRENT_BINARY_DIR} modules
# Update timestamps for targets.
# In some cases Kbuild system may decide do not update resulted files
# even in case when depencies newer.
# Updating timestamps prevent command to be executed every make call. 
        COMMAND touch "${CMAKE_CURRENT_BINARY_DIR}/${module_name}.ko"
            "${CMAKE_CURRENT_BINARY_DIR}/${_kbuild_symvers}"
        DEPENDS ${depend_files}
            ${CMAKE_CURRENT_BINARY_DIR}/${_kbuild_symvers_imported_near}
            ${CMAKE_CURRENT_BINARY_DIR}/${_kbuild_symvers_imported_far}
        COMMENT "Building kernel module ${name}"
    )
    
    # By default, empty 'symvers_imported_near' file is created.
    # For every 'near' link additional depends and command are added
    # via APPEND option for add_custom_command().
    add_custom_command(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${_kbuild_symvers_imported_near}"
        COMMAND truncate -s 0 "${CMAKE_CURRENT_BINARY_DIR}/${_kbuild_symvers_imported_near}"
# Comment is not precise: not 'all' symvers files are collected here, only for 'near' links.
# But 'far' links are rare, and they are processed in different target.
        COMMENT "Collecting all symvers files which kernel module depends on"
    )

    # Fill properties for the target.
    set_property(TARGET ${name} PROPERTY KMODULE_TYPE "kmodule")
    set_property(TARGET ${name} PROPERTY KMODULE_IMPORTED "FALSE")
    set_property(TARGET ${name} PROPERTY KMODULE_MODULE_NAME "${module_name}")
    set_property(TARGET ${name} PROPERTY KMODULE_BINARY_DIR
        "${CMAKE_CURRENT_BINARY_DIR}"
    )
    set_property(TARGET ${name} PROPERTY KMODULE_MODULE_LOCATION
        "${CMAKE_CURRENT_BINARY_DIR}/${module_name}.ko"
    )
    set_property(TARGET ${name} PROPERTY KMODULE_SYMVERS_LOCATION
        "${CMAKE_CURRENT_BINARY_DIR}/${_kbuild_symvers}"
    )
    
    set_property(TARGET ${name} PROPERTY KMODULE_FAR_SYMVERS)
    set_property(TARGET ${name} PROPERTY KMODULE_FAR_DEPEND_TARGETS)

    # Add target to the list for later linking.
    set_property(GLOBAL APPEND PROPERTY KMODULE_TARGETS "${name}")
    
    # The rule to clean files
	_kbuild_module_clean_files(${name}
		C_SOURCE ${c_sources_noext_abs}
		ASM_SOURCE ${asm_sources_noext_abs}
		SHIPPED_SOURCE ${shipped_sources_noext_abs})
endfunction(kbuild_add_module name)

# kbuild_include_directories(dir1 .. dirn)
macro(kbuild_include_directories)
    set_property(DIRECTORY APPEND PROPERTY KBUILD_INCLUDE_DIRECTORIES ${ARGN})
endmacro(kbuild_include_directories)

# kbuild_add_definitions (flags)
#
# Specify additional compiler flags for the module.
function(kbuild_add_definitions flags)
    get_property(current_flags DIRECTORY PROPERTY KBUILD_COMPILE_DEFINITIONS)
    _string_join(" " current_flags "${current_flags}" "${flags}")
    set_property(DIRECTORY PROPERTY KBUILD_COMPILE_DEFINITIONS "${current_flags}")
endfunction(kbuild_add_definitions flags)

# Flags for make has no control except default values.
# Support for kbuild_add_make_definitions may be added if needed.


# kbuild_module_link(<name> [<link> ...])
#
# Link kernel module with other modules, that allows to use symbols from
# other modules.
#
# <link> may be target name for other compiled kernel module or
# absolute path to symvers file of other kernel module.
#
# Analogue for target_link_library().
#
# TODO: if module is linked within same directory, where it is created,
# process link with add_custom_command(...APPEND) instead of adding target.
function(kbuild_link_module name)
    # Check that @name corresponds to kernel module target.
    get_property(kmodules_list GLOBAL PROPERTY KMODULE_TARGETS)
    list(FIND kmodules_list kmodule_index "${name}")
    if(kmodule_index EQUAL "-1")
        message(FATAL_ERROR "kbuild_module_link: passed <name>\n\t\"${name}\"\n which is not target name for compiled kernel module.")
    endif(kmodule_index EQUAL "-1")
    
    # List of imported symvers files
    set(symvers_locations)
    # Optional target dependencies
    set(depend_targets)
    foreach(l ${ARGN})
        string(REGEX MATCH "/" link_is_file ${l})
        if(link_is_file)
            string(REGEX MATCH "^/" file_is_absolute ${l})
            if(NOT file_is_absolute)
                message(FATAL_ERROR "kbuild_module_link: passed filename\n\t\"${link}\"\nwhich is not absolute as <link>.")
            endif(NOT file_is_absolute)
            set(symvers_location "${l}")
            # Do not require symvers file to be already existed.
        else(link_is_file)
            if(NOT TARGET ${l})
                message(FATAL_ERROR "kbuild_module_link: passed link\n\t\"${l}\"\n which is neither an absolute path to symvers file nor a target.")
            endif(NOT TARGET ${l})
            get_property(kmodule_type TARGET ${l} PROPERTY KMODULE_TYPE)
            if(NOT kmodule_type)
                message(FATAL_ERROR "kbuild_module_link: passed target\n\t\"${l}\"\n which is not a target for kernel module as link.")
            endif(NOT kmodule_type)
            get_property(kmodule_imported TARGET ${l} PROPERTY KMODULE_IMPORTED)
            if(kmodule_imported)
                get_property(symvers_location TARGET ${l} PROPERTY KMODULE_IMPORTED_SYMVERS_LOCATION)
                if(NOT symvers_location)
                    message(FATAL_ERROR "kbuild_module_link: passed imported target\n\t\"${l}\"\n without \"KMODULE_IMPORTED_SYMVERS_LOCATION\" property set as link.")
                endif(NOT symvers_location)
            else(kmodule_imported)
                get_property(symvers_location TARGET ${l} PROPERTY KMODULE_SYMVERS_LOCATION)
                # Target dependency is added only for non-imported target.
                list(APPEND depend_targets ${l})
            endif(kmodule_imported)
        endif(link_is_file)
        list(APPEND symvers_locations ${symvers_location})
    endforeach(l ${ARGN})

    # No links at all? Return immediately.
    if(NOT symvers_locations)
        return()
    endif(NOT symvers_locations)

    # Concrete actions depends on whether linking is 'near' or 'far'
    get_property(module_binary_dir TARGET ${name} PROPERTY KMODULE_BINARY_DIR)

    if(CMAKE_CURRENT_BINARY_DIR STREQUAL "${module_binary_dir}")
        # 'Near' linking. Create(append) new command immediately.
        add_custom_command(
            OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${_kbuild_symvers_imported_near}"
            COMMAND cat ${symvers_locations} >> "${CMAKE_CURRENT_BINARY_DIR}/${_kbuild_symvers_imported_near}"
            DEPENDS ${symvers_locations}
            APPEND
        )
        if(depend_targets)
            add_dependencies(${name} ${depend_targets})
        endif(depend_targets)

    else(CMAKE_CURRENT_BINARY_DIR STREQUAL "${module_binary_dir}")
        # 'Far' linking. Postpone new target creation to kbuild_finalize_linking().
        set_property(TARGET ${name} APPEND PROPERTY KMODULE_FAR_SYMVERS ${symvers_locations})
        if(depend_targets)
            set_property(TARGET ${name} APPEND PROPERTY KMODULE_FAR_DEPEND_TARGETS ${depend_targets})
        endif(depend_targets)
    endif(CMAKE_CURRENT_BINARY_DIR STREQUAL "${module_binary_dir}")

endfunction(kbuild_link_module name)

# Should be called after all kernel modules and their links defined.
function(kbuild_finalize_linking)
    get_property(kmodule_targets GLOBAL PROPERTY KMODULE_TARGETS)
    foreach(m ${kmodule_targets})
        get_property(module_binary_dir TARGET ${m} PROPERTY KMODULE_BINARY_DIR)
        get_property(far_symvers TARGET ${m} PROPERTY KMODULE_FAR_SYMVERS)

        if(far_symvers)
            # Name of the target which should fill 'far' imported symvers file.
            set(intermediate_target "_kmodule_far_link_${m}")
            
            add_custom_target("${intermediate_target}"
                DEPENDS "${module_binary_dir}/${_kbuild_symvers_imported_far}"
            )

            add_custom_command(OUTPUT "${module_binary_dir}/${_kbuild_symvers_imported_far}"
                COMMAND cat ${far_symvers} > "${module_binary_dir}/${_kbuild_symvers_imported_far}"
                DEPENDS ${far_symvers}
            )

            add_dependencies(${m} ${intermediate_target})
            
            get_property(far_depend_targets TARGET ${m} PROPERTY KMODULE_FAR_DEPEND_TARGETS)
            if(far_depend_targets)
                add_dependencies(${intermediate_target} ${far_depend_targets})
            endif(far_depend_targets)
        else(far_symvers)
            # Nor 'far' links? Just precreate empty file.
            file_update("${module_binary_dir}/${_kbuild_symvers_imported_far}" "")
        endif(far_symvers)
    endforeach(m ${kmodule_targets})
endfunction(kbuild_finalize_linking)

# kbuild_install(TARGETS <module_name> ...
#    [[MODULE|SYMVERS]
#      DESTINATION <dir>
#      [CONFIGURATIONS [...]]
#      [COMPONENT <component>]
#    ]+)
#
# Install kernel module(s) and/or symvers file(s) into given directory.
#
# Almost all options means same as for install() cmake command.
# 'MODULE' refers to kernel module itself, SYMVERS refers symvers file.
#
# Unlike to standard install() command, there is no default destination
# directory neither for modules nor for symvers files.
# So, at least one rule should be defined, and 'DESTINATION' option
# should be set for every rule.
#
# TODO: support for 'EXPORT' mode.
function(kbuild_install type)
    if(NOT type STREQUAL "TARGETS")
        message(FATAL_ERROR "Only 'TARGETS' mode is currently supported")
    endif(NOT type STREQUAL "TARGETS")
    
    parse_install_arguments(kbuild_install
        "MODULE;SYMVERS" # Section types
        "" "DESTINATION;COMPONENT" "CONFIGURATIONS" # Section keywords classification.
        ${ARGN}
    )
    
    if(NOT kbuild_install_TARGETS)
        message(FATAL_ERROR "No targets given for kbuild_install() command")
    endif(NOT kbuild_install_TARGETS)
    
    if(NOT kbuild_install_sections)
        message(FATAL_ERROR "There is no default destination for install kernel module components, but no section described it is given.")
    endif(NOT kbuild_install_sections)

    foreach(section_type ${kbuild_install_sections})
        if(NOT kbuild_install_${section_type}_DESTINATION)
            message(FATAL_ERROR "DESTINATION is not defined for section ${section_type}")
        endif(NOT kbuild_install_${section_type}_DESTINATION)
        # All additional arguments for given section.
        set(install_args_${section_type}
            DESTINATION "${kbuild_install_${section_type}_DESTINATION}")
        if(kbuild_install_${section_type}_CONFIGURATION)
            list(APPEND install_args_${section_type}
                CONFIGURATION "${kbuild_install_${section_type}_CONFIGURATION}"
            )
        endif(kbuild_install_${section_type}_CONFIGURATION)
        if(kbuild_install_${section_type}_COMPONENT)
            list(APPEND install_args_${section_type}
                COMPONENT "${kbuild_install_${section_type}_COMPONENT}"
            )
        endif(kbuild_install_${section_type}_COMPONENT)
        # Module installation.
        if(section_type STREQUAL "MODULE" OR section_type STREQUAL "ALL")
            # Combine locations for all modules in one list.
            set(module_locations)
            foreach(t ${kbuild_install_TARGETS})
                kbuild_get_module_location(module_location ${t})
                list(APPEND module_locations ${module_location})
            endforeach(t ${kbuild_install_TARGETS})
            # .. and install them at once.
            install(FILES ${module_locations} ${install_args_${section_type}})
        endif(section_type STREQUAL "MODULE" OR section_type STREQUAL "ALL")
        # Symvers installation.
        if(section_type STREQUAL "SYMVERS" OR section_type STREQUAL "ALL")
            # Because of renaming, symvers files should be installed separately.
            foreach(t ${kbuild_install_TARGETS})
                kbuild_get_symvers_location(symvers_location ${t})
                get_property(module_name TARGET ${t} PROPERTY KMODULE_MODULE_NAME)
                install(FILES ${symvers_location}
                    RENAME "${module_name}.symvers"
                    ${install_args_${section_type}}
                )
            endforeach(t ${kbuild_install_TARGETS})
        endif(section_type STREQUAL "SYMVERS" OR section_type STREQUAL "ALL")
    endforeach(section_type ${kbuild_install_sections})
endfunction(kbuild_install type)


# kbuild_try_compile(RESULT_VAR bindir srcfile|SOURCES src ...
#           [CMAKE_FLAGS <Flags>]
#           [KBUILD_COMPILE_DEFINITIONS flags ...]
#           [OUTPUT_VARIABLE var])
#
# Similar to try_module in simplified form, but compile srcfile as
# kernel module, instead of user space program.
#
# KBUILD_COMPILE_DEFINITIONS contains compiler definition flags for
# build kernel module.
#
# Possible CMAKE_FLAGS which has special semantic:
#  KBUILD_INCLUDE_DIRECTORIES - include directories for build kernel module
#  KBUILD_LINK_MODULE - symvers file(s) for link module with other modules.
function(kbuild_try_compile RESULT_VAR bindir srcfile)
    cmake_parse_arguments(kbuild_try_compile "" "OUTPUT_VARIABLE" "CMAKE_FLAGS;KBUILD_COMPILE_DEFINITIONS" ${ARGN})
    if(srcfile STREQUAL "SOURCES")
        set(srcfiles ${kbuild_try_compile_UNPARSED_ARGUMENTS})
    else(srcfile STREQUAL "SOURCES")
        set(srcfiles ${srcfile})
    endif(srcfile STREQUAL "SOURCES")

    # Inside try_compile project values of variables
    # CMAKE_CURRENT_BINARY_DIR and CMAKE_CURRENT_SOURCE_DIR
    # differs from ones in current project.
    #
    # So make all paths absolute before pass them into try_compile project.
    # Note, that copiing files into binary dir is nevertheless performed
    # in the try_compile project, because it bases on real current binary dir.
    to_abs_path(srcfiles_abs ${srcfiles})
    
    # Collect parameters to try_compile() function
    set(cmake_flags
        "-DSOURCES:STRING=${srcfiles_abs}" # Source file(s)
        "-DCMAKE_MODULE_PATH:PATH=${CMAKE_MODULE_PATH}" # Path for search include files.
        ${kbuild_try_compile_CMAKE_FLAGS}
    )

    # Parameters for compiler
    if(kbuild_try_compile_KBUILD_COMPILE_DEFINITIONS)
        list(APPEND cmake_flags
            "-DKBUILD_COMPILE_DEFINITIONS:STRING=${kbuild_try_compile_KBUILD_COMPILE_DEFINITIONS}"
        )
    endif(kbuild_try_compile_KBUILD_COMPILE_DEFINITIONS)

    # Other user-defined cmake flags
    if(kbuild_try_compile_CMAKE_FLAGS)
        
    endif(kbuild_try_compile_CMAKE_FLAGS)

    # Possible definition of output variable.
    if(kbuild_try_compile_OUTPUT_VARIABLE)
        set(output_variable_def "OUTPUT_VARIABLE" "output_tmp")
    else(build_try_compile_OUTPUT_VARIABLE)
        set(output_variable_def)
    endif(kbuild_try_compile_OUTPUT_VARIABLE)

    
    try_compile(result_tmp # Result variable(temporary)
        "${bindir}" # Binary directory
        "${kbuild_aux_dir}/try_compile_project" # Source directory
        "kmodule" # Project name
         CMAKE_FLAGS # Flags to CMake:
            ${cmake_flags}
            ${kbuild_try_compile_flags}
        ${output_variable_def}
    )
    
    if(kbuild_try_compile_OUTPUT_VARIABLE)
        # Set output variable for the caller
        set("${kbuild_try_compile_OUTPUT_VARIABLE}" "${output_tmp}" PARENT_SCOPE)
    endif(kbuild_try_compile_OUTPUT_VARIABLE)
    # Set result variable for the caller
    set("${RESULT_VAR}" "${result_tmp}" PARENT_SCOPE)
endfunction(kbuild_try_compile RESULT_VAR bindir srcfile)

########### Auxiliary functions for internal use #######################

# _get_per_build_var(RESULT_VAR variable)
#
# Return value of per-build variable.
macro(_get_per_build_var RESULT_VAR variable)
    if(CMAKE_BUILD_TYPE)
        string(TOUPPER "${CMAKE_BUILD_TYPE}" _build_type_uppercase)
        set(${RESULT_VAR} "${${variable}_${_build_type_uppercase}}")
    else(CMAKE_BUILD_TYPE)
        set(${RESULT_VAR})
    endif(CMAKE_BUILD_TYPE)
endmacro(_get_per_build_var RESULT_VAR variable)

#
# _string_join(sep RESULT_VAR str1 str2)
# Join strings <str1> and <str2> using <sep> as glue.
#
# Note, that precisely 2 string are joined, not a list of strings.
# This prevents automatic replacing of ';' inside strings while parsing arguments.
macro(_string_join sep RESULT_VAR str1 str2)
    if("${str1}" STREQUAL "")
        set("${RESULT_VAR}" "${str2}")
    elseif("${str2}" STREQUAL "")
        set("${RESULT_VAR}" "${str1}")
    else("${str1}" STREQUAL "")
        set("${RESULT_VAR}" "${str1}${sep}${str2}")
    endif("${str1}" STREQUAL "")
endmacro(_string_join sep RESULT_VAR str1 str2)
# _build_get_directory_property_chained(RESULT_VAR <propert_name> [<separator>])
#
# Return list of all values for given property in the current directory
# and all parent directories.
#
# If <separator> is given, it is used as glue for join values.
# By default, cmake list separator (';') is used.
function(_get_directory_property_chained RESULT_VAR property_name)
    set(sep ";")
    foreach(arg ${ARGN})
        set(sep "${arg}")
    endforeach(arg ${ARGN})
    set(result "")
    set(d "${CMAKE_CURRENT_SOURCE_DIR}")
    while(NOT "${d}" STREQUAL "")
        get_property(p DIRECTORY "${d}" PROPERTY "${property_name}")
        # debug
        # message("Property ${property_name} for directory ${d}: '${p}'")
        _string_join("${sep}" result "${p}" "${result}")
        # message("Intermediate result: '${result}'")
        get_property(d DIRECTORY "${d}" PROPERTY PARENT_DIRECTORY)
    endwhile(NOT "${d}" STREQUAL "")
    set("${RESULT_VAR}" "${result}" PARENT_SCOPE)
endfunction(_get_directory_property_chained RESULT_VAR property_name)

# Collect all compile flags for given scope.
function(_kbuild_get_compile_flags RESULT_VAR)
    _get_directory_property_chained(compile_flags KBUILD_COMPILE_DEFINITIONS " ")
    # Common flags comes first.
    _string_join(" " compile_flags "${KBUILD_C_FLAGS}" "${compile_flags}")
    # But per-build flags are appended after all.
    _get_per_build_var(compile_flags_per_build KBUILD_C_FLAGS)
    _string_join(" " compile_flags "${compile_flags}" "${compile_flags_per_build}")

    set("${RESULT_VAR}" "${compile_flags}" PARENT_SCOPE)
endfunction(_kbuild_get_compile_flags RESULT_VAR)

# Collect all include directories for given scope
function(_kbuild_get_include_directories RESULT_VAR)
    _get_directory_property_chained(dirs KBUILD_INCLUDE_DIRECTORIES)
    # debug
    # message("dirs: '${dirs}'")
    set("${RESULT_VAR}" "${dirs}" PARENT_SCOPE)
endfunction(_kbuild_get_include_directories RESULT_VAR)

# _kbuild_module_clean_files(module_name
# 	[C_SOURCE c_source_noext_abs ...]
# 	[ASM_SOURCE asm_source_noext_abs ...]
#	[SHIPPED_SOURCE shipped_source_noext_abs ...])
#
# Tell CMake that intermediate files, created by kbuild system,
# should be cleaned with 'make clean'.
function(_kbuild_module_clean_files module_name)
    cmake_parse_arguments(kbuild_module_clean "" "" "C_SOURCE;ASM_SOURCE;SHIPPED_SOURCE" ${ARGN})
    if(kbuild_module_clean_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "Unparsed arguments")
    endif(kbuild_module_clean_UNPARSED_ARGUMENTS)

	# List common files (names only) for cleaning
	set(common_files_names
        ".tmp_versions" # Directory
        "modules.order"
		"Module.markers"
    )
	# List module name-depending files (extensions only) for cleaning
	set(name_files_ext
		".o"
		".mod.c"
		".mod.o"
    )
	# Same but for the files with names starting with a dot ('.').
	set(name_files_dot_ext
		".ko.cmd"
		".mod.o.cmd"
		".o.cmd"
    )
	# List source name-depending files (extensions only) for cleaning
	set(source_name_files_ext
		".o"
    )
	# Same but for the files with names starting with a dot ('.')
	set(source_name_files_dot_ext
		".o.cmd"
		".o.d" # This file is created in case of unsuccessfull build
    )
	
	# Now collect all sort of files into list
	set(files_list)

	foreach(name ${common_files_names})
		list(APPEND files_list "${CMAKE_CURRENT_BINARY_DIR}/${name}")
	endforeach(name ${common_files_names})
	
	foreach(ext ${name_files_ext})
		list(APPEND files_list
			"${CMAKE_CURRENT_BINARY_DIR}/${module_name}${ext}")
	endforeach(ext ${name_files_ext})
	
	foreach(ext ${name_files_dot_ext})
		list(APPEND files_list
			"${CMAKE_CURRENT_BINARY_DIR}/.${module_name}${ext}")
	endforeach(ext ${name_files_ext})
	
    # All the types of sources are processed in a similar way
    foreach(obj_source_noext_abs ${kbuild_module_clean_C_SOURCE}
        ${kbuild_module_clean_ASM_SOURCE} ${kbuild_module_clean_SHIPPED_SOURCE})

        get_filename_component(dir ${obj_source_noext_abs} PATH)
        get_filename_component(name ${obj_source_noext_abs} NAME)
        foreach(ext ${source_name_files_ext})
            list(APPEND files_list "${dir}/${name}${ext}")
        endforeach(ext ${source_name_files_ext})
        foreach(ext ${source_name_files_dot_ext})
            list(APPEND files_list "${dir}/.${name}${ext}")
        endforeach(ext ${source_name_files_ext})
    endforeach(obj_source_noext_abs)
	# Tell CMake that given files should be cleaned.
	set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "${files_list}")
endfunction(_kbuild_module_clean_files module_name)

#  parse_install_arguments(prefix <section-type-keywords> <options> <one-value-keywords> <multiple-value-keywords> args..)
#
# Helper for parse arguments for install-like command.
#
# All arguments before the first keyword are classified as TARGETS
# and stored into list ${prefix}_TARGETS.
#
# <section-type-keywords> describe possible keywords denoting section type.
# At most one section may exist for every type.
# Special type ALL means generic section. 
#
# <options>, <one-value-keywords> and <multiple-value-keywords> describe
# all keywords inside section.
# Values corresponded to these keywords are stored under
# <prefix>_<section-type>_*.
# Note, that unlike to cmake_parse_arguments(), all arguments inside
# section definition should be either keyword or its value(s).
# Additionally, <prefix>_<section-type> is set to TRUE for every
# encountered section and <prefix>_sections contains list of such sections.
#
# If non section is currently active, the first section keyword starts
# special generic section. Definitions for this section are stored as
# for section of type ALL.
# If such section exists, it should be the only section.
#
# Function is generic, but currently is used only there.
function(parse_install_arguments prefix section_types options one_value_keywords multiple_value_keywords)
    set(all_keywords ${section_types} ${options} ${one_value_keywords} ${multiple_value_keywords})
    # Type of the currently parsed section('ALL' for ALL section).
    set(current_section_type)
    # Last section-related keyword for current section.
    set(current_section_keyword)
    # Classification for @_current_section_keyword:
    # 'OPTION', 'ONE' or 'MULTY'.
    set(current_section_keyword_type)
    
    # Clean all previous keyword values.
    set("${prefix}_TARGETS")
    foreach(section_type "ALL" ${section_types})
        set(${prefix}_${section_type} "FALSE")
        foreach(opt ${options})
            set("${prefix}_${section_type}_${opt}" "FALSE")
        endforeach(opt ${options})
        foreach(keyword ${one_value_keywords} ${multiple_value_keywords})
            set("${prefix}_${section_type}_${keyword}")
        endforeach(keyword ${one_value_keywords} ${multiple_value_keywords})
    endforeach(section_type "ALL" ${section_types})
    set("${prefix}_sections")

    foreach(arg ${ARGN})
        list(FIND all_keywords ${arg} keyword_index)
        if(keyword_index EQUAL "-1")
            if(current_section_type)
                if(current_section_keyword)
                    if(current_section_keyword_type STREQUAL "OPTION")
                        message(FATAL_ERROR "Another keyword should come after option ${current_section_keyword}.")
                    elseif(current_section_keyword_type STREQUAL "ONE" AND "${prefix}_${current_section_type}_${current_section_keyword}")
                        message(FATAL_ERROR "Several values for one-value-keyword ${current_section_keyword}.")
                    endif(current_section_keyword_type STREQUAL "OPTION")
                else(NOT current_section_keyword)
                    # 'ALL' section always has '_current_section_keyword'
                    message(FATAL_ERROR "Keyword should come at the beginning of section ${current_section_type}.")
                endif(current_section_keyword)
                list(APPEND "${prefix}_${current_section_type}_${current_section_keyword}" "${arg}")
            else(current_section_type)
                list(APPEND "${prefix}_TARGETS" "${arg}")
            endif(current_section_type)
        else(keyword_index EQUAL "-1")
            if(current_section_type AND current_section_keyword AND current_section_keyword_type STREQUAL "ONE")
                message(FATAL_ERROR "Value should be specified after ${current_section_keyword} keyword.")
            endif(current_section_type AND current_section_keyword AND current_section_keyword_type STREQUAL "ONE")
            list(FIND section_types "${arg}" section_type_index)
            if(section_type_index EQUAL "-1")
                if(NOT current_section_type)
                    set(current_section_type "ALL")
                    set("${prefix}_${current_section_type}" "TRUE")
                    list(APPEND "${prefix}_sections" "${current_section_type}")
                endif(NOT current_section_type)
                set(current_section_keyword ${arg})
                list(FIND options ${current_section_keyword} option_index)
                if(option_index EQUAL "-1")
                    list(FIND one_value_keywords ${current_section_keyword} one_index)
                    if(one_index EQUAL "-1")
                        set(current_section_keyword_type "MULTY")
                    else(one_index EQUAL "-1")
                        set(current_section_keyword_type "ONE")
                    endif(one_index EQUAL "-1")
                    set("${prefix}_${current_section_type}_${current_section_keyword}")
                else(option_index EQUAL "-1")
                    set(current_section_keyword_type "OPTION")
                    set("${prefix}_${current_section_type}_${current_section_keyword}" "TRUE")
                endif(option_index EQUAL "-1")
            else(section_type_index EQUAL "-1")
                if("${prefix}_ALL")
                    message(FATAL_ERROR "Generic section should be the only section defined")
                endif("${prefix}_ALL")
                set(current_section_type "${arg}")
                if("${prefix}_${current_section_type}")
                    message(FATAL_ERROR "Section ${current_section_type} is defined twice.")
                endif("${prefix}_${current_section_type}")
                set("${prefix}_${current_section_type}" "TRUE")
                list(APPEND "${prefix}_sections" "${current_section_type}")
                # Current keyword is initially undefined for such section.
                set(current_section_keyword)
                set(current_section_keyword_type)
            endif(section_type_index EQUAL "-1")
        endif(keyword_index EQUAL "-1")
    endforeach(arg ${ARGN})

    # propagate the result variables to the caller
    set("${prefix}_TARGETS" ${${prefix}_TARGETS} PARENT_SCOPE)
    foreach(section_type "ALL" ${section_types})
        set(${prefix}_${section_type} ${${prefix}_${section_type}} PARENT_SCOPE)
        foreach(keyword ${options} ${one_value_keywords} ${multiple_value_keywords})
            set("${prefix}_${section_type}_${keyword}" ${${prefix}_${section_type}_${keyword}} PARENT_SCOPE)
        endforeach(keyword ${options} ${one_value_keywords} ${multiple_value_keywords})
    endforeach(section_type "ALL" ${section_types})
    set("${prefix}_sections" ${${prefix}_sections} PARENT_SCOPE)
endfunction(parse_install_arguments prefix section_types options one_value_keywords multiple_value_keywords)
