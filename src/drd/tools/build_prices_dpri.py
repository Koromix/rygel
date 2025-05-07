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
import configparser
import csv
import io
import sys
import zipfile
import json

# -------------------------------------------------------------------------
# Parse
# -------------------------------------------------------------------------

def process_zip_file(zip_filename):
    build = None
    date = None
    prices = {
        'Public': {},
        'Private': {}
    }

    with zipfile.ZipFile(zip_filename) as zip:
        for info in zip.infolist():
            new_build = f'{info.date_time[0]:04}-{info.date_time[1]:02}-{info.date_time[2]:02}'
            if build is None or new_build > build:
                build = new_build

            with zip.open(info.filename, 'r') as csv_file:
                csv_file = io.TextIOWrapper(csv_file, encoding = 'ISO-8859-1')

                if info.filename.startswith('ghs_pub'):
                    prices['Public']['GHS'], date = extract_ghs_prices(csv_file)
                elif info.filename.startswith('ghs_pri'):
                    prices['Private']['GHS'], date = extract_ghs_prices(csv_file)
                elif info.filename.startswith('sup_pub'):
                    prices['Public']['Supplements'] = extract_supplements_prices(csv_file)
                elif info.filename.startswith('sup_pri'):
                    prices['Private']['Supplements'] = extract_supplements_prices(csv_file)

    return date, build, prices

def extract_ghs_prices(csv_file):
    reader = csv.DictReader(csv_file, delimiter = ';')
    date = None
    ghs = {}
    for row in reader:
        date = row['DATE-EFFET']
        ghs_info = {'PriceCents': parse_price_cents(row['GHS-PRI'])}
        if int(row['SEU-BAS']):
            ghs_info['ExbThreshold'] = int(row['SEU-BAS'])
            ghs_info['ExbCents'] = parse_price_cents(row['EXB-JOURNALIER'])
            if not ghs_info['ExbCents']:
                ghs_info['ExbCents'] = parse_price_cents(row['EXB-FORFAIT'])
                ghs_info['ExbType'] = 'Once'
        if int(row['SEU-HAU']):
            ghs_info['ExhThreshold'] = int(row['SEU-HAU']) + 1
            ghs_info['ExhCents'] = parse_price_cents(row['EXH-PRI'])
        ghs[int(row['GHS-NRO'])] = ghs_info
    date = '-'.join(reversed(date.split('/')))
    return ghs, date

def extract_supplements_prices(csv_file):
    reader = csv.DictReader(csv_file, delimiter = ';')
    supplements_dict = {}
    for row in reader:
        supplements_dict[row['CODE'].upper()] = parse_price_cents(row['TARIF'])
    return supplements_dict

def parse_price_cents(str):
    parts = str.split(',')
    if len(parts) > 1:
        return int(f"{parts[0]}{parts[1]:<02}")
    else:
        return int(parts[0]) * 100

# -------------------------------------------------------------------------
# Main
# -------------------------------------------------------------------------

def write_prices_ini(date, build, sector, ghs, supplements, file):
    print(f'Date = {date}', file = file)
    print(f'Build = {build}', file = file)
    print(f'Sector = {sector}', file = file)
    print('', file = file)

    cfg = configparser.ConfigParser()
    cfg.optionxform = str
    cfg.read_dict(ghs)
    cfg.read_dict({'Supplements': supplements})
    cfg.write(file)

def write_prices_json(date, build, sector, ghs, supplements, file):
    obj = {
        'Date': date,
        'Build': build,
        'Sector': sector,
        'GHS': ghs,
        'Supplements': supplements
    }

    json.dump(obj, file, ensure_ascii = False, indent = 4)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description = 'Convert ATIH ZIP price files to INI files.')
    parser.add_argument('filenames', metavar = 'zip_filename', type = str, nargs = '+',
                        help = 'path to GHS price ZIP files')
    parser.add_argument('-f', '--format', dest = 'format', action = 'store',
                        default = 'ini', choices = ['ini', 'json'],
                        help = 'destination format')
    parser.add_argument('-D', '--destination', dest = 'destination', action = 'store',
                        required = True, help = 'destination directory')
    args = parser.parse_args()

    for zip_filename in args.filenames:
        date, build, prices = process_zip_file(zip_filename)
        prefix = date.replace('-', '')

        if args.format == 'ini':
            with open(f'{args.destination}/{prefix}_public.dpri', 'w') as ini_file:
                write_prices_ini(date, build, 'Public', prices['Public']['GHS'],
                                 prices['Public']['Supplements'], ini_file)
            with open(f'{args.destination}/{prefix}_private.dpri', 'w') as ini_file:
                write_prices_ini(date, build, 'Private', prices['Private']['GHS'],
                                 prices['Private']['Supplements'], ini_file)
        elif args.format == 'json':
            with open(f'{args.destination}/{prefix}_public.dprj', 'w') as json_file:
                write_prices_json(date, build, 'Public', prices['Public']['GHS'],
                                  prices['Public']['Supplements'], json_file)
            with open(f'{args.destination}/{prefix}_private.dprj', 'w') as json_file:
                write_prices_json(date, build, 'Private', prices['Private']['GHS'],
                                  prices['Private']['Supplements'], json_file)
