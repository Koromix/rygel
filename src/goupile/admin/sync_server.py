#!/bin/env python3

import configparser
import io
import os
import re
import sys
import subprocess
from dataclasses import dataclass

PROFILES_DIRECTORY = '/srv/www/goupile/profiles'
NGINX_FILE = '/srv/www/goupile/nginx_goupile.conf'
GOUPILE_BINARY = '/srv/www/goupile/goupile'
DOMAIN_NAME = 'goupile.fr'

@dataclass
class InstanceConfig:
    directory = None

    domain = None
    base_url = None
    port = None

    mismatch = False

def list_instances():
    instances = {}

    used_ports = {}
    try_port = 9000

    for instance in os.listdir(PROFILES_DIRECTORY):
        directory = os.path.join(PROFILES_DIRECTORY, instance)
        filename = os.path.join(directory, 'goupile.ini')

        if os.path.isfile(filename):
            config = configparser.ConfigParser()
            config.optionxform = str
            config.read(filename)

            info = InstanceConfig()
            info.directory = directory
            info.domain, info.base_url = instance.split('_', 1)
            info.port = config.getint('HTTP', 'Port', fallback = None)

            info.domain = f'{info.domain}.{DOMAIN_NAME}'
            info.base_url = f'/{info.base_url}/'

            # Fix BaseUrl setting if needed
            if config.get('HTTP', 'BaseUrl', fallback = None) != info.base_url:
                print(f'Assigning BaseUrl "{info.base_url}" to {instance}', file = sys.stderr)
                info.mismatch = True

            # Fix Port setting if needed
            if info.port is not None:
                prev_instance = used_ports.get(info.port)
                if prev_instance is None:
                    used_ports[info.port] = instance
                else:
                    print(f'Conflict on port {info.port}, used by {prev_instance} and {instance}', file = sys.stderr)
                    info.port = None
            if info.port is None:
                while try_port in used_ports:
                    try_port += 1
                print(f'Assigning Port {try_port} to {instance}', file = sys.stderr)
                info.port = try_port
                info.mismatch = True

            instances[instance] = info

    return instances

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
                services[name] = (parts[3] == 'active')

    return services

def run_service_command(instance, cmd):
    service = f'goupile@{instance}.service'
    print(f'{cmd.capitalize()} {service}', file = sys.stderr)
    subprocess.run(['systemctl', cmd, '--quiet', service])

def update_instance_config(info):
    filename = os.path.join(info.directory, 'goupile.ini')

    config = configparser.ConfigParser()
    config.optionxform = str
    config.read(filename)

    if not config.has_section('HTTP'):
        config.add_section('HTTP')

    config.set('HTTP', 'BaseUrl', info.base_url)
    config.set('HTTP', 'Port', str(info.port))

    with open(filename, 'w') as f:
        config.write(f)

def write_nginx_entry(f, info):
    print(f'location {info.base_url} {{', file = f)
    print(f'    proxy_pass http://127.0.0.1:{info.port};', file = f)
    print(f'}}', file = f)

if __name__ == '__main__':
    instances = list_instances()
    services = list_services()

    # Make missing goupile symlinks
    for instance, info in instances.items():
        symlink = os.path.join(info.directory, 'goupile')
        if not os.path.exists(symlink):
            print(f'Link {symlink} to {GOUPILE_BINARY}', file = sys.stderr)
            os.symlink(GOUPILE_BINARY, symlink)

    # Update configuration files
    print('Write configuration files', file = sys.stderr)
    for instance, info in instances.items():
        update_instance_config(info)
    with open(NGINX_FILE, 'w') as f:
        for instance, info in instances.items():
            write_nginx_entry(f, info)

    # Sync systemd services
    for instance in services:
        info = instances.get(instance)
        if info is None:
            run_service_command(instance, 'stop')
            run_service_command(instance, 'disable')
    for instance, info in instances.items():
        status = services.get(instance)
        if status is None:
            run_service_command(instance, 'enable')
        if info.mismatch or not status:
            run_service_command(instance, 'restart')

    # Reload NGINX configuration
    print('Reload NGINX server', file = sys.stderr)
    subprocess.run(['systemctl', 'reload', 'nginx.service'])
