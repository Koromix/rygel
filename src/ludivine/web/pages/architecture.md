# Architecture de données

L'architecture de données du logiciel [Ludivine](https://codeberg.org/Koromix/ludivine), qui sous-tend la plateforme Lignes de Vie, a été conçu selon deux principes fondateurs :

- Utiliser un chiffrement de bout-en-bout (*end-to-end*) pour toutes les données qui n'ont pas vocation à être utilisées pour la recherche.
- Maintenir une séparation nette entre les données identifiantes (mail), les données privées (avatar, journal de bord, etc.) et les données de recherche.

## Inscription

Lors de l'inscription d'un participant, le client Javascript gènère plusieurs informations :

- **VID** (vault ID) : identifiant aléatoire de type [UUIDv4](https://en.wikipedia.org/wiki/Universally_unique_identifier#Version_4_(random))
- **RID** (research ID) : identifiant aléatoire de type UUIDv4
- **VKEY** (vault key) : clé aléatoire de chiffrement de 32 octets utilisé pour une base SQLite en chiffrement AES 256 (mode CBC)

Le client génère également une clé aléatoire **TKEY**, avec laquelle il chiffre les 3 informations sus-citées (VID, RID, VKEY) pour former un **token** chiffré (XSalsa20-Poly1305).

Ces informations sont transmises au serveur via une requête POST contenant *l'adresse e-mail du participant, le VID, le RID, le token et la TKEY*.

<div class="columns">
    <img src="{{ ASSET ../assets/architecture/signup.svg }}" width="800" height="550" alt=""/>
</div>

Le serveur vérifie qu'aucun compte n'existe pour l'adresse mail fournie, puis il effectue les actions suivantes :

- Création d'une ligne utilisateur (user) indexée par un UID aléatoire (généré par le serveur), contenant le mail et le token chiffré
- Création d'une ligne de coffre-fort (vault) correspondant au VID généré
- Création d'une ligne de participant (research) correspondant au RID généré

Il envoie par la suite un courriel de connexion à l'utilisateur contenant un lien magique (*magic link*) qui contient deux informations :

- **UID** : identifiant utilisateur aléatoire de type UUIDv4
- **TKEY** : clé permettant de déchiffrer le token utilisateur qui a été envoyée par le client

Une fois le courriel de connexion envoyé, le serveur **n'enregistre pas la TKEY** qui n'existe donc que dans le mail de connexion initial. Cette architecture permet de découpler le compte utilisateur (*UID*), le coffre-fort chiffré (*VID*) et les données de recherche (*RID*) puisque seul le token chiffré permet de lier un UID au VID et au RID. Ce token ne peut être lu qu'avec la *TKEY* qui est présente dans le mail de connexion, et n'est pas connue par le serveur

> [!IMPORTANT]
> Ceci implique qu'en cas de perte du mail de connexion, il n'est **pas possible de relier un utilisateur** (*UID ou mail*) à son coffre-fort (*VID*) ou bien à ses données de recherche (*RID*) !

## Connexion

Pour se connecter, l'utilisateur utilise le **lien magique** (*magic link*) présent dans le mail de connexion. Cet lien contient deux informations essentielles dans la partie hash (#) :

- **UID** : identifiant utilisateur de type UUIDv4
- **TKEY** : clé de déchiffrement du token chiffré de l'utilisateur

> [!NOTE]
> L'utilisation de la *partie hash de l'URL (#)* permet d'éviter l'envoi de ces paramètres au serveur lors de la requête GET générée par l'ouverture du lien. Ainsi, en dehors de l'envoi du mail initial au moment de l'inscription, la *TKEY* ne transite jamais par le serveur !

Le client Javascript transmet la valeur *UID* au serveur (1), qui répond en envoyant le *token chiffré* (2). Le client utilise la *TKEY* (3b) pour déchiffrer le token et récupérer 3 valeurs :

- **VID** : identifiant de coffre-fort/vault
- **RID** : identifiant de recherche
- **VKEY** : clé aléatoire de chiffrement de 32 octets pour accéder au coffre-fort

<div class="columns">
    <img src="{{ ASSET ../assets/architecture/login.svg }}" width="800" height="587" alt=""/>
</div>

Le client demande alors au serveur de lui envoyer le coffre-fort/vault **VID** (4). S'il existe, il s'agit d'une base de données SQLite3 chiffrée (SQLCipher AES 256 bit), la clé de chiffrement étant la **VKEY**. Cette base est stockée et lue côté client, la clé VKEY ne quitte jamais la machine client. Le serveur se content d'envoyer au client la dernière version de la base chiffrée (5). Le client ouvre cettte base SQLite à l'aide de la clé VKEY (6b).

Chaque modification du coffre-fort est répliquée vers le serveur sous forme chiffrée. Le serveur ne connait pas la VKEY et ne peut donc pas lire le coffre-fort, qui existe sous forme de base SQLite chiffrée.

## Enregistrements

Le serveur gère 3 flux de données par utilisateur :

- Le **coffre-fort/vault** contient toutes les données, il s'agit d'une base SQLite3 chiffrée (SQLCipher 256 bit) dont la clé VKEY n'est connue que par le client. Chaque modification faite sur cette base est répliquée physiquement vers le serveur.
- La **table recherche** contient les données destinées à la recherche.
- La **table de rappels** est alimentée par le client, qui indique au serveur les échéances des études auxquelles le participant est inscrit. Le client indique les échéances auxquelles le participant est attendu, afin que le serveur puisse le notifier par mail en cas de retard.

Chacun de ces flux a son propre identifiant (VID, RID, UID) et seul le client est en mesure de faire le lien entre ces identifiants, qui sont contenus dans le *token chiffré*.

<div class="columns">
    <img src="{{ ASSET ../assets/architecture/data.svg }}" width="800" height="547" alt=""/>
</div>

> [!IMPORTANT]
> Ceci implique qu'en cas de perte du mail de connexion, il n'est **pas possible de supprimer** les données de coffre-fort et les données de recherche du participant, qui sont considérées comme anonymes. Le serveur n'est pas en mesure de faire le lien entre un utilisateur (UID) et son VID ou son RID !
