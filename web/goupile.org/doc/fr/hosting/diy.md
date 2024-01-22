# Auto-hébergement

## Paquet Debian

*XXX*

## Compilation

Le serveur Goupile est multi-plateforme, mais il est **recommandé de l'utiliser sur Linux** pour une sécurité maximale.

En effet, sur Linux Goupile peut fonctionner en mode sandboxé grâce à seccomp et les espaces de noms Linux (appel système unshare). Le support du sandboxing est envisagé à long terme pour d'autres systèmes d'exploitation mais n'est pas disponible pour le moment. L'utilisation de la distribution Debian 10+ est recommandée.

Goupile repose sur du C++ (côté serveur) et HTML/CSS/JS (côté client). La compilation de Goupile utilise un outil dédié qui est inclus directement dans le dépôt.

Commencez par récupérer le code depuis le dépôt Git : https://framagit.org/interhop/goupile

```sh
git clone https://framagit.org/interhop/goupile
cd goupile
```

### Linux

Pour compiler une **version de développement** et de test procédez comme ceci depuis la racine du dépôt :

```sh
# Préparation de l'outil Felix utilisé pour compiler Goupile
./bootstrap.sh

# L'exécutable sera déposé dans le dossier bin/Debug
./felix
```

Pour une **utilisation en production**, il est recommandé de compiler Goupile [en mode Paranoid](technical/architecture.md#options-de-compilation) à l'aide de Clang 11+ et le lieur LLD 11+. Sous Debian 10, vous pouvez faire comme ceci :

```sh
# Préparation de l'outil Felix utilisé pour compiler Goupile
./bootstrap.sh

# Installation de LLVM décrite ici et recopiée ci-dessous : https://apt.llvm.org/
sudo bash -c "$(wget -O - https://apt.llvm.org/llvm.sh)"
sudo apt install clang-11 lld-11

# L'exécutable sera déposé dans le dossier bin/Paranoid
./felix -pParanoid --host=,clang-11,lld-11
```

### Autres systèmes

Pour compiler une version de développement et de test procédez comme ceci depuis la racine du dépôt :

#### Systèmes POSIX (macOS, WSL, etc.)

```sh
# Préparation de l'outil Felix utilisé pour compiler Goupile
./bootstrap.sh

# L'exécutable sera déposé dans le dossier bin/Debug
./felix
```

#### Windows

```sh
# Préparation de l'outil Felix utilisé pour compiler Goupile
# Il peut être nécessaires d'utiliser l'environnement console de Visual Studio avant de continuer.
bootstrap.bat

# L'exécutable sera déposé dans le dossier bin/Debug
felix
```

Il n'est pas recommandé d'utiliser Goupile en production sur un autre système, car le mode bac à sable (sandboxing) et la compilation en mode Paranoid n'y sont pas disponibles pour le moment.

Cependant, vous pouvez utiliser la commande ./felix --help (ou felix --help sur Windows) pour consulter les modes et options de compilations disponibles pour votre système.

## Exécution manuelle

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
