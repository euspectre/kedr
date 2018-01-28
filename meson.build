# Note. The packagers might want to build the components of KEDR separately
# and place them in separate packages:
#
# * the GCC plugin works on the machine where the target kernel or modules
#   are built;
# * the core kernel module works on the machine where the target kernel or
#   modules run.
#
# ninja will only build the user-space components of KEDR by default.
#
# To build the kernel-mode parts for the current kernel, cd to the directory
# where they are located (usually, /usr/src/kedr*/) and run 'make'. See the
# Makefile in that directory for the build options.
############################################################################

project('kedr',
	['c', 'cpp'],
	version : '1.0',
	license : 'GPL-2.0',
	meson_version : '>= 0.44',
	default_options : ['buildtype=debugoptimized',	# -g -O2
			   'warning_level=2' # -Wextra
			   ])

# Common installation paths, relative to the prefix.
kedr_libdir = join_paths(get_option('libdir'), 'kedr')
kedr_datadir = join_paths(get_option('datadir'), 'kedr')
kedr_kmod_srcdir = 'src/kedr'

# Stubs
kedr_stubs_src = files('i13n/kedr_stubs.c')
kedr_stubs_inc = join_paths(meson.source_root(), 'kernel/include')

subdir('i13n')
subdir('kernel')

# Keep the tests subdir last: it may use variables defined in meson.build
# files from the subdirectories processed above.
subdir('tests')
