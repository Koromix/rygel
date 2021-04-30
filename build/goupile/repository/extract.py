#!/usr/bin/env python3

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program. If not, see https://www.gnu.org/licenses/.

import os
import os.path
import subprocess
import shutil

SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))

# Navigate to project root directory
os.chdir(SCRIPT_DIR)
while not os.path.exists('FelixBuild.ini'):
    cwd = os.getcwd()
    os.chdir('..')
    if os.getcwd() == cwd:
        raise FileNotFoundError('FelixBuild.ini')

# Clone or update repository
subprocess.run(['git', 'clone', '.', 'bin/Goupile', '--no-local'], check = True)
os.chdir('bin/Goupile')

# FelixBuild.ini blobs
ids = subprocess.check_output(['git', 'rev-list', '--objects', '--all', '--no-object-names', '--', 'FelixBuild.ini'])
ids = ids.decode('utf-8').splitlines()

NEWLINE = '\\n'
REWRITE_FELIXBUILD = f'''
  from collections import OrderedDict
  import configparser
  import io

  class MultiOrderedDict(OrderedDict):
    def __setitem__(self, key, value):
        if isinstance(value, list) and key in self:
            self[key].extend(value)
        else:
            super().__setitem__(key, value)

  ids = ['{"','".join(ids)}']

  id = blob.original_id.decode('utf-8')
  if not id in ids:
    return

  ini = blob.data.decode('utf-8')

  config = configparser.RawConfigParser(dict_type = MultiOrderedDict, strict = False)
  config.optionxform = str
  config.read_string(ini)

  imports = []
  if config.has_section('goupile'):
    imports.append('goupile')
  if config.has_section('goupile_admin'):
    imports.append('goupile_admin')
  if config.has_section('goupil'):
    imports.append('goupil')
  if config.has_section('goupil_admin'):
    imports.append('goupil_admin')
  if config.has_section('felix'):
    imports.append('felix')
  if len(imports) == 0:
    return

  next_imports = imports.copy()
  while len(next_imports) > 0:
    new_imports = []
    for imp in next_imports:
      import_str = config.get(imp, 'ImportFrom', fallback = '').strip()
      if import_str:
        new_imports.extend(import_str.split(' '))
    imports.extend(new_imports)
    next_imports = new_imports

  for section in config.sections():
    if not section in imports:
      config.remove_section(section)

  with io.StringIO() as f:
    for section in config.sections():
      if not section in imports:
        continue
      print('[' + section + ']' , file = f)
      for k, vv in config.items(section):
        vv = vv.split('{NEWLINE}')
        for v in vv:
          print(k + ' = ' + v.strip(), file = f)
      print('', file = f)
    blob.data = f.getvalue().encode('utf-8')
'''

# Filter it out and rewrite FelixBuild.ini
subprocess.run(['python', SCRIPT_DIR + '/git-filter-repo.py',
                '--blob-callback', REWRITE_FELIXBUILD,
                '--paths-from-file', SCRIPT_DIR + '/keep.txt'])
