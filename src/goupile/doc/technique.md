Goupile : documentation technique
======

Goupile est un éditeur de formulaires pour le recueil de données pour la recherche. Il s’agit d’une application web utilisable sur ordinateur, sur mobile et hors ligne.
L’association InterHop est l’éditeur du logiciel libre Goupile.

Le présent document détaille les choix architecturaux mis en place dans Goupile.

Il s'agit d'un outil en développement et certains choix sont encore en cours. Les indications **[WIP]** désignent des fonctionnalités ou options en cours de développement ou d'amélioration.

# Présentation générale

Goupile est un outil de conception de Clinical Report Forms (CRF), libre et gratuit, qui s’efforce de rendre la création de formulaires et la saisie de données à la fois puissantes et faciles.

Le Clinical Report Form (CRF) est un document (traditionnellement papier) destiné à recueillir les données des personnes incluses dans un projet de recherche, que ce soit au moment de l’inclusion dans un projet de recherche ou, par la suite, dans le suivi de la personne.
Ces documents sont de plus en plus remplacés (ou complétés) par des portails numériques, on parle alors d’eCRF (’‘e’’ pour electronic).

Goupile permet de concevoir un eCRF avec une approche un peu différente des outils habituels, puisqu’il s’agit de programmer le contenu des formulaires, tout en automatisant les autres aspects communs à tous les eCRF :

- Types de champs préprogrammés et flexibles
- Publication des formulaires
- Enregistrement et synchronisation des données
- Recueil en ligne et hors ligne, sur ordinateur, tablette ou mobile
- Gestion des utilisateurs et droits

En plus des fonctionnalités habituelles, nous nous sommes efforcés de réduire au maximum le délai entre le développement d’un formulaire et la saisie des données.

## Domaines et projets

Chaque service Goupile dispose d'un domaine (ou sous-domaine). Par exemple, *demo.goupile.fr* et *psy-lille.goupile.fr* sont des services distincts avec des bases de données séparées et des utilisateurs différents (même si possiblement hébergés sur un même serveur).

Lors de la création d'un domaine, un (ou plusieurs) administrateurs de confiance sont désignés pour en gérer les projets et les utilisateurs. Une paire de clé de chiffrement est générée pour réaliser les backups des bases de données du domaine. La clé publique est stockée sur le serveur pour créer les backups. La clé privée est confiée aux administrateurs désignés et n'est pas stockée; sa perte **entraîne la perte de tous les backups** de ce domaine.

