from collections import OrderedDict, defaultdict
import sys
from datetime import datetime, date
import csv
import json

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
                parts = [part.strip() for part in line.rstrip("\r\n").split(";")]

                first_das = 32
                first_dad = first_das + int(parts[23])
                first_procedure = first_dad + int(parts[24])

                rum = {}
                rum['iep'] = int(parts[7])
                rum['sex'] = int(parts[9])
                rum['birthdate'] = date_value(parts[8])
                rum['entry_date'] = date_value(parts[14])
                try:
                    rum['entry_mode'] = int(parts[15])
                except ValueError:
                    rum['entry_mode'] = parts[15]
                if parts[16]:
                    try:
                        rum['entry_origin'] = int(parts[16])
                    except ValueError:
                        rum['entry_origin'] = parts[16]
                rum['exit_date'] = date_value(parts[17])
                try:
                    rum['exit_mode'] = int(parts[18])
                except ValueError:
                    rum['exit_mode'] = parts[18]
                if parts[19]:
                    try:
                        rum['exit_destination'] = int(parts[19])
                    except ValueError:
                        rum['exit_destination'] = parts[19]
                rum['unit'] = int(parts[10])
                if parts[22]:
                    rum['session_count'] = int(parts[22])
                if parts[28]:
                    rum['igs2'] = int(parts[28])
                if parts[30]:
                    rum['last_menstrual_period'] = date_value(parts[30])
                if parts[21]:
                    rum['newborn_weight'] = int(parts[21])
                if parts[29]:
                    rum['gestational_age'] = int(parts[29])

                rum['dp'] = parts[26]
                if parts[27]:
                    rum['dr'] = parts[27]
                if int(parts[23]):
                    rum['das'] = [parts[first_das + i] for i in range(0, int(parts[23]))]

                if int(parts[25]):
                    rum['procedures'] = [{
                        'date': date_value(parts[first_procedure + i * 11 + 0]),
                        'code': parts[first_procedure + i * 11 + 1],
                        'phase': int(parts[first_procedure + i * 11 + 2]),
                        'count': int(parts[first_procedure + i * 11 + 9])
                    } for i in range(0, int(parts[25]))]

                yield rum
            except Exception as e:
                print(e, file = sys.stderr)

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
