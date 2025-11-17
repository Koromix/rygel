# Windows

> [!IMPORTANT]
> Rekkord currently is currently in active development, we recommend use for **redundant backups**.

Download ready-to-use binaries here: https://download.koromix.dev/rekkord/

# macOS

> [!IMPORTANT]
> Rekkord currently is currently in active development, we recommend use for **redundant backups**.

Download ready-to-use binaries here: https://download.koromix.dev/rekkord/

# Linux

## Debian / Ubuntu

> [!IMPORTANT]
> Rekkord currently is currently in active development, we recommend use for **redundant backups**.

A signed Debian repository is provided, and should work with Debian 12 and Debian derivatives (such as Ubuntu).

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

## RedHat, Fedora, Rocky Linux (RPM)

> [!IMPORTANT]
> Rekkord currently is currently in active development, we recommend use for **redundant backups**.

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
