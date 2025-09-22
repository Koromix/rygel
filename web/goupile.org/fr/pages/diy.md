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

Vous devriez configurer votre serveur pour les mises à jour automatiques :

```sh
apt install unattended-upgrades
dpkg-reconfigure -pmedium unattended-upgrades
```

## Backups

Les données de Goupile sont stockées dans le répertoire `/var/lib/goupile`. Vous devriez régulièrement faire des copies de ce dossier.

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

# Compilation

Le serveur Goupile est multi-plateforme, mais il est **recommandé de l'utiliser sur Linux** pour une sécurité maximale.

En effet, sur Linux Goupile peut fonctionner en mode sandboxé grâce à seccomp et les espaces de noms Linux (appel système unshare). Le support du sandboxing est envisagé à long terme pour d'autres systèmes d'exploitation mais n'est pas disponible pour le moment. L'utilisation de la distribution Debian 12+ est recommandée.

Goupile repose sur du C++ (côté serveur) et HTML/CSS/JS (côté client). La compilation de Goupile utilise un outil dédié qui est inclus directement dans le dépôt.

Commencez par récupérer le code depuis le dépôt Git : https://framagit.org/interhop/goupile

```sh
git clone https://framagit.org/interhop/goupile
cd goupile
```

## Linux

Pour compiler une **version de développement** et de test procédez comme ceci depuis la racine du dépôt :

```sh
# Préparation de l'outil felix utilisé pour compiler Goupile
./bootstrap.sh

# L'exécutable sera déposé dans le dossier bin/Debug
./felix
```

Pour une **utilisation en production**, il est recommandé de compiler Goupile [en mode Paranoid](technical/architecture.md#options-de-compilation) à l'aide de Clang 18+ et le lieur LLD 18+. Sous Debian 12, vous pouvez faire comme ceci :

```sh
# Préparation de l'outil felix utilisé pour compiler Goupile
./bootstrap.sh

# Installation de LLVM décrite ici et recopiée ci-dessous : https://apt.llvm.org/
sudo bash -c "$(wget -O - https://apt.llvm.org/llvm.sh)"
sudo apt install clang-18 lld-18

# L'exécutable sera déposé dans le dossier bin/Paranoid
./felix -pParanoid --host=,clang-18,lld-18
```

## Autres systèmes

Pour compiler une version de développement et de test procédez comme ceci depuis la racine du dépôt :

### Systèmes POSIX (macOS, WSL, etc.)

```sh
# Préparation de l'outil felix utilisé pour compiler Goupile
./bootstrap.sh

# L'exécutable sera déposé dans le dossier bin/Debug
./felix
```

### Windows

```sh
# Préparation de l'outil felix utilisé pour compiler Goupile
# Il peut être nécessaires d'utiliser l'environnement console de Visual Studio avant de continuer.
bootstrap.bat

# L'exécutable sera déposé dans le dossier bin/Debug
felix
```

Il n'est pas recommandé d'utiliser Goupile en production sur un autre système, car le mode bac à sable (sandboxing) et la compilation en mode Paranoid n'y sont pas disponibles pour le moment.

Cependant, vous pouvez utiliser la commande ./felix --help (ou felix --help sur Windows) pour consulter les modes et options de compilations disponibles pour votre système.

# Exécution manuelle

Une fois l'exécutable Goupile compilé, il est possible de créer un domaine Goupile à l'aide de la commande suivante :

```sh
# Pour cet exemple, nous allons créer ce domaine dans un sous-dossier tmp
# du dépôt, mais vous pouvez le créer où vous le souhaiter !
mkdir tmp

# L'exécution de cette commande vous demandera de créer un premier
# compte administrateur et de définir son mot de passe.
bin/Paranoid/goupile init tmp/domaine_test
```

L'initialisation de ce domaine va créer une clé de récupération d'archive que vous devez stocker afin de pouvoir restaurer une archive créée dans le panneau d'administration du domaine Goupile. Si elle est perdue, cette clé peut être modifiée mais les archives créées avec la clé précédente ne seront pas récupérables !

Pour accéder à ce domaine via un navigateur web, vous pouvez le lancer à l'aide de la commande suivante :

```sh
# Avec cette commande, Goupile sera accessible via http://localhost:8889/
bin/Paranoid/goupile -C tmp/domaine_test/goupile.ini
```

Pour un mise en production, il est recommandé de faire fonctionner Goupile derrière un reverse proxy HTTPS comme par exemple NGINX.

Pour automatiser le déploiement d'un serveur complet en production (avec plusieurs domaines Goupile et NGINX configuré automatiquement), nous fournissons un playbook et des rôles Ansible prêts à l'emploi que vous pouvez utiliser tel quel ou adapter à vos besoins.
