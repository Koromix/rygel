# Create repository

> [!CAUTION]
> This software has not been stabilized yet and **must not be used as your primary backup** tool.
> You've been warned!

## Create configuration

Rekkord provides a step-by-step interactive command that will help you configure a repository.

Run the following command to get started:

```sh
rekkord setup
```

This command only creates the configuration but it **does not initialize the repository**. Follow the next section to continue.

> [!NOTE]
> You can also [configure Rekkord manually](#manual-configuration) if you prefer.

## Initialize repository

Once you have configured your repository, through a configuration file or with environment variables, you can initialize it with the following command:

```sh
rekkord init
```

> [!NOTE]
> rekkord looks for `~/.config/rekkord/rekkord.ini` or `/etc/rekkord/rekkord.ini` by default.
>
> Set `REKKORD_CUSTOM_FILE` to a different path if your config file lives somewhere else.
>
> ```sh
> export REKKORD_CONFIG_FILE=/path/to/config.ini
> rekkord init
> ```
>
> You can also use the `-C filename` option to use a custom config file path.
>
> ```sh
> rekkord -C /path/to/config.ini init
> ```

This command will initialize the repository with a random 256-bit master key.

After initialization, the **master key** is exported to a binary file, named `master.key` by default (in the current directory). You can change the export file with the `--key_file <file>` option.

> [!IMPORTANT]
> You should **store this master key in a secure place**. It cannot be recreated if you lose the file. If it leaks, everyone will be able to decrypt or edit your snapshots!

Rekkord uses **multiple encryption keys** which are derived from this master key:

- The *config key (ckey)* is paired with an *access key (akey)* to sign config files and user keyfiles
- The *data key (dkey)* is paired with a *write key (wkey)* for data encryption (snapshot information, directory metadata, file content)
- The *log key (lkey)* is paired with a *tag key (tkey)*, to manage snapshots and record snapshot information

For simple use cases, you can simply use the master key for everything. However, we recommend that you create [restricted keyfiles](#restricted-keys), for two reasons:

- Each restricted keyfile can have limited capabilities: *ReadWrite, WriteOnly or LogOnly*
- Snapshots are signed with each keyfile-specific signing key, this can be used to detect cross-server tampering during repository checks

# Advanced topics

## Manual configuration

### S3 storage

To create a repository stored on an S3-compatible server, create an INI file (name it how you want) with the following configuration:

```ini
[Repository]
URL = https://<endpoint>/<bucket>

[S3]
AccessKeyID = <AWS access key ID>
SecretKey = <AWS secret key>
```

To help you form a valid URL, here are a few examples for common S3 providers:

| Provider                       | URL                                                  | Conditional writes |
|--------------------------------|------------------------------------------------------|--------------------|
| Amazon S3 (virtual-host style) | `https://<bucket>.s3.<region>.amazonaws.com`         | Yes                |
| Amazon S3 (path style)         | `https://s3.<region>.amazonaws.com/<bucket>`         | Yes                |
| Backblaze B2                   | `https://s3.<region>.backblazeb2.com/<bucket>`       | No                 |
| Cloudflare R2                  | `https://<userid>.r2.cloudflarestorage.com/<bucket>` | Yes                |
| Exoscale Object Storage        | `https://sos-<region>.exo.io/<bucket>`               | Yes                |
| Scaleway Object Storage        | `https://<bucket>.s3.<region>.scw.cloud`             | No                 |

> [!NOTE]
> Rekkord may issue ListObjects API calls to reduce blob overwrites on hosts without conditional write support, which may incur additional cost.
>
> It is recommended to use a provider **with support for conditional writes**!

You can omit the `SecretKey` value, in which case a prompt will ask you the access key.

Once this is done, use [rekkord init](#initialize-repository) to create the repository.

### SFTP server

To create a repository stored on an SFTP host, create an INI file (name it how you want) with the following configuration:

```ini
[Repository]
URL = <user>@<host>:/path/to/repository

[SFTP]
Password = <SSH password>
KeyFile = <SSH keyfile>
```

Use either a password or a key file. If you omit both, a prompt will ask you for a password.

Just like with `ssh`, you will need to validate the host fingerprint the first time you connect to it. However, you can instead directly provide the correct host fingerprint as the `Fingerprint` to avoid this, as shown in the example below:

```ini
[Repository]
URL = foo@example.com:/backup

[SFTP]
KeyFile = /home/bar/.ssh/id_ed25519
Fingerprint = SHA256:Y9pmJfkaok8t0dFJrfi8/LLUNhOYwAZGHUNGsYAiJUM
```

Once this is done, use [rekkord init](#initialize-repository) to create the repository.

### Local filesystem

To create a repository in a local directory, create an INI file (name it how you want) with the following configuration:

```ini
[Repository]
URL = /path/to/repository
```

Once this is done, use [rekkord init](#initialize-repository) to create the repository.

## Restricted keys

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

Most Rekkord commands require you to specify the keyfile, you can do this in one of two ways:

- Set the `REKKORD_KEYFILE` environment variable (e.g. `export REKKORD_KEYFILE=write.key`)
- Set the *KeyFile* setting in the *Repository* section of the config file (see example below)

```ini
[Repository]
URL = ssh://foo@example.com/backup
KeyFile = write.key

[SFTP]
KeyFile = <SSH keyfile>
Fingerprint = SHA256:<fingerprint>
```
