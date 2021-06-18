# Vagrant

Execute from the directory with the relevant Vagrantfile (e.g. _vagrant/pknet_ for PKnet).

## Initialize VMs

```sh
vagrant up --no-provision
```

## Deploy

```sh
vagrant provision
```

# HDS

## Preproduction

The secret ansible-vault key is needed for this. You must __never ever__ distribute it or put it in the repository!

```sh
ansible-playbook hds.yml -i inventories/hds/preprod.yml -u <USER> --key-file <KEY_FILE> --vault-password-file ansible-hds.vault --ask-become-pass
```

## Production

Ask GPLExpert :)

# PK network

The secret ansible-vault key is needed for this. You must __never ever__ distribute it or put it in the repository!

```sh
ansible-playbook hds.yml -i inventories/pknet/prod.yml -u <USER> --key-file <KEY_FILE> --vault-password-file ansible-pknet.vault --ask-become-pass
```
