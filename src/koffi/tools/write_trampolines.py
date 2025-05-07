#!/usr/bin/env python3

# Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy of
# this software and associated documentation files (the “Software”), to deal in
# the Software without restriction, including without limitation the rights to use,
# copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
# Software, and to permit persons to whom the Software is furnished to do so,
# subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
# OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
# HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
# WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.

import argparse
import os

def write_asm_trampolines(filename, comment_char, n,
                          fmt_export, fmt_export_x, fmt_proc, fmt_proc_x, end = None):
    with open(filename) as f:
        lines = f.readlines()

    with open(filename, 'w') as f:
        for line in lines:
            if not line.startswith(comment_char):
                break
            f.write(line)

        print('', file = f)
        for i in range(0, n):
            print(fmt_export.format(i), file = f)
        print('', file = f)
        for i in range(0, n):
            print(fmt_export_x.format(i), file = f)

        print('', file = f)
        for i in range(0, n):
            print(fmt_proc.format(i), file = f)
        print('', file = f)
        for i in range(0, n):
            print(fmt_proc_x.format(i), file = f)

        if end is not None:
            print('', file = f)
            print(end, file = f)

def write_cxx_trampolines(filename, n):
    with open(filename) as f:
        lines = f.readlines()

    with open(filename, 'w') as f:
        for line in lines:
            if not line.startswith('//'):
                break
            f.write(line)

        print('', file = f)
        for i in range(0, n):
            print('extern "C" int Trampoline{0}; extern "C" int TrampolineX{0};'.format(i), file = f)

        print('', file = f)
        print('static void *const Trampolines[][2] = {', file = f)
        for i in range(0, n):
            if i + 1 < n:
                print('    {{ &Trampoline{0}, &TrampolineX{0} }},'.format(i), file = f)
            else:
                print('    {{ &Trampoline{0}, &TrampolineX{0} }}'.format(i), file = f)
        print('};', file = f)
        print('static_assert(RG_LEN(Trampolines) == MaxTrampolines);', file = f)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description = 'Generate static trampolines')
    parser.add_argument('n', metavar = 'n', type = int, help = 'Number of trampolines')
    args = parser.parse_args()

    src_dir = os.path.dirname(__file__) + '/../src'

    write_asm_trampolines(src_dir + '/trampolines/gnu.inc', '#', args.n,
        '.global SYMBOL(Trampoline{0})',
        '.global SYMBOL(TrampolineX{0})',
        'SYMBOL(Trampoline{0}):\n    trampoline {0}',
        'SYMBOL(TrampolineX{0}):\n    trampoline_vec {0}'
    )
    write_asm_trampolines(src_dir + '/trampolines/armasm.inc', ';', args.n,
        '    EXPORT Trampoline{0}',
        '    EXPORT TrampolineX{0}',
        'Trampoline{0} PROC\n    trampoline {0}\n    ENDP',
        'TrampolineX{0} PROC\n    trampoline_vec {0}\n    ENDP',
        '    END'
    )
    write_asm_trampolines(src_dir + '/trampolines/masm64.inc', ';', args.n,
        'public Trampoline{0}',
        'public TrampolineX{0}',
        'Trampoline{0} proc frame\n    trampoline {0}\nTrampoline{0} endp',
        'TrampolineX{0} proc frame\n    trampoline_vec {0}\nTrampolineX{0} endp'
    )
    write_asm_trampolines(src_dir + '/trampolines/masm32.inc', ';', args.n,
        'public Trampoline{0}',
        'public TrampolineX{0}',
        'Trampoline{0} proc\n    trampoline {0}\nTrampoline{0} endp',
        'TrampolineX{0} proc\n    trampoline_vec {0}\nTrampolineX{0} endp'
    )

    write_cxx_trampolines(src_dir + '/trampolines/prototypes.inc', args.n)
