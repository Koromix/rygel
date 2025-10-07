# Get started

> [!NOTE]
> Goupile in Docker requires a Linux kernel with **Landlock support**. Use a distribution with Linux 5.13 or newer to run Goupile inside Docker or Podman, such as *Debian 12*.

## Manual command

Run the most recent release with the following command:

```sh
docker run -p 8889:80 koromix/goupile:latest
```

Once Goupile is running, open http://localhost:8889/ in your browser and click on the Administration button.

> [!CAUTION]
> With this command, the data will be saved inside the container and may be **lost once the container is destroyed!**
>
> Follow on to prevent this from occuring.

Mount a volume on `/data` to prevent this:

```sh
mkdir -p data
docker run -p 8889:80 -v $PWD/data:/data koromix/goupile:latest
```

Some features, such as scheduled exports, rely on the local timezone to work correctly. It is not available in the container by default, which means Goupile will run in the GMT timezone (UTC+0).

Mount the server localtime and timezone config files (read-only) to fix this:

```sh
mkdir -p data
docker run -p 8889:80 -v $PWD/data:/data -v /etc/localtime:/etc/localtime:ro -v /etc/timezone:/etc/timezone:ro koromix/goupile:latest
```

## Docker Compose

Here is a sample `docker-compose.yml` file for use with Docker Compose:

```yaml
services:
  goupile:
    container_name: goupile
    image: koromix/goupile:latest

    restart: on-failure:3

    ports:
      - 8889:80
    volumes:
      - ./data:/data
      - /etc/localtime:/etc/localtime:ro
      - /etc/timezone:/etc/timezone:ro
```

> [!TIP]
> It is recommended to use a **specific version tag** (such as *3.11*) instead of `latest` and update manually, even though it is used in the documentation to make things simpler.

## Podman Quadlet

Here is a sample Quadlet unit file for use with Podman Quadlet:

```ini
[Unit]
Description=Goupile eCRF
After=local-fs.target

[Container]
Image=docker.io/koromix/goupile:latest
PublishPort=8889:80
Volume=/srv/goupile:/data
Volume=/etc/localtime:/etc/localtime:ro
Volume=/etc/timezone:/etc/timezone:ro

[Install]
WantedBy=multi-user.target
```

> [!NOTE]
> This unit stores data in `/srv/goupile`, change it or make sure this directory exists.

Save this file somewhere the quadlet systemd generator can find it, such as `/etc/containers/systemd/goupile.container`.

Start it with the following commands:

```sh
systemctl daemon-reload
systemctl start goupile.service
```

## Development image

Development images are regularly pushed to the Docker Hub with the `dev` tag, use it with the following command:

```sh
docker run -p 8889:80 koromix/goupile:dev
```

Use this for evaluation only, please **don't run this in production**!

# Reverse proxy

## NGINX

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

        client_max_body_size 256M;

        proxy_set_header Host $host;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;

        proxy_pass http://127.0.0.1:8888/;
    }
}
```

## Apache 2

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
