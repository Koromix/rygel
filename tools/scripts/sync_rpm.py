#!/usr/bin/env python3

import argparse
import os
import shutil
import subprocess

DEFAULT_NAME = 'koromix'
DEFAULT_USER = 'niels.martignene@protonmail.com'

def move_packages(src, dest, ext):
    names = os.listdir(src)

    for name in names:
        if name == ext or not name.endswith(ext):
            continue

        src_filename = os.path.join(src, name)
        dest_filename = os.path.join(dest, name)

        shutil.move(src_filename, dest_filename)

if __name__ == '__main__':
    script_dir = os.path.dirname(__file__)

    parser = argparse.ArgumentParser(description = 'Update RPM repository')
    parser.add_argument('directory', metavar = 'directory', type = str,
                        help = 'main repository directory')
    parser.add_argument('--name', type = str, default = DEFAULT_NAME,
                        help = 'change repository name')
    parser.add_argument('--user', type = str, default = DEFAULT_USER,
                        help = 'change GPG user')
    parser.add_argument('--import', metavar = 'path', type = str,
                        dest = 'imports', default = [], action = 'append',
                        help = 'import additional packages')
    args = parser.parse_args()

    for src in args.imports:
        move_packages(src, args.directory, '.rpm')

    subprocess.run([script_dir + '/../package/repo/rpm/update.sh', args.name, args.user], check = True, cwd = args.directory)
