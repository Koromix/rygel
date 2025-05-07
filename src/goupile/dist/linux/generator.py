#!/usr/bin/python3 -B

# Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

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
