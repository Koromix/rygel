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
import importlib
import os
import os.path
import subprocess
import stat
import sys
import tempfile

DEFAULT_REMOTES = {
    'goupile': 'git@framagit.org:interhop/goupile.git',
    'ansible-hds': 'git@framagit.org:interhop/hds/ansible-hds.git'
}

def update_repository(root_directory, clone_directory, remote_url):
    # Clone or update repository
    if os.path.exists(clone_directory):
        os.chdir(clone_directory)
        subprocess.run(['git', 'reset', '--hard', 'origin/master'], check = True)
        subprocess.run(['git', 'pull'], check = True)
    else:
        os.makedirs(os.path.dirname(clone_directory), exist_ok = True)
        subprocess.run(['git', 'clone', '--no-local', root_directory, clone_directory], check = True)
        os.chdir(clone_directory)
        subprocess.run(['git', 'remote', 'add', 'peer', remote_url], check = True)

def publish_peer(push):
    subprocess.run(['git', 'fetch', 'peer'], check = True)

    # Find common ancestor commit
    subject = subprocess.check_output(['git', 'show', 'peer/master', '-s', '--pretty=format:%s']).decode('utf-8').strip()
    base = subprocess.check_output(['git', 'log', '--pretty=format:%H', '--grep=' + subject]).decode('utf-8').strip()
    head = subprocess.check_output(['git', 'show', '-s', '--pretty=format:%H']).decode('utf-8').strip()

    # Apply and push changes
    if base != head:
        subprocess.run(['git', 'reset', '--hard', 'peer/master'], check = True)
        subprocess.run(['git', 'cherry-pick', base + '..' + head], check = True)
        if push:
            subprocess.run(['git', 'push', '-u', 'peer', 'master'], check = True)

if __name__ == "__main__":
    start_directory = os.getcwd()

    # Parse arguments
    parser = argparse.ArgumentParser(description = 'Clone Goupile-specific repository')
    parser.add_argument('-O', '--clone_dir', dest = 'clone_dir', action = 'store', help = 'Clone in this directory')
    parser.add_argument('--remote', dest = 'remote_url', action = 'store', help = 'Change remote URL')
    parser.add_argument('--no_push', dest = 'push', action = 'store_false', help = 'Disable final remote push')
    parser.add_argument('project', help = 'Project to rewrite and publish')
    args = parser.parse_args()

    # Find remote URL
    if not args.project in DEFAULT_REMOTES:
        raise ValueError(f'Invalid project {args.project}')
    if args.remote_url is None:
        remote_url = DEFAULT_REMOTES[args.project]
    else:
        remote_url = args.remote_url

    # Always work from project directory
    script_directory = os.path.dirname(os.path.abspath(__file__))
    project_directory = os.path.join(script_directory, args.project)
    os.chdir(project_directory)

    # Find repository directory
    root_directory = script_directory
    while not os.path.exists(root_directory + '/FelixBuild.ini'):
        new_directory = os.path.realpath(root_directory + '/..')
        if new_directory == root_directory:
            raise ValueError('Could not find FelixBuild.ini')
        root_directory = new_directory

    # Clone directory
    if args.clone_dir is None:
        clone_directory = os.path.join(root_directory, 'bin', 'SubRepo', args.project)
    else:
        clone_directory = os.path.normpath(os.path.join(start_directory, args.clone_dir))

    update_repository(root_directory, clone_directory, remote_url)

    # Run the rewrite script
    # Why the hell is it so "complicated" to import and run Python code? Why is import so weird?
    project_filename = os.path.join(project_directory, 'rewrite.py')
    with open(project_filename) as f:
        script = f.read()
        exec(script, {
            'PROJECT_DIRECTORY': project_directory,
            'FILTER_SCRIPT': os.path.join(script_directory, 'git-filter-repo.py')
        })

    publish_peer(args.push)
