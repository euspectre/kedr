# Location of this CMake module
set(template_generation_module_dir "${CMAKE_SOURCE_DIR}/cmake/modules")

# Script for update dependencies for template generation process.
set(update_dependencies_script "${template_generation_module_dir}/template_generation_files/update_deps.sh")

# Generate file using kedr_gen tool.
#
# kedr_generate (filename datafile template_dir)
#
# filename - name of file to be generated. File will be created in the
# CMAKE_CURRENT_BINARY_DIR.
# datafile - file with template data
# template_dir - directory with templates used for generation.
#
# If 'datafile' contains non-absolute path, it is assumed to be relative
# to CMAKE_CURRENT_SOURCE_DIR if file exists, or to CMAKE_CURRENT_BINARY_DIR
# (see to_abs_path() function).
function(kedr_generate filename datafile template_dir)
    to_abs_path(datafile_abs ${datafile})
    set(output_file "${CMAKE_CURRENT_BINARY_DIR}/${filename}")

    # File contained dependencies definitions of generation process.
    set(deps_file "${CMAKE_CURRENT_BINARY_DIR}/.${filename}.deps")

    # Usually, templates are static files, and already exist at
    # 'configure' stage(cmake command executing).
    # So perform dependencies generation at that stage.
    #
    # If template files are created at build stage, then dependencies
    # will be regenerated at the first build. In that case second build
    # call(with unchanged templates) will trigger automatic reconfiguration,
    # but build stage itself will not do anything.
    execute_process(COMMAND sh "${update_dependencies_script}" "${deps_file}" ${template_dir}
	RESULT_VARIABLE setup_dependency_result
    )
    if(setup_dependency_result)
	message(FATAL_ERROR "Failed to generate dependencies of template generation for ${output_file}")
    endif(setup_dependency_result)
    
    
    set(deps_list)
    # 'deps_file' set 'deps_list' variable to list of real dependencies.
    include("${deps_file}")
    
    add_custom_command(OUTPUT "${output_file}"
# Because output file is created via shell redirection mechanizm, it exists
# whenever generation process is succeed or failed.
#
# Second part of the COMMAND(after ||) remove output file in case of error, and also return error.
	    COMMAND ${KEDR_GEN_TOOL} ${template_dir} ${datafile_abs} > "${output_file}" || (rm ${output_file} && /bin/false)
# Update dependencies file if dependencies changed.
#
# Because 'deps_file' is used in include() cmake command, its changing
# will trigger reconfiguration at the next build.
	    COMMAND sh "${update_dependencies_script}" "${deps_file}" ${template_dir}
	    DEPENDS ${datafile_abs} ${deps_list}
    )
    # Add empty rules for generate dependencies files.
    #
    # It is useful when some template files will be removed:
    # without these rules build process will be terminated with error
    # (because of absent dependency file).
    #
    # Note, that output file will be rebuilt in that case, so dependencies
    # will be recalculated.
    foreach(dep ${deps_list})
	add_custom_command(OUTPUT ${dep} COMMAND /bin/true)
    endforeach(dep ${deps_list})
endfunction(kedr_generate filename datafile template_dir)