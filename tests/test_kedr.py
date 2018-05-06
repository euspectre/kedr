#!/usr/bin/env python3

''' The tests for KEDR.

Usage:
    test_kedr.py <name_of_test_suite>
'''

import sys
import os
import os.path
import shutil
import subprocess
import unittest
import time
import argparse
import re


CORE_SUBDIR = 'core'
KEDR_CORE = CORE_SUBDIR + '/kernel/kedr/kedr.ko'
KEDR_ENABLED = '/sys/kernel/kedr/enabled'

def build_kedr_core(topsrcdir, test_dir, kernel):
    '''Build KEDR core for the given kernel.'''
    print('Building the KEDR core module.')
    shutil.copytree(os.path.join(topsrcdir, 'kernel'),
                    os.path.join(test_dir, CORE_SUBDIR))
    subprocess.run(['make',
                    'KBUILD_DIR=/lib/modules/%s/build/' % kernel,
                    '-C', CORE_SUBDIR],
                   check=True)
    if not os.path.exists(KEDR_CORE):
        raise RuntimeError(KEDR_CORE + ' is missing.')

    print('Successfully built ' + os.path.join(test_dir, KEDR_CORE))


class KedrTest(unittest.TestCase):
    '''The common base class for the tests.'''

    # Class variables allow making parameterized test cases.
    kernel = None
    testdir = '.'
    topsrcdir = '.'
    topbuilddir = '.'

    # Regexps to match events in dmesg.
    re_event_alloc = re.compile(
        r'kedr: alloc at .*size == ([0-9]+), addr == ([0-9a-f]+)')
    re_event_free = re.compile(
        r'kedr: free at .*: addr == ([0-9a-f]+)')

    def __init__(self, methodName='runTest'):
        unittest.TestCase.__init__(self, methodName)
        self.last_dmesg_line = None

        # The events to look for in dmesg after the test.
        self.events = []
        self.event_no = 0

        # The mapping {symbolic_name => address} for the addresses referred
        # to by the events.
        self.addrs = {}

    @staticmethod
    def build_target_module(target_dir, target_mod, kernel, rules):
        '''Build the target module.

         The function builds the target module for the given kernel using
         the instrumentation rules from the specified file.

         target_dir - directory with the sources of the target module;
         target_mod - name of the target module ('<something>.ko');
         rules - path to the file with the instrumentation rules.

         The function returns True if the module has been built
         successfully, False otherwise.
        '''
        print('Building the target kernel module: %s.' % target_mod)

        env = os.environ
        env['KEDR_RULES_FILE'] = rules
        proc = subprocess.Popen(
            ['make',
             'KBUILD_DIR=/lib/modules/%s/build/' % kernel,
             '-C', target_dir],
            env=env)
        proc.wait(timeout=300)

        if proc.returncode != 0:
            print('Failed to build %s, exit code: %d' % (target_mod, proc.returncode))
            return False

        if not os.path.exists(target_mod):
            print('%s is missing.' % target_mod)
            return False

        print('Successfully built %s.' % target_mod)
        return True

    @staticmethod
    def load_module(mod, debug=False):
        '''Load the specified kernel module.

        mod - path to .ko file.

        If 'debug' is True, debug (more verbose) mode will be enabled
        for the module. Mostly applicable to the KEDR core.
        '''
        print('Loading %s.' % mod)
        cmd = ['insmod', mod]
        if debug:
            cmd.append('debug=1')
        subprocess.run(cmd, check=True)

    @staticmethod
    def unload_module(mod):
        '''Unload the specified kernel module.

        mod - path to .ko file.
        '''
        modname, _, _ = os.path.basename(mod).rpartition('.')
        modname = modname.replace('-', '_')

        if os.path.exists('/sys/module/%s/' % modname):
            print('Unloading "%s"' % modname)
            subprocess.run(['rmmod', modname], check=True)
        else:
            print('Module "%s" is not loaded, nothing to unload.' % modname)

    def enable_kedr(self):
        '''Enable KEDR core.'''
        subprocess.run(['sh', '-c', 'echo 1 > ' + KEDR_ENABLED], check=True)
        with open('/sys/kernel/kedr/enabled', 'r') as ke:
            line = ke.readline().strip()
            self.assertEqual(int(line), 1)

    def disable_kedr(self, check=True):
        '''Disable KEDR core.'''
        subprocess.run(['sh', '-c', 'echo 0 > ' + KEDR_ENABLED], check=check)
        if check:
            with open('/sys/kernel/kedr/enabled', 'r') as ke:
                line = ke.readline().strip()
                self.assertEqual(int(line), 0)

    def match_alloc_event(self, line, min_size, sym_addr):
        '''Check if the given line of dmesg contains the "alloc" event.'''
        mobj = re.search(self.re_event_alloc, line)
        if mobj:
            size = int(mobj.group(1))
            addr = mobj.group(2)
            if size < min_size:
                return False
            self.addrs[sym_addr] = addr
            return True
        return False

    def match_free_event(self, line, sym_addr):
        '''Check if the given line of dmesg contains the "free" event.'''
        if sym_addr not in self.addrs:
            raise RuntimeError('Unknown symbolic name for an address: %s' % sym_addr)
        addr = self.addrs[sym_addr]
        mobj = re.search(self.re_event_free, line)
        if mobj:
            return addr == mobj.group(1)
        return False

    def check_dmesg_line(self, line, dmesg_file):
        '''Look for kernel problems reported in the given line of dmesg.'''
        if line.find(' BUG: ') != -1 or line.find(' general protection fault: ') != -1:
            print('Got kernel BUG:\n' + line)
            print('See %s for details.' % os.path.join(os.getcwd(), dmesg_file))
            raise RuntimeError('Kernel BUG was found.')

        if line.find(' WARNING: ') != -1:
            print('Got kernel WARNING:\n' + line)
            print('See %s for details.' % os.path.join(os.getcwd(), dmesg_file))
            raise RuntimeError('Kernel WARNING was found.')

        if self.events and self.event_no < len(self.events):
            event = self.events[self.event_no]
            matched = False
            if event[0] == 'alloc':
                matched = self.match_alloc_event(line, int(event[1]), event[2])
            elif event[0] == 'free':
                matched = self.match_free_event(line, event[1])
            else:
                raise RuntimeError(
                    'Expected event #%d: unknown event type \"%s\".' % (self.event_no, event[0]))
            if matched:
                self.event_no = self.event_no + 1

    def save_dmesg_before(self):
        '''Save dmesg in a file before the test.'''
        dmesg_file = './dmesg-before.log'
        subprocess.run(['sh', '-c', 'dmesg > ' + dmesg_file], check=True)
        with open(dmesg_file, 'r') as dmesg:
            for ln in dmesg:
                line = ln
        self.last_dmesg_line = line

    def _read_events(self, events_file):
        if not os.path.exists(events_file):
            raise RuntimeError('File not found: %s.\n' % events_file)

        self.events = []
        self.event_no = 0
        self.addrs = {}

        with open(events_file, 'r') as evf:
            for ev_line in evf:
                ev_line = ev_line.strip()
                if not ev_line or ev_line.startswith('#'):
                    continue
                self.events.append(ev_line.split())
        if not self.events:
            raise RuntimeError(
                'File %s lists no expected events.\n' % events_file)

    def check_dmesg_after(self, events_file=None):
        '''Check the new messages from dmesg after the test.

        events_file (if set) - the file with the list of expected events
        to look for in dmesg. The events are expected to appear in dmesg
        in the order they are listed. If there are additional events in
        dmesg, it is OK, but if an expected event is not found there, it is
        an error.
        '''
        print('Checking dmesg after the test.')

        if events_file:
            self._read_events(events_file)

        dmesg_file = './dmesg-after.log'
        subprocess.run(['sh', '-c', 'dmesg > ' + dmesg_file], check=True)
        check = False
        with open(dmesg_file, 'r') as dmesg:
            for ln in dmesg:
                if check:
                    self.check_dmesg_line(ln.strip(), dmesg_file)
                elif ln.startswith(self.last_dmesg_line):
                    check = True
        # dmesg has fully rotated during the test - check the whole dmesg.
        # Some of it may already be lost, but checking the rest is better
        # than nothing.
        if not check:
            print('Note: dmesg has fully rotated. We may miss some messages.')
            with open(dmesg_file, 'r') as dmesg:
                for ln in dmesg:
                    self.check_dmesg_line(ln.strip(), dmesg_file)

        if events_file:
            if self.event_no == len(self.events):
                print('Found all expected events in dmesg.')
            else:
                str_event = ' '.join(self.events[self.event_no])
                raise RuntimeError(
                    'Failed to find the event \"%s\" in dmesg.' % str_event)

        print('Found no problems in dmesg.')


