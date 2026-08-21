# Installation

## Linux (Debian)

A signed Debian repository is available and can be used with Debian 12 and Debian derivatives (e.g., Ubuntu).

Run the following commands (as root) to add the repository to your system:

```sh
mkdir -p -m0755 /etc/apt/keyrings
curl https://download.koromix.dev/debian/koromix-archive-keyring.gpg -o /etc/apt/keyrings/koromix-archive-keyring.gpg
echo "deb [signed-by=/etc/apt/keyrings/koromix-archive-keyring.gpg] https://download.koromix.dev/debian stable main" > /etc/apt/sources.list.d/koromix.dev-stable.list
```

Once this is done, refresh the package cache and install Redropp:

```sh
apt update
apt install redropp
```

For other distributions, you can [compile the code](build) manually.

## Linux (RPM)

A signed RPM repository is available and can be used with RHEL, Fedora, and Rocky Linux (9+).

Run the following commands (as root) to add the repository to your system:

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
dnf install redropp
```

For other distributions, you can [compile the code](build) manually.

# Configuration

Before you can run Redropp, you must create configure it. Edit `/etc/redropp/redropp.ini`, the following settings are mandatory before Redropp can start:

- Title and public URL of this instance
- S3 location and keys
- SMTP server and authentication settings

Once configured, start the systemd service:

```sh
systemctl start redropp
```

# Reverse Proxy

## NGINX

Edit your NGINX configuration (directly or in a server file in `/etc/nginx/sites-available`) to set it up as a reverse proxy for Redropp.

The server block should look something like this:

```
server {
    # ...

    location / {
        proxy_http_version 1.1;
        proxy_buffering on;
        proxy_read_timeout 180;
        send_timeout 180;

        client_max_body_size 256M;

        proxy_set_header Host $host;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;

        proxy_pass http://127.0.0.1:8894/;
    }
}
```

## Apache 2

Edit your Apache 2 configuration (directly or in a server file in `/etc/apache2/sites-available`) to set it up as a reverse proxy for Redropp.

The VirtualHost block should look something like this:

```
<VirtualHost *:443>
    # ...

    LimitRequestBody 67108864

    ProxyPreserveHost On
    ProxyPass "/" "http://127.0.0.1:8894/"
    ProxyPassReverse "/" "http://127.0.0.1:8894/"
</VirtualHost>
```
