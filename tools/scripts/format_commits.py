#!/usr/bin/env python3

# Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

import os
import re
import sys
import subprocess

for commit in sys.stdin.readlines():
    commit = commit.strip()

    if commit:
        env = os.environ | { 'LANG': 'C', 'LANGUAGE': 'C' }
        log = subprocess.check_output(['git', 'show', commit, '--stat'], ).decode('utf-8').splitlines()

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
