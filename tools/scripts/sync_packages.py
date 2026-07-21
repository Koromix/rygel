#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

import argparse
import hashlib
import json
import os
import re
import sys
import shutil
import subprocess

REPOSITORY_NAME = 'koromix'
GPG_SIGNING_USER = 'niels.martignene@protonmail.com'
MINISIGN_KEY_FILE = os.path.expanduser('~/.minisign/minisign.key')

DEFAULT_IMPORT_DIR = '../../bin/Packages'

def import_packages(src, dest, ext):
    names = os.listdir(src)

    for name in names:
        if name == ext or not name.endswith(ext):
            continue

        src_filename = os.path.join(src, name)
        dest_filename = os.path.join(dest, name)

        print(f'  - Importing {os.path.basename(src_filename)}...')
        shutil.copyfile(src_filename, dest_filename)

def import_releases(src, root, func):
    names = os.listdir(src)

    for name in names:
        pkg, _, _ = split_name(name)
        if pkg is None:
            continue

        if name.endswith('.rpm') or name.endswith('.deb'):
            suffix = '/linux'
        elif name.endswith('_win64.zip') or name.endswith('_win64.exe') or name.endswith('_win64.msi'):
            suffix = '/windows'
        elif name.endswith('_macos.tar.gz') or name.endswith('_macos.tar.xz'):
            suffix = '/macos'
        elif name.endswith('.tar.gz') or name.endswith('.tar.xz'):
            suffix = ''
        else:
            continue

        directory = root + '/' + pkg + suffix
        os.makedirs(directory, exist_ok = True)

        filename = src + '/' + name
        dest = directory + '/' + name

        print(f'  * Importing {name}...')
        func(filename, dest)

def process_releases(root):
    dirs = (pkg.name for pkg in os.scandir(root) if pkg.is_dir())
    packages = {}

    for pkg in dirs:
        directory = root + '/' + pkg
        subdirs = os.scandir(directory)

        prune_old_versions(directory)

        for subdir in subdirs:
            if not subdir.is_dir():
                continue

            prune_old_versions(subdir.path)
            names = os.listdir(subdir.path)

            for name in names:
                pkg, version, remain = split_name(name)
                if pkg is None:
                    continue
                host = subdir.name

                if subdir.name == 'linux':
                    arch = remain[-1].split('.')[0]

                    match arch:
                        case 'amd64':
                            host += '_x86_64'
                        case 'arm64':
                            host += '_arm64'
                        case _:
                            continue

                if pkg not in packages:
                    packages[pkg] = []

                packages[pkg].append({
                    'file': f'{subdir.name}/{name}',
                    'host': host,
                    'version': version,
                    'hash': hash_file(subdir.path + '/' + name)
                })

    with open(root + '/packages.json', 'w') as f:
        json.dump(packages, f, indent = 4)
    run_command('gzip', ['-f', root + '/packages.json'])
    run_command('minisign', ['-S', '-s', MINISIGN_KEY_FILE, '-f', '-m', root + '/packages.json.gz'], capture = False)

def split_name(name):
    parts = re.split(r'[\-_]', name)

    if len(parts) < 3:
        return None, None, None

    pkg = parts[0]
    version = parts[1]
    remain = parts[2:]

    return pkg, version, remain

def hash_file(filename):
    with open(filename, 'rb') as f:
        digest = hashlib.file_digest(f, 'sha256')
    return digest.hexdigest()

def prune_old_versions(directory):
    files = (entry for entry in os.scandir(directory) if entry.is_file())

    packages = {}

    for entry in files:
        pkg, version, _ = split_name(entry.name)
        if pkg is None:
            continue
        prev = packages.get(pkg, None)

        if prev is None or is_greater_nat(version, prev):
            packages[pkg] = version

    for entry in files:
        pkg, version, _ = split_name(entry.name)
        if pkg is None:
            continue

        if pkg in packages and version != packages[pkg]:
            print(f'  * Deleting {os.path.basename(entry.path)}...')
            os.remove(entry.path)

def is_greater_nat(a, b):
    a = [int(t) if t.isdigit() else t.lower() for t in re.split(r'(\d+)', a)]
    b = [int(t) if t.isdigit() else t.lower() for t in re.split(r'(\d+)', b)]

    return a > b

def run_command(cmd, args, cwd = None, capture = True):
    ret = subprocess.run([cmd] + args, check = True, cwd = cwd,
                                       stdout = subprocess.PIPE if capture else None,
                                       stderr = subprocess.STDOUT if capture else None)

    if ret.returncode != 0:
        if capture:
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
        run_command(script_dir + '/../package/repo/apt/update.sh', [REPOSITORY_NAME, GPG_SIGNING_USER], cwd = args.directory + '/debian')

    if 'rpm' in args.types:
        print('>> Processing RPM repository...')
        os.makedirs(args.directory + '/rpm', exist_ok = True)
        for path in args.imports:
            import_packages(path, args.directory + '/rpm', '.rpm')
        prune_old_versions(args.directory + '/rpm')
        run_command(script_dir + '/../package/repo/rpm/update.sh', [REPOSITORY_NAME, GPG_SIGNING_USER], cwd = args.directory + '/rpm')

    if 'releases' in args.types:
        print('>> Processing releases...')
        os.makedirs(args.directory + '/releases', exist_ok = True)
        for path in args.imports:
            import_releases(path, args.directory + '/releases', shutil.copyfile)
        import_releases(args.directory + '/releases', args.directory + '/releases', shutil.move)
        process_releases(args.directory + '/releases')
