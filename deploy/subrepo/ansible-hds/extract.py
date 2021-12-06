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

DEFAULT_REMOTE = 'git@framagit.org:interhop/hds/ansible-hds.git'

def rewrite_repository(root_directory, clone_directory, push):
    # Clone or update repository
    subprocess.run(['git', 'clone', root_directory, clone_directory, '--no-local'], check = True)
    os.chdir(clone_directory)

    # Filter it out and rewrite FelixBuild.ini
    subprocess.run([sys.executable, script_directory + '/git-filter-repo.py',
                    '--paths-from-file', script_directory + '/keep.txt'], check = True)
    subprocess.run([sys.executable, script_directory + '/git-filter-repo.py',
                    '--invert-paths', '--paths-from-file', script_directory + '/remove.txt'], check = True)

    # Fetch remote repository
    subprocess.run(['git', 'remote', 'add', 'distant', args.remote_url], check = True)
    subprocess.run(['git', 'fetch', 'distant'], check = True)

    # Cherry-pick commits since last common ancestor commit
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
    parser = argparse.ArgumentParser(description = 'Clone HDS-specific repository')
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
