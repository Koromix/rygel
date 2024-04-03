# Introduction

Goupile is an open-source electronic data capture application that strives to make form creation and data entry both powerful and easy.

It is licensed under the [AGPL 3 license](https://www.gnu.org/licenses/#AGPL). You can freely download and use the goupile source code. Everyone is granted the right to copy, modify and redistribute it.

# Get started

## Installation

### Linux (Debian)

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
apt install goupile
```

For other distributions, you can [build the code from source](#build-from-source) as documented below.

### Linux (RPM)

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
dnf install goupile
```

For other distributions, you can [build the code from source](#build-from-source) as documented below.

## Domains

### Create domain

To create a new domain, run the following command:

```sh
/usr/lib/goupile/manage.py create <name> [-p <HTTP port>]
```

Don't forget to **securely store** the backup decryption key!

### Delete domain

Delete the corresponding INI file in `/etc/goupile/domains.d` and stop the service:

```sh
rm /etc/goupile/domains.d/<name>.ini
/usr/lib/goupile/manage.py sync
```

## Servicing

### Updates

You should ideally configure your server for automated updates:

```sh
apt install unattended-upgrades
dpkg-reconfigure -pmedium unattended-upgrades
```

### Backups

The live Goupile data lives in `/var/lib/goupile`. You should backup this directory regularly.

## Reverse proxy

### NGINX

Edit your NGINX config (directly or in a server file in `/etc/nginx/sites-available`) to make it work as a reverse proxy for Goupile.

The server block should you something like this:

```
server {
    # ...

    location / {
        proxy_http_version 1.1;
        proxy_buffering on;
        proxy_read_timeout 180;
        send_timeout 180;

        proxy_request_buffering off;
        client_max_body_size 256M;

        proxy_set_header Host $host;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;

        proxy_pass http://127.0.0.1:8888/;
    }
}
```

### Apache 2

Edit your Apache 2 config (directly or in a server file in `/etc/apache2/sites-available`) to make it work as a reverse proxy for Goupile.

The VirtualHost block should you something like this:

```
<VirtualHost *:443>
    # ...

    LimitRequestBody 268435456

    ProxyPreserveHost On
    ProxyPass "/" "http://127.0.0.1:8888/"
    ProxyPassReverse "/" "http://127.0.0.1:8888/"
</VirtualHost>
```

# Build from source

This repository uses a dedicated build tool called felix. To get started, you need to build
this tool. You can use the bootstrap scripts at the root of the repository to bootstrap it:

* Run `./bootstrap.sh` on Linux and macOS
* Run `bootstrap.bat` on Windows

This will create a felix binary at the root of the source tree. You can then start it to
build all projects defined in *FelixBuild.ini*: `felix` on Windows or `./felix` on Linux and macOS.

The following compilers are supported: GCC, Clang and MSVC (on Windows).

Use `./felix --help` for more information.

# Documentation

You can find more information on [the official website](https://goupile.fr/), in french and english.