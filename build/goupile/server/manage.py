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

def run_build(config):
    felix_binary = os.path.join(config['Build.SourceDirectory'], 'felix')

    # Update source
    print('Update source', file = sys.stderr)
    if not os.path.exists(config['Build.SourceDirectory']):
        subprocess.run(['sudo', '-u', config['Build.SudoUser'],
                        'git', 'clone', config['Build.SourceRepository'], config['Build.SourceDirectory']])
    subprocess.run(['sudo', '-u', config['Build.SudoUser'],
                    'git', '-C', config['Build.SourceDirectory'], 'pull'])
    subprocess.run(['sudo', '-u', config['Build.SudoUser'],
                    'git', '-C', config['Build.SourceDirectory'], 'checkout', config['Build.SourceCommit']])

    # Build felix if needed
    if not os.path.exists(felix_binary):
        print('Bootstrap felix')
        build_felix = os.path.join(config['Build.SourceDirectory'], 'build/felix/build_posix.sh')
        subprocess.run(['sudo', '-u', config['Build.SudoUser'], build_felix])

    # Build goupile
    print('Build goupile')
    build_filename = os.path.join(config['Build.SourceDirectory'], 'FelixBuild.ini')
    subprocess.run(['sudo', '-u', config['Build.SudoUser'],
                    felix_binary, '-mFast', '-q', '-C', build_filename,
                    '-O', config['Build.BuildDirectory'], 'goupile'])

    # Install goupile
    src_binary = os.path.join(config['Build.BuildDirectory'], 'goupile')
    subprocess.run(['install', src_binary, config['Build.BinaryDirectory'] + '/'])

def list_domains(root_dir):
    domains = {}

    for domain in sorted(os.listdir(root_dir)):
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

def create_domain(binary, root_dir, domain, owner_user):
    directory = os.path.join(root_dir, domain)
    print(f'Create domain {domain} ({directory})', file = sys.stderr)
    subprocess.run([binary, 'init', '-o', owner_user, directory])

def migrate_domain(domain, info):
    print(f'Migrate domain {domain} ({info.directory})', file = sys.stderr)
    filename = os.path.join(info.directory, 'goupile.ini')
    subprocess.run([info.binary, 'migrate', '-C', filename])

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
                    except:
                        status.running = False

                services[name] = status

    return services

def run_service_command(domain, cmd):
    service = f'goupile@{domain}.service'
    print(f'{cmd.capitalize()} {service}', file = sys.stderr)
    subprocess.run(['systemctl', cmd, '--quiet', service])

def update_systemd_unit(run_user):
    SYSTEMD_SERVICE = f'''\
[Service]
Type=simple

User={run_user}

RuntimeDirectory=goupile
ExecStart=/srv/www/goupile/domains/%i/goupile
WorkingDirectory=/srv/www/goupile/domains/%i

Restart=on-failure
TimeoutStopSec=30

[Install]
WantedBy=multi-user.target
'''

    if not os.path.exists('/etc/systemd/system/goupile@.service'):
        with open('/etc/systemd/system/goupile@.service', 'w') as f:
            f.write(SYSTEMD_SERVICE)

def update_domain_config(info):
    filename = os.path.join(info.directory, 'goupile.ini')
    ini = parse_ini(filename)

    if not ini.has_section('HTTP'):
        ini.add_section('HTTP')
    ini.set('HTTP', 'SocketType', 'Unix')
    ini.set('HTTP', 'UnixPath', info.socket)
    ini.remove_option('HTTP', 'Port')

    with open(filename, 'w') as f:
        ini.write(f)

def update_nginx_config(directory, domain, socket, include = None):
    filename = os.path.join(directory, f'{domain}.conf')

    compat_dir = os.path.join(directory, 'compat.d')
    custom_dir = os.path.join(directory, 'custom.d')

    with open(filename, 'w') as f:
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

def run_sync(config):
    default_binary = os.path.join(config['Build.BinaryDirectory'], 'goupile')

    # List existing domains and services
    domains = list_domains(config['Goupile.DomainDirectory'])
    services = list_services()

    # Create missing domains
    for domain in config['Domains']:
        info = domains.get(domain)
        if info is None:
            create_domain(default_binary, config['Goupile.DomainDirectory'], domain, config['Goupile.RunUser'])

    # Detect binary mismatches
    for domain in config['Domains']:
        info = domains.get(domain)
        if not os.path.exists(info.binary):
            os.symlink(default_binary, info.binary)
        binary_inode = os.stat(info.binary).st_ino
        status = services.get(domain)
        if status is not None and status.running and status.inode != binary_inode:
            print(f'Domain {domain} is running old version')
            info.mismatch = True

    # Update instance configuration files
    print('Write configuration files', file = sys.stderr)
    for domain, info in config['Domains']:
        info = domains.get(domain)
        update_domain_config(info)

    # Update NGINX configuration files
    print('Write NGINX configuration files', file = sys.stderr)
    for name in os.listdir(config['NGINX.ConfigDirectory']):
        if name.endswith('.conf'):
            filename = os.path.join(config['NGINX.ConfigDirectory'], name)
            os.unlink(filename)
    for domain in config['Domains']:
        info = domains.get(domain)
        update_nginx_config(config['NGINX.ConfigDirectory'], domain, info.socket,
                            include = config.get('NGINX.ServerInclude'))

    # Sync systemd services
    update_systemd_unit(config['Goupile.RunUser'])
    for domain in services:
        info = domains.get(domain)
        if info is None:
            run_service_command(domain, 'stop')
            run_service_command(domain, 'disable')
    for domain in config['Domains']:
        info = domains.get(domain)
        status = services.get(domain)
        if status is None:
            run_service_command(domain, 'enable')
        if status is None or info.mismatch or not status.running:
            run_service_command(domain, 'stop')
            migrate_domain(domain, info)
            run_service_command(domain, 'start')

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

    # Always work from manage.py directory
    directory = os.path.dirname(os.path.abspath(__file__))
    os.chdir(directory)

    if not args.build and not args.sync:
        raise ValueError('Call with --sync and/or --build')

    config = load_config(args.config, array_sections = ['Domains'])

    if args.build:
        run_build(config)
    if args.sync:
        run_sync(config)
