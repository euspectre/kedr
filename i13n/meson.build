cpp = meson.get_compiler('cpp')
if cpp.get_id() != 'gcc' or cpp.version().version_compare('< 5.4')
	error('GCC version 5.4 or later is required.')
endif

# The helpers created here will be compiled into the kernel components
# to be analyzed. This is why it is needed to install them.
gen_stubs = find_program('gen_stubs.py')
stub_src = custom_target('gen-stub-sources',
			 input : ['kedr_helpers.h.in', 'kedr_helpers.c.in'],
			 output : ['kedr_helpers.h', 'kedr_helpers.c'],
			 command : [gen_stubs, kedr_core_src, '@INPUT@', '@OUTPUT@'],
			 install : true,
			 install_dir : kedr_kmod_srcdir)

# The rules to use during the instrumentation.
install_data('rules.yml', install_dir : kedr_datadir)

subdir('plugin')
