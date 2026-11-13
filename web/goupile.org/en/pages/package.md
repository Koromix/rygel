# Installation

## Linux (Debian)

A signed Debian repository is available and can be used with Debian 12 and Debian derivatives (e.g., Ubuntu).

Run the following commands (as root) to add the repository to your system:

```sh
mkdir -p -m0755 /etc/apt/keyrings
curl https://download.koromix.dev/debian/koromix-archive-keyring.gpg -o /etc/apt/keyrings/koromix-archive-keyring.gpg
echo "deb [signed-by=/etc/apt/keyrings/koromix-archive-keyring.gpg] https://download.koromix.dev/debian stable main" > /etc/apt/sources.list.d/koromix.dev-stable.list
```

Once this is done, refresh the package cache and install Goupile:

```sh
apt update
apt install goupile
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
dnf install goupile
```

For other distributions, you can [compile the code](build) manually.

# Domains

## Creating a Domain

To create a new domain, run the following command:

```sh
/usr/lib/goupile/manage.py create <name> [-p <HTTP port>]
```

Once the domain is created, open it in your web browser (the port is indicated after running `manage.py create`), click on *Administration* and configure your new Goupile domain.

## Deleting a Domain

Delete the corresponding INI file in `/etc/goupile/domains.d` and stop the service:

```sh
rm /etc/goupile/domains.d/<name>.ini
/usr/lib/goupile/manage.py sync
```

# Maintenance

## Updates

You should configure your server for automatic updates. On Debian, use something like this:

```sh
apt install unattended-upgrades
dpkg-reconfigure -pmedium unattended-upgrades
```

## Backups

Goupile data is stored in the `/var/lib/goupile` directory. You should regularly back up this folder.

# Reverse Proxy

## NGINX

Edit your NGINX configuration (directly or in a server file in `/etc/nginx/sites-available`) to set it up as a reverse proxy for Goupile.

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
        proxy_set_header X-Forwarded-Proto $scheme; # Use $http_x_forwarded_proto if NGINX is behind another trusted reverse proxy that sets X-Forwarded-Proto

        proxy_pass http://127.0.0.1:8889/;
    }
}
```

> [!IMPORTANT]
> When Goupile is used behind a reverse proxy, make sure the proxy correctly sets the `X-Forwarded-For` and `X-Forwarded-Proto` headers.
>
> If the reverse proxy is running on another machine, you will need to change the `TrustedProxies` setting in the *HTTP* section of the configuration file to include the address or network of your reverse proxy or proxies.
>
> Goupile only uses forwarding headers such as `X-Forwarded-For` and `X-Forwarded-Proto` when they come from a proxy declared as trusted. This information is used, among other things, to mitigate certain attacks, for example by blocking repeated login attempts, and to strengthen cookie security.

## Apache 2

Edit your Apache 2 configuration (directly or in a server file in `/etc/apache2/sites-available`) to set it up as a reverse proxy for Goupile.

The VirtualHost block should look something like this:

```
<VirtualHost *:443>
    # ...

    LimitRequestBody 268435456

    ProxyPreserveHost On
    ProxyAddHeaders On
    RequestHeader set "X-Forwarded-Proto" expr=%{REQUEST_SCHEME}

    ProxyPass "/" "http://127.0.0.1:8889/"
    ProxyPassReverse "/" "http://127.0.0.1:8889/"
</VirtualHost>
```

> [!IMPORTANT]
> When Goupile is used behind a reverse proxy, make sure the proxy correctly sets the `X-Forwarded-For` and `X-Forwarded-Proto` headers.
>
> If the reverse proxy is running on another machine, you will need to change the `TrustedProxies` setting in the *HTTP* section of the configuration file to include the address or network of your reverse proxy or proxies.
>
> Goupile only uses forwarding headers such as `X-Forwarded-For` and `X-Forwarded-Proto` when they come from a proxy declared as trusted. This information is used, among other things, to mitigate certain attacks, for example by blocking repeated login attempts, and to strengthen cookie security.
