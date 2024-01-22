# Architecture de Goupile

## Vue générale

Le serveur Goupile est développé en C++. Le binaire compilé contient directement le moteur de base de données ([SQLite](https://sqlite.org/)), un serveur HTTP ([libmicrohttpd](https://www.gnu.org/software/libmicrohttpd/)) ainsi que le code HTML/CSS/Javascript envoyé aux navigateurs web.

Plusieurs bases de données SQLite sont créées et utilisées pour chaque domaine. Tout d'abord, il existe une base maitre qui contient la liste des projets, des utilisateurs et les permissions. Ensuite, chaque projet utilise 1 à plusieurs bases (1 pour le projet + 1 par centre en cas de projet multi-centrique). Par exemple, un domaine avec 2 projets dont un multi-centrique pourrait utiliser les fichiers suivants :

```sh
goupile.db # Base principale
instances/projet1.db
instances/projet2.db
instances/projet2@lille.db
instances/projet2@paris.db
```

## Isolation des services

Chaque domaine est géré par un service dédié (par exemple lancé par systemd), qui est capable de s'auto-containériser sur Linux (utilisation des capabilities POSIX, des namespaces et filtres seccomp) dans un namespace avec pratiquement aucun accès sauf au fichier SQLite.

Ce service peut utiliser un seul (mode mono-processus) ou plusieurs processus (mode multi-processus **[WIP]**) pour gérer chaque projet. Dans le mode multi-processus, la communication HTTP est relayée par le processus maître au processus en charge de la gestion du projet concerné.

Dans tous les cas, lorsque le serveur valide les données du formulaire (option non systématique selon les besoins de validation de données d'un formulaire), le code Javascript est exécuté par le moteur SpiderMonkey dans un processus forké avec des droits complètement restreints (pas d'accès au système de fichier ou à la base de données).

## Options de compilation

En plus de la containerisation, plusieurs options de compilation Clang sont utilisées pour mitiger la vulnérabilité du serveur en cas de faille. Lors de la compilation de Goupile décrite plus loin, il s'agit du mode Paranoid.

Plusieurs mesures sont destinées à empêcher les attaques par corruption de la pile d'appels ou de détournement du flux d'exécution :

- *Stack Smashing Protection* (et Stack Clash Protection) pour limiter les attaques par corruption de pile
- *Safe Stack* pour limiter les attaques de type ROP
- *Compilation en PIE* pour le support ASLR (qui complète la mesure précédente)
- *CFI (Control Flow Integrity)* : coarse grained forward-edge protection
- *Options de lien* : relro, noexecstack, separate-code

Par ailleurs, pendant le développement nous utilisons différents sanitizers (ASan, TSan et UBsan) pour détecter des erreurs d'accès mémoire, de multi-threading et l'utilisation de comportements non définis en C/C++.

## Format des données

Chaque base de données Goupile est chiffrée au repos ([SQLite3 Multiple Ciphers](https://github.com/utelle/SQLite3MultipleCiphers)). La clé de chiffrement de la base principale est communiquée à Goupile lors du lancement par un moyen à déterminer par la personne qui administre le serveur. Chaque autre base a une clé spécifique stockée dans la base principale.

Le script des formulaires d'un projet sont stockés et versionnées dans les bases SQLite.

Les données saisies dans un projet sont stockées dans la base SQLite correspondante (pour les études multi-centriques, chaque centre dispose d'une base séparée). Deux tables SQLite sont utilisées pour les données :

- *Table rec_entries* : cette table contient une ligne par enregistrement avec les informations récapitulatives
- *Table rec_fragments* : cette table contient toutes les modifications historisées d'un enregistrement (audit trail)

La clé principale d'un enregistrement est au [format ULID](https://github.com/ulid/spec). Ceci permet de générer les identifiants d'enregistrement client (avec risque infinitésimal de collision) ce qui simplifie l'implémentation du mode hors ligne, tout en évitant les problèmes de performance posés par l'indexation des identifiants UUID.

## Validation des données

La vérification de la validité des données par rapport aux contraintes imposées a lieu côté client (systématiquement) et côté serveur (sur les pages où cette option est activée). Celle-ci repose sur le code Javascript de chaque page, qui peut définir des conditions et des erreurs en fonction des données saisies.

Ces erreurs alimentent la base de *contrôles qualités* qui peuvent ensuite être monitorées **[WIP]**.

Pour assurer la sécurité du serveur malgré l'exécution de code Javascript potentiellement malveillant, plusieurs mesures sont prises et détaillées dans la section [Architecture du serveur](#architecture-du-serveur).

## Support hors ligne

Les eCRF développés avec Goupile peuvent fonctionner en mode hors ligne (si cette option est activée). Dans ce cas, Goupile utilise les fonctionnalités PWA des navigateurs modernes pour pouvoir mettre en cache ses fichiers et les scripts des formulaires, et être installé en tant que pseudo-application native.

Dans ce cas, les données sont synchronisées dans un deuxième temps lorsque la connexion est disponible.

Les données hors ligne sont chiffrées symétriquement à l'aide d'une clé spécifique à chaque utilisateur et connue du serveur. Cette clé est communiquée au navigateur après une authentification réussie.

Pour que l'utilisateur puisse se connecter à son application hors ligne, une copie de son profil (dont la clé de chiffrement des données hors ligne) est stockée sur sa machine, chiffrée par une clé dérivée de son mot de passe. Lorsqu'il cherche à se connecter hors ligne, son identifiant et son mot de passe sont utilisés pour déchiffrer ce profil et pouvoir accéder aux données locales.
