# Manage HDS goupile domains

Use the script `manage_hds.py` to add and remove goupile subdomains.

You may need to install several Python dependencies before it works:

```sh
pip install pynacl ruamel.yaml ansible-vault
```

The secret ansible-vault key is needed for this. You must __never ever__ distribute it or put it in the repository!

## Add domain

```sh
./manage_hds.py add_goupile -v /path/to/ansible-hds.vault XXXXX.goupile.fr
```

The script will give you back the secret archive decryption key. You __must save__ it or goupile archives
created through the administration panel cannot be restored, and this key cannot be retrieved later on.

## Remove domain

```sh
./manage_hds.py remove_goupile XXXXX.goupile.fr
```

# Deploying with ansible

## HDS

### Preproduction

The secret ansible-vault key is needed for this. You must __never ever__ distribute it or put it in the repository!

```sh
ansible-playbook hds.yml -i inventories/hds/preprod/hosts.yml -u <USER> --key-file <KEY_FILE> --vault-password-file /path/to/ansible-hds.vault --ask-become-pass
```

### Production

Ask GPLExpert :)

## Vagrant

Execute from the directory with the relevant Vagrantfile (e.g. _vagrant/pknet_ for PKnet).

### Initialize VMs

```sh
vagrant up --no-provision
```

### Deploy

```sh
vagrant provision
```

## PK network

The secret ansible-vault key is needed for this. You must __never ever__ distribute it or put it in the repository!

```sh
ansible-playbook pknet.yml -i inventories/pknet/prod/hosts.yml -u <USER> --key-file <KEY_FILE> --vault-password-file /path/to/ansible-pknet.vault --ask-become-pass
```

## Add a new host/VPS

```sh
ansible-playbook playbook.yml -i inventories/pknet/prod -e ansible_user=debian -e ansible_password=PASSWORD -l HOST --tags=base pknet.yml
```
