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

def int_optional(int_str, default = None):
    int_str = int_str.strip()
    return int(int_str) if int_str else default
def int_or_string(s):
    try:
        return int(s)
    except ValueError:
        return str(s)
def parse_date(date_str):
    date_str = date_str.decode()
    return datetime.strptime(date_str, "%d%m%Y").date()
def parse_date_optional(date_str, default = None):
    date_str = date_str.strip()
    return parse_date(date_str) if date_str else default

def sweep(data):
    def recurse(x):
        if isinstance(x, dict):
            return {k: recurse(v) for k, v in x.items() if v is not None and v != {}}
        else:
            return x
    return recurse(data)

def write_json(data, dest):
    class ExtendedJsonEncoder(json.JSONEncoder):
        def default(self, obj):
            if isinstance(obj, date):
                return obj.strftime('%Y-%m-%d')
            return json.JSONEncoder.default(self, obj)

    json.dump(data, dest, sort_keys = True, indent = 4, cls = ExtendedJsonEncoder)

# -------------------------------------------------------------------------
# Stays
# -------------------------------------------------------------------------

def parse_mco_grp(filename):
    with open(filename, 'rb') as f:
        for line in f:
            buf = io.BytesIO(line)

            try:
                rum = {}
                buf.seek(2, io.SEEK_CUR) # Skip GHM Version
                rum['test'] = {
                    'ghm': buf.read(6).decode()
                }
                buf.seek(1, io.SEEK_CUR) # Skip filler byte
                rss_version = int(buf.read(3))
                if rss_version < 116:
                    raise ValueError(f'Unsupported RSS format {rss_version}')
                rum['test']['error'] = int_optional(buf.read(3))
                buf.seek(12, io.SEEK_CUR) # Skip FINESS and RUM version
                rum['bill_id'] = int(buf.read(20))
                rum['stay_id'] = int(buf.read(20))
                buf.seek(10, io.SEEK_CUR) # Skip RUM identifier
                rum['birthdate'] = parse_date(buf.read(8))
                rum['sex'] = int(buf.read(1))
                rum['unit'] = int(buf.read(4))
                rum['bed_authorization'] = int_optional(buf.read(2))
                rum['entry_date'] = parse_date(buf.read(8))
                rum['entry_mode'] = int(buf.read(1))
                rum['entry_origin'] = buf.read(1).decode().upper()
                if rum['entry_origin'] != 'R':
                    rum['entry_origin'] = int_optional(rum['entry_origin'])
                rum['exit_date'] = parse_date(buf.read(8))
                rum['exit_mode'] = int(buf.read(1))
                rum['exit_destination'] = int_optional(buf.read(1))
                buf.seek(5, io.SEEK_CUR) # Skip postal code
                rum['newborn_weight'] = int_optional(buf.read(4))
                rum['gestational_age'] = int_optional(buf.read(2))
                rum['last_menstrual_period'] = parse_date_optional(buf.read(8))
                rum['session_count'] = int_optional(buf.read(2))
                das_count = int(buf.read(2))
                dad_count = int(buf.read(2))
                proc_count = int(buf.read(3))
                rum['dp'] = buf.read(8).decode().strip()
                rum['dr'] = buf.read(8).decode().strip()
                if not rum['dr']:
                    rum['dr'] = None
                rum['igs2'] = int_optional(buf.read(3))
                buf.seek(33, io.SEEK_CUR) # Skip confirmation, innovation, etc.

                if das_count:
                    rum['das'] = []
                    for i in range(0, das_count):
                        rum['das'].append(buf.read(8).decode().strip())
                buf.seek(dad_count * 8, io.SEEK_CUR)

                if proc_count:
                    rum['procedures'] = []
                    for i in range(0, proc_count):
                        proc = {}
                        proc['date'] = parse_date(buf.read(8))
                        proc['code'] = buf.read(7).decode().strip()
                        if rss_version >= 117:
                            buf.seek(3, io.SEEK_CUR) # Skip CCAM code extension
                        proc['phase'] = int(buf.read(1))
                        proc['activity'] = int(buf.read(1))
                        buf.seek(7, io.SEEK_CUR) # Skip modificators, doc extension, etc.
                        proc['count'] = int(buf.read(2))
                        rum['procedures'].append(proc)

                yield sweep(rum)
            except Exception as e:
                traceback.print_exc()

