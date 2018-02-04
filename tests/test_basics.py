#!/usr/bin/env python3

''' The tests for the basic functionality of the instrumentation system.
'''

import sys
import os
import os.path
import shutil
import subprocess
import unittest
import time


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
KEDR_ENABLED = '/sys/kernel/kedr/enabled'

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


TARGET_WRITE_CMD = 'echo 1234567890 > /dev/cfake0'
TARGET_READ_CMD = 'dd if=/dev/cfake0 of=/dev/null bs=40 count=1'
TARGET_LOOP_CMD = ' '.join(['while true; do',
                            TARGET_WRITE_CMD, ';',
                            TARGET_READ_CMD, '2>/dev/null;',
                            'done'])

def _load_module(mod):
    print('Loading %s.' % mod)
    subprocess.run(['insmod', mod], check=True)

def _unload_module(mod):
    modname, _, _ = os.path.basename(mod).rpartition('.')
    modname = modname.replace('-', '_')

    if os.path.exists('/sys/module/%s/' % modname):
        print('Unloading "%s"' % modname)
        subprocess.run(['rmmod', modname], check=True)
    else:
        print('Module "%s" is not loaded, nothing to unload.' % modname)

class KedrTestBasics(unittest.TestCase):
    '''Test the basic operations with KEDR.'''
    def __init__(self, methodName='runTest'):
        unittest.TestCase.__init__(self, methodName)
        self.last_dmesg_line = None

    def _enable_kedr(self):
        subprocess.run(['sh', '-c', 'echo 1 > ' + KEDR_ENABLED], check=True)
        with open('/sys/kernel/kedr/enabled', 'r') as ke:
            line = ke.readline().strip()
            self.assertEqual(int(line), 1)

    def _disable_kedr(self):
        subprocess.run(['sh', '-c', 'echo 0 > ' + KEDR_ENABLED], check=True)
        with open('/sys/kernel/kedr/enabled', 'r') as ke:
            line = ke.readline().strip()
            self.assertEqual(int(line), 0)

    def _check_dmesg_line(self, line, dmesg_file):
        '''Look for kernel problems reported in the given line of dmesg.'''
        if line.find(' BUG: ') != -1 or line.find(' general protection fault: ') != -1:
            print('Got kernel BUG:\n' + line)
            print('See %s for details.' % os.path.join(os.getcwd(), dmesg_file))
            raise RuntimeError('Kernel BUG was found.')

        if line.find(' WARNING: ') != -1:
            print('Got kernel WARNING:\n' + line)
            print('See %s for details.' % os.path.join(os.getcwd(), dmesg_file))
            raise RuntimeError('Kernel WARNING was found.')


    def _save_dmesg_before(self):
        dmesg_file = './dmesg-before.log'
        subprocess.run(['sh', '-c', 'dmesg > ' + dmesg_file], check=True)
        with open(dmesg_file, 'r') as dmesg:
            for ln in dmesg:
                line = ln
        self.last_dmesg_line = line

    def _check_dmesg_after(self):
        print('Checking dmesg after the test.')
        dmesg_file = './dmesg-after.log'
        subprocess.run(['sh', '-c', 'dmesg > ' + dmesg_file], check=True)
        check = False
        with open(dmesg_file, 'r') as dmesg:
            for ln in dmesg:
                if check:
                    self._check_dmesg_line(ln.strip(), dmesg_file)
                elif ln.startswith(self.last_dmesg_line):
                    check = True
        # dmesg has fully rotated during the test - check the whole dmesg.
        # Some of it may already be lost, but checking the rest is better
        # than nothing.
        if not check:
            with open(dmesg_file, 'r') as dmesg:
                for ln in dmesg:
                    self._check_dmesg_line(ln.strip(), dmesg_file)

        print('Found no problems in dmesg.')

    def _target_do_read(self):
        print('Trying to read from /dev/cfake0.')
        proc = subprocess.run(['sh', '-c', TARGET_READ_CMD])
        self.assertEqual(proc.returncode, 0)

    def _target_do_write(self):
        print('Trying to write to /dev/cfake0.')
        proc = subprocess.run(['sh', '-c', TARGET_WRITE_CMD])
        self.assertEqual(proc.returncode, 0)

    def _target_start_rw_loop(self):
        print('Starting to access /dev/cfake0 in an infinite loop.')
        return subprocess.Popen(['sh', '-c', TARGET_LOOP_CMD])

    def setUp(self):
        self._save_dmesg_before()
        _load_module(KEDR_CORE)

    def tearDown(self):
        # Disable KEDR, just in case it remains enabled for some reason.
        # The core module cannot be unloaded while KEDR is enabled.
        subprocess.run(['sh', '-c', 'echo 0 > ' + KEDR_ENABLED])
        _unload_module(KEDR_CORE)

        # Just in case the test failed somehow and did not unload the target
        # module itself. It is OK to call it even, if the target is not
        # loaded.
        _unload_module(TARGET_MOD)

        print('Waiting a bit before checking dmesg...')
        time.sleep(5)
        self._check_dmesg_after()

    def test_enable_disable(self):
        '''Check enable/disable operations.'''
        self._enable_kedr()

        _load_module(TARGET_MOD)

        self._target_do_read()
        self._target_do_write()

        self._disable_kedr()
        self._target_do_read()
        self._target_do_write()

        self._enable_kedr()
        self._target_do_read()
        self._target_do_write()

        _unload_module(TARGET_MOD)
        self._disable_kedr()

    def test_enable_disable_under_load(self):
        '''Check enable/disable operations when the target is working.'''
        _load_module(TARGET_MOD)
        proc = self._target_start_rw_loop()

        # Wait a bit and check if the loop is actually running.
        time.sleep(5)
        self.assertTrue(proc.poll() is None)

        # Now enable KEDR, let it monitor the target a little, then disable.
        self._enable_kedr()
        time.sleep(5)
        self._disable_kedr()

        # ... and do it again, to check repetitive enable/disable operations
        # in such conditions.
        self._enable_kedr()
        time.sleep(5)
        self._disable_kedr()

        # Wait a bit more and check if the loop is still running.
        time.sleep(5)
        self.assertTrue(proc.poll() is None)

        # Stop the loop.
        stopped = False
        proc.kill()
        try:
            proc.wait(timeout=10)
            stopped = True
        except subprocess.TimeoutExpired:
            print('Process %d did not stop.' % proc.pid)

        if not stopped:
            # Wait longer.
            try:
                proc.wait(timeout=30)
                stopped = True
            except subprocess.TimeoutExpired:
                print('Process %d did not stop (may be hung?).' % proc.pid)

        self.assertTrue(stopped)
        _unload_module(TARGET_MOD)


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

    # run the tests
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(KedrTestBasics)
    result = unittest.TextTestRunner().run(suite)
    if result.failures or result.errors:
        sys.exit(1)
