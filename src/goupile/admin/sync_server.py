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

@dataclass
class InstanceConfig:
    directory = None
    base_url = None
    port = None

def list_instances():
    instances = {}

    for name in os.listdir(PROFILES_DIRECTORY):
        directory = os.path.join(PROFILES_DIRECTORY, name)
        filename = os.path.join(directory, 'goupile.ini')

        if os.path.isfile(filename):
            config = configparser.ConfigParser()
            config.optionxform = str
            config.read(filename)

            info = InstanceConfig()
            info.directory = directory
            info.base_url = config.get('HTTP', 'BaseUrl', fallback = None)
            info.port = config.getint('HTTP', 'Port', fallback = None)

            instances[name] = info

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

    # Adjust instance ports
    next_port = 9000
    used_ports = {}
    for instance, info in instances.items():
        if info.port is not None:
            prev_instance = used_ports.get(info.port)
            next_port = max(next_port, info.port)
            if prev_instance is None:
                used_ports[info.port] = instance
            else:
                info.port = None
    for instance, info in instances.items():
        if info.port is None:
            info.port = next_port
            next_port = next_port + 1

    # Adjust instance URLs
    for instance, info in instances.items():
        parts = instance.split('_')
        info.base_url = parts[1]

    # Update configuration files
    print('Update configuration files', file = sys.stderr)
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
    for instance in instances:
        status = services.get(instance)
        if status is None:
            run_service_command(instance, 'enable')
        if not status:
            run_service_command(instance, 'restart')

    # Reload NGINX configuration
    print('Reload NGINX server', file = sys.stderr)
    subprocess.run(['systemctl', 'reload', 'nginx.service'])
