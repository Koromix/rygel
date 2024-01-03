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
import subprocess
from dataclasses import dataclass

BINARY_FILENAME = '/usr/bin/goupile'

DOMAINS_FILENAME = '/etc/goupile/domains.ini'
TEMPLATE_FILENAME = '/etc/goupile/template.ini'
UNIT_FILENAME = '/usr/lib/systemd/system/goupile@.service'

CONFIG_DIRECTORY = '/run/goupile'
ROOT_DIRECTORY = '/var/lib/goupile'

@dataclass
class DomainConfig:
    name = None
    archive_key = None
    port = None

def load_config(filename):
    config = configparser.ConfigParser()
    config.optionxform = str

    with open(filename, 'r') as f:
        config.read_file(f)

    return config

def load_domains(config):
    domains = []

    used_names = set()
    used_ports = set()

    for name in config.sections():
        domain = DomainConfig()
        section = config[name]

        domain.name = name
        domain.archive_key = decode_key(section['ArchiveKey'])
        domain.port = decode_port(section['Port'])

        if domain.name in used_names:
            raise ValueError(f'Name "{domain.name}" used multiple times')
        if domain.port in used_ports:
            raise ValueError(f'Port {domain.port} used multiple times')

        used_names.add(domain.name)
        used_ports.add(domain.port)

        domains.append(domain)

    return domains

def decode_key(str):
    key = base64.b64decode(str)
    if len(key) != 32:
        raise ValueError(f'Malformed archive key "{str}"');
    return key

def decode_port(str):
    port = int(str)
    if port <= 0 or port > 65535:
        raise ValueError(f'Invalid TCP port "{str}"')
    return port

def sync_domains(path, domains, template):
    os.makedirs(path, mode = 0o755, exist_ok = True)

    for domain in domains:
        config = load_config(template)

        root_directory = os.path.join(ROOT_DIRECTORY, domain.name)
        ini_filename = os.path.join(path, domain.name) + '.ini'

        data = list(config['Data'].items())

        config['Domain']['Title'] = domain.name
        config['Data']['RootDirectory'] = root_directory
        for key, value in data:
            config.remove_option('Data', key)
            config['Data'][key] = value
        config['Archives']['PublicKey'] = base64.b64encode(domain.archive_key).decode('UTF-8')
        config['HTTP']['Port'] = str(domain.port)

        with open(ini_filename, 'w') as f:
            config.write(f)

def execute_command(args):
    subprocess.run(args, check = True)

def sync_units(path, domains):
    directory = os.path.join(path, 'multi-user.target.wants')
    prefix, _ = os.path.splitext(os.path.basename(UNIT_FILENAME))

    os.makedirs(directory, exist_ok = True)

    for name in os.listdir(directory):
        if not name.startswith(prefix):
            continue

        unit_filename = os.path.join(directory, name)
        os.unlink(unit_filename)

    for domain in domains:
        unit_filename = os.path.join(directory, prefix + domain.name) + '.service'
        os.symlink(UNIT_FILENAME, unit_filename)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description = 'Generate goupile systemd units')
    parser.add_argument('normal', metavar = 'path', type = str, help = 'path for normal units')
    parser.add_argument('early', metavar = 'early', type = str, help = 'path for early units', nargs = '?')
    parser.add_argument('late', metavar = 'late', type = str, help = 'path for late units', nargs = '?')
    args = parser.parse_args()

    config = load_config(DOMAINS_FILENAME)
    domains = load_domains(config)

    sync_domains(CONFIG_DIRECTORY, domains, TEMPLATE_FILENAME)
    sync_units(args.normal, domains)
