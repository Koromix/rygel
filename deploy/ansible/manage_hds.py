#!/usr/bin/env python3

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
# along with this program. If not, see <https://www.gnu.org/licenses/>.

# pip install pynacl ruamel.yaml ansible-vault

import os
from nacl import public as nacl
import base64
from ruamel.yaml import YAML
from ruamel.yaml.scalarstring import LiteralScalarString
from ansible_vault import Vault
import argparse

class VaultTag:
    yaml_tag = '!vault'

    def __init__(self, encrypted):
        self.encrypted = encrypted

    @classmethod
    def to_yaml(cls, representer, node):
        return representer.represent_scalar(cls.yaml_tag, node.encrypted, style = '|')

    @classmethod
    def from_yaml(cls, constructor, node):
        return cls(node.value)

def add_goupile(path, vault, domain):
    nginx, goupile = load_inventories(path)

    if any(it['name'] == domain for it in nginx['nginx_domains']) or \
       any(it['name'] == domain for it in goupile['goupile_domains']):
        raise NameError(f'Domain "{domain}" already exists in this inventory')

    for it in goupile['goupile_domains']:
        if vault is not None:
            try:
                vault.load(it['archive_key'].encrypted)
            except:
                raise ValueError('This vault password does not match the one used for previous domains') from None
        else:
            if not isinstance(it['archive_key'], str):
                raise ValueError('This inventory contains encrypted archive keys, but no vault password was passed')

    skey = nacl.PrivateKey.generate()
    pkey = base64.b64encode(bytes(skey.public_key)).decode()
    skey = base64.b64encode(bytes(skey)).decode()

    nginx['nginx_domains'].append({
        'name': domain,
        'config': LiteralScalarString(
            'location / {\n' +
            '    proxy_http_version 1.1;\n' +
            '    proxy_request_buffering off;\n' +
            '    proxy_buffering on;\n' +
            '    proxy_read_timeout 180;\n' +
            '    send_timeout 180;\n' +
            '\n' +
            '    proxy_set_header Host $host;\n' +
            '    proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;\n' +
            '\n' +
            '    proxy_pass http://unix:/run/goupile/' + domain + '.sock:;\n' +
            '}\n'
        ),
        'ssl_certbot_email': 'interhop@riseup.net'
    })
    goupile['goupile_domains'].append({
        'name': domain,
        'archive_key': VaultTag(vault.dump_raw(pkey))
    })

    save_inventories(path, nginx, goupile)

    print(f'Added domain {domain}\n')
    print(f'Secret archive decryption key: {skey}')
    print(f'                   public key: {pkey}')
    print(f'This key cannot be retrieved, you must save it now or encrypted goupile archives will be lost!')

def remove_goupile(path, domain):
    nginx, goupile = load_inventories(path)

    if not any(it['name'] == domain for it in nginx['nginx_domains']) and \
       not any(it['name'] == domain for it in goupile['goupile_domains']):
        raise NameError(f'Domain "{domain}" does not exist in this inventory')

    nginx['nginx_domains'] = list(filter(lambda it: it['name'] != domain, nginx['nginx_domains']))
    goupile['goupile_domains'] = list(filter(lambda it: it['name'] != domain, goupile['goupile_domains']))

    save_inventories(path, nginx, goupile)

    print(f'Removed domain {domain}\n')

def load_inventories(path):
    yaml = YAML()
    yaml.register_class(VaultTag)

    with open(path + '/group_vars/www/nginx.yml', 'r') as f:
        nginx = yaml.load(f)
    with open(path + '/group_vars/server/goupile.yml', 'r') as f:
        goupile = yaml.load(f)

    return nginx, goupile

def save_inventories(path, nginx, goupile):
    yaml = YAML()
    yaml.register_class(VaultTag)
    yaml.indent(sequence = 4, offset = 2)

    with open(path + '/group_vars/www/nginx.yml', 'w') as f:
        yaml.dump(nginx, f)
    with open(path + '/group_vars/server/goupile.yml', 'w') as f:
        yaml.dump(goupile, f)

if __name__ == "__main__":
    parser = argparse.ArgumentParser()

    subparsers = parser.add_subparsers(title = 'Commands', dest = 'command')
    parser.add_argument('-e', '--environment', type = str, help = 'change environment (defaults to prod)',
                        default = 'prod', choices = ['prod', 'preprod'])

    sub = subparsers.add_parser('add_goupile', help = 'add goupile domain')
    sub.add_argument('domain', type = str, help = 'domain name')
    sub.add_argument('-v', '--vault', type = str, help = 'set vault password file')
    sub = subparsers.add_parser('remove_goupile', help = 'remove goupile domain')
    sub.add_argument('domain', type = str, help = 'domain name')

    args = parser.parse_args()

    script_dir = os.path.dirname(os.path.abspath(__file__))
    path = os.path.join(script_dir, 'inventories/hds/' + args.environment)

    if args.command is None:
        parser.print_help()
    elif args.command == 'add_goupile':
        if args.vault is not None:
            with open(args.vault, 'r') as f:
                password = f.read()
                vault = Vault(password)
        else:
            vault = None

        add_goupile(path, vault, args.domain)
    elif args.command == 'remove_goupile':
        remove_goupile(path, args.domain)
