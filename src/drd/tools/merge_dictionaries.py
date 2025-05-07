#!/usr/bin/env python3

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
import json

def merge_catalogs(catalogs):
    final = {}

    for cat in catalogs:
        for dictionary_key, dictionary in cat.items():
            if not dictionary_key in final:
                final[dictionary_key] = {
                    'title': dictionary['title'],
                    'definitions': {}
                }
            definitions = final[dictionary_key]['definitions']

            for v in dictionary['definitions']:
                definitions[v['code']] = v['label']

    for dictionary_key, dictionary in final.items():
        dictionary['definitions'] = [{'code': k, 'label': v} for k, v in dictionary['definitions'].items()]
        dictionary['definitions'].sort(key = lambda defn: defn['code'])

    return final

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description = 'Merge THOP catalogs.')
    parser.add_argument('filenames', type = str, nargs = '+', help = 'path to THOP catalogs')
    parser.add_argument('-D', '--destination', dest = 'destination', action = 'store',
                        required = True, help = 'destination file')
    args = parser.parse_args()

    catalogs = []
    for filename in args.filenames:
        with open(filename) as f:
            catalog = json.load(f)
            catalogs.append(catalog)

    final = merge_catalogs(catalogs)

    with open(args.destination, 'w') as f:
        json.dump(final, f, ensure_ascii = False, indent = 4)