*Les détails sur le chiffrement utilisé sont détaillés dans la section sur les [choix architecturaux](#Choix-architecturaux).*

Ce sont les administrateurs qui peuvent créér les projets et leur affecter des utilisateurs, soit pour qu'ils conçoivent les formulaires, soit pour qu'ils y saisissent les données.

## Gestion des utilisateurs

Chaque domaine Goupile contient une liste d'utilisateurs.

Ces comptes utilisateurs sont gérés par le ou les administrateurs désignés pour ce domaine, qui peuvent les créer, les modifier et les supprimer.

Chaque utilisateur peut être affecté à un ou plusieurs projets, avec un ensemble de droits en fonction de ses préprogatives. Il existe deux ensembles de droits :

* Droits de développement, qui permettent de configurer un projet et ses formulaires
* Droits d'accès, qui permettent d'accéder aux données

Ces droits sont détaillés dans les tableaux qui suivent :

| Droit       | Explication |
| ------------| ----------- |
| *Develop*   | Modification des formulaires |
| *Publish*   | Publication des formulaires modifiés |
| *Configure* | Configuration du projet et des centres (multi-centrique) |
| *Assign*    | Modification des droits des utilisateurs sur le projet |

| Droit    | Explication |
| -------- | ----------- |
| *Load*   | Lecture des enregistrements |
| *Save*   | Modification des enregistrements |
| *Export* | Export facile des données (CSV, XLSX, etc.) |
| *Batch*  | Recalcul de toutes les variables calculées sur tous les enregistrements |

Par défaut l'authentification des utilisateurs repose sur un couple identifiant / mot de passe. Ce mot de passe est stocké hashé en base (libsodium pwhash).

Plusieurs modes d'authentification forte sont disponibles ou prévus : 

* Fichier clé supplémentaire stocké sur clé USB (ce mode présente l'avantage d'être compatible avec une utilisation chiffrée hors ligne)
* **[WIP]** Support de tokens TOTP avec applications d'authentification (Authy, FreeOTP, Google Authenticator, etc.)
* **[WIP]** Support de clés physiques avec Webauthn (Yubikey, etc.)

# Utilisation de Goupile

Goupile est codé comme une application web de type SPA (Single Page Application). Un bref aperçu des différentes facettes de Goupile est donné ci-après; référez-vous au manuel utilisateur pour des informations plus détaillées.

## Conception d'un eCRF

Lors de la conception d'un formulaire, l'utilisateur le programme en Javascript via un éditeur texte en ligne, en appelant des fonctions prédéfinies qui génèrent les champs voulus par l'utilisateur. L'eCRF s'affiche directement sur le même écran.

Pour créer un eCRF, l'utilisateur commence par définir l'organisation et la succession des pages de saisie et des tables de données sous-jacentes. Il est possible de créer simplement des eCRF à plusieurs tables avec des relations 1-à-1 et 1-à-N (parent-enfants) à partir de ce mode.

![](https://goupile.fr/static/screenshots/editor.webp)

Le contenu des pages est également défini en Javascript. Le fait de programmer nous donne beaucoup de possibilités, notamment la réalisation de formulaires complexes (conditions, boucles, calculs dynamiques, widgets spécifiques, etc.), sans sacrifier la simplicité pour les formulaires usuels.

## Validation des données

La vérification de la validité des données par rapport aux contraintes imposées a lieu côté client (systématiquement) et côté serveur (sur les pages où cette option est activée). Celle-ci repose sur le code Javascript de chaque page, qui peut définir des conditions et des erreurs en fonction des données saisies.

Ces erreurs alimentent la base de *contrôles qualités* qui peuvent ensuite être monitorées **[WIP]**.

Pour assurer la sécurité du serveur malgré l'exécution de code Javascript potentiellement malveillant, plusieurs mesures sont prises et détaillées dans la section [Architecture du serveur](#Architecture-du-serveur).

## Support hors ligne

Les eCRF développés avec Goupile peuvent fonctionner en mode hors ligne (si cette option est activée). Dans ce cas, Goupile utilise les fonctionnalités PWA des navigateurs modernes pour pouvoir mettre en cache ses fichiers et les scripts des formulaires, et être installé en tant que pseudo-application native.

Dans ce cas, les données sont synchronisées dans un deuxième temps lorsque la connexion est disponible.

Les données hors ligne sont chiffrées symétriquement à l'aide d'une clé spécifique à chaque utilisateur et connue du serveur. Cette clé est communiquée au navigateur après une authentification réussie.

Pour que l'utilisateur puisse se connecter à son application hors ligne, une copie de son profil (dont la clé de chiffrement des données hors ligne) est stockée sur sa machine, chiffrée par une clé dérivée de son mot de passe. Lorsqu'il cherche à se connecter hors ligne, son identifiant et son mot de passe sont utilisés pour déchiffrer ce profil et pouvoir accéder aux données locales.

Si le client installable est utilisé (basé sur Electron), l'authentification hors ligne peut aussi être configurée en mode fort, avec nécessité de brancher une clé USB contenant une seconde clé de chiffrement pour pouvoir se connecter à l'eCRF.

# Architecture du serveur

## Vue générale

Le serveur Goupile est développé en C++. Le binaire compilé contient directement le moteur de base de données ([SQLite](https://sqlite.org/)), un serveur HTTP ([libmicrohttpd](https://www.gnu.org/software/libmicrohttpd/)) ainsi que le code HTML/CSS/Javascript envoyé aux navigateurs web.

Plusieurs bases de données SQLite sont créées et utilisées pour chaque domaine. Tout d'abord, il existe une base maitre qui contient la liste des projets, des utilisateurs et les permissions. Ensuite, chaque projet utilise 1 à plusieurs bases (1 pour le projet + 1 par centre en cas de projet multi-centrique). Par exemple, un domaine avec 2 projets dont un multi-centrique pourrait utiliser les fichiers suivants :

```bash
goupile.db # Base principale
instances/projet1.db
instances/projet2.db
instances/projet2@lille.db
instances/projet2@paris.db
```

*Le support de PostgreSQL pour pouvoir déporter la base de données sur une autre machine est prévu pour plus tard **[WIP]**.*

## Isolation des services

Chaque domaine est géré par un service dédié (par exemple lancé par systemd), qui est capable de s'auto-containériser sur Linux (utilisation des capabilities POSIX, des namespaces et filtres seccomp) dans un namespace avec pratiquement aucun accès sauf au fichier SQLite.

Ce service peut utiliser un seul (mode mono-processus) ou plusieurs processus (mode multi-processus **[WIP]**) pour gérer chaque projet. Dans le mode multi-processus, la communication HTTP est relayée par le processus maître au processus en charge de la gestion du projet concerné.

Dans tous les cas, lorsque le serveur valide les données du formulaire (option non systématique selon les besoins de validation de données d'un formulaire), le code Javascript est exécuté par le moteur V8 (en mode jitless) dans un processus forké avec des droits complètement restreints (pas d'accès au système de fichier ou à la base de données).

## Options de compilation

En plus de la containerisation, plusieurs options de compilation Clang sont utilisées pour mitiger la vulnérabilité du serveur en cas de faille. Lors de la compilation de Goupile décrite plus loin, il s'agit du *mode Paranoid*.

Plusieurs mesures sont destinées à empêcher les attaques par corruption de la pile d'appels ou de détournement du flux d'exécution :

* *Stack Smashing Protection* (et Stack Clash Protection) pour limiter les attaques par corruption de pile
* *Safe Stack* pour limiter les attaques de type ROP
* *Compilation en PIE* pour le support ASLR (qui complète la mesure précédente)
* *CFI (Control Flow Integrity)* : coarse grained forward-edge protection
* *Options de lien* : relro, noexecstack, separate-code

Par ailleurs, pendant le développement nous utilisons *différents sanitizers (ASan, TSan et UBsan)* pour détecter des erreurs d'accès mémoire, de multi-threading et l'utilisation de comportements non définis en C/C++.

## Format des données

Chaque base de données Goupile est chiffrée au repos ([SQLite3 Multiple Ciphers](https://github.com/utelle/SQLite3MultipleCiphers)). La clé de chiffrement de la base principale est communiquée à Goupile lors du lancement par un moyen à déterminer par la personne qui administre le serveur. Chaque autre base a une clé spécifique stockée dans la base principale.

Le script des formulaires d'un projet sont stockés et versionnées dans les bases SQLite.

Les données saisies dans un projet sont stockées dans la base SQLite correspondante (pour les études multi-centriques, chaque centre dispose d'une base séparée). Deux tables SQLite sont utilisées pour les données : 

* *Table rec_entries* : cette table contient une ligne par enregistrement avec les informations récapitulatives
* *Table rec_fragments* : cette table contient toutes les modifications historisées d'un enregistrement (audit trail)

La clé principale d'un enregistrement est au [format ULID](https://github.com/ulid/spec). Ceci permet de générer les identifiants d'enregistrement client (avec risque infinitésimal de collision) ce qui simplifie l'implémentation du mode hors ligne, tout en évitant les problèmes de performance posés par l'indexation des identifiants UUID.

# Installation de Goupile

## Compilation

Le serveur Goupile est multi-plateforme, mais il est **recommandé de l'utiliser sur Linux** pour une sécurité maximale.

En effet, sur Linux Goupile peut fonctionner en mode sandboxé grâce à seccomp et les espaces de noms Linux (appel système unshare). Le support du sandboxing est envisagé à long terme pour d'autres systèmes d'exploitation mais n'est pas disponible pour le moment. L'utilisation de la distribution Debian 10+ est recommandée.

Goupile repose sur du C++ (côté serveur) et HTML/CSS/JS (côté client). La compilation de Goupile utilise un outil dédié qui est inclus directement dans le dépôt.

Commencez par récupérer le code depuis le dépôt Git : https://framagit.org/interhop/goupile

```bash
git clone https://framagit.org/interhop/goupile
cd goupile
```

### Linux

Pour compiler une **version de développement** et de test procédez comme ceci depuis la racine du dépôt :

```bash
# Préparation de l'outil Felix utilisé pour compiler Goupile
./bootstrap.sh

# L'exécutable sera déposé dans le dossier bin/DebugSlow
./felix
```

Pour une **utilisation en production**, il est recommandé de compiler Goupile [en mode Paranoid](#Options-de-compilation) à l'aide de Clang 11+ et le lieur LLD 11+. Sous Debian 10, vous pouvez faire comme ceci :

```bash
# Préparation de l'outil Felix utilisé pour compiler Goupile
./bootstrap.sh

# Installation de LLVM décrite ici et recopiée ci-dessous : https://apt.llvm.org/
sudo bash -c "$(wget -O - https://apt.llvm.org/llvm.sh)"
sudo apt install clang-11 lld-11

# L'exécutable sera déposé dans le dossier bin/Paranoid
./felix -pParanoid --compiler=clang-11:lld-11
```

### Autres systèmes

Pour compiler une **version de développement** et de test procédez comme ceci depuis la racine du dépôt :

#### Systèmes POSIX (macOS, WSL, etc.)

```bash
# Préparation de l'outil Felix utilisé pour compiler Goupile
./bootstrap.sh

# L'exécutable sera déposé dans le dossier bin/DebugSlow
./felix
```

#### Windows

```bash
# Préparation de l'outil Felix utilisé pour compiler Goupile
# Il peut être nécessaires d'utiliser l'environnement console de
# Visual Studio avant de continuer
bootstrap.bat

# L'exécutable sera déposé dans le dossier bin/DebugSlow
felix
```

Il n'est pas recommandé d'utiliser Goupile en production sur un autre système, car le mode bac à sable (sandboxing) et la compilation en mode Paranoid n'y sont pas disponibles pour le moment.

Cependant, vous pouvez utiliser la commande `./felix --help` (ou `felix --help` sur Windows) pour consulter les modes et options de compilations disponibles pour votre système.

## Déploiement manuel

Une fois l'exécutable Goupile compilé, il est possible de créer un domaine Goupile à l'aide de la commande suivante :

```bash
# Pour cet exemple, nous allons créer ce domaine dans un sous-dossier tmp
# du dépôt, mais vous pouvez le créer où vous le souhaiter !
mkdir tmp

# L'exécution de cette commande vous demandera de créer un premier
# compte administrateur et de définir son mot de passe.
bin/Paranoid/goupile init tmp/domaine_test
```

L'initilisation de ce domaine va créer **une clé de récupération d'archive** que vous devez stocker afin de pouvoir restaurer une archive créée dans le panneau d'administration du domaine Goupile. Si elle est perdue, cette clé peut être modifiée mais les archives créées avec la clé précédente ne seront pas récupérables !

Pour accéder à ce domaine via un navigateur web, vous pouvez le lancer à l'aide de la commande suivante :

```bash
# Avec cette commande, Goupile sera accessible via http://localhost:8888/
bin/Paranoid/goupile -C tmp/domaine_test/goupile.ini
```

Pour un mise en production, il est *recommandé de faire fonctionner Goupile derrière un reverse proxy HTTPS* comme par exemple NGINX.

Pour automiser le déploiement d'un serveur complet en production (avec plusieurs domaines Goupile et NGINX configuré automatiquement), nous *fournissons un playbook et des rôles Ansible* prêts à l'emploi que vous pouvez utiliser tel quel ou adapter à vos besoins.

## Déploiement automatisé via Ansible (Debian 10+)

Les scripts Ansible fournis sont adaptés à un déploiement sécurisé sur Debian 10+. Ils peuvent théoriquement être utilisés et/ou adaptés pour d'autres systèmes mais ceci n'est pas testé régulièrement.

Ce playbook PKnet est configuré pour installer les services suivants :

* *Goupile*
* *Nginx* : reverse proxy HTTP (avec auto-configuration Let's Encrypt optionelle)
* *Borg* : backups quotidiens des bases de données SQLite utilisées par Goupile
* *Prometheus et Grafana* : surveillance des machines

Dans ce playbook, ces services sont répartis sur 3 machines :

* *host2* (machine principale avec Goupile)
* *backup* (stockage des backups)
* *monitor* (collectionneur Prometheus et tableau de bord Grafana)

Vous pouvez tester rapidement ce playbook à l'aide du script Vagrant qui est inclus dans le dépôt à l'aide des commandes suivantes :

```bash
cd deploy
vagrant up --no-provision
vagrant provision
```

Les domaines de test suivants seront alors configurés et accessibles sur la machine locale :

* https://goupile1.pknet.local/ : domaine Goupile (HTTPS via certificat auto-signé)
* https://goupile2.pknet.local/ : domaine Goupile (HTTPS via certificat auto-signé)
* https://pknet-monitor.local/grafana : tableau de bord de surveillance préconfiguré

Le playbook est défini par *deploy/pknet.yml* et l'inventaire Vagrant qui sert d'exemple est défini dans *deploy/inventories/vagrant/hosts.yml*. Vous pouvez copier l'inventaire et l'adapter pour configurer votre propre environnement de production, avec vos propres machines et vos propres domaines. Celui-ci contient des commentaires qui expliquent les différents réglages disponibles.

# Configuration serveur HDS

## Environnements et serveurs

Nos serveurs HDS sont déployés automatiquement à l'aide de scripts Ansible, qui sont exécutés par notre hébergeur GPLExpert (sous-traitant HDS et infogérance).

Nous utilisons deux environnements de déploiement : un environnement de pré-production (qui gère les sous-domaines `*.preprod.goupile.fr`) et un environnement de production. L'environnement de pré-production est identique à la production et nous permet de tester nos scripts de déploiement. Il ne contient que des domaines et données de test.

Chaque environnement utilise deux serveurs : 
* *Serveur proxy*, qui filtre les connexions via NGINX et nous permet de rapidement rediriger les requêtes (vers un autre back-end) en cas de problème.
* *Serveur back-end*, qui contient les services et bases de données Goupile. Les serveurs Goupile sont accessibles derrière un deuxième service NGINX qui tourne sur le serveur back-end.

La communication entre le serveur proxy et le serveur back-end a lieu via un canal sécurisé (IPSec et TLS 1.2+). Les échanges entre les deux services NGINX sont protégés par des certificats serveur et client signés par un certificat interne créé au moment du déploiement (et donc la clé privée est supprimée immédiatement).

## Plan de reprise d'activité **[WIP]**

Les environnements serveur sont configurés intégralement par des scripts Ansible automatisés et peuvent être reproduits à l'identique en quelques minutes.

La restauration des données après perte du serveur principal peut être effectuée à partir de plusieurs sources :

1. Bases répliquées en continu sur un autre serveur **[WIP]** 
2. Backup nocturne chiffré des bases SQLite réalisé et copié sur un serveur à part dans un autre datacenter **[WIP]** 
3. Snapshot des VPS réalisé chaque nuit et conservé 14 jours, qui peut être restauré en une heure par GPLExpert
