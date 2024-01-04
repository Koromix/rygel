#!/usr/bin/python3

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program. If not, see https://www.gnu.org/licenses/.

import argparse
import base64
import configparser
import os
import re
import secrets
import subprocess
import sys

BINARY_FILENAME = '/usr/bin/goupile'
GENERATOR_FILENAME = '/lib/systemd/system-generators/goupile-systemd-generator'

DOMAINS_DIRECTORY = '/etc/goupile/domains.d'

def derive_public_key(key):
    output = subprocess.check_output([BINARY_FILENAME, 'keys', '-k', decrypt_key], stderr=subprocess.STDOUT)

    m = re.search('Public key: ([a-zA-Z0-9+/=]+)', output.decode('UTF-8'))
    archive_key = m.group(1)

    return archive_key

def make_config(archive_key, port):
    config = configparser.ConfigParser()
    config.optionxform = str

    config.add_section('Domain')
    config['Domain']['ArchiveKey'] = archive_key
    config['Domain']['Port'] = str(port)

    return config

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description = 'Generate goupile domain config')
    parser.add_argument('name', metavar = 'name', type = str, help = 'domain name')
    parser.add_argument('-f', '--force', dest = 'force', action = 'store_true', help = 'Overwrite existing domain config')
    parser.add_argument('-p', '--port', dest = 'port', type = int, required = True, help = 'HTTP port')
    args = parser.parse_args()

    if not re.match('^[a-zA-Z0-9_\-\.]+$', args.name):
        raise ValueError('Name must only contain alphanumeric, \'.\', \'-\' or \'_\' characters')
    if args.port <= 0 or args.port > 65535:
        raise ValueError(f'Invalid port value {args.port}')

    sk = secrets.token_bytes(32)
    decrypt_key = base64.b64encode(sk).decode('UTF-8')
    archive_key = derive_public_key(decrypt_key)

    ini_filename = os.path.join(DOMAINS_DIRECTORY, args.name) + '.ini'
    config = make_config(archive_key, args.port)

    with open(ini_filename, 'w' if args.force else 'x') as f:
        config.write(f)

    print(f'Domain config file: {ini_filename}\n', file = sys.stderr)
    subprocess.run([BINARY_FILENAME, 'keys', '-k', decrypt_key], check = True)

    subprocess.run([GENERATOR_FILENAME, '/run/systemd/generator'], check = True)
    subprocess.run(['/usr/bin/systemctl', 'daemon-reload'], check = True)
