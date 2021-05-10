# Vagrant environment

## Initialize VMs

```
vagrant up --no-provision
```

## Deploy

### Through vagrant helper

```
vagrant provision
```

### Directly

```
ansible-playbook -i inventories/vagrant.yml HDS.yml
```

# Preproduction

The secret ansible-vault key is needed for this. You must __never ever__ distribute it or put it in the repository!

```
ansible-playbook -i inventories/preprod.yml HDS.yml --vault-password-file FICHIER_CLE_VAULT
```
