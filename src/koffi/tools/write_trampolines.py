#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import argparse
import os

def write_asm_trampolines(filename, comment_char, n,
                          fmt_export, fmt_export_x, fmt_proc, fmt_proc_x, end = None):
    with open(filename, 'w') as f:
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
    with open(filename, 'w') as f:
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
        print('static_assert(K_LEN(Trampolines) == MaxTrampolines);', file = f)

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
