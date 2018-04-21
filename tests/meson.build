testenv = environment()
testenv.set('MESON_SOURCE_ROOT', meson.source_root())
testenv.set('MESON_BUILD_ROOT', meson.build_root())

# Directory the test can use to build additional components it needs, e.g.
# kernel modules for the given kernel version.
testenv.set('TEST_BUILD_ROOT', meson.current_build_dir())

# The rules of instrumentation.
testenv.set('KEDR_RULES_FILE', kedr_i13n_rules)

test_basics = find_program('test_basics.py')
test('basic operations', test_basics, env : testenv, is_parallel : false,
     timeout : 300)

subdir('common_target')

# TODO: more tests
