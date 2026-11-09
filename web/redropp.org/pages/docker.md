# Configure Redropp

Redropp can be configured with environment variables or with a configuration file.

## Environment variables

Use the following template to configure Redropp with environment variables.

```ini
# Name/title of this instance
TITLE=

# Public URL
URL=

# Drop settings: per-user quota
# DROP_QUOTA=10G
# DROP_DELAYS=1d *7d 30d 90d

# SMTP information, read the CURL documentation for supported URLs: https://everything.curl.dev/usingcurl/smtp.html
SMTP_URL=
SMTP_USERNAME=
SMTP_PASSWORD=
SMTP_FROM=

# S3 location should look something like this: https://sos-de-fra-1.exo.io/bucket
S3_LOCATION=
S3_ACCESS_KEY_ID=
S3_SECRET_ACCESS_KEY=

# Internal authentication settings
# AUTH_INTERNAL=Yes
# AUTH_REGISTER=Yes

# Configure SSO support
# SSO_TITLE=
# SSO_URL=https://dev-stpsilwgnjk561dj.eu.auth0.com
# SSO_CLIENT_ID=r11Ah7sLNB0AN5FphbRCkNVEyK7XsQ9i
# SSO_CLIENT_SECRET=sTd6h6TywNolfTnHGKBDE37ys6rmZCnQabNvkvBmVZqOKb5Ncpn75ELkTpfp-wma
```

> [!NOTE]
> Environment variables are limited. Some advanced settings (such as multiple SSO providers) can only be changed with a configuration file.

## Configuration file

You can create a default configuration file with the following command:

```sh
mkdir config
docker run koromix/redropp:latest /app/redropp init > config/redropp.ini
```

You must configure mandatory settings (title, URL, S3) inside `config/redropp.ini` before you can run it.

# Run Docker image

## Manual command

Run the most recent release with the following command:

```sh
mkdir -p data
docker run --env-file=ENV -p 8894:80 -v $PWD/data:/data koromix/redropp:latest
```

Once Redropp is running, open http://localhost:8894/ in your browser. That's it!

> [!NOTE]
> This docker command (and the Docker Compose file below) assumes the use of an environment file (named `ENV`).
>
> In order to use a configuration file (such as `config/redropp.ini`), you need to mount the config directory and run Redropp with the path to the config file, like this:
>
> ```sh
> docker run -p 8894:80 -v $PWD/data:/data -v $PWD/config:/config koromix/redropp:latest /app/redropp -C /config/redropp.ini
> ```

## Docker Compose

Here is a sample `docker-compose.yml` file for use with Docker Compose:

```yaml
services:
  redropp:
    container_name: redropp
    image: koromix/redropp:latest

    restart: on-failure:3

    env_file: ENV

    ports:
      - 8894:80
    volumes:
      - ./data:/data
      # - ./config:/config
```

> [!TIP]
> It is recommended to use a **specific version tag** instead of `latest` and update manually, even though it is used in the documentation to make things simpler.

## Podman Quadlet

Here is a sample Quadlet unit file for use with Podman Quadlet:

```ini
[Unit]
Description=Redropp file transers
After=local-fs.target

[Service]
EnvironmentFile=/srv/redropp/ENV

[Container]
Image=docker.io/koromix/redropp:latest
PublishPort=8894:80
Volume=/srv/redropp/data:/data
# Volume=/srv/redropp/config:/config

[Install]
WantedBy=multi-user.target
```

> [!NOTE]
> This unit reads the config file from `/srv/redropp/config/redropp.ini`, and stores the database in `/srv/redropp/data`. Change the corresponding Quadlet settings if needed. Make sure the config file and the data directory exist.

Save this file somewhere the quadlet systemd generator can find it, such as `/etc/containers/systemd/redropp.container`.

Start it with the following commands:

```sh
systemctl daemon-reload
systemctl start redropp.service
```

## Development image

Development images are regularly pushed to the Docker Hub with the `dev` tag, use it with the following command:

```sh
docker run --env-file=/path/to/env/file -p 8894:80 koromix/redropp:dev
```

Use this for evaluation only, please **don't run this in production**!

# Reverse proxy

## NGINX

Edit your NGINX config (directly or in a server file in `/etc/nginx/sites-available`) to make it work as a reverse proxy for Redropp.

The server block should you something like this:

```
server {
    # ...

    location / {
        proxy_http_version 1.1;
        proxy_buffering on;
        proxy_read_timeout 180;
        send_timeout 180;

        client_max_body_size 64M;

        proxy_set_header Host $host;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;

        proxy_pass http://127.0.0.1:8894/;
    }
}
```

> [!IMPORTANT]
> When using Redropp behind a reverse proxy, you should make sure that the `X-Forwarded-For` header is correctly set by the proxy.
>
> You should also set the following settings in redropp.ini:
>
> ```
> [HTTP]
> ClientAddress = X-Forwarded-For
> SecureCookies = Yes
> ```

## Apache 2

Edit your Apache 2 config (directly or in a server file in `/etc/apache2/sites-available`) to make it work as a reverse proxy for Redropp.

The VirtualHost block should you something like this:

```
<VirtualHost *:443>
    # ...

    LimitRequestBody 67108864

    ProxyPreserveHost On
    ProxyAddHeaders On
    ProxyPass "/" "http://127.0.0.1:8894/"
    ProxyPassReverse "/" "http://127.0.0.1:8894/"
</VirtualHost>
```

> [!IMPORTANT]
> When using Redropp behind a reverse proxy, you should make sure that the `X-Forwarded-For` header is correctly set by the proxy.
>
> You should also set the following settings in redropp.ini:
>
> ```
> [HTTP]
> ClientAddress = X-Forwarded-For
> SecureCookies = Yes
> ```