def parse_mco_rsa(filename):
    with open(filename, 'rb') as f:
        for rss_id, line in enumerate(f, start = 1):
            buf = io.BytesIO(line)

            try:
                rss = {}
                rss['stay_id'] = rss_id
                rss['bill_id'] = rss_id
                buf.seek(9, io.SEEK_CUR) # Skip FINESS
                rsa_version = int(buf.read(3))
                if rsa_version < 220:
                    raise ValueError(f'Unsupported RSA format {rsa_version}')
                buf.seek(29, io.SEEK_CUR) # Skip many fields
                rss['test'] = {
                    'ghm': buf.read(6).decode(),
                    'error': int_optional(buf.read(3))
                }
                rum_count = int(buf.read(2))
                age = int_optional(buf.read(3))
                age_days = int_optional(buf.read(3))
                rss['sex'] = int(buf.read(1))
                entry_mode = int(buf.read(1))
                entry_origin = buf.read(1).decode().upper()
                if entry_origin != 'R':
                    entry_origin = int_optional(entry_origin)
                exit_date = parse_date(b'01' + buf.read(6))
                if exit_date.month == 12:
                    exit_date = exit_date.replace(day = 31)
                else:
                    exit_date = exit_date.replace(month = exit_date.month + 1, day = 1) - timedelta(days = 1)
                exit_mode = int(buf.read(1))
                exit_destination = int_optional(buf.read(1))
                buf.seek(1, io.SEEK_CUR) # Skip stay type
                duration = int(buf.read(4))
                entry_date = exit_date - timedelta(days = duration)
                if age:
                    rss['birthdate'] = date(entry_date.year - age, 1, 1)
                else:
                    rss['birthdate'] = entry_date - timedelta(days = age_days)
                buf.seek(5, io.SEEK_CUR) # Skip geography code
                rss['newborn_weight'] = int_optional(buf.read(4))
                rss['gestational_age'] = int_optional(buf.read(2))
                try:
                    lmp_delay = int(buf.read(3))
                    rss['last_menstrual_period'] = entry_date - timedelta(days = lmp_delay)
                except ValueError:
                    pass
                rss['session_count'] = int_optional(buf.read(2))
                rss['test']['ghs'] = int(buf.read(4))
                buf.seek(14, io.SEEK_CUR) # Skip many fields
                global_auth_count = int(buf.read(1))
                buf.seek(21, io.SEEK_CUR) # Skip many fields
                radiotherapy_supp_count = int(buf.read(1))
                if rsa_version >= 222:
                    buf.seek(18, io.SEEK_CUR) # Skip many fields
                else:
                    buf.seek(26, io.SEEK_CUR) # Skip many fields
                rss['test']['rea'] = int_optional(buf.read(3))
                rss['test']['reasi'] = int_optional(buf.read(3))
                rss['test']['si'] = int_optional(buf.read(3)) - rss['test']['reasi']
                rss['test']['src'] = int_optional(buf.read(3))
                rss['test']['nn1'] = int_optional(buf.read(3))
                rss['test']['nn2'] = int_optional(buf.read(3))
                rss['test']['nn3'] = int_optional(buf.read(3))
                rss['test']['rep'] = int_optional(buf.read(3))
                if int(buf.read(1)):
                    rss['bed_authorization'] = 8
                if rsa_version >= 223:
                    buf.seek(64, io.SEEK_CUR) # Skip many fields
                elif rsa_version >= 222:
                    buf.seek(49, io.SEEK_CUR) # Skip many fields
                else:
                    buf.seek(41, io.SEEK_CUR) # Skip many fields
                buf.seek(2 * global_auth_count, io.SEEK_CUR)
                buf.seek(7 * radiotherapy_supp_count, io.SEEK_CUR)

                current_date = entry_date

                rums = []
                for i in range(0, rum_count):
                    rum = rss.copy()
                    buf.seek(14, io.SEEK_CUR)
                    rum['dp'] = buf.read(6).decode().strip()
                    rum['dr'] = buf.read(6).decode().strip()
                    if not rum['dr']:
                        rum['dr'] = None
                    rum['igs2'] = int_optional(buf.read(3))
                    if rsa_version >= 221:
                        rum['gestational_age'] = int_optional(buf.read(2))
                    rum['das_count'] = int(buf.read(2))
                    rum['procedures_count'] = int(buf.read(3))
                    duration = int(buf.read(4))
                    rum['entry_date'] = current_date
                    if i > 0:
                        rum['entry_mode'] = 6
                        rum['entry_origin'] = 1
                    else:
                        rum['entry_mode'] = entry_mode
                        rum['entry_origin'] = entry_origin
                    current_date += timedelta(duration)
                    rum['exit_date'] = current_date
                    if i + 1 < rum_count:
                        rum['exit_mode'] = 6
                        rum['exit_destination'] = 1
                    else:
                        rum['exit_mode'] = exit_mode
                        rum['exit_destination'] = exit_destination
                    rum['unit'] = 10000 + int_optional(buf.read(2))
                    buf.seek(18, io.SEEK_CUR)
                    rums.append(rum)

                for rum in rums:
                    rum['das'] = []
                    for i in range(0, rum['das_count']):
                        rum['das'].append(buf.read(6).decode().strip())
                    rum.pop('das_count', None)

                for rum in rums:
                    rum['procedures'] = []
                    for i in range(0, rum['procedures_count']):
                        proc = {}
                        proc_delay = int(buf.read(3))
                        proc['date'] = rum['entry_date'] + timedelta(days = proc_delay)
                        proc['code'] = buf.read(7).decode().strip()
                        if rsa_version >= 222:
                            buf.seek(2, io.SEEK_CUR) # Skip CCAM code extension
                        proc['phase'] = int(buf.read(1))
                        proc['activity'] = int(buf.read(1))
                        buf.seek(7, io.SEEK_CUR) # Skip modificators, doc extension, etc.
                        proc['count'] = int(buf.read(2))
                        buf.seek(1, io.SEEK_CUR)
                        rum['procedures'].append(proc)
                    rum.pop('procedures_count', None)

                for rum in rums:
                    yield sweep(rum)
            except Exception as e:
                traceback.print_exc()

