#!/usr/bin/env python3

''' The script looks through the source code of KEDR core and generates
declarations and definitions of a stub for each event handler it finds. The
output source file and header file will be prepared by appending these
declarations and definitions to the input files.

Usage:
    gen_stubs.py <KEDR_core_main_source_file> \
                 <input_header> <input_source> \
                 <output_header> <output_source>
'''

import os.path
import os
import shutil
import argparse
import sys
import re


# Idea: https://stackoverflow.com/a/241506/689077.
def strip_code(text):
    '''Strip the given C code of comments, strings, etc.

    Single-line preprocessor directives are also removed.
    '''
    pattern = re.compile(
        r'#.*?$|//.*?$|/\*.*?\*/|\'(?:\\.|[^\\\'])*\'|"(?:\\.|[^\\"])*"',
        re.DOTALL | re.MULTILINE
    )
    return re.sub(pattern, " ", text)


def write_files(out_hdr, out_src, src_defs):
    '''Write the stub declarations and definitions to the files.'''
    for sdef in src_defs:
        name_type, _, arglist = sdef.partition('(')
        if not name_type or not arglist or arglist[-1] != ')':
            sys.stderr.write(
                'Unable to parse handler definition: \"%s\"\n' % sdef)
            sys.exit(1)
        name_type = name_type.strip().replace(
            'kedr_handle_', 'kedr_stub_handle_', 1)
        arglist = arglist[:-1].strip()

        out_hdr.write('%s(%s);\n' % (name_type, arglist))
        out_src.write(
            '\n__kedr_stub %s(\n\t%s)\n{\n' % (name_type, arglist))

        for arg in arglist.split(','):
            _, _, argname = arg.replace('*', '').rpartition(' ')
            out_src.write('\t(void)%s;\n' % argname)
        out_src.write('\tasm volatile ("");\n')

        out_src.write('}\n')


if __name__ == '__main__':
    desc = '''The script generates declarations and definitions of the stubs
    based on which event handlers are defined in KEDR core. The stubs will be
    used in the instrumented code and must remain consistent with these
    event handlers - this is what this script is for.'''

    parser = argparse.ArgumentParser(description=desc)
    parser.add_argument(
        'core_src', metavar='core_source_file',
        help='source file of the KEDR core that defines event handlers')
    parser.add_argument(
        'in_hdr', metavar='input_header_file',
        help='common part of the header file without declarations of the stubs')
    parser.add_argument(
        'in_src', metavar='input_source_file',
        help='common part of the source file without definitions of the stubs')
    parser.add_argument(
        'out_hdr', metavar='output_header_file',
        help='the complete header file with declarations of the stubs')
    parser.add_argument(
        'out_src', metavar='output_source_file',
        help='the complete source file with declarations of the stubs')
    args = parser.parse_args()

    for in_file in [args.core_src, args.in_hdr, args.in_src]:
        if not os.path.exists(in_file):
            sys.stderr.write('File not found: %s\n' % in_file)
            sys.exit(1)

    with open(args.core_src, 'r') as core_src_file:
        core_src = strip_code(core_src_file.read())

    handler_re = re.compile(
        r'__kedr_handler\s+void\s+kedr_handle_.*?\{',
        re.DOTALL | re.MULTILINE)
    found_defs = re.findall(handler_re, core_src)
    if not found_defs:
        sys.stderr.write(
            'Found no handler definitions in: %s\n' % args.core_src)
        sys.exit(1)

    src_defs = []
    for sdef in found_defs:
        sdef = re.sub(r'__kedr_handler\s+|\{|\n', '', sdef)
        sdef = re.sub(r'\s+', ' ', sdef)
        sdef = sdef.strip()
        src_defs.append(sdef)

    for sdef in src_defs:
        name_type, _, arglist = sdef.partition('(')
        if not name_type or not arglist or arglist[-1] != ')':
            sys.stderr.write(
                'Unable to parse handler definition: \"%s\"\n' % sdef)
            sys.exit(1)
        name_type = name_type.strip().replace(
            'kedr_handle_', 'kedr_stub_handle_', 1)
        arglist = arglist[:-1].strip()

    if os.path.exists(args.out_hdr):
        os.remove(args.out_hdr)
    if os.path.exists(args.out_src):
        os.remove(args.out_src)
    shutil.copy(args.in_src, args.out_src)

    with open(args.in_hdr, 'r') as in_hdr:
        hdr_text = in_hdr.read()

    with open(args.out_hdr, 'w') as out_hdr:
        out_hdr.write('#ifndef KEDR_HELPERS_1712_INCLUDED\n')
        out_hdr.write('#define KEDR_HELPERS_1712_INCLUDED\n\n')
        out_hdr.write(hdr_text)
        out_hdr.write('\n')

        with open(args.out_src, 'a') as out_src:
            write_files(out_hdr, out_src, src_defs)

        out_hdr.write('\n#endif /*KEDR_HELPERS_1712_INCLUDED*/\n')
