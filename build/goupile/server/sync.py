#!/bin/env python3

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import configparser
import hashlib
import io
import itertools
import os
import re
import sys
import subprocess
from dataclasses import dataclass

@dataclass
class DomainConfig:
    directory = None
    binary = None
    socket = None
    mismatch = False

@dataclass
class ServiceStatus:
    running = False
    inode = None

def parse_ini(filename, allow_no_value = False):
    ini = configparser.ConfigParser(allow_no_value = allow_no_value)
    ini.optionxform = str

    with open(filename, 'r') as f:
        ini.read_file(f)

    return ini

def load_config(filename, array_sections = []):
    ini = parse_ini(filename, allow_no_value = True)
    config = {}

    for section in ini.sections():
        if section in array_sections:
            for key, value in ini.items(section):
                if value:
                    raise ValueError(f'Unexpected value in {filename}')

                array = config.get(section)
                if array is None:
                    array = []
                    config[section] = array
                array.append(key)
        else:
            for key, value in ini.items(section):
                name = f'{section}.{key}'
                config[name] = value

    return config

def execute_command(args):
    subprocess.run(args, check = True)

def commit_file(filename, data):
    try:
        with open(filename, 'rb') as f:
            m = hashlib.sha256()
            for chunk in iter(lambda: f.read(4096), b""):
                m.update(chunk)
            hash1 = m.digest()
    except Exception:
        hash1 = None

    data = data.encode('UTF-8')
    m = hashlib.sha256()
    m.update(data)
    hash2 = m.digest()

    if hash1 != hash2:
        with open(filename, 'wb') as f:
            f.write(data)
        return True
    else:
        return False

def list_domains(root_dir, names):
    domains = {}

    for domain in names:
        directory = os.path.join(root_dir, domain)
        filename = os.path.join(directory, 'goupile.ini')

        if os.path.isfile(filename):
            config = load_config(filename)

            info = DomainConfig()
            info.directory = directory
            info.binary = os.path.join(directory, 'goupile')
            info.socket = f'/run/goupile/{domain}.sock'

            prev_socket = config.get('HTTP.UnixPath')
            if prev_socket != info.socket:
                info.mismatch = True

            domains[domain] = info

    return domains

def create_domain(binary, root_dir, domain, owner_user, admin_username, admin_password):
    directory = os.path.join(root_dir, domain)
    print(f'>>> Create domain {domain} ({directory})', file = sys.stderr)
    execute_command([binary, 'init', '-o', owner_user,
                    '--username', admin_username, '--password', admin_password, directory])

def migrate_domain(domain, info):
    print(f'>>> Migrate domain {domain} ({info.directory})', file = sys.stderr)
    filename = os.path.join(info.directory, 'goupile.ini')
    execute_command([info.binary, 'migrate', '-C', filename])

def list_services():
    services = {}

    output = subprocess.check_output(['systemctl', 'list-units', '--type=service', '--all'])
    output = output.decode()

    for line in output.splitlines():
        parts = re.split(' +', line)

        if len(parts) >= 4:
            match = re.search('^goupile@([0-9A-Za-z_\\-\\.]+)\\.service$', parts[1])

            if match is not None:
                name = match.group(1)

                status = ServiceStatus()
                status.running = (parts[3] == 'active')

                if status.running:
                    try:
                        pid = int(subprocess.check_output(['systemctl', 'show', '-p', 'ExecMainPID', '--value', parts[1]]))

                        sb = os.stat(f'/proc/{pid}/exe')
                        status.inode = sb.st_ino
                    except Exception:
                        status.running = False

                services[name] = status

    return services

def run_service_command(domain, cmd):
    service = f'goupile@{domain}.service'
    print(f'>>> {cmd.capitalize()} {service}', file = sys.stderr)
    execute_command(['systemctl', cmd, '--quiet', service])

def update_domain_config(info):
    filename = os.path.join(info.directory, 'goupile.ini')
    ini = parse_ini(filename)

    if not ini.has_section('HTTP'):
        ini.add_section('HTTP')
    ini.set('HTTP', 'SocketType', 'Unix')
    ini.set('HTTP', 'UnixPath', info.socket)
    ini.remove_option('HTTP', 'Port')

    with io.StringIO() as f:
        ini.write(f)
        return commit_file(filename, f.getvalue())

