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

PROJECTS = {
    'goupile': ['goupile2', 'git@framagit.org:interhop/goupile.git'],
    'ansible-hds': ['master', 'git@framagit.org:interhop/hds/ansible-hds.git']
}

def update_repository(root_directory, clone_directory, remote_url, remote_branch):
    # Clone or update repository
    if not os.path.exists(clone_directory + '/.git'):
        os.makedirs(os.path.dirname(clone_directory), exist_ok = True)
        subprocess.run(['git', 'clone', '--no-local', root_directory, clone_directory], check = True)
        os.chdir(clone_directory)
        subprocess.run(['git', 'remote', 'add', 'peer', remote_url], check = True)
    else:
        os.chdir(clone_directory)

    subprocess.run(['git', 'checkout', 'master'], check = True)
    subprocess.run(['git', 'fetch'], check = True)
    subprocess.run(['git', 'reset', '--hard', 'origin/' + remote_branch], check = True)

def publish_peer(master, deploy):
    subprocess.run(['git', 'fetch', 'peer'], check = True)

    # Find common ancestor commit
    subject = subprocess.check_output(['git', 'show', 'peer/master', '-s', '--pretty=format:%s']).decode('utf-8').strip()
    base = subprocess.check_output(['git', 'log', '--pretty=format:%H', '--grep=' + subject]).decode('utf-8').strip()
    head = subprocess.check_output(['git', 'show', '-s', '--pretty=format:%H']).decode('utf-8').strip()

    # Apply changes
    subprocess.run(['git', 'reset', '--hard', 'peer/master'], check = True)
    if base != head:
        subprocess.run(['git', 'cherry-pick', '--allow-empty', base + '..' + head], check = True)

    # Push changes
    if master:
        subprocess.run(['git', 'push', 'peer', 'master:master'], check = True)
    for suffix in deploy:
        subprocess.run(['git', 'push', 'peer', 'master:deploy_' + suffix], check = True)

if __name__ == "__main__":
    start_directory = os.getcwd()

    # Parse arguments
    parser = argparse.ArgumentParser(description = 'Clone Goupile-specific repository')
    parser.add_argument('-O', '--clone_dir', dest = 'clone_dir', action = 'store', help = 'Clone in this directory')
    parser.add_argument('--remote', dest = 'remote_url', action = 'store', help = 'Change remote URL')
    parser.add_argument('--branch', dest = 'remote_branch', action = 'store', help = 'Change remote branch')
    parser.add_argument('--no_master', dest = 'master', action = 'store_false', help = 'Disable push to master branch')
    parser.add_argument('-d', '--deploy', dest = 'deploy', action = 'store', nargs = '+', default = [], help = 'Push to deploy branches')
    parser.add_argument('project', help = 'Project to rewrite and publish')
    args = parser.parse_args()

    # Find remote URL
    if not args.project in PROJECTS:
        raise ValueError(f'Invalid project {args.project}')
    if args.remote_url is None:
        remote_url = PROJECTS[args.project][1]
    else:
        remote_url = args.remote_url
    if args.remote_branch is None:
        remote_branch = PROJECTS[args.project][0]
    else:
        remote_branch = args.remote_branch

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

    update_repository(root_directory, clone_directory, remote_url, remote_branch)

    # Run the rewrite script
    # Why the hell is it so "complicated" to import and run Python code? Why is import so weird?
    project_filename = os.path.join(project_directory, 'rewrite.py')
    with open(project_filename) as f:
        script = f.read()
        exec(script, {
            'PROJECT_DIRECTORY': project_directory,
            'FILTER_SCRIPT': os.path.join(script_directory, 'git-filter-repo.py')
        })

    publish_peer(args.master, args.deploy)
