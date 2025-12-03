# Gestion des utilisateurs

## Liste des utilisateurs

Les utilisateurs de Goupile sont partagés entre les différents projets, auxquels ils peuvent être assignés individuellement.

<div class="screenshot"><img src="{{ ASSET static/help/admin/users.webp }}" height="180" alt=""/></div>

Il existe donc une seule liste d'utilisateurs, peu importe le nombre de projets. Mais en cliquant sur l'action « Droits » d'un projet (dans la liste de projets), vous pouvez paramétrer les [droits de chaque utilisateur sur le projet](#systeme-de-droits) concerné.

> [!NOTE]
> Quand un projet est créé, l'utilisateur qui l'a créé a tous les droits sur celui-ci, et les autres utilisateurs n'en ont aucun.

## Classes d'utilisateurs

On distingue **trois classes d'utilisateurs** dans Goupile.

Les deux premières classes dépendent de l'activation de l'option « Super-administrateur » lors de la création ou de la modification d'un utilisateur :

- Les **utilisateurs normaux** n'ont pas accès au module d'administration, mais uniquement aux projets sur lesquels ils sont assignés
- Les **super-administrateurs** ont accès au module d'administration et peuvent modifier les projets, les utilisateurs et tous les droits

Lorsqu'un utilisateur normal a le *droit d'administration* sur au moins un projet, il est promu en **utilisateur administrateur** ce qui lui donne un accès limité au module d'administration.

> [!NOTE]
> Les utilisateurs administrateurs peuvent donc ouvrir le module d'administration et effectuer certaines actions :
>
> - Un utilisateur administrateur peut voir ses projets, les configurer et les diviser (études multi-centriques)
> - Un utilisateur administrateur peut créer des utilisateurs et les assigner à ses projets
> - Un utilisateur administrateur ne peut pas gérer les archives, ou modifier les comptes d'un super-administrateur

## Créer un utilisateur

Cliquez sur le bouton « Créer un utilisateur » pour créer un nouvel utilisateur :

<div class="screenshot"><img src="{{ ASSET static/help/admin/user.webp }}" height="580" alt=""/></div>

Le **nom d'utilisateur** doit comporter uniquement des caractères minuscules non accentués, des chiffres, ou les caractères '_', '.' et '-'. Il doit comporter moins de 64 caractères. Certains noms d'utilisateurs sont interdits, comme par exemple `goupile`.

