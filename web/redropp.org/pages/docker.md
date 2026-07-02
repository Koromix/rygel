# Create configuration file

Before you can run Redropp, you must create a configuration file. For Docker, you can use an environment file:

```
# Name/title of this instance
DROP_TITLE=

# Public URL
DROP_URL=

# Per-user usage quota
DROP_QUOTA=1G

# SMTP information, read the CURL documentation for supported URLs: https://everything.curl.dev/usingcurl/smtp.html
SMTP_URL=
SMTP_USERNAME=
SMTP_PASSWORD=
SMTP_FROM=

# S3 location should look something like this: https://sos-de-fra-1.exo.io/bucket
S3_LOCATION=
S3_ACCESS_KEY_ID=
S3_SECRET_ACCESS_KEY=
```

# Run Docker image

> [!NOTE]
> Redropp in Docker requires a Linux kernel with **Landlock support**. Use a distribution with Linux 5.13 or newer to run Redropp inside Docker or Podman, such as *Debian 12*.

## Manual command

Run the most recent release with the following command:

```sh
docker run --env-file=/path/to/env/file -p 8894:80 koromix/redropp:latest
```

Once Redropp is running, open http://localhost:8894/ in your browser and click on the Administration button.

> [!CAUTION]
> With this command, the data will be saved inside the container and may be **lost once the container is destroyed!**
>
> Follow on to prevent this from occuring.

Mount a volume on `/data` to prevent this:

```sh
mkdir -p data
docker run --env-file=/path/to/env/file -p 8894:80 -v $PWD/data:/data koromix/redropp:latest
```

## Docker Compose

Here is a sample `docker-compose.yml` file for use with Docker Compose:

```yaml
services:
  redropp:
    container_name: redropp
    image: koromix/redropp:latest

    restart: on-failure:3

    ports:
      - 8894:80
    volumes:
      - ./data:/data
    env_file: /path/to/env/file
```

> [!TIP]
> It is recommended to use a **specific version tag** (such as *3.11*) instead of `latest` and update manually, even though it is used in the documentation to make things simpler.

## Podman Quadlet

Here is a sample Quadlet unit file for use with Podman Quadlet:

```ini
[Unit]
Description=Redropp file transers
After=local-fs.target

[Container]
Image=docker.io/koromix/redropp:latest
PublishPort=8894:80
Volume=/srv/redropp:/data
EnvironmentFile=/path/to/env/file

[Install]
WantedBy=multi-user.target
```

> [!NOTE]
> This unit stores data in `/srv/redropp`, change it or make sure this directory exists.

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

## Apache 2

Edit your Apache 2 config (directly or in a server file in `/etc/apache2/sites-available`) to make it work as a reverse proxy for Redropp.

The VirtualHost block should you something like this:

```
<VirtualHost *:443>
    # ...

    LimitRequestBody 268435456

    ProxyPreserveHost On
    ProxyPass "/" "http://127.0.0.1:8894/"
    ProxyPassReverse "/" "http://127.0.0.1:8894/"
</VirtualHost>
```
