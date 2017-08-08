from collections import OrderedDict, defaultdict
import sys
from datetime import datetime, date
import csv
import json
import traceback

# -------------------------------------------------------------------------
# Constants
# -------------------------------------------------------------------------

FINESS_JURIDIQUE = 590780193

# -------------------------------------------------------------------------
# Utility
# -------------------------------------------------------------------------

def date_value(date_str):
    return datetime.strptime(date_str, "%d%m%Y").date()
def int_value(int_str, default = 0):
    return int(int_str) if int_str else default

def write_json(data, dest):
    class ExtendedJsonEncoder(json.JSONEncoder):
        def default(self, obj):
            if isinstance(obj, date):
                return obj.strftime('%Y-%m-%d')
            return json.JSONEncoder.default(self, obj)

    json.dump(data, dest, sort_keys = True, indent = 4, cls = ExtendedJsonEncoder)

# -------------------------------------------------------------------------
# Converters
# -------------------------------------------------------------------------

def parse_rums(filename):
    with open(filename) as f:
        for line in f:
            try:
                first_das = 192
                first_dad = first_das + int(line[133:135]) * 8
                first_procedure = first_dad + int(line[135:137]) * 8

                rum = {}
                rum['iep'] = int(line[47:67])
                rum['sex'] = int(line[85])
                rum['birthdate'] = date_value(line[77:85])
                rum['entry_date'] = date_value(line[92:100])
                rum['entry_mode'] = int(line[100])
                if line[101].strip():
                    if line[101] == 'R' or line[101] == 'r':
                        rum['entry_origin'] = 'R'
                    else:
                        rum['entry_origin'] = int(line[101])
                rum['exit_date'] = date_value(line[102:110])
                rum['exit_mode'] = int(line[110])
                if line[111].strip():
                    rum['exit_destination'] = int(line[111])
                rum['unit'] = int(line[86:90])
                if line[131:133].strip():
                    rum['session_count'] = int(line[131:133])
                if line[156:159].strip():
                    rum['igs2'] = int(line[156:159])
                if line[123:131].strip():
                    rum['last_menstrual_period'] = date_value(line[123:131])
                if line[117:121].strip():
                    rum['newborn_weight'] = int(line[117:121])
                if line[121:123].strip():
                    rum['gestational_age'] = int(line[121:123])

                rum['dp'] = line[140:148].strip()
                if line[148:156].strip():
                    rum['dr'] = line[148:156].strip()
                if int(line[133:135]):
                    rum['das'] = [line[(first_das + i * 8):(first_das + (i + 1) * 8)].strip()
                                  for i in range(0, int(line[133:135]))]

                if int(line[137:140]):
                    rum['procedures'] = [{
                        'date': date_value(line[i:(i + 8)]),
                        'code': line[(i + 8):(i + 15)].strip(),
                        'phase': int(line[i + 18]),
                        'activity': int(line[i + 19]),
                        'count': int(line[(i + 27):(i + 29)])
                    } for i in range(first_procedure, first_procedure + int(line[137:140]) * 29, 29)]

                yield rum
            except Exception as e:
                traceback.print_exc()

def parse_structure(filename):
    units = {}

    with open(filename, newline = '') as f:
        reader = csv.DictReader(f, delimiter = ';')
        for row in reader:
            # TODO: We want to keep units with patients, this is not enough
            if row['REF - Libellé long UF'].startswith('FERME'):
                continue
            if int(row['GEO - Matricule Finess']) == FINESS_JURIDIQUE:
                continue

            uf = int(row['REF - Code UF'])

            unit = {}
            unit['facility'] = int(row['GEO - Matricule Finess'])

            units[uf] = unit

    return units

def parse_ficum(filename):
    authorizations = defaultdict(list)

    with open(filename) as f:
        for line in f:
            try:
                type = line[13:16].strip()
                if type == '00':
                    continue

                authorization = {}
                authorization['date'] = date_value(line[16:24])
                if line[0:4] == '$$$$':
                    authorization['facility'] = int(line[4:13])
                else:
                    authorization['unit'] = int(line[0:4])

                authorizations[type].append(authorization)
            except Exception as e:
                print(e, file = sys.stderr)

    return authorizations

# -------------------------------------------------------------------------
# Main
# -------------------------------------------------------------------------

MAIN_USAGE = \
"""Usage: convert_chu_lille.py command options

Commands:
    stays               Convert RUM exports from CoRa
    units               Convert structure and authorization data"""
STAYS_USAGE = \
"""
"""
UNITS_USAGE = \
"""
"""

def process_stays(rum_filename):
    write_json(list(parse_rums(rum_filename)), sys.stdout)
def process_units(structure_filename, ficum_filename):
    write_json({
        'units': parse_structure(structure_filename),
        'authorizations': parse_ficum(ficum_filename)
    }, sys.stdout)

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
    elif cmd == "units":
        if len(sys.argv) < 4:
            print(UNITS_USAGE, file = sys.stderr)
            sys.exit(1)

        process_units(sys.argv[2], sys.argv[3])
    else:
        print(f"Unknown command '{cmd}'", file = sys.stderr)
        print(MAIN_USAGE, file = sys.stderr)
        sys.exit(1)

    sys.exit(0)
