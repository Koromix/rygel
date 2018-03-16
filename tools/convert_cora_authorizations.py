# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from collections import OrderedDict, defaultdict
import sys
from datetime import datetime, date, timedelta
import csv
import json
import traceback
import io
import os.path
import operator
import itertools

# -------------------------------------------------------------------------
# Utility
# -------------------------------------------------------------------------

def int_or_string(s):
    try:
        return int(s)
    except ValueError:
        return str(s)

def write_json(data, dest):
    class ExtendedJsonEncoder(json.JSONEncoder):
        def default(self, obj):
            if isinstance(obj, date):
                return obj.strftime('%Y-%m-%d')
            return json.JSONEncoder.default(self, obj)

    json.dump(data, dest, sort_keys = True, indent = 4, cls = ExtendedJsonEncoder)

# -------------------------------------------------------------------------
# Authorizations
# -------------------------------------------------------------------------

def parse_authorizations(*filenames):
    def load_structure_table(filename):
        with open(filename, newline = '') as f:
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
        prev_end = None
        for type, type_rows in itertools.groupby(reversed(uf_rows), lambda it: it[2]):
            type_rows = list(type_rows)
            if type and type_rows[-1][1] != prev_end:
                auth = {}
                auth['unit'] = int(uf)
                auth['authorization'] = int_or_string(type)
                auth['begin_date'] = type_rows[-1][1]
                if prev_end:
                    auth['end_date'] = prev_end
                authorizations.append(auth)
            prev_end = type_rows[-1][1]
    authorizations = sorted(authorizations, key = lambda it: (it['unit'], it['begin_date']))

    return authorizations

def process_authorizations(unite_filename, hist_filename):
    authorizations = parse_authorizations(unite_filename, hist_filename)
    write_json(authorizations, sys.stdout)

# -------------------------------------------------------------------------
# Main
# -------------------------------------------------------------------------

USAGE = \
"""Usage: convert_cora_authorizations.py unite_csv unite_historique_csv"""

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(USAGE, file = sys.stderr)
        sys.exit(1)

    process_authorizations(sys.argv[1], sys.argv[2])
    sys.exit(0)
