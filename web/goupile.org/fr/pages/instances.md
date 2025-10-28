# Module d'administration

Le module d'administration est disponible en ouvrant l'adresse sur laquelle vous avez installé Goupile (par exemple `http://localhost:8889`) et en cliquant sur le lien d'administration.

<div class="screenshot"><img src="{{ ASSET static/help/admin/root.webp }}" height="240" alt=""/></div>

Après vous être connecté, vous aurez accès au panneau d'administration qui se présente comme ci-dessous :

<div class="screenshot"><img src="{{ ASSET static/help/admin/admin.webp }}" height="220" alt=""/></div>

Le panneau d'administration est **organisé en panneaux**, qui peuvent être affichés individuellement ou côte à côte. Lorsque vous vous connectez, et si votre écran est assez large, vous verrez :

- Le *panneau de projets* à gauche
- Le *panneau d'utilisateurs* à droite

Vous pouvez changer les panneaux actifs à l'aide des onglets du menu principal (encadré rouge sur la capture).

# Créer un projet

Créez un projet en cliquant sur « Créer un projet » au-dessus de la liste des projets. Une nouvelle fenêtre apparaît (« Création d'un projet ») dans laquelle vous devez paramétrer :

- Le **nom du projet** (on parle aussi de clé de projet), qui définit l'URL par laquelle le projet sera accessible. Par exemple, avec le nom `demo`, le projet sera disponible sur le chemin `/demo/`.
- Le **titre du projet** qui sera visible par l'utilisateur dans la barre de titre du navigateur et dans le menu de sélection de centre.
- La **langue du projet**, chaque projet Goupile définit la langue qu'il utilise, afin de maintenir une cohérence entre la langue utilisée dans les formulaires et la langue de l'interface Goupile.
- Les **pages par défaut** vous permettent de démarrer plus vite en ajoutant quelques pages d'exemple au projet créé. Sans cette option, le projet sera initialement vide.

<div class="screenshot"><img src="{{ ASSET static/help/instance/create.webp }}" height="340" alt=""/></div>

> [!NOTE]
> Certains noms sont interdits, comme par exemple `admin` ou bien `static`, pour éviter des conflits avec certaines URLs globales de Goupile.

<div class="screenshot"><img src="{{ ASSET static/help/instance/list.webp }}" height="140" alt=""/></div>

Une fois le projet créé il apparaît dans la liste de projet, avec différentes actions possibles :

- Lien **d'accès** : permet d'ouvrir le projet dans un autre onglet
- Fonction **diviser** : permet de transformer un projet mono-centrique (par défaut) en [projet multi-centrique](#projets-multi-centriques), et d'ajouter des centres par la suite
- Fonction **droits** : permet d'assigner des utilisateurs au projet et [paramétrer leurs droits](users#assigner-les-utilisateurs)
- Fonction **configurer** : permet de modifier certains réglages, comme le titre ou bien l'autorisation d'accès aux utilisateurs invités (pour les questionnaires publics)
- Fonction de **suppression** : permet de supprimer un projet

> [!NOTE]
> L'utilisateur qui a créé le projet bénéficie par défauts de tous les droits sur celui-ci. Pour que d'autres utilisateurs participent, vous devez les créer (si besoin) et les assigner au projet.
>
> Suivez la documentation sur la [gestion des utilisateurs](users#assign-users) pour en savoir plus.

Suivez le lien d'accès pour commencer à créer vos questionnaires, ou bien lisez la suite pour modifier certains paramétres du projet.

# Configurer un projet

<div class="screenshot"><img src="{{ ASSET static/help/instance/config.webp }}" height="340" alt=""/></div>

> [!WARNING]
> Nous **n'aborderons pas les paramètres de l'onglet avancé**, qui sont destinés à débloquer certains problèmes rares. Évitez d'y toucher !

## Projets avec accès invité

Par défaut, les projets ne peuvent être ouverts que par des utilisateurs assignés au projet, et avec des droits suffisants.

Vous pouvez activer le mode invité qui rend votre **projet public**. Ceux-ci pourront répondre à votre questionnaire, ils n'auront évidemment pas accès aux données enregistrées par les autres utilisateurs ou invités.

## Fonctionnement hors ligne

Activez le mode hors ligne pour que votre enquête puisse fonctionner sans connexion internet.

L'activation de ce mode a plusieurs conséquences :

- Les fichiers nécessaires à Goupile et le contenu de vos formulaires est mis en cache lors de l'ouverture du projet, la première fois et après chaque modification des formulaires.
- Votre projet peut être installé comme une [application web progressive](https://developer.mozilla.org/en-US/docs/Web/Progressive_web_apps) ou Progressive Web App (PWA) depuis un navigateur supporté.
- Les utilisateurs qui se sont connectés au moins une fois en ligne peuvent se connecter (de manière limitée) à leur profil hors ligne, à l'aide du dernier mot de passe utilisé.

> [!TIP]
> N'activez pas ce mode pour un projet qui n'en a pas besoin, pour limiter le téléchargement et la mise en cache inutile des fichiers de Goupile et de votre projet.

## Projets multi-centriques

Utilisez la fonction de division pour transformer un projet mono-centrique en projet multi-centrique.

Chaque division entraine la création d'un nouveau centre, comme sur la capture ci-dessous où 2 centres sont créés pour le projet "test".

<div class="screenshot"><img src="{{ ASSET static/help/instance/split.webp }}" height="240" alt=""/></div>

Les centres d'un projet **partagent les scripts** des formulaires (et donc la structure du recueil et des exports), mais les **données sont séparées** en plusieurs bases :

- Les *identifiants* (comme le numéro de séquence) sont spécifiques à chaque centre
- Les *exports* sont effectués séparémment
- Les *droits utilisateurs* (en dehors des droits relatifs au développement) doivent être définis pour chaque centre

> [!NOTE]
> Après transformation en projet multi-centrique, les droits des utilisateurs sont scindés :
>
> - Les droits relatifs au développement doivent être définis sur le projet racine
> - Les droits relatifs aux données doivent être définis pour chaque centre

## Exports automatisés

Vous pouvez paramétrer le projet pour réaliser un export automatisé certains jours, à une heure fixe. Chaque export sera disponible dans la [liste des exports](data#exports-de-donnees) sous le tableau de suivi.

<div class="screenshot"><img src="{{ ASSET static/help/instance/export.webp }}" height="380" alt=""/></div>

Vous pouvez choisir d'exporter à chaque fois tous les enregistrements, ou bien uniquement les nouveaux enregistrements depuis le dernier export automatisé.

# Archivage des données

<div class="screenshot"><img src="{{ ASSET static/help/admin/archives.webp }}" height="280" alt=""/></div>

Les archives enregistrent l'état des projets, des utilisateurs et des données au moment où elle sont créées.

Elles sont créées à différents moments :

- Une **archive quotidienne** est créée chaque nuit, avec tous les projets
- Une archive peut être **créée manuellement** avec le bouton « Créer une archive »
- Une archive est créée juste **avant la suppression d'un projet**

Les archives créées *au cours des 7 derniers jours sont gardées* sur le serveur, avant d'être supprimées. Vous pouvez cependant les télécharger (fichiers avec l'extension `.goarch`) et les stocker à l'abri par un moyen adéquat, afin d'en récupérer le contenu ou de les restaurer en cas de besoin.

> [!WARNING]
> Attention, pour pouvoir ouvrir ou restaurer une archive, vous devez **conserver la clé de restauration qui vous a été communiquée lors de l'installation de Goupile**.
>
> <div class="screenshot"><img src="{{ ASSET static/help/admin/key.webp }}" height="100" alt=""/></div>
>
> Sans cette clé, la restauration est impossible et les données sont perdues. Par ailleurs, il nous est strictement **impossible de récupérer cette clé** si vous la perdez.
