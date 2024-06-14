# Vue générale

Le serveur Goupile est développé en C++. Le binaire compilé contient directement le moteur de base de données ([SQLite](https://sqlite.org/)), un serveur HTTP ([libmicrohttpd](https://www.gnu.org/software/libmicrohttpd/)) ainsi que le code HTML/CSS/Javascript envoyé aux navigateurs web.

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

Pour valider les données envoyées par le client lors de l'enregistrement d'un formulaire, le serveur exécute le code JavaScript de ce formulaire avec le moteur SpiderMonkey (utilisé dans Firefox) compilé en [WebAssembly/WASI](https://github.com/bytecodealliance/spidermonkey-wasi-embedding), retransformé en C avec [wasm2c](https://github.com/WebAssembly/wabt/blob/main/wasm2c/README.md) et utilisé via [RLBox](https://rlbox.dev/). Avant d'être interpréré par SpiderMonkey/WASM, les données envoyées par le client (au format JSON) sont parsées et recréées une première fois avec [RapidJSON](https://rapidjson.org/).

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

- *Table rec_entries* : cette table contient une ligne par enregistrement avec les informations récapitulatives
- *Table rec_fragments* : cette table contient toutes les modifications historisées d'un enregistrement (audit trail)

La clé principale d'un enregistrement est au [format ULID](https://github.com/ulid/spec). Ceci permet de générer les identifiants d'enregistrement du côté client (avec risque infinitésimal de collision) ce qui simplifie l'implémentation du mode hors ligne, tout en évitant les problèmes de performance posés par l'indexation des identifiants UUIDv4.

# Validation des données

La vérification de la validité des données par rapport aux contraintes imposées a lieu côté client (systématiquement) et côté serveur (via SpiderMonkey/WASM comme décrit auparant).

Celle-ci repose sur le code Javascript de chaque page, qui peut définir des conditions et des erreurs en fonction des données saisies. Ces erreurs alimentent la base de *contrôles qualités* qui peuvent ensuite être monitorées.

# Support hors ligne

Les eCRF développés avec Goupile peuvent fonctionner en mode hors ligne (si cette option est activée). Dans ce cas, Goupile utilise les fonctionnalités PWA des navigateurs modernes pour pouvoir mettre en cache ses fichiers et les scripts des formulaires, et être installé en tant que pseudo-application native.

Dans ce cas, les données sont synchronisées dans un deuxième temps lorsque la connexion est disponible.

Les données hors ligne sont chiffrées symétriquement à l'aide d'une clé spécifique à chaque utilisateur et connue du serveur. Cette clé est communiquée au navigateur après une authentification réussie.

Pour que l'utilisateur puisse se connecter à son application hors ligne, une copie de son profil (dont la clé de chiffrement des données hors ligne) est stockée sur sa machine, chiffrée par une clé dérivée de son mot de passe (with [PBKDF2](https://developer.mozilla.org/en-US/docs/Web/API/SubtleCrypto/deriveBits)). Lorsqu'il cherche à se connecter hors ligne, son identifiant et son mot de passe sont utilisés pour déchiffrer ce profil et pouvoir accéder aux données locales.
