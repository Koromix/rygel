#!/bin/env python3

import configparser
import io
import os
import re
import sys
import subprocess

PROFILES_DIRECTORY = '/srv/www/goupile/profiles'
NGINX_FILE = '/srv/www/goupile/nginx_goupile.conf'
GOUPILE_BINARY = '/srv/www/goupile/goupile'

def list_fs_instances():
    instances = {}

    for name in os.listdir(PROFILES_DIRECTORY):
        directory = os.path.join(PROFILES_DIRECTORY, name)
        filename = os.path.join(directory, 'goupile.ini')

        if os.path.isfile(filename):
            config = configparser.ConfigParser()
            config.optionxform = str
            config.read(filename)

            info = {
                'directory': directory,
                'base_url': config.get('HTTP', 'BaseUrl'),
                'port': config.getint('HTTP', 'Port')
            }
            instances[name] = info

    return instances

def list_systemd_instances():
    instances = {}

    output = subprocess.check_output(['systemctl', 'list-units', '--type=service', '--all'])
    output = output.decode()

    for line in output.splitlines():
        parts = re.split(' +', line)

        if len(parts) >= 4:
            match = re.search('^goupile@([0-9A-Za-z_\\-]+)\\.service$', parts[1])

            if match is not None:
                name = match.group(1)
                instances[name] = (parts[3] == 'active')

    return instances

def run_systemd_instance(instance, cmd):
    service = f'goupile@{instance}.service'
    print(f'{cmd.capitalize()} {service}', file = sys.stderr)
    subprocess.run(['systemctl', cmd, '--quiet', service])

def write_nginx_entry(f, info):
    print(f'location {info["base_url"]} {{', file = f)
    print(f'    proxy_pass http://127.0.0.1:{info["port"]};', file = f)
    print(f'}}', file = f)

if __name__ == '__main__':
    fs_instances = list_fs_instances()
    systemd_instances = list_systemd_instances()

    # Sanity checks
    ports = {}
    for instance, info in fs_instances.items():
        prev_instance = ports.get(info['port'])
        if prev_instance is not None:
            raise ValueError(f'Port {info["port"]} is used by {prev_instance} and {instance}')
        ports[info['port']] = instance

    # Make missing goupile symlinks
    for instance, info in fs_instances.items():
        symlink = os.path.join(info['directory'], 'goupile')
        if not os.path.exists(symlink):
            print(f'Link {symlink} to {GOUPILE_BINARY}', file = sys.stderr)
            os.symlink(GOUPILE_BINARY, symlink)

    # Update NGINX locations
    print('Update NGINX configuration', file = sys.stderr)
    with open(NGINX_FILE, 'w') as f:
        for instance, info in fs_instances.items():
            write_nginx_entry(f, info)

    # Sync systemd services
    for instance in systemd_instances:
        info = fs_instances.get(instance)
        if info is None:
            run_systemd_instance(instance, 'stop')
            run_systemd_instance(instance, 'disable')
    for instance in fs_instances:
        status = systemd_instances.get(instance)
        if status is None:
            run_systemd_instance(instance, 'enable')
        if not status:
            run_systemd_instance(instance, 'start')

    # Reload NGINX configuration
    print('Reload NGINX server', file = sys.stderr)
    subprocess.run(['systemctl', 'reload', 'nginx.service'])
