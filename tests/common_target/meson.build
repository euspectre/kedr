# Prepare the sources of this test module in the build tree.

files_to_copy = ['cfake.c', 'cfake.h', 'Makefile']
foreach f : files_to_copy
	configure_file(
		input : f,
		output : f,
		configuration : configuration_data())
endforeach

configure_file(
	input : kedr_stubs_src,
	output : 'kedr_stubs.c',
	configuration : configuration_data())

kbuild_conf = configuration_data()
kbuild_conf.set('PLUGIN_PATH', plugin_so.full_path())
kbuild_conf.set('STUBS_INC_DIR', kedr_stubs_inc)

configure_file(
	input : 'Kbuild.in',
	output : 'Kbuild',
	configuration : kbuild_conf)
