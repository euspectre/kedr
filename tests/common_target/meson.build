# Prepare the sources of this test module in the build tree.

files_to_copy = ['cfake.c', 'cfake.h', 'Makefile']
foreach f : files_to_copy
	configure_file(
		input : f,
		output : f,
		configuration : configuration_data())
endforeach

# kedr_helpers.c is generated during the build. The tests need a copy here.
copy_helper = custom_target('copy-helper',
			    output : 'kedr_helpers.c',
			    command : ['cp', '-f', kedr_helpers_src, '@OUTPUT@'],
			    depends : stub_src,
			    build_by_default : true)

kbuild_conf = configuration_data()
kbuild_conf.set('PLUGIN_PATH', plugin_so.full_path())
kbuild_conf.set('KEDR_INC_DIR', kedr_helpers_inc_dir)
kbuild_conf.set('HELPERS_INC', kedr_helpers_hdr)

configure_file(
	input : 'Kbuild.in',
	output : 'Kbuild',
	configuration : kbuild_conf)
