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

import argparse
import os
import os.path
import subprocess
import stat
import sys
import tempfile

DEFAULT_REMOTE = 'git@framagit.org:interhop/goupile.git'

def rewrite_repository(root_directory, clone_directory, push):
    # Clone or update repository
    subprocess.run(['git', 'clone', '--bare', root_directory, clone_directory, '--no-local'], check = True)
    os.chdir(clone_directory)

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
          for suffix in ['', '_Linux', '_Win32', '_macOS', '_Linux', '_POSIX']:
            import_str = config.get(imp, 'ImportFrom' + suffix, fallback = '').strip()
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
    subprocess.run([sys.executable, script_directory + '/git-filter-repo.py',
                    '--blob-callback', REWRITE_FELIXBUILD,
                    '--paths-from-file', script_directory + '/keep.txt'], check = True)
    subprocess.run([sys.executable, script_directory + '/git-filter-repo.py',
                    '--invert-paths', '--paths-from-file', script_directory + '/remove.txt'], check = True)

    # Fetch remote repository
    subprocess.run(['git', 'remote', 'add', 'distant', args.remote_url], check = True)
    subprocess.run(['git', 'fetch', 'distant'], check = True)

    # Find common ancestor commit
    subject = subprocess.check_output(['git', 'show', 'distant/master', '-s', '--pretty=format:%s']).decode('utf-8').strip()
    base = subprocess.check_output(['git', 'log', '--pretty=format:%H', '--grep=' + subject]).decode('utf-8').strip()
    head = subprocess.check_output(['git', 'show', '-s', '--pretty=format:%H']).decode('utf-8').strip()

    # Apply and push changes
    if base != head:
        subprocess.run(['git', 'reset', '--hard', 'distant/master'])
        subprocess.run(['git', 'cherry-pick', base + '..' + head])
        if push:
            subprocess.run(['git', 'push', '-u', 'distant', 'master'])

if __name__ == "__main__":
    start_directory = os.getcwd()

    # Always work from build.py directory
    script_filename = os.path.abspath(__file__)
    script_directory = os.path.dirname(script_filename)
    os.chdir(script_directory)

    # Parse arguments
    parser = argparse.ArgumentParser(description = 'Clone Goupile-specific repository')
    parser.add_argument('-O', '--clone_dir', dest = 'clone_dir', action = 'store', help = 'Clone in this directory')
    parser.add_argument('--remote', dest = 'remote_url', action = 'store', default = DEFAULT_REMOTE, help = 'Change remote URL')
    parser.add_argument('--no_push', dest = 'push', action = 'store_false', help = 'Disable final remote push')
    args = parser.parse_args()

    # Clone directory
    if args.clone_dir is not None:
        clone_directory = os.path.normpath(os.path.join(start_directory, args.clone_dir))
    else:
        clone_directory = None

    # Find repository directory
    root_directory = script_directory
    while not os.path.exists(root_directory + '/FelixBuild.ini'):
        new_directory = os.path.realpath(root_directory + '/..')
        if new_directory == root_directory:
            raise ValueError('Could not find FelixBuild.ini')
        root_directory = new_directory

    if clone_directory is not None:
        rewrite_repository(root_directory, clone_directory, args.push)
    else:
        if not args.push:
            raise ValueError('Cannot use --no_push without output directory')

        with tempfile.TemporaryDirectory() as clone_directory:
            rewrite_repository(root_directory, clone_directory, True)
