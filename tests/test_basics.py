#!/usr/bin/env python3

''' The tests for the basic functionality of the instrumentation system.
'''

import sys
import os
import os.path
import shutil
import subprocess
import unittest


def _read_env():
    if not 'MESON_SOURCE_ROOT' in os.environ:
        raise RuntimeError('Environment variable MESON_SOURCE_ROOT is not set.')
    if not 'MESON_BUILD_ROOT' in os.environ:
        raise RuntimeError('Environment variable MESON_BUILD_ROOT is not set.')
    if not 'TEST_BUILD_ROOT' in os.environ:
        raise RuntimeError('Environment variable TEST_BUILD_ROOT is not set.')

    return (os.environ['MESON_SOURCE_ROOT'],
            os.environ['MESON_BUILD_ROOT'],
            os.environ['TEST_BUILD_ROOT'])

CORE_SUBDIR = 'core'
KEDR_CORE = CORE_SUBDIR + '/kernel/kedr/kedr.ko'

def _build_kedr_core(source_root, test_dir, kernel):
    '''Build KEDR core for the given kernel.'''
    print('Building the KEDR core module.')
    shutil.copytree(os.path.join(source_root, 'kernel'),
                    os.path.join(test_dir, CORE_SUBDIR))
    subprocess.run(['make',
                    'KBUILD_DIR=/lib/modules/%s/build/' % kernel,
                    '-C', CORE_SUBDIR],
                   check=True)
    if not os.path.exists(KEDR_CORE):
        raise RuntimeError(KEDR_CORE + ' is missing.')

    print('Successfully built ' + os.path.join(test_dir, KEDR_CORE))


TARGET_SUBDIR = 'target'
TARGET_MOD = TARGET_SUBDIR + '/kedr_common_target.ko'

def _build_target_module(build_root, test_dir, kernel):
    '''Build the target module for the given kernel.'''
    print('Building the target kernel module.')
    shutil.copytree(os.path.join(build_root, 'tests/common_target'),
                    os.path.join(test_dir, TARGET_SUBDIR))
    subprocess.run(['make',
                    'KBUILD_DIR=/lib/modules/%s/build/' % kernel,
                    '-C', TARGET_SUBDIR],
                   check=True)
    if not os.path.exists(TARGET_MOD):
        raise RuntimeError(TARGET_MOD + ' is missing.')

    print('Successfully built ' + os.path.join(test_dir, TARGET_MOD))


if __name__ == '__main__':
    source_root, build_root, test_build_root = _read_env()
    kernel = os.uname().release
    print('Target kernel: ' + kernel)
    sys.stdout.flush()

    test_dir = os.path.join(test_build_root, kernel)
    if os.path.exists(test_dir):
        shutil.rmtree(test_dir)

    os.mkdir(test_dir)
    os.chdir(test_dir)

    _build_kedr_core(source_root, test_dir, kernel)
    _build_target_module(build_root, test_dir, kernel)

# TODO

    # Fail, until the test has been implemented.
    print('%s: not implemented yet. CWD: %s' % (sys.argv[0], os.getcwd()))
    sys.exit(1)
