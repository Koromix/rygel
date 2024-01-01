# Overview

Rekord is a **multi-platform backup tool**, with the following features:

- Write-only passwords / keys (using asymmetic encryption)
- Data deduplication based on content-defined chunking
- Data compression with LZ4
- Local and remote storage back-ends: local directory, S3 storage, SFTP servers

This software has not been stabilized yet and **must not be used as your primary backup** tool. You've been warned!

# Install

## Windows

Ready-to-use Windows binaries will be provided once the software stabilizes. In the meantime, you can build it yourself or [try it on Debian](#linux).

## macOS

Ready-to-use macOS binaries will be provided once the software stabilizes. In the meantime, you can build it yourself or [try it on Debian](#linux).

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
apt install rekord
```

For other distributions, you can [build the code from source](#build-from-source) as documented below.

## Linux (RPM)

A signed RPM repository is provided, and should work with RHEL and Fedora.

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
dnf install rekord
```

For other distributions, you can [build the code from source](#build-from-source) as documented below.

# Configuration

## Local filesystem

To create a repository in a local directory, create an INI file (name it how you want) with the following configuration:

```ini
[Repository]
URL = /path/to/repository
```

Once this is done, use [rekord init](#initialize-repository) to create the repository.

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

Once this is done, use [rekord init](#initialize-repository) to create the repository.

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

Once this is done, use [rekord init](#initialize-repository) to create the repository.

# Commands

## Initialize repository

Once you have configured your repository, through a configuration file or with environment variables, you can initialize it with the following command:

```sh
export REKORD_CONFIG_FILE = /path/to/config.ini
rekord init

# Alternative: rekord init -C /path/to/config.ini
```

This command will initialize the repository with a random 256-bit master key.

The command will give you this **master key** in base64 form. You should **store it in a secure place**, it can be used to reset user accounts and passwords (see below). However, if it leaks, everyone will be able to decrypt your snapshots.

The **write-only key** (public key) is derived from this master key (secret key).

Rekord repositories support multiple user accounts. A **user account named default** is created when the repository is initialized. The master key and the write-key are each encrypted and stored in the repository with two account passwords:

- *Master password*, which allows writing and reading from the repository
- *Write-only password*, which can be used to create snapshots but cannot be used to list or restore existing snapshots

You will need one or the other to use other rekord commands. Please store these passwords securely.

However, if you lose them, you will still be able to reset any account (including the default one) **as long as you have the master key**. As mentioned previously, this one, you must not lose or leak!

## Create snapshots

Each snapshot has a unique hash (which is actually a BLAKE3 hash in hexadecimal form), which is generated automatically when the snapshot is created.

You need to give snapshots a name (or use `--anonymous` to skip this). This name does not have to be unique and only exists to help you categorize snapshots.

```sh
export REKORD_CONFIG_FILE = /path/to/config.ini
rekord put -n <NAME> <PATHS...>
```

The command will give you the snapshot hash once it finishes. You can retrieve the hash later with [rekord list](#list-snapshots).

## List snapshots

```sh
export REKORD_CONFIG_FILE = /path/to/config.ini
rekord list
```

The output looks something like this:

```
# <hash>                                                           <name>                   <date>

DA24E2C01C2AF6ACADF94FED087FD2695DF1E5352FA5474E091DABE38A104641   webserver1               [2023-11-26 06:09:57 +0100]
96389F2763173C9575C85A9D8972FE8DC06FC220BA7A05A673D7C19E520C22EB   webserver1               [2023-11-27 06:05:15 +0100]
221FA094C07FAABA1A6B8710EED613F441410C95855D69653A2BDCBB590C8E9F   webserver1               [2023-11-28 06:18:15 +0100]
```

Use `--format JSON` or `--format XML` to get this list in a JSON or XML format.

## Explore snapshot

You can list the directories and files in a snapshot with the `rekord tree` command. You need to know the unique [snapshot hash](#list-snapshots) for this.

```sh
export REKORD_CONFIG_FILE = /path/to/config.ini
rekord tree <hash>
```

The output looks something like this:

```
# <type> <name>                                                   <mode> <date>                      <size>

[S] vpn/opt                                                              [2023-12-15 06:18:10 +0100] 
  [D] opt/                                                        (0755) [2023-10-05 17:03:38 +0200] 
    [D] certificates/                                             (0750) [2023-12-13 09:43:10 +0100] 
      [F] cloud.intra.example.com.key                             (0640) [2023-12-13 09:43:10 +0100] 241 B
      [F] forum.intra.example.com.crt                             (0640) [2023-12-13 09:43:10 +0100] 5.269 kB
    [D] agnos/                                                    (0755) [2023-10-02 12:38:29 +0200] 
      [D] certificates/                                           (0750) [2023-10-02 12:38:50 +0200] 
        [F] cloud.intra.example.com.key                           (0640) [2023-11-20 23:26:06 +0100] 241 B
        [F] forum.intra.example.com.crt                           (0640) [2023-11-20 23:26:07 +0100] 5.269 kB
      [F] agnos                                                   (0755) [2023-09-21 16:53:54 +0200] 11.24 MB
      [F] agnos.toml                                              (0644) [2023-10-02 12:38:28 +0200] 1.893 kB
      [D] accounts/                                               (0750) [2023-09-21 16:53:55 +0200] 
        [F] main.key                                              (0644) [2023-09-21 16:53:56 +0200] 3.243 kB
    [D] rekord/                                                   (0755) [2023-12-13 15:38:23 +0100] 
      [F] ENV                                                     (0600) [2023-10-05 17:03:45 +0200] 186 B
      [F] rekord.sh                                               (0755) [2023-12-13 15:37:58 +0100] 102 B
      [F] rekord.deb                                              (0644) [2023-12-11 20:45:18 +0100] 5.348 MB
    [D] wireguard/                                                (0755) [2023-09-21 14:28:46 +0200] 
      [D] safe/                                                   (0755) [2023-09-21 15:59:39 +0200] 
        [F] docker-compose.yml                                    (0644) [2023-09-21 14:28:47 +0200] 408 B
        [F] ENV                                                   (0600) [2023-09-21 15:59:39 +0200] 209 B
        [D] etc/                                                  (0700) [2023-09-21 14:29:06 +0200] 
          [F] wg0.json                                            (0640) [2023-12-10 06:00:19 +0100] 7.969 kB
          [F] wg0.conf                                            (0600) [2023-12-10 06:00:19 +0100] 4.025 kB
    [D] rclone/                                                   (0755) [2023-10-05 15:32:04 +0200] 
      [F] rclone.cfg                                              (0644) [2023-10-05 15:32:04 +0200] 3.705 kB
      [F] rclone.deb                                              (0644) [2023-09-21 14:26:52 +0200] 18.55 MB
    [D] coredns/                                                  (0755) [2023-12-13 09:42:56 +0100] 
      [F] coredns.tar.gz                                          (0644) [2023-09-21 16:50:53 +0200] 15.79 MB
      [F] coredns.conf                                            (0644) [2023-09-21 16:50:56 +0200] 132 B
      [F] domains.zone                                            (0644) [2023-12-13 09:42:55 +0100] 505 B
      [F] coredns                                                 (0755) [2023-02-06 19:27:11 +0100] 53.41 MB
    [D] nginx/                                                    (0755) [2023-12-11 20:44:06 +0100] 
      [D] ssl/                                                    (0755) [2023-12-11 20:43:55 +0100] 
        [D] webroot/                                              (0755) [2023-09-21 14:28:34 +0200] 
          [D] vpn.example.com/                                    (0755) [2023-11-20 19:10:04 +0100] 
        [F] dhparam.pem                                           (0644) [2023-09-21 14:28:21 +0200] 424 B
        [D] letsencrypt/                                          (0755) [2023-12-11 20:44:22 +0100] 
          [L] vpn.example.com.crt                                        [2023-12-11 20:44:22 +0100] 
          [L] 57.128.60.175.key                                          [2023-12-11 20:44:07 +0100] 
          [L] vpn.example.com.key                                        [2023-12-11 20:44:19 +0100] 
          [L] 57.128.60.175.crt                                          [2023-12-11 20:44:07 +0100] 
        [D] internal/                                             (0755) [2023-12-13 09:43:27 +0100] 
          [F] ca.crt                                              (0644) [2023-12-13 09:43:24 +0100] 1.883 kB
          [F] vpn.crt                                             (0644) [2023-12-13 09:43:26 +0100] 1.891 kB
          [F] vpn.key                                             (0644) [2023-12-13 09:43:25 +0100] 1.704 kB
      [F] nginx.conf                                              (0644) [2023-12-11 20:44:06 +0100] 7.086 kB
```

Use `--format JSON` or `--format XML` to get the file tree in a JSON or XML format.

## Restore snapshot

Use the `rekord get` command to restore the files from a snapshot onto the local filesystem. You need to know the unique [snapshot hash](#list-snapshots) for this.

```sh
export REKORD_CONFIG_FILE = /path/to/config.ini
rekord get <hash> -O <path>
```

# Build from source

Start by cloning the git repository here: [https://github.com/Koromix/rygel](https://github.com/Koromix/rygel):

```sh
git clone https://github.com/Koromix/rygel
cd rygel
```

## Windows

In order to build Rekord on Windows, clone the repository and run these commands from the root directory in a _Visual Studio command prompt_ (x64 or x86, as you prefer):

```sh
bootstrap.bat
felix -pFast rekord
```

After that, the binary will be available in the `bin/Fast` directory.

## Linux / macOS

In order to build Rekord on Linux, clone the repository and run these commands from the root directory:

```sh
# Install required dependencies on Debian or Ubuntu:
sudo apt install build-essential

# Build binaries
./bootstrap.sh
./felix -pFast rekord
```

After that, the binary will be available in the `bin/Fast` directory.

# License

This program is free software: you can redistribute it and/or modify it under the terms of the **GNU Affero General Public License** as published by the Free Software Foundation, either **version 3 of the License**, or (at your option) any later version.

Find more information here: [https://www.gnu.org/licenses/](https://www.gnu.org/licenses/)
