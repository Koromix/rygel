# Configure repository

> [!CAUTION]
> This software has not been stabilized yet and **must not be used as your primary backup** tool.
> You've been warned!

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
| Scaleway Object Storage        | `https://<bucket>.s3.<region>.scw.cloud`             | No                 |

> [!NOTE]
> Rekkord tries to use conditional PutObject operations (`If-None-Match=*` header) which is not supported by all providers.
>
> Some will ignore the header (such as Scaleway Object Storage), some will refuse to run (such as Backblaze B2). In the latter case, set `ConditionalWrites = No` as shown below:
>
> ```ini
> # ...
>
> [S3]
> KeyID = <AWS access key ID>
> SecretKey = <AWS secret key>
> ConditionalWrites = No
> ```
>
> However, it is better to use a provider with conditional write support.

You can omit the `SecretKey` value, in which case a prompt will ask you the access key.

Once this is done, use [rekkord init](#initialize-repository) to create the repository.

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

Once this is done, use [rekkord init](#initialize-repository) to create the repository.

## Local filesystem

To create a repository in a local directory, create an INI file (name it how you want) with the following configuration:

```ini
[Repository]
URL = /path/to/repository
```

Once this is done, use [rekkord init](#initialize-repository) to create the repository.

# Initialize repository

Once you have configured your repository, through a configuration file or with environment variables, you can initialize it with the following command:

```sh
export REKKORD_CONFIG_FILE=/path/to/config.ini
rekkord init

# Alternative: rekkord init -C /path/to/config.ini
```

This command will initialize the repository with a random 256-bit master key.

After initialization, the **master key** is exported to a binary file, named `master.key` by default (in the current directory). You can change the export file with the `--key_file <file>` option.

> [!IMPORTANT]
> You should **store this master key in a secure place**. It cannot be recreated if you lose the file. If it leaks, everyone will be able to decrypt or edit your snapshots!

Rekkord uses **multiple encryption keys** which are derived from this master key:

- The *config key (ckey)* is paired with an *access key (akey)* to sign config files and user key files
- The *data key (dkey)* is paired with a *write key (wkey)* for data encryption (snapshot information, directory metadata, file content)
- The *log key (lkey)* is paired with a *tag key (tkey)*, to manage snapshots and record snapshot information

For simple use cases, you can simply use the master key for everything. However, we recommend that you create separate users, for two reasons:

- Each user can have a restricted role: *Admin, ReadWrite, WriteOnly or LogOnly*
- Snapshots are signed with each user-specific signing key, this can be used to detect cross-server tampering during repository checks

# Repository users

User roles are used to restrict the set of possible actions each user can do, as shown in the table below.

| Role      | Permissions                            |
|-----------|----------------------------------------|
| Admin     | Manage users, read and write snapshots |
| ReadWrite | Read and write snapshots               |
| WriteOnly | Create snapshots                       |
| LogOnly   | List snapshots                         |

In addition, each user has its own key pair with which snapshots are signed.

> [!WARNING]
> Repository users contain the necessary encryption keys for a given role, protected by a password.
>
> These have **nothing to do with the SSH login name or the S3 access keys** (or any other backend that may appear one day), which you have to manage yourself!

Use the following commands to manage repository users:

- `rekkord add_user name -r role`
- `rekkord delete_user name`
- `rekkord list_users`

To create a new user, you must either use the master key file (created by `rekkord init`) or use an existing user with the *Admin* role.

```sh
export REKKORD_CONFIG_FILE=/path/to/config.ini

# Use the master key file
rekkord add_user -K master.key john -r ReadWrite

# Or use existing user to create new user
export REKKORD_USER=admin
rekkord add_user joe -r ReadWrite
```

Most Rekkord commands require you to specify the user, you can do this in one of two ways:

- Set the `REKKORD_USER` environment variable (e.g. `export REKKORD_USER=joe`)
- Set the *User* setting in the *Repository* section of the config file (see example below)

```ini
[Repository]
URL = ssh://foo@example.com/backup
User = joe
# Password = Set the password here to avoid password prompt on each command

[SFTP]
KeyFile = <SSH keyfile>
Fingerprint = SHA256:<fingerprint>
```
