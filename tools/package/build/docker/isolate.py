#!/usr/bin/env python3

import argparse
import os
import re
import shutil
import subprocess

def read_binary(binary):
    env = os.environ | { 'LANG': 'C', 'LANGUAGE': 'C' }
    output = subprocess.check_output(['readelf', '-hd', binary], env = env).decode('utf-8')

    triplet = None
    loader = None
    libraries = []

    for line in output.splitlines():
        line = line.strip()

        m = re.search(r'Machine: (.*)', line)
        if m is not None:
            name = m.group(1).strip()
            if name == 'Advanced Micro Devices X86-64':
                triplet = 'x86_64-linux-gnu'
            elif name == 'AArch64':
                triplet = 'aarch64-linux-gnu'

        m = re.search(r'Shared library: \[(.*)\]', line)
        if m is not None:
            lib = m.group(1)
            if lib.startswith('ld-linux-'):
                loader = lib
            libraries.append(m.group(1))

    return triplet, loader, libraries

def patch_binary(binary, ld, rpath):
    subprocess.run(['patchelf', '--set-interpreter', ld, '--set-rpath', rpath, binary], check = True)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description = 'Copy binary and shared libraries to single directory')
    parser.add_argument('binary', metavar = 'binary', type = str, help = 'main binary file')
    parser.add_argument('-p', '--prefix', type = str, required = True, help = 'absolute path inside container')
    parser.add_argument('-O', '--output_dir', type = str, required = True, help = 'destination directory')
    args = parser.parse_args()

    triplet, loader, libraries = read_binary(args.binary)

    os.mkdir(args.output_dir)

    binary = os.path.join(args.output_dir, os.path.basename(args.binary))
    shutil.copy(args.binary, binary)

    for lib in libraries:
        src = os.path.join('/lib', triplet, lib)
        dest = os.path.join(args.output_dir, lib)
        shutil.copy(src, dest)

    ld = os.path.join(args.prefix, loader)
    patch_binary(binary, ld, args.prefix)
