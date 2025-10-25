#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

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
