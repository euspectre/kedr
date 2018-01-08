plugin_name = meson.project_name() + '-i13n'

cpp = meson.get_compiler('cpp')

# The compiler object cannot be used in run_command() yet (meson 0.41.2)
# but we need to run it to get the location of the header files for GCC
# plugins.
# For now, let us get 'gcc' directly. This is better than nothing but may
# cause problems though, if the user chooses a different GCC when building.
gcc = find_program('gcc')

plugin_dir_result = run_command(gcc, '-print-file-name=plugin')
plugin_dir = plugin_dir_result.stdout().strip()
if plugin_dir_result.returncode() != 0 or plugin_dir == ''
	error('Failed to find the development files for the GCC plugins.')
endif
message('Found development files for GCC plugins in ' + plugin_dir + '.')

plugin_inc = include_directories(join_paths(plugin_dir, 'include'))

# -Wno-literal-suffix is a workaround for the issue with -std=c++11 and
# the GCC headers: "invalid suffix on literal <...>"
cpp_args = ['-fno-rtti', '-Wno-literal-suffix']
if cpp.has_argument('-fvisibility=hidden')
	cpp_args += '-fvisibility=hidden'
endif

plugin_src = ['i13n.cpp',
	      'classes.cpp',
	      'i13n.h',
	      'common_includes.h']

shared_module(plugin_name,
	       sources : plugin_src,
	       name_prefix : '',
	       include_directories : plugin_inc,
	       cpp_args : cpp_args,
	       install : true,
	       install_dir : kedr_libdir)
