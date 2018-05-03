test_basics = find_program('test_kedr.py')

test('basic operations',
     test_basics,
     args : ['--topsrcdir=' + meson.source_root(),
             '--topbuilddir=' + meson.build_root(),
             '--testdir=' + meson.current_build_dir(),
             'basics'],
     is_parallel : false,
     timeout : 600)

subdir('common_target')

# TODO: more tests
