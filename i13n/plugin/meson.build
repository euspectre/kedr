plugin_name = meson.project_name() + '-i13n'

cpp = meson.get_compiler('cpp')

# The compiler object cannot be used in run_command() yet (meson 0.44)
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

dep_yaml = dependency('yaml-0.1', version: '>=0.1.6')

flex = find_program('flex')
lexer_src = custom_target('lexer_src',
	input: 'rules_lexer.l',
	output: 'rules_lexer.cpp',
	command: [flex, '-o', '@OUTPUT@', '@INPUT@'])

# The compiler may warn about unused functions and some other things
# in the generated lexer code. I can do nothing about it except silence
# these warnings. GCC pragmas could be used for that but I'd like to keep
# all custom build options here rather than in the source files.
# On the other hand, Meson does not support per-source compiler flags
# by design. It is recommended to build the respective sources as a static
# library instead: https://github.com/mesonbuild/meson/issues/1367.
lexer_lib = static_library('rules_lexer', sources: lexer_src,
	cpp_args: ['-Wno-unused-function', '-Wno-sign-compare'])

plugin_src = ['i13n.cpp',
	      'rules.cpp',
	      'rules.h',
	      'internal.h',
	      'common_includes.h']

plugin_so = shared_module(
	plugin_name,
	dependencies: dep_yaml,
	sources : plugin_src,
	name_prefix : '',
	include_directories : plugin_inc,
	cpp_args : cpp_args,
	link_with: lexer_lib,
	install : true,
	install_dir : kedr_libdir)
