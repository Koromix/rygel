# Configure repository

> [!CAUTION]
> This software has not been stabilized yet and **must not be used as your primary backup** tool.
> You've been warned!

## Local filesystem

To create a repository in a local directory, create an INI file (name it how you want) with the following configuration:

```ini
[Repository]
URL = /path/to/repository
```

Once this is done, use [rekkord init](#initialize-repository) to create the repository.

## S3 storage

To create a repository stored on an S3-compatible server, create an INI file (name it how you want) with the following configuration:

```ini
[Repository]
URL = https://<bucket>.s3.<region>.amazonaws.com/prefix/to/repository

[S3]
KeyID = <AWS access key ID>
SecretKey = <AWS secret key>
```

You can omit the `SecretKey` value, in which case a prompt will ask you the access key.

Once this is done, use [rekkord init](#initialize-repository) to create the repository.

## SFTP server

To create a repository stored on an SFTP host, create an INI file (name it how you want) with the following configuration:

```ini
[Repository]
URL = ssh://<user>@<host>/path/to/repository

[SFTP]
Password = <SSH password>
KeyFile = <SSH keyfile>
```

Use either a password or a key file. If you omit both, a prompt will ask you for a password.

Just like with `ssh`, you will need to validate the host fingerprint the first time you connect to it. However, you can instead directly provide the correct host fingerprint as the `Fingerprint` to avoid this, as shown in the example below:

```ini
[Repository]
URL = ssh://foo@example.com/backup

[SFTP]
KeyFile = /home/bar/.ssh/id_ed25519
Fingerprint = SHA256:Y9pmJfkaok8t0dFJrfi8/LLUNhOYwAZGHUNGsYAiJUM
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
> You should **store this master key in a secure place**, it can be used to reset user accounts and passwords (see below). It cannot be recreated if you lose the file.
> However, if it leaks, everyone will be able to decrypt your snapshots!

Rekkord uses **multiple encryption keys** which are derived from this master key:

- The *config key (ckey)* is paired with an *access key (akey)* to sign config files and user key files
- The *data key (dkey)* is paired with a *write key (wkey)* for data encryption (snapshot information, directory metadata, file content)
- The *log key (lkey)* is paired with a *tag key (tkey)*, to manage snapshots and record snapshot information
- The *sign key (skey)* is paired with a *verify key (vkey)* to detect tampering with tags

Rekkord repositories support multiple user accounts. By default, three user accounts are created:

- *admin*: This user has the keys necessary to manage users and access data
- *data*: This user has the keys necessary for read-write access to stored data
- *write*: This user can only write new data but cannot read historical data, or list existing snapshots

You will need one or the other to use other rekkord commands. Please store these passwords securely.

> [!IMPORTANT]
> You can reset any account or password (including the default one) **as long as you have the master key** file.
> As mentioned previously, this one, you must not lose or leak!

You can skip the creation of these two default users with `rekkord init --skip_users`, in which case you will have to create at least one user yourself with the [help of the master key](#repository-users).

# Repository users

As stated before, by default Rekkord creates three users named *admin*, *data* and *write*. These are enough for simple needs.

> [!WARNING]
> Repository users contain the necessary encryption keys for a given role, protected by a password.
>
> These have **nothing to do with the SSH login name or the S3 access keys** (or any other backend that may appear one day), which you have to manage yourself!

Use the following commands to manage repository users:

- *rekkord add_user*
- *rekkord delete_user*
- *rekkord list_users*

To create a new user, you must either use an existing user with the necessary keys (a user with write-only role cannot create a user with read-write role), or use the master key file created by `rekkord init`.

```sh
export REKKORD_CONFIG_FILE=/path/to/config.ini

# Use existing user to create new user
export REKKORD_USER=full
rekkord add_user joe -r ReadWrite

# Or use the master key file
rekkord add_user -K master.key john -r ReadWrite
```

Most Rekkord commands require you to specify the user, you can do this in one of two ways:

- Set the `REKKORD_USER` environment variable (e.g. `export REKKORD_USER=joe`)
- Set the *User* setting in the *Repository* section of the config file (see example below)

> [!NOTE]
> If the user is not set, Rekkord tries the *write* user by default.

```ini
[Repository]
URL = ssh://foo@example.com/backup
User = joe
# Password = Set the password here to avoid password prompt on each command

[SFTP]
KeyFile = <SSH keyfile>
Fingerprint = SHA256:<fingerprint>
```
