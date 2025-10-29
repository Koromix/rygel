#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import os
import re
import sys
import subprocess

for commit in sys.stdin.readlines():
    commit = re.sub(r' .*$', '', commit.strip())

    if commit:
        env = os.environ | { 'LANG': 'C', 'LANGUAGE': 'C' }
        log = subprocess.check_output(['git', 'show', commit, '--stat'], env = env).decode('utf-8').splitlines()

        commit = log[0][7:].strip()
        meta = []
        desc = []
        for line in log[1:]:
            if re.match(r'^[a-zA-Z]+: ', line):
                meta.append(line.strip())
            if re.match(r'    ', line):
                desc.append(line.strip())
        stat = log[-1].strip()

        print(f'- Koromix/rygel@{commit}')
        for line in meta:
            print(f'  *{line}*')
        print()
        for line in desc:
            print(f'  > {line}')
        print(f'  >')
        print(f'  > {stat}')
        print()