def process_stays(filename):
    _, ext = os.path.splitext(filename)
    if ext == '.rsa':
        rums = list(parse_mco_rsa(filename))
    elif ext in ('.rss', '.grp'):
        rums = list(parse_mco_grp(filename))
    else:
        raise ValueError(f'Unsupported extension "{ext}"')
    rums.sort(key = operator.itemgetter('bill_id'))

    def update_rss_count(first, last):
        for rum in rums[first:last]:
            rum['test']['cluster_len'] = last - first

    first_rss_rum = 0
    for i, rum in enumerate(rums):
        if rums[first_rss_rum]['bill_id'] != rum['bill_id']:
            update_rss_count(first_rss_rum, i)
            first_rss_rum = i
    update_rss_count(first_rss_rum, len(rums))

    write_json(rums, sys.stdout)

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

MAIN_USAGE = \
"""Usage: convert_chu_lille.py command options

Commands:
    authorizations      Convert structure to authorization data
    stays               Convert RUM files"""
STAYS_USAGE = \
"""Usage: convert_chu_lille.py stays file

Supported file types:
    RSA (.rsa)
    RSS (.rss, .grp)"""
UNITS_USAGE = \
"""Usage: convert_chu_lille.py authorizations unite_csv unite_historique_csv"""

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(MAIN_USAGE, file = sys.stderr)
        sys.exit(1)

    cmd = sys.argv[1]
    if cmd == "stays":
        if len(sys.argv) < 3:
            print(STAYS_USAGE, file = sys.stderr)
            sys.exit(1)

        process_stays(sys.argv[2])
    elif cmd == "authorizations":
        if len(sys.argv) < 4:
            print(UNITS_USAGE, file = sys.stderr)
            sys.exit(1)

        process_authorizations(sys.argv[2], sys.argv[3])
    else:
        print(f"Unknown command '{cmd}'", file = sys.stderr)
        print(MAIN_USAGE, file = sys.stderr)
        sys.exit(1)

    sys.exit(0)
