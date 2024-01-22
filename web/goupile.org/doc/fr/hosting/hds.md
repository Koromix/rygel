# Détails HDS

## Environnements et serveurs

Nos serveurs HDS sont déployés automatiquement à l'aide de scripts Ansible, qui sont exécutés par notre hébergeur [GPLExpert](https://gplexpert.com/) (sous-traitant HDS et infogérance).

Nous utilisons deux environnements de déploiement : un environnement de pré-production (qui gère les sous-domaines \*.preprod.goupile.fr) et un environnement de production. L'environnement de pré-production est identique à la production et nous permet de tester nos scripts de déploiement. Il ne contient que des domaines et données de test.

Chaque environnement utilise deux serveurs :

- *Serveur proxy*, qui filtre les connexions via NGINX et nous permet de rapidement rediriger les requêtes (vers un autre back-end) en cas de problème.
- *Serveur back-end*, qui contient les services et bases de données Goupile. Les serveurs Goupile sont accessibles derrière un deuxième service NGINX qui tourne sur le serveur back-end.

La communication entre le serveur proxy et le serveur back-end a lieu via un canal sécurisé (IPSec et TLS 1.2+). Les échanges entre les deux services NGINX sont protégés par des certificats serveur et client signés par un certificat interne créé au moment du déploiement (et donc la clé privée est supprimée immédiatement).

## Plan de reprise d'activité [WIP]

Les environnements serveur sont configurés intégralement par des scripts Ansible automatisés et peuvent être reproduits à l'identique en quelques minutes.

La restauration des données après perte du serveur principal peut être effectuée à partir de plusieurs sources :

- Bases répliquées en continu sur un autre serveur [WIP]
- Backup nocturne chiffré des bases SQLite réalisé et copié sur un serveur à part dans un autre datacenter [WIP]
- Snapshot des VPS réalisé chaque nuit et conservé 14 jours, qui peut être restauré rapidement par GPLExpert
