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

After initialization, the **master key** is exported to a binary file, named `master.key` by default (in the current directory). You can change the export file with the `--key_file <file>` option, or skip it altogether with `--skip_key`.

> [!IMPORTANT]
> You should **store this master key in a secure place**, it can be used to reset user accounts and passwords (see below).
> However, if it leaks, everyone will be able to decrypt your snapshots!

The **write-only key** (public key) is derived from this master key (secret key).

Rekkord repositories support multiple user accounts. A **user account named default** is created when the repository is initialized. The master key and the write-key are each encrypted and stored in the repository with two account passwords:

- *Master password*, which allows writing and reading from the repository
- *Write-only password*, which can be used to create snapshots but cannot be used to list or restore existing snapshots

You will need one or the other to use other rekkord commands. Please store these passwords securely.

> [!IMPORTANT]
> You can reset any account or password (including the default one) **as long as you have the master key** file.
> As mentioned previously, this one, you must not lose or leak!

You can use `rekkord export_key` to re-export the master key at any time.
