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
import shutil
import stat
import sys

if __name__ == "__main__":
    start_directory = os.getcwd()

    # Always work from build.py directory
    script_filename = os.path.abspath(__file__)
    script_directory = os.path.dirname(script_filename)
    os.chdir(script_directory)

    # Parse arguments
    parser = argparse.ArgumentParser(description = 'Clone Goupile-specific repository')
    parser.add_argument('-O', '--output_dir', dest = 'output_dir', action = 'store', help = 'Output directory')
    parser.add_argument('--push', dest = 'push_url', action = 'store', help = 'Push to repository')
    args = parser.parse_args()

    # Find repository directory
    root_directory = script_directory
    while not os.path.exists(root_directory + '/FelixBuild.ini'):
        new_directory = os.path.realpath(root_directory + '/..')
        if new_directory == root_directory:
            raise ValueError('Could not find FelixBuild.ini')
        root_directory = new_directory

    # Prepare clone directory
    if args.output_dir is None:
        clone_directory = os.path.join(root_directory, 'bin/subrepo/ansible-hds')
    elif not os.path.isabs(args.output_dir):
        clone_directory = os.path.join(start_directory, args.output_directory)
    else:
        clone_directory = args.output_dir
    if os.path.exists(clone_directory):
        def onerror(func, path, exc_info):
            if not os.access(path, os.W_OK):
                os.chmod(path, stat.S_IWUSR)
                func(path)
            else:
                raise
        shutil.rmtree(clone_directory, onerror = onerror)

    def onerror(func, path, exc_info):
        if not os.access(path, os.W_OK):
            os.chmod(path, stat.S_IWUSR)
            func(path)
        else:
            raise

    # Clone or update repository
    subprocess.run(['git', 'clone', root_directory, clone_directory, '--no-local'], check = True)
    os.chdir(clone_directory)

    # Filter it out and rewrite FelixBuild.ini
    subprocess.run([sys.executable, script_directory + '/git-filter-repo.py',
                    '--paths-from-file', script_directory + '/keep.txt'])

    # Push to repository
    if args.push_url is not None:
        subprocess.run(['git', 'remote', 'add', 'origin', args.push_url])
        subprocess.run(['git', 'push', '-u', 'origin', 'master'])
