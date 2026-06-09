# Overview

TODO

# Secure S3

## Object locks

TODO

## PUT-only S3 permissions

TODO

# Restrict keyfiles

The master key can do everything. It is possible to create restricted keyfiles with limited capabilities; these keyfiles only contain the encryption keys needed to perform the correspond action, and nothing else.

At the moment, Rekkord supports three kinds of restricted keyfiles:

| Type      | Permissions                            |
|-----------|----------------------------------------|
| ReadWrite | Read and write snapshots               |
| WriteOnly | Create snapshots                       |
| LogOnly   | List snapshots                         |

In addition, each keyfile has its own key pair with which snapshots are signed. These signatures are checked during repository scans.

You must use the master key to create a restricted (or derived) keyfile. Use the `rekkord derive` command to create a restricted keyfile:

```sh
export REKKORD_CONFIG_FILE=/path/to/config.ini

rekkord derive -K master.key -t ReadWrite -O readwrite.key
rekkord derive -K master.key -t WriteOnly -O write.key
```

Most Rekkord commands require you to specify the keyfile, you can do this in one of three ways:

- Set the `REKKORD_KEY_FILE` environment variable (e.g. `export REKKORD_KEY_FILE=write.key`)
- Use the `-K file.key` argument (e.g. `rekkord -K write.key snapshots`)
- Set the *KeyFile* setting in the *Repository* section of the config file (see example below)

```ini
[Repository]
URL = ssh://foo@example.com/backup
KeyFile = write.key

[SFTP]
KeyFile = <SSH keyfile>
Fingerprint = SHA256:<fingerprint>
```
