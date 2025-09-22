# Installation

## Linux (Debian)

Un dépôt Debian signé est disponible et peut être utilisé avec Debian 12 and les dérivés Debian (par exemple Ubuntu).

Exécutez les commandes suivantes (en tant que root) pour ajouter le dépôt à votre système :

```sh
mkdir -p -m0755 /etc/apt/keyrings
curl https://download.koromix.dev/debian/koromix-archive-keyring.gpg -o /etc/apt/keyrings/koromix-archive-keyring.gpg
echo "deb [signed-by=/etc/apt/keyrings/koromix-archive-keyring.gpg] https://download.koromix.dev/debian stable main" > /etc/apt/sources.list.d/koromix.dev-stable.list
```

Une fois cela fait, rafraîchissez le cache des paquets et installer Goupile :

```sh
apt update
apt install goupile
```

Pour les autres distributions, vous pouvez [compiler le code](#compilation) comme indiqué ci-dessous.

## Linux (RPM)

Un dépôt RPM signé est disponible et peut être utilisé avec RHEL, Fedora et Rocky Linux (9+).

Exécutez les commandes suivantes (en tant que root) pour ajouter le dépôt à votre système :

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

Une fois cela fait, installez le paquet avec cette commande :

```sh
dnf install goupile
```

Pour les autres distributions, vous pouvez [compiler le code](#compilation) comme indiqué ci-dessous.

# Domaines

## Création d'un domaine

Pour créer un nouveau domaine, exécutez la commande suivante :

```sh
/usr/lib/goupile/manage.py create <name> [-p <HTTP port>]
```

N'oubliez pas de **stocker de manière sécurisée** la clé de décryption qui vous est communiquée lors de la création du domaine !

## Supprimer un domaine

Supprimez le fichier INI correspondand dans `/etc/goupile/domains.d` et stoppez le service :

```sh
rm /etc/goupile/domains.d/<name>.ini
/usr/lib/goupile/manage.py sync
```

# Entretien

## Mises à jour

Vous devriez configurer votre serveur pour les mises à jour automatiques. Sur Debian, utilisez ces commandes :

```sh
apt install unattended-upgrades
dpkg-reconfigure -pmedium unattended-upgrades
```

## Backups

Les données de Goupile sont stockées dans le répertoire `/var/lib/goupile`. Vous devriez régulièrement faire des sauvegardes de ce dossier.

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
