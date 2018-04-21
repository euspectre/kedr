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
kedr_libdir = join_paths(get_option('libdir'), meson.project_name())
kedr_datadir = join_paths(get_option('datadir'), meson.project_name())
kedr_kmod_srcdir = 'src/kedr'

# Location of the files which may be needed for testing.
kedr_helpers_hdr = join_paths(meson.build_root(), 'i13n/kedr_helpers.h')
kedr_helpers_src = join_paths(meson.build_root(), 'i13n/kedr_helpers.c')
# The helpers may need the header files from here:
kedr_helpers_inc_dir = join_paths(meson.source_root(), 'kernel/include')
# The rules of instrumentation:
kedr_i13n_rules = join_paths(meson.source_root(), 'i13n/rules.yml')

# Main source file of the KEDR core module.
kedr_core_src = files('kernel/kernel/kedr/core.c')

subdir('i13n')
subdir('kernel')

# Keep the tests subdir last: it may use variables defined in meson.build
# files from the subdirectories processed above.
subdir('tests')
