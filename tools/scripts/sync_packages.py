#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

import argparse
import os
import re
import sys
import shutil
import subprocess

DEFAULT_NAME = 'koromix'
DEFAULT_USER = 'niels.martignene@protonmail.com'
DEFAULT_IMPORT_DIR = '../../bin/Packages'

def prune_old_versions(directory):
    entries = list(os.scandir(directory))
    files = []

    for entry in entries:
        if entry.is_dir():
            entries.extend(os.scandir(entry.path))
        else:
            files.append(entry)

    packages = {}

    for entry in files:
        parts = re.split(r'[\-_]', entry.name)
        if len(parts) < 2:
            continue
        pkg = parts[0]
        version = parts[1]

        prev = packages.get(pkg, None)

        if prev is None or is_greater_nat(version, prev):
            packages[pkg] = version

    for entry in files:
        parts = re.split(r'[\-_]', entry.name)
        if len(parts) < 2:
            continue
        pkg = parts[0]
        version = parts[1]

        if pkg in packages and version != packages[pkg]:
            print(f'  - Removing {os.path.basename(entry.path)}...')
            os.remove(entry.path)

def is_greater_nat(a, b):
    a = [int(t) if t.isdigit() else t.lower() for t in re.split(r'(\d+)', a)]
    b = [int(t) if t.isdigit() else t.lower() for t in re.split(r'(\d+)', b)]

    return a > b

def import_packages(src, dest, ext):
    names = os.listdir(src)

    for name in names:
        if name == ext or not name.endswith(ext):
            continue

        src_filename = os.path.join(src, name)
        dest_filename = os.path.join(dest, name)

        print(f'  - Importing {os.path.basename(src_filename)}...')
        shutil.copyfile(src_filename, dest_filename)

def process_releases(src, root, func):
    names = os.listdir(src)

    for name in names:
        parts = re.split(r'[\-_]', name)
        if len(parts) < 2:
            continue
        pkg = parts[0]

        if name.endswith('.rpm') or name.endswith('.deb'):
            suffix = '/linux'
        elif name.endswith('-win64.zip') or name.endswith('.exe') or name.endswith('.msi'):
            suffix = '/windows'
        elif name.endswith('-macos.tar.gz') or name.endswith('-macos.tar.xz'):
            suffix = '/macos'
        elif name.endswith('.tar.gz') or name.endswith('.tar.xz'):
            suffix = ''
        else:
            continue

        directory = root + '/' + pkg + suffix
        os.makedirs(directory, exist_ok = True)

        filename = src + '/' + name
        dest = directory + '/' + name

        print(f'  - Handling {name}...')
        func(filename, dest)

def run_command(cmd, args, cwd):
    ret = subprocess.run([cmd] + args, check = True, cwd = cwd, stdout = subprocess.PIPE, stderr = subprocess.STDOUT)

    if ret.returncode != 0:
        output = ret.stdout.decode('utf-8')
        print(output)

        raise RuntimeError(f'Command {cmd} failed with status {ret.returncode}')

if __name__ == '__main__':
    script_dir = os.path.dirname(__file__)

    parser = argparse.ArgumentParser(description = 'Update package repositories')
    parser.add_argument('directory', metavar = 'directory', type = str,
                        help = 'Root download/package directory')
    parser.add_argument('--debian', const = 'debian', dest = 'types', action = 'append_const',
                        help = 'Update Debian repository in debian subdirectory')
    parser.add_argument('--rpm', const = 'rpm', dest = 'types', action = 'append_const',
                        help = 'Update RPM repository directory in rpm subdirectory')
    parser.add_argument('--releases', const = 'releases', dest = 'types', action = 'append_const',
                        help = 'Update releases for manual downloads')
    parser.add_argument('--name', type = str, default = DEFAULT_NAME,
                        help = 'change repository name')
    parser.add_argument('--user', type = str, default = DEFAULT_USER,
                        help = 'change GPG user')
    parser.add_argument('--import', metavar = 'path', dest = 'imports', nargs = '?',
                        type = str, default = [], const = None, action = 'append',
                        help = 'import additional packages')
    args = parser.parse_args()

    if not args.types:
        args.types = []
        if os.path.isdir(args.directory + '/debian'):
            args.types.append('debian')
        if os.path.isdir(args.directory + '/rpm'):
            args.types.append('rpm')
        if os.path.isdir(args.directory + '/releases'):
            args.types.append('releases')

    if None in args.imports:
        default_import = script_dir + '/' + DEFAULT_IMPORT_DIR
        if os.path.isdir(default_import):
            args.imports.append(default_import)

    seen = { None: None }
    args.imports = [seen.setdefault(path, path) for path in args.imports if path not in seen]

    if 'debian' in args.types:
        print('>> Processing Debian/APT repository...')
        os.makedirs(args.directory + '/debian', exist_ok = True)
        os.makedirs(args.directory + '/debian/pool', exist_ok = True)
        for path in args.imports:
            import_packages(path, args.directory + '/debian/pool', '.deb')
        prune_old_versions(args.directory + '/debian/pool')
        print('  - Updating repository...')
        run_command(script_dir + '/../package/repo/apt/update.sh', [args.name, args.user], args.directory + '/debian')

    if 'rpm' in args.types:
        print('>> Processing RPM repository...')
        os.makedirs(args.directory + '/rpm', exist_ok = True)
        for path in args.imports:
            import_packages(path, args.directory + '/rpm', '.rpm')
        prune_old_versions(args.directory + '/rpm')
        print('  - Updating repository...')
        run_command(script_dir + '/../package/repo/rpm/update.sh', [args.name, args.user], args.directory + '/rpm')

    if 'releases' in args.types:
        print('>> Processing releases...')
        os.makedirs(args.directory + '/releases', exist_ok = True)
        for path in args.imports:
            process_releases(path, args.directory + '/releases', shutil.copyfile)
        prune_old_versions(args.directory + '/releases')
        process_releases(args.directory + '/releases', args.directory + '/releases', shutil.move)
