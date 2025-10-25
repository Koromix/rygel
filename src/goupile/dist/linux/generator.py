#!/usr/bin/python3 -B
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import argparse
import manage

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description = 'Generate goupile systemd units')
    parser.add_argument('normal', metavar = 'path', type = str, help = 'path for normal units', nargs = '?', default = manage.GENERATOR_DIRECTORY)
    parser.add_argument('early', metavar = 'early', type = str, help = 'path for early units', nargs = '?')
    parser.add_argument('late', metavar = 'late', type = str, help = 'path for late units', nargs = '?')
    args = parser.parse_args()

    domains = manage.load_domains(manage.DOMAINS_DIRECTORY)

    manage.sync_domains(manage.CONFIG_DIRECTORY, domains, manage.TEMPLATE_FILENAME)
    manage.sync_units(args.normal, domains)
