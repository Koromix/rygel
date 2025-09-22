# Image de développement

Exécutez une version de développement récente de Goupile avec la commande suivante :

```sh
docker run -p 8888:80 koromix/goupile:dev
```

Une fois Goupile en cours d'exécution, ouvrez http://localhost:8888/ dans votre navigateur et cliquez sur le bouton `Administration`.

> [!IMPORTANT]
> Goupile nécessite un noyau Linux avec le support de Landlock, soit la version Linux 5.13 ou plus récent.

Avec cette commande, les données seront enregistrées dans le conteneur, et seront perdues une fois celui-ci détruit !

Montez un volume sur `/data` pour éviter cela :

```sh
mkdir $PWD/goupile
docker run -p 8888:80 -v $PWD/goupile:/data koromix/goupile:dev
```

# Proxy inversé

## NGINX

Modifiez votre configuration NGINX (directement ou dans un fichier de serveur dans `/etc/nginx/sites-available`) pour qu'elle fonctionne comme un proxy inversé (*reverse proxy*) pour Goupile.

Le bloc `server` devrait ressembler à ceci :

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

Modifiez votre configuration Apache 2 (directement ou dans un fichier de serveur dans `/etc/apache2/sites-available`) pour qu'elle fonctionne comme un proxy inversé (*reverse proxy*) pour Goupile.

Le bloc `VirtualHost` devrait ressembler à ceci :

```
<VirtualHost *:443>
    # ...

    LimitRequestBody 268435456

    ProxyPreserveHost On
    ProxyPass "/" "http://127.0.0.1:8888/"
    ProxyPassReverse "/" "http://127.0.0.1:8888/"
</VirtualHost>
```
