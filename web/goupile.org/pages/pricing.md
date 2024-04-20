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

## Formation

Les séances de formations sont actuellement réalisées en visio avec un formateur habitué à l’utilisation de Goupile.

Séance | Description | Durée | Tarif
------ | ----------- | ----- | -----
**Prise en main** | Présentation de Goupile (suivi, données, conception, recueil), conception de recueils simples (utilisation des champs, organisation en sections), publication | 1 à 2 heures | 100€/participant
**Avancé** | Création d’un eCRF complet (conditions, gestion des erreurs, pages et tables multiples et liées entre elles) | 2 heures | 100€/participant
~~**Monitoring**~~ | ~~Work in progress~~ | ~~2 heures~~ | ~~100€/participant~~

Il est également possible d’organiser des formations plus spécifiques en fonction de vos besoins (par exemple : intégration d’une expérience dynamique avec diffusion de vidéos et réponses à des questions réalisable en ligne).

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
