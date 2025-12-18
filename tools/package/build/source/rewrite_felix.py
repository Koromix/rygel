#!/usr/bin/env python3

import argparse
import configparser
import itertools

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description = 'Rewrite FelixBuild.ini with only specified targets')
    parser.add_argument('source', metavar = 'binary', type = str, help = 'source FelixBuild.ini file')
    parser.add_argument('-O', '--output_file', type = str, required = True, help = 'destination file')
    parser.add_argument('-t', '--targets', nargs = '*', required = True, help = 'list of targets')
    args = parser.parse_args()

    with open(args.source, 'r') as f:
        lines = f.readlines()

    with open(args.output_file, 'w') as f:
        keep = True
        for line in lines:
            if line.startswith('['):
                target = line.strip('[] \r\n')
                keep = target in args.targets
            if keep or line.startswith('# SPDX'):
                f.write(line)