def update_nginx_config(directory, domain, socket, include = None):
    filename = os.path.join(directory, f'{domain}.conf')

    compat_dir = os.path.join(directory, 'compat.d')
    custom_dir = os.path.join(directory, 'custom.d')

    with io.StringIO() as f:
        print(f'server {{', file = f)
        print(f'    server_name {domain};', file = f)
        print(f'    client_max_body_size 32M;', file = f)
        if include is not None:
            print(f'    include {include};', file = f)
        print(f'    location / {{', file = f)
        print(f'        proxy_pass http://unix:{socket}:;', file = f)
        print(f'    }}', file = f)
        if os.path.isdir(compat_dir):
            print(f'    include {compat_dir}/{domain}[.]conf;', file = f)
        if os.path.isdir(custom_dir):
            print(f'    include {custom_dir}/{domain}[.]conf;', file = f)
        print(f'}}', file = f)

        return commit_file(filename, f.getvalue())

def run_sync(config):
    default_binary = os.path.join(config['Goupile.BinaryDirectory'], 'goupile')

    # Create missing domains
    for domain in config['Domains']:
        directory = os.path.join(config['Goupile.DomainDirectory'], domain)
        if not os.path.exists(directory):
            create_domain(default_binary, config['Goupile.DomainDirectory'], domain,
                          config['Goupile.RunUser'], config['Goupile.DefaultAdmin'], config['Goupile.DefaultPassword'])

    # List existing domains and services
    domains = list_domains(config['Goupile.DomainDirectory'], config['Domains'])
    services = list_services()

    # Detect binary mismatches
    for domain, info in domains.items():
        if not os.path.exists(info.binary):
            try:
                os.unlink(info.binary)
            except Exception:
                pass
            os.symlink(default_binary, info.binary)
        binary_inode = os.stat(info.binary).st_ino
        status = services.get(domain)
        if status is not None and status.running and status.inode != binary_inode:
            print(f'+++ Domain {domain} is running old version')
            info.mismatch = True

    changed = False

    # Update instance configuration files
    print('>>> Write configuration files', file = sys.stderr)
    for domain, info in domains.items():
        if update_domain_config(info):
            changed = True

    # Update NGINX configuration files
    print('>>> Write NGINX configuration files', file = sys.stderr)
    for name in os.listdir(config['NGINX.ConfigDirectory']):
        match = re.search('^([0-9A-Za-z_\\-\\.]+)\\.conf$', name)
        if match is not None:
            domain = match.group(1)
            if domains.get(domain) is None:
                filename = os.path.join(config['NGINX.ConfigDirectory'], name)
                os.unlink(filename)
                changed = True
    for domain, info in domains.items():
        if update_nginx_config(config['NGINX.ConfigDirectory'], domain, info.socket,
                               include = config.get('NGINX.ServerInclude')):
            changed = True

    # Sync systemd services
    for domain in services:
        info = domains.get(domain)
        if info is None:
            run_service_command(domain, 'stop')
            run_service_command(domain, 'disable')
            changed = True
    for domain, info in domains.items():
        status = services.get(domain)
        if status is None:
            run_service_command(domain, 'enable')
        if status is None or info.mismatch or not status.running:
            run_service_command(domain, 'stop')
            migrate_domain(domain, info)
            run_service_command(domain, 'start')
            changed = True

    # Nothing changed!
    if not changed:
        print('>>> Nothing has changed', file = sys.stderr)
        return

    # Reload NGINX configuration
    print('>>> Reload NGINX server', file = sys.stderr)
    execute_command(['systemctl', 'reload', config['NGINX.ServiceName']])

if __name__ == '__main__':
    # Always work from sync.py directory
    script = os.path.abspath(__file__)
    directory = os.path.dirname(script)
    os.chdir(directory)

    # Parse configuration
    config = load_config('sync.ini', array_sections = ['Domains'])
    config['Goupile.BinaryDirectory'] = os.path.abspath(config['Goupile.BinaryDirectory'])
    config['Goupile.DomainDirectory'] = os.path.abspath(config['Goupile.DomainDirectory'])
    config['NGINX.ConfigDirectory'] = os.path.abspath(config['NGINX.ConfigDirectory'])
    config['NGINX.ServerInclude'] = os.path.abspath(config['NGINX.ServerInclude'])

    run_sync(config)
