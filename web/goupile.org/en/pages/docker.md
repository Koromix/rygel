# Development image

Run the most recent development image with the following command:

```sh
docker run -p 8888:80 koromix/goupile:dev
```

Once Goupile is running, open http://localhost:8888/ in your browser and click on the Administration button.

> [!IMPORTANT]
> Goupile requires a Linux kernel with Landlock support, you must use Linux 5.13 or newer.

With this command, the data will be saved inside the container and will be lost once the container is destroyed!

Mount a volume on `/data` to prevent this:

```sh
mkdir $PWD/goupile
docker run -p 8888:80 -v $PWD/goupile:/data koromix/goupile:dev
```

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
