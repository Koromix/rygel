# Vagrant

## Initialize VMs

```
vagrant up --no-provision
```

## Deploy

```
vagrant provision
```

# Preproduction

The secret ansible-vault key is needed for this. You must __never ever__ distribute it or put it in the repository!

```
ansible-playbook hds.yml -i inventories/preprod.yml -u <USER> --key-file <KEY_FILE> --vault-password-file <VAULT_FILE> --ask-become-pass
```

# Production

Ask GPLExpert :)
