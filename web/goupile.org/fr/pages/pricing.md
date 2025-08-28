# Grille tarifaire

## Hébergement HDS

Prestation | Description |Tarif
---------- | ----------- | -----
**Projet HDS** (structure publique ou association) | Hébergement d’un eCRF unique par nos soins | 150€/mois
**Projet HDS** (structure privée) | Hébergement d’un eCRF unique par nos soins | 300€/mois
**Forfait HDS** | Hébergement d’un domaine Goupile (avec nombre d’eCRF illimité) par nos soins | Sur devis

## Aide à la conception

Prestation | Description | Tarif
---------- | ----------- | -----
**Aide à la conception** | Demandes par tickets et mails pour l’aide au développement de votre eCRF | 100€/mois
**Création eCRF** | Développement d’un eCRF complet par notre équipe | Sur devis
**Fonctionnalités** | Ajout de fonctionnalités spécifiques à Goupile pour vos besoins | Sur devis

# Détails HDS

## Environnements et serveurs

Nos serveurs HDS sont déployés automatiquement à l'aide de scripts Ansible, qui sont exécutés par notre hébergeur [GPLExpert](https://gplexpert.com/) (sous-traitant HDS et infogérance).

Nous utilisons deux environnements de déploiement : un environnement de pré-production (qui gère les sous-domaines \*.preprod.goupile.fr) et un environnement de production. L'environnement de pré-production est identique à la production et nous permet de tester nos scripts de déploiement. Il ne contient que des domaines et données de test.

Chaque environnement utilise deux serveurs :

- *Serveur proxy*, qui filtre les connexions via NGINX et nous permet de rapidement rediriger les requêtes (vers un autre back-end) en cas de problème.
- *Serveur back-end*, qui contient les services et bases de données Goupile. Les serveurs Goupile sont accessibles derrière un deuxième service NGINX qui tourne sur le serveur back-end.

La communication entre le serveur proxy et le serveur back-end a lieu via un canal sécurisé (IPSec et TLS 1.2+). Les échanges entre les deux services NGINX sont protégés par des certificats serveur et client signés par un certificat interne créé au moment du déploiement (et dont la clé privée est supprimée immédiatement).

## Plan de reprise d'activité

Les environnements serveur sont configurés intégralement par des scripts Ansible automatisés et peuvent être reproduits à l'identique en quelques minutes.

La restauration des données après perte du serveur principal peut être effectuée à partir des snapshots des VPS réalisés chaque nuit et conservés 14 jours, qui peuvent être restaurés rapidement par GPLExpert.
