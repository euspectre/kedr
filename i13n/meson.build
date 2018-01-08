#<>
#message('1. kedr_libdir: ' + kedr_libdir)
#message('2. kedr_datadir: ' + kedr_datadir)
#message('3. kedr_kmod_srcdir: ' + kedr_kmod_srcdir)
#<>

cpp = meson.get_compiler('cpp')
if cpp.get_id() != 'gcc' or cpp.version().version_compare('< 5.4')
	error('GCC version 5.4 or later is required.')
endif

# The stubs will be compiled into the kernel components to be analyzed.
install_data('kedr_stubs.c', install_dir : kedr_kmod_srcdir)

subdir('plugin')
