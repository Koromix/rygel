#!/usr/bin/env python3

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

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
