# Compiler Goupile

Le serveur Goupile est pour l'instant **uniquement disponible pour Linux** et utilise le LSM Landlock (et seccomp), qui nécessite Linux 5.13 ou une version plus récente.

> [!NOTE]
> En effet, sur Linux Goupile peut fonctionner en mode sandboxé grâce à seccomp et LandLock. Le support du sandboxing est envisagé à long terme pour d'autres systèmes d'exploitation mais n'est pas disponible pour le moment.
>
> L'utilisation de la distribution Debian 12+ est recommandée.

Goupile repose sur du C++ (côté serveur) et HTML/CSS/JS (côté client). La compilation de Goupile utilise un outil dédié qui est inclus directement dans le dépôt.

Commencez par récupérer le code depuis le dépôt Git : https://codeberg.org/Koromix/rygel

```sh
git clone https://codeberg.org/Koromix/rygel
cd rygel
```

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

# Exécuter Goupile

Une fois l'exécutable Goupile compilé, il est possible de créer un domaine Goupile à l'aide de la commande suivante :

```sh
# Pour cet exemple, nous allons créer ce domaine dans un sous-dossier tmp du dépôt.
# Mais vous pouvez le créer où vous le souhaiter !

mkdir -p tmp
bin/Debug/Goupile init tmp/test
```

Pour accéder à ce domaine via un navigateur web, vous pouvez le lancer à l'aide de la commande suivante :

```sh
# Avec cette commande, Goupile sera accessible via http://localhost:8889/
bin/Debug/goupile -C tmp/test/goupile.ini
```

> [!NOTE]
> Pour un mise en production, il est recommandé de faire fonctionner Goupile derrière un reverse proxy HTTPS comme par exemple NGINX.