class KedrTestBasics(KedrTest):
    '''Test the basic operations with KEDR.'''
    target_subdir = 'target'
    target_mod = target_subdir + '/kedr_common_target.ko'
    target_write_cmd = 'echo 1234567890 > /dev/cfake0'
    target_read_cmd = 'dd if=/dev/cfake0 of=/dev/null bs=40 count=1'
    target_loop_cmd = ' '.join(['while true; do',
                                target_write_cmd, ';',
                                target_read_cmd, '2>/dev/null;',
                                'done'])

    def __init__(self, methodName='runTest'):
        KedrTest.__init__(self, methodName)

        rules = os.path.join(self.topsrcdir, 'i13n/rules.yml')
        if not os.path.exists(rules):
            raise RuntimeError('File not found: %s.\n' % rules)

        if os.path.exists(self.target_mod):
            print('%s already exists, will not rebuild it.' % self.target_mod)
        else:
            shutil.copytree(os.path.join(self.topbuilddir, 'tests/common_target'),
                            os.path.join(self.testdir, self.target_subdir))
            built_ok = self.build_target_module(
                self.target_subdir, self.target_mod, self.kernel, rules)
            if not built_ok:
                raise RuntimeError('Failed to build target module')

        self.events_file = os.path.join(
            self.topsrcdir, 'tests/events_basics.txt')

    def target_do_read(self):
        '''Read something from the file managed by the target module.'''
        print('Trying to read from /dev/cfake0.')
        proc = subprocess.run(['sh', '-c', self.target_read_cmd])
        self.assertEqual(proc.returncode, 0)

    def target_do_write(self):
        '''Write something to the file managed by the target module.'''
        print('Trying to write to /dev/cfake0.')
        proc = subprocess.run(['sh', '-c', self.target_write_cmd])
        self.assertEqual(proc.returncode, 0)

    def target_start_rw_loop(self):
        '''Read from / write to the file in a loop.'''
        print('Starting to access /dev/cfake0 in an infinite loop.')
        return subprocess.Popen(['sh', '-c', self.target_loop_cmd])

    def setUp(self):
        self.save_dmesg_before()
        self.load_module(KEDR_CORE, debug=True)

    def tearDown(self):
        # Disable KEDR, just in case it remains enabled for some reason.
        # The core module cannot be unloaded while KEDR is enabled.
        self.disable_kedr(check=False)
        self.unload_module(KEDR_CORE)

        # Just in case the test failed somehow and did not unload the target
        # module itself. It is OK to call it even, if the target is not
        # loaded.
        self.unload_module(self.target_mod)

        print('Waiting a bit before checking dmesg...')
        time.sleep(5)
        self.check_dmesg_after(events_file=self.events_file)

    def test_enable_disable(self):
        '''Check enable/disable operations.'''
        self.enable_kedr()

        self.load_module(self.target_mod)

        self.target_do_read()
        self.target_do_write()

        self.disable_kedr()
        self.target_do_read()
        self.target_do_write()

        self.enable_kedr()
        self.target_do_read()
        self.target_do_write()

        self.unload_module(self.target_mod)
        self.disable_kedr()

    def test_enable_disable_under_load(self):
        '''Check enable/disable operations when the target is working.'''

        # Enable KEDR to capture allocation events.
        self.enable_kedr()

        self.load_module(self.target_mod)
        proc = self.target_start_rw_loop()

        # Wait a bit and check if the loop is actually running.
        time.sleep(5)
        self.assertTrue(proc.poll() is None)

        # Now disable and then re-enable KEDR, let it monitor the target
        # a little, then disable.
        self.disable_kedr()
        time.sleep(5)
        self.enable_kedr()
        time.sleep(5)
        self.disable_kedr()

        # ... and do it again, to check repetitive enable/disable operations
        # in such conditions.
        self.enable_kedr()
        time.sleep(5)
        self.disable_kedr()

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

        # Enable KEDR to capture 'free' events.
        self.enable_kedr()
        self.unload_module(self.target_mod)
        self.disable_kedr()


