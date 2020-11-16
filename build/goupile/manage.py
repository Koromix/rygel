#!/bin/env python3

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import configparser
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
    domain = None
    port = None
    mismatch = False

@dataclass
class ServiceStatus:
    running = False
    inode = None

def load_config(filename):
    config = {}

    ini = configparser.ConfigParser()
    ini.optionxform = str
    with open(filename, 'r') as f:
        ini.read_file(f)

    for section in ini.sections():
        for key, value in ini.items(section):
            name = f'{section}.{key}'
            config[name] = value

    return config

def run_build(config):
    print('Build goupile and goupile_admin', file = sys.stderr)

    build_filename = os.path.join(config['Goupile.SourceDirectory'], 'FelixBuild.ini')
    subprocess.run(['felix', '-mFast', '-q', '-C', build_filename,
                    '-O', config['Goupile.BinaryDirectory'], 'goupile', 'goupile_admin'])

def list_domains(root):
    domains = {}

    for domain in sorted(os.listdir(root)):
        directory = os.path.join(root, domain)
        filename = os.path.join(directory, 'goupile.ini')

        if os.path.isfile(filename):
            config = load_config(filename)

            info = DomainConfig()
            info.directory = directory
            info.domain = domain
            info.port = config.get('HTTP.Port')

            domains[domain] = info

    used_ports = {}
    try_port = 9000

    # Fix port conflicts
    for domain, info in domains.items():
        if info.port is not None:
            prev_domain = used_ports.get(info.port)
            if prev_domain is None:
                used_ports[info.port] = domain
            else:
                print(f'Conflict on port {info.port}, used by {prev_domain} and {domain}', file = sys.stderr)
                info.port = None
    for domain, info in domains.items():
        if info.port is None:
            while try_port in used_ports:
                try_port += 1
            print(f'Assigning Port {try_port} to {domain}', file = sys.stderr)
            info.port = try_port
            used_ports[try_port] = domain
            info.mismatch = True

    return domains

def list_services():
    services = {}

    output = subprocess.check_output(['systemctl', 'list-units', '--type=service', '--all'])
    output = output.decode()

    for line in output.splitlines():
        parts = re.split(' +', line)

        if len(parts) >= 4:
            match = re.search('^goupile@([0-9A-Za-z_\\-]+)\\.service$', parts[1])

            if match is not None:
                name = match.group(1)

                status = ServiceStatus()
                status.running = (parts[3] == 'active')

                if status.running:
                    try:
                        pid = int(subprocess.check_output(['systemctl', 'show', '-p', 'ExecMainPID', '--value', parts[1]]))

                        sb = os.stat(f'/proc/{pid}/exe')
                        status.inode = sb.st_ino
                    except:
                        status.running = False

                services[name] = status

    return services

def run_service_command(domain, cmd):
    service = f'goupile@{domain}.service'
    print(f'{cmd.capitalize()} {service}', file = sys.stderr)
    subprocess.run(['systemctl', cmd, '--quiet', service])

def update_domain_config(info):
    filename = os.path.join(info.directory, 'goupile.ini')
    ini = parse_ini(filename)

    if not ini.has_section('HTTP'):
        ini.add_section('HTTP')
    ini.set('HTTP', 'Port', str(info.port))

    with open(filename, 'w') as f:
        ini.write(f)

def update_nginx_config(filename, domains, include = None):
    with open(filename, 'w') as f:
        for domain, info in domains.items():
            print(f'server {{', file = f)
            print(f'    server_name {domain};', file = f)
            if include is not None:
                print(f'    include {include};', file = f)
            print(f'    location / {{')
            print(f'        proxy_pass http://127.0.0.1:{info.port};', file = f)
            print(f'    }}', file = f)
            print(f'}}', file = f)

def run_sync(config):
    domains = list_domains(config['Goupile.DomainDirectory'])
    services = list_services()

    # Detect binary mismatches
    binary = os.path.join(config['Goupile.BinaryDirectory'], 'goupile')
    binary_inode = os.stat(binary).st_ino
    for domain, info in domains.items():
        status = services.get(domain)
        if status is not None and status.running and status.inode != binary_inode:
            print(f'Domain {domain} is running old version')
            info.mismatch = True

    # Update configuration files
    print('Write configuration files', file = sys.stderr)
    for domain, info in domains.items():
        update_domain_config(info)
    update_nginx_config(config['NGINX.ConfigFile'], domains, config.get('NGINX.ServerInclude'))

    # Sync systemd services
    for domain in services:
        info = domains.get(domain)
        if info is None:
            run_service_command(domain, 'stop')
            run_service_command(domain, 'disable')
    for domain, info in domains.items():
        status = services.get(domain)
        if status is None:
            run_service_command(domain, 'enable')
            run_service_command(domain, 'start')
        elif info.mismatch or not status.running:
            run_service_command(domain, 'restart')

    # Reload NGINX configuration
    print('Reload NGINX server', file = sys.stderr)
    subprocess.run(['systemctl', 'reload', 'nginx.service'])

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description = 'Manage goupile.fr domains')
    parser.add_argument('-C', '--config', dest = 'config', action = 'store',
                        default = os.path.join(os.path.dirname(os.path.realpath(__file__)), 'config.ini'),
                        help = 'Change configuration file')
    parser.add_argument('-b', '--build', dest = 'build', action = 'store_true',
                        default = False, help = 'Build and install goupile binaries')
    parser.add_argument('-s', '--sync', dest = 'sync', action = 'store_true',
                        default = False, help = 'Sync domains, NGINX and systemd')
    args = parser.parse_args()

    if not args.build and not args.sync:
        raise ValueError('Call with --sync and/or --build')

    config = load_config(args.config)

    if args.build:
        run_build(config)
    if args.sync:
        run_sync(config)
