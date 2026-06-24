# Repository backends

## S3 storage

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
| Scaleway Object Storage        | `https://<bucket>.s3.<region>.scw.cloud`             | Yes                |

> [!NOTE]
> Rekkord may issue ListObjects API calls to reduce blob overwrites on hosts without conditional write support, which may incur additional cost, and requires [broader S3 permissions](protect#fine-grained-s3-bucket-policy).
>
> It is recommended to use a provider **with support for conditional writes**!

You can omit the `SecretKey` value, in which case a prompt will ask you the access key.

Once this is done, use [rekkord init -C config.ini](#initialize-repository) to create the repository.

## SFTP server

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

Once this is done, use [rekkord -C config.ini init](#initialize-repository) to create the repository.

## Local filesystem

To create a repository in a local directory, create an INI file (name it how you want) with the following configuration:

```ini
[Repository]
URL = /path/to/repository
```

Once this is done, use [rekkord -C config.ini init](#initialize-repository) to create the repository.

# Restricted keyfiles

The master key can do everything. It is possible to create restricted keyfiles with limited capabilities; these keyfiles only contain the encryption keys needed to perform the corresponding action, and nothing else.

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

> [!TIP]
> By default, `rekkord derive` tries to open the repository to make sure that the master key matches the repository. This requires a valid config file, and access to repository (with the S3 key and secret, for example).
>
> However, this is not strictly necessary; use the `--offline` argument to derive keyfiles without accessing the repository:
>
> ```sh
> rekkord derive -K master.key -t WriteOnly --offline -O write.key
> ```
