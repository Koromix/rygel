#!/bin/env python3

import argparse
import configparser
import io
import itertools
import os
import re
import sqlite3
import sys
import subprocess
from dataclasses import dataclass

@dataclass
class InstanceConfig:
    directory = None
    binary = None
    domain = None
    base_url = None
    port = None
    mismatch = False
    inode = None

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

def list_instances(root, domain):
    instances = {}

    for instance in sorted(os.listdir(root)):
        directory = os.path.join(root, instance)
        filename = os.path.join(directory, 'database.db')

        if os.path.isfile(filename):
            info = InstanceConfig()

            path_domain, path_base_url = instance.split('_', 1)
            path_domain = f'{path_domain}.{domain}'
            path_base_url = f'/{path_base_url}/'

            db = sqlite3.connect(filename)
            for k, v in db.execute('SELECT key, value FROM fs_settings;'):
                if k == 'HTTP.BaseUrl':
                    info.base_url = v
                elif k == 'HTTP.Port' and v is not None:
                    info.port = int(v)
            db.close()

            info.binary = os.path.join(directory, 'goupile')
            info.directory = directory
            info.domain = path_domain
            if info.base_url != path_base_url:
                print(f'Assigning BaseUrl "{path_base_url}" to {instance}', file = sys.stderr)
                info.base_url = path_base_url
                info.mismatch = True

            try:
                sb = os.stat(info.binary)
                info.inode = sb.st_ino
            except:
                pass

            instances[instance] = info

    used_ports = {}
    try_port = 9000

    # Fix port conflicts
    for instance, info in instances.items():
        if info.port is not None:
            prev_instance = used_ports.get(info.port)
            if prev_instance is None:
                used_ports[info.port] = instance
            else:
                print(f'Conflict on port {info.port}, used by {prev_instance} and {instance}', file = sys.stderr)
                info.port = None
    for instance, info in instances.items():
        if info.port is None:
            while try_port in used_ports:
                try_port += 1
            print(f'Assigning Port {try_port} to {instance}', file = sys.stderr)
            info.port = try_port
            used_ports[try_port] = instance
            info.mismatch = True

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

def run_service_command(instance, cmd):
    service = f'goupile@{instance}.service'
    print(f'{cmd.capitalize()} {service}', file = sys.stderr)
    subprocess.run(['systemctl', cmd, '--quiet', service])

def update_instance_config(info):
    filename = os.path.join(info.directory, 'database.db')

    db = sqlite3.connect(filename)
    db.executemany('UPDATE fs_settings SET value = ? WHERE key = ?', [
        (info.base_url, 'HTTP.BaseUrl'),
        (info.port, 'HTTP.Port')
    ])
    db.commit()
    db.close()

def update_nginx_config(filename, instances, include = None):
    instances = list(instances.items())
    instances.sort(key = lambda t: t[1].domain)

    with open(filename, 'w') as f:
        for domain, items in itertools.groupby(instances, key = lambda t: t[1].domain):
            print(f'server {{', file = f)
            print(f'    server_name {domain};', file = f)
            if include is not None:
                print(f'    include {include};', file = f)

            for instance, info in items:
                print(f'    location {info.base_url} {{', file = f)
                print(f'        proxy_pass http://127.0.0.1:{info.port};', file = f)
                print(f'    }}', file = f)

            print(f'}}', file = f)

def run_sync(config):
    instances = list_instances(config['Goupile.ProfileDirectory'], config['Goupile.DomainName'])
    services = list_services()

    # Make missing goupile symlinks
    binary = os.path.join(config['Goupile.BinaryDirectory'], 'goupile')
    for instance, info in instances.items():
        if info.inode is None:
            print(f'Link {info.binary} to {binary}', file = sys.stderr)
            os.symlink(binary, info.binary)

    # Update configuration files
    print('Write configuration files', file = sys.stderr)
    for instance, info in instances.items():
        update_instance_config(info)
    update_nginx_config(config['NGINX.ConfigFile'], instances, config.get('NGINX.ServerInclude'))

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
            run_service_command(instance, 'start')
        elif info.mismatch or not status.running or info.inode != status.inode:
            run_service_command(instance, 'restart')

    # Reload NGINX configuration
    print('Reload NGINX server', file = sys.stderr)
    subprocess.run(['systemctl', 'reload', 'nginx.service'])

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description = 'Manage goupile.fr instances')
    parser.add_argument('-C', '--config', dest = 'config', action = 'store',
                        default = os.path.join(os.path.dirname(os.path.realpath(__file__)), 'config.ini'),
                        help = 'Change configuration file')
    parser.add_argument('-b', '--build', dest = 'build', action = 'store_true',
                        default = False, help = 'Build and install goupile binaries')
    parser.add_argument('-s', '--sync', dest = 'sync', action = 'store_true',
                        default = False, help = 'Sync instances, NGINX and systemd')
    args = parser.parse_args()

    if not args.build and not args.sync:
        raise ValueError('Call with --sync and/or --build')

    config = load_config(args.config)

    if args.build:
        run_build(config)
    if args.sync:
        run_sync(config)
