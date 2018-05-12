#!/usr/bin/env python3

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys
from datetime import datetime, date, timedelta
import csv
import os.path
import itertools

# -------------------------------------------------------------------------
# CoRa
# -------------------------------------------------------------------------

def parse_cora_authorizations(*filenames):
    def load_structure_table(filename):
        with open(filename, encoding = 'iso-8859-1', newline = '') as f:
            reader = csv.DictReader(f, delimiter = ';')
            for row in reader:
                if row['FINESS_GEO']:
                    yield (row['CODE_UNITE'],
                           datetime.strptime(row['DATE_EFFET_AUTORISATION'], '%d/%m/%y').strftime('%Y-%m-%d') if row['DATE_EFFET_AUTORISATION'] else '',
                           row['TYPE_AUTORISATION'])

    rows = []
    for filename in filenames:
        structure = load_structure_table(filename)
        rows.extend(structure)
    rows = sorted(rows)

    authorizations = []
    for uf, uf_rows in itertools.groupby(rows, lambda it: it[0]):
        uf_rows = list(uf_rows)
        for type, type_rows in itertools.groupby(reversed(uf_rows), lambda it: it[2]):
            type_rows = list(type_rows)
            if type:
                auth = {
                    'Unit': int(uf),
                    'Date': type_rows[-1][1],
                    'Authorization': type
                }
                authorizations.append(auth)
    authorizations = sorted(authorizations, key = lambda it: (it['Unit'], it['Date']))

    return authorizations

# -------------------------------------------------------------------------
# Main
# -------------------------------------------------------------------------

USAGE = \
"""Usage: convert_cora_authorizations.py unite_csv unite_historique_csv"""

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(USAGE, file = sys.stderr)
        sys.exit(1)

    authorizations = parse_cora_authorizations(sys.argv[1], sys.argv[2])

    # configparser does not support duplicate sections
    for auth in authorizations:
        print(f'[{auth["Unit"]}]')
        print(f'Date = {auth["Date"]}')
        print(f'Authorization = {auth["Authorization"]}')
        print('')
