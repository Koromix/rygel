# Install

> [!CAUTION]
> This software has not been stabilized yet and **must not be used as your primary backup** tool.
> You've been warned!

## Windows

Download ready-to-use binaries here: [https://download.koromix.dev/windows/](https://download.koromix.dev/windows/)

## macOS

Download ready-to-use binaries here: [https://download.koromix.dev/macos/](https://download.koromix.dev/macos/)

## Linux (Debian)

A signed Debian repository is provided, and should work with Debian 11 and Debian derivatives (such as Ubuntu).

Execute the following commands (as root) to add the repository to your system:

```sh
mkdir -p -m0755 /etc/apt/keyrings
curl https://download.koromix.dev/debian/koromix-archive-keyring.gpg -o /etc/apt/keyrings/koromix-archive-keyring.gpg
echo "deb [signed-by=/etc/apt/keyrings/koromix-archive-keyring.gpg] https://download.koromix.dev/debian stable main" > /etc/apt/sources.list.d/koromix.dev-stable.list
```

Once this is done, refresh the repository cache and install the package:

```sh
apt update
apt install rekkord
```

For other distributions, you can [build the code from source](#build-from-source) as documented below.

## Linux (RPM)

A signed RPM repository is provided, and should work with RHEL, Fedora and Rocky Linux (9+).

Execute the following commands (as root) to add the repository to your system:

```sh
curl https://download.koromix.dev/rpm/koromix-repo.asc -o /etc/pki/rpm-gpg/koromix-repo.asc

echo "[koromix]
name=koromix repository
baseurl=https://download.koromix.dev/rpm
enabled=1
gpgcheck=1
repo_gpgcheck=1
gpgkey=file:///etc/pki/rpm-gpg/koromix-repo.asc" > /etc/yum.repos.d/koromix.repo
```

Once this is done, install the package with this command:

```sh
dnf install rekkord
```

For other distributions, you can [build the code from source](#build-from-source) as documented below.

# Configuration

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

The command will give you this **master key** in base64 form.

> [!IMPORTANT]
> You should **store it in a secure place**, it can be used to reset user accounts and passwords (see below).
> However, if it leaks, everyone will be able to decrypt your snapshots!

The **write-only key** (public key) is derived from this master key (secret key).

Rekkord repositories support multiple user accounts. A **user account named default** is created when the repository is initialized. The master key and the write-key are each encrypted and stored in the repository with two account passwords:

- *Master password*, which allows writing and reading from the repository
- *Write-only password*, which can be used to create snapshots but cannot be used to list or restore existing snapshots

You will need one or the other to use other rekkord commands. Please store these passwords securely.

> [!IMPORTANT]
> You can reset any account or password (including the default one) **as long as you have the master key**.
> As mentioned previously, this one, you must not lose or leak!