if __name__ == '__main__':
    tests = {'basics' : KedrTestBasics}

    parser = argparse.ArgumentParser(description='The tests for KEDR.')
    parser.add_argument(
        'test_name', choices=tests.keys(),
        help='name of the test case to run')
    parser.add_argument(
        '--topsrcdir', required=True,
        help='the top directory of the source tree')
    parser.add_argument(
        '--topbuilddir', required=True,
        help='the top directory of the build tree')
    parser.add_argument(
        '--testdir', required=True,
        help='directory the tests may use for their data')
    args = parser.parse_args()

    kernel = os.uname().release
    print('Target kernel: ' + kernel)
    sys.stdout.flush()

    testdir = os.path.join(args.testdir, kernel, args.test_name)
    if os.path.exists(testdir):
        shutil.rmtree(testdir)

    os.makedirs(testdir)
    os.chdir(testdir)

    build_kedr_core(args.topsrcdir, testdir, kernel)

    testclass = tests[args.test_name]
    testclass.kernel = kernel
    testclass.testdir = testdir
    testclass.topsrcdir = args.topsrcdir
    testclass.topbuilddir = args.topbuilddir

    # run the tests
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(testclass)
    result = unittest.TextTestRunner().run(suite)
    if result.failures or result.errors:
        sys.exit(1)