Vous devez paramétrer le **mot de passe initial**, qui n'a pas à respecter les contraintes décrites dans la section sur les [classes d'utilisateurs](#classes-d-utilisateurs). Il doit simplement comporter au moins 10 caractères. En revanche, si vous activez l'option « Exiger un changement de mot de passe » (cochée par défaut), l'utilisateur devra le modifier lors de la première connexion, et le nouveau mot de passe devra respecter ces contraintes.

Vous pouvez exiger l'utilisation d'une **authentification à 2 facteurs** par code TOTP. Si l'option est activée, le nouvel utilisateur devra récupérer la clé (texte ou via un QR code) après le paramétrage de son mot de passe. Les utilisateurs peuvent à tout moment activer ou reconfigurer l'authentification à 2 facteurs; en revanche, seul les administrateurs peuvent désactiver le TOTP d'un compte existant.

> [!NOTE]
> Goupile est principalement destiné à des études cliniques, il n'y a pas de système de récupération de mot de passe par courriel.
>
> En cas d'oubli, un nouveau mot de passe doit être configuré par un super-administrateur ou bien un utilisateur administrateur avec les droits sur un projet auquel l'utilisateur est assigné.

Les champs de courriel et de téléphone sont optionnels, et sont là à titre purement informatif.

Enfin, si vous avez le statut de **super-administrateur**, vous pouvez créer un autre super-administrateur en activant l'option correspondante. Les super-administrateurs ont le même statut que l'utilisateur créé lors de l'installation initiale. Ils sont affichés en rouge et avec une petite couronne ♛ à côté du nom d'utilisateur.

# Système de droits

<div class="screenshot"><img src="{{ ASSET static/help/admin/assign.webp }}" height="130" alt=""/></div>

## Droits de développement

Ces droits sont relatifs à la **conception** et à la *gestion des utilisateurs** assignés au projet.

<table class="permissions" style="--background: #b518bf;">
    <thead><tr><th colspan="2">Développement</th></tr></thead>
    <tbody>
        <tr><td>Code</td><td>Accès au mode de conception pour modifier les scripts de projet et de formulaire.</td></tr>
        <tr><td>Publish</td><td>Publication (ou déploiement) des scripts modifiés qui seront accessibles en dehors du mode de conception.</td></tr>
        <tr><td>Admin</td><td>Accès partiel au module d'administration pour le projet concerné, et les utilisateurs assignés à ce projet.</td></tr>
    </tbody>
</table>

Lorsqu'un utilisateur a le droit *Admin* sur au moins un projet, il est promu en [utilisateur administrateur](#classes-d-utilisateurs).

## Droits sur les données

Ces droits sont relatifs à l'accès aux données, ils s'appliquent au projet dans les études mono-centriques ou à chaque centre dans les études multi-centriques.

<table class="permissions" style="--background: #258264;">
    <thead><tr><th colspan="2">Saisie et données</th></tr></thead>
    <tbody>
        <tr><td>Read</td><td>Accès en lecture à tous les enregistrements du projet ou du centre.</td></tr>
        <tr><td>Save</td><td>Création de nouveaux enregistrements et modification de enregistrements : soit tous les enregistrements du projet ou centre (si le droit <i>Read</i> est activé) créés par l'utilisateur concerné.</td></tr>
        <tr><td>Delete</td><td>Suppression d'enregistrements auxquels l'utilisateur a accès : soit tous les enregistrements du projet ou centre (si le droit <i>Read</i> est activé), soit les enregistrements créés par l'utilisateur concerné.</td></tr>
        <tr><td>Audit</td><td>Consultation de l'audit trail et <a href="app#verrouillage">déverrouillage</a> des enregistrements.</td></tr>
        <tr><td>Offline</td><td>Accès au fonctionnement hors ligne des projets pour lesquels c'est autorisé</td></tr>
    </tbody>
</table>

> [!NOTE]
> Les utilisateurs **sans le droit _Read_ mais avec le droit _Save_** peuvent créer des enregistrements, et ils peuvent aussi lire et modifier leurs propres enregistrements.
>
> Utilisez les fonctions de [verrouillage et d'oubli](app#verrouillage-et-oubli) pour créer des enregistrements qui sont verrouillés ou inaccessibles une fois bien remplis.

## Droits d'exports

Pour plus de clarté, les droits relatifs aux exports sont placés dans une catégorie à part. Cela permet également de repérer visuellement les utilisateurs ayant des droits d'export dans la liste d'utilisateurs (droits en bleu).

<table class="permissions" style="--background: #3364bd;">
    <thead><tr><th colspan="2">Exports</th></tr></thead>
    <tbody>
        <tr><td>Create</td><td>Création et téléchargement d'un nouvel export, avec toutes les données ou bien uniquement les données créées depuis un export précédent.</td></tr>
        <tr><td>Download</td><td>Téléchargement d'un export pré-existant, qui a été créé manuellement (par un utilisateur avec le droit <i>Create</i>) ou bien automatiquement via un <a href="instances#exports-automatises">export programmé</a>.</td></tr>
    </tbody>
</table>

> [!CAUTION]
> Les utilisateurs avec le droit *ExportCreate* peuvent télécharger l'export qu'ils ont créé, et ils ont accès à la liste des exports existants. En revanche ils ne peuvent pas télécharger un export déjà existant.

## Droits liés au messages

Ces droits sont relatifs à des fonctionnalités avancées de Goupile, utilisés uniquement dans certains projets.

Ils nécessitent par ailleurs que le serveur soit configuré pour pouvoir envoyer des mails (configuration SMTP) et/ou des SMS (via Twilio par exemple).

<table class="permissions" style="--background: #c97f1a;">
    <thead><tr><th colspan="2">Messages</th></tr></thead>
    <tbody>
        <tr><td>Mail</td><td>Envoi de mails automatisé par le script de formulaire.</td></tr>
        <tr><td>Text</td><td>Envoi de SMS automatisé par le script de formulaire.</td></tr>
    </tbody>
</table>

<style>
    .permissions { width: 90%; }
    .permissions th {
        background: var(--background);
        color: white;
    }
    .permissions td:first-of-type {
        width: 100px;
        font-style: italic;
    }
</style>
