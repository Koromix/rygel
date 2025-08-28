# Vue générale

Le serveur Goupile est développé en C++. Le binaire compilé contient directement le moteur de base de données ([SQLite](https://sqlite.org/)), un serveur HTTP ainsi que le code HTML/CSS/Javascript envoyé aux navigateurs web.

Le client web est développé en HTML, CSS et Javascript. La modification des pages via le code Javascript utilise très largement des [templates lit-html](https://lit.dev/docs/templates/overview/). L'ensemble du code JavaScript et CSS est assemblé (bundling) et minifié par [esbuild](https://esbuild.github.io/). Les fichiers statiques (images, etc.) et les fichiers générés par esbuild sont inclus dans le binaire du serveur Goupile au moment de la compilation.

# Bases de données

Plusieurs bases de données SQLite sont créées et utilisées pour chaque domaine. Tout d'abord, il existe une base maitre qui contient la liste des projets, des utilisateurs et les permissions. Ensuite, chaque projet utilise 1 à plusieurs bases (1 pour le projet + 1 par centre en cas de projet multi-centrique). Par exemple, un domaine avec 2 projets dont un multi-centrique pourrait utiliser les fichiers suivants :

```sh
goupile.db # Base principale
instances/projet1.db
instances/projet2.db
instances/projet2@lille.db
instances/projet2@paris.db
```

# Isolation des services

Chaque domaine est géré par un service dédié (par exemple lancé par systemd), qui est capable de s'auto-containériser sur Linux (utilisation des capabilities POSIX, des namespaces et filtres seccomp) dans un namespace avec pratiquement aucun accès sauf aux fichiers SQLite.

Ce processus garde malgré tout la capacité de se diviser (fork), et un processus enfant est créé pour gérer chaque projet présent sur le domaine. Ceci permet une isolation de chaque projet dans un processus dédié, qui a en charge les accès HTTP (statiques et API) pour ce projet.

# Options de compilation

En plus de la containerisation, plusieurs options de compilation Clang sont utilisées pour mitiger la vulnérabilité du serveur en cas de faille. Lors de la compilation de Goupile décrite plus loin, il s'agit du mode Paranoid.

Plusieurs mesures sont destinées à empêcher les attaques par corruption de la pile d'appels ou de détournement du flux d'exécution :

- *Stack Smashing Protection* (et Stack Clash Protection) pour limiter les attaques par corruption de pile
- *Safe Stack* pour limiter les attaques de type ROP
- *Compilation en PIE* pour le support ASLR (qui complète la mesure précédente)
- *CFI (Control Flow Integrity)* : coarse grained forward-edge protection
- *Options de lien* : relro, noexecstack, separate-code

Par ailleurs, pendant le développement nous utilisons différents sanitizers (ASan, TSan et UBsan) pour détecter des erreurs d'accès mémoire, de multi-threading et l'utilisation de comportements non définis en C/C++.

# Format des données

Le script des formulaires d'un projet sont stockés et versionnés dans les bases SQLite.

Les données saisies dans un projet sont stockées dans la base SQLite correspondante (pour les études multi-centriques, chaque centre dispose d'une base séparée). Deux tables SQLite sont utilisées pour les données :

- *Table rec_threads*: cette table contient une ligne par enregistrement
- *Table rec_entries* : cette table contient une ligne par page d'enregistrement, avec les informations récapitulatives
- *Table rec_fragments* : cette table contient toutes les modifications historisées de chaque page de chaque enregistrement (audit trail)

La clé principale d'un enregistrement est au [format ULID](https://github.com/ulid/spec). Ceci permet de générer les identifiants d'enregistrement du côté client (avec risque infinitésimal de collision) ce qui simplifie l'implémentation du mode hors ligne, tout en évitant les problèmes de performance posés par l'indexation des identifiants UUIDv4.

# Validation des données

Les contrôles de validité des données par rapport aux contraintes s’effectuent côté client (de manière systématique) et côté serveur uniquement lorsque l’administrateur du projet en fait la demande avec la permission `Batch`.

Pour effectuer une validation des données côté serveur en mode batch, Goupiel crée un processus *zygote* qui s’exécute en parallèle. Lorsqu’une validation des données doit avoir lieu, une requête contenant les données nécessaires est envoyée au processus zygote, lequel se duplique et exécute les scripts dans un espace de noms (namespace) fortement restreint, sans accès au système de fichiers principal, avec des permissions limitées et des appels système filtrés (*seccomp*).

Ces vérifications reposent sur le code JavaScript de chaque page, qui peut définir des conditions et signaler des erreurs en fonction des données saisies. Les erreurs sont consignées dans les métadonnées en parallèle de chaque enregistrement de données.

# Exécution de Goupile en mode debug

Pour travailler sur Goupile, utilisez des builds en mode debug. Commencez par cloner le dépôt principal, puis compilez l’outil de build (felix) à l’aide du script bootstrap.

```sh
git clone https://codeberg.org/Koromix/rygel.git
./bootstrap.sh
```

Une fois cette étape terminée, vous pouvez utiliser felix pour compiler et exécuter directement le binaire goupile. Vous devez commencer par créer un nouveau domaine, puis le lancer, avec les commandes suivantes :

```sh
cd rygel
mkdir tmp

./felix --run goupile init tmp/domain # Crée le domaine
./felix --run goupile -C tmp/domain   # Exécute le domaine
```

Vous pouvez ensuite accéder à Goupile depuis votre navigateur. Par défaut, il fonctionne sur le port 8889, donc l’adresse est `http://localhost:8889/`. Accédez au panneau d’administration via `http://localhost:8889/admin/`.

> [!NOTE]
> L’initialisation d’un domaine génèrera une clé de récupération d’archive que vous devez stocker afin de pouvoir restaurer une archive créée depuis le panneau d’administration du domaine Goupile. Si cette clé est perdue, elle peut être remplacée, mais les archives créées avec la clé précédente ne seront pas récupérables !

Consultez la [page sur l'auto-hébergement](diy#compilation) pour en savoir davantage sur la compilation de Goupile.
