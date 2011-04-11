if(KEDR_STANDARD_CALLM_PAYLOADS)
	set(templates_dir "${CMAKE_CURRENT_SOURCE_DIR}/templates/") 

	add_subdirectory(trigger_payload)
	add_subdirectory(trigger_target)
	
	configure_file(${CMAKE_CURRENT_SOURCE_DIR}/test.sh.in
			${CMAKE_CURRENT_BINARY_DIR}/test.sh
			@ONLY)

	#Add trigger target for user space access functions.
	# Other job for test call interception of these function
	# is done in call monitor payloads directory.
	add_subdirectory(trigger_target_user_space_access)
endif(KEDR_STANDARD_CALLM_PAYLOADS)
