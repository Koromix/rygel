# Vagrant

Execute from the directory with the Vagrantfile (e.g. _vagrant/hds_).

## Initialize VMs

```
vagrant up --no-provision
```

## Deploy

```
vagrant provision
```

# HDS

## Preproduction

The secret ansible-vault key is needed for this. You must __never ever__ distribute it or put it in the repository!

```
ansible-playbook hds.yml -i inventories/hds/preprod.yml -u <USER> --key-file <KEY_FILE> --vault-password-file ansible-hds.vault --ask-become-pass
```

## Production

Ask GPLExpert :)

# OVH

The secret ansible-vault key is needed for this. You must __never ever__ distribute it or put it in the repository!

```
ansible-playbook hds.yml -i inventories/ovh/prod.yml -u <USER> --key-file <KEY_FILE> --vault-password-file ansible-ovh.vault --ask-become-pass
```
