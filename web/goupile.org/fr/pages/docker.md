# Démarrage rapide

> [!NOTE]
> Goupile dans Docker nécessite un noyau Linux avec le **support de Landlock**. Utilisez une distribution basée sur Linux 5.13 ou plus récent pour exécuter Goupile dans Docker ou Podman, par exemple *Debian 12*.

## Commande manuelle

Exécutez la version la plus récente avec la commande suivante :

```sh
docker run -p 8889:80 koromix/goupile:latest
```

Une fois Goupile démarré, ouvrez http://localhost:8889/ dans votre navigateur et cliquez sur le bouton Administration pour configurer Goupile.

> [!CAUTION]
> Avec cette commande, les données seront sauvegardées dans le conteneur et pourront être **perdues si le conteneur est détruit !**
>
> Lisez la suite pour éviter cela.

Montez un volume sur `/data` pour stocker les données en dehors du conteneur :

```sh
mkdir -p data
docker run -p 8889:80 -v $PWD/data:/data koromix/goupile:latest
```

Certaines fonctionnalités, comme les exports planifiés, dépendent du fuseau horaire local pour fonctionner correctement. Celui-ci n'est pas disponible dans le conteneur par défaut, ce qui signifie que Goupile fonctionnera avec le fuseau horaire GMT (UTC+0).

Montez les fichiers de configuration du fuseau horaire et de l'heure locale du serveur (en lecture seule) pour éviter ce problème :

```sh
mkdir -p data
docker run -p 8889:80 -v $PWD/data:/data -v /etc/localtime:/etc/localtime:ro -v /etc/timezone:/etc/timezone:ro koromix/goupile:latest
```

## Docker Compose

Voici un exemple de fichier `docker-compose.yml` qui peut être utilisé avec Docker Compose :

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
> Il est recommandé d'utiliser une **étiquette de version spécifique** (comme *3.11*) au lieu de `latest` et de mettre à jour manuellement, même si ce tag est utilisé dans la documentation pour simplifier.

## Podman Quadlet

Voici un exemple d'unité Quadlet qui peut être utilisé avec Podman Quadlet :

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
> Avec cette configuration, les données de Goupile seront stockées dans `/srv/goupile`. Vous pouvez modifier ce répertoire, assurez-vous qu'il existe.

Enregistrez cette unité à un emplacement indiqué par le générateur systemd, par exemple `/etc/containers/systemd/goupile.container`

 Utilisez les commandes suivantes pour démarrer le conteneur :

```sh
systemctl daemon-reload
systemctl start goupile.service
```

## Image de développement

Les images de développement sont régulièrement déployées sur le Docker Hub avec l'étiquette `dev`, testez-les avec la commande suivante :

```sh
docker run -p 8889:80 koromix/goupile:dev
```

Utilisez les versions dévéloppement pour faire des tests, **ne les utilisez pas en production !**

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
