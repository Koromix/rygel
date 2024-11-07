# Interface d'administration

Cette interface vous permet de créer et gérer vos projets, d'ajouter des utilisateurs, de configurer leurs droits et d'archiver vos projets.

## Authentification

<div class="screenshot"><img src="{{ ASSET static/help/admin_login.webp }}" style="height: 360px;" alt=""/></div>

L'authentification se réalise via le portail de connexion présenté ci-dessous. Le nom d'utilisateur et le mot de passe sont transmis par e-mail. Le nom d'utilisateur est généralement (par convention) au format « **nom.prenom** ». Après la saisie du nom d'utilisateur et du mot de passe, cliquez sur « Se connecter ».

## Panneaux d'administration

<div class="screenshot"><img src="{{ ASSET static/help/admin_overview.webp }}" alt=""/></div>

La page d'accueil se présente sous la forme suivante. Elle peut être schématiquement décomposée en 3 parties. Le bandeau [1] vous permet de configurer l'affichage de la page. Vous pouvez afficher (ou non) un ou deux panneaux en sélectionnant (ou déselectionnant) le(s) panneau(x) d'intérêt. Par défaut, les panneaux « Projets » [2] et « Utilisateurs » [3] sont sélectionnés.</p>

<div class="screenshot"><img src="{{ ASSET static/help/admin_panels.webp }}" alt=""/></div>

# Gestion des projets

Pour créer un nouveau projet, il faut se connecter sur l'interface administrateur, afficher le panneau de configuration « Projets » [1] puis cliquer sur « Créer un projet » [2].

<div class="screenshot"><img src="{{ ASSET static/help/admin_project_new1.webp }}" alt=""/></div>

Une nouvelle fenêtre apparaît (« Création d'un projet »). Vous devez y définir une clé de projet (« Clé du projet »), un nom de projet (« Nom ») et vous aurez le choix pour ajouter (ou non) des pages par défaut (« Ajouter les pages par défaut »).

La clé du projet apparaîtra dans l'URL de connexion au projet. Son format doit être alphanumérique et ne peut excéder 24 caractères. Les majuscules, les accents et les caractères spéciaux ne sont pas autorisés (sauf le tiret court ou tiret du 6, ‘-‘). Le nom du projet correspond au nom que vous souhaitez donner à votre projet. Son format peut être numérique ou alphanumérique. Les majuscules et les caractères spéciaux sont autorisés. Les « pages par défaut » permettent l'ajout de quelques pages d'exemple vous permettant de vous familiariser avec la conception d'un eCRF avec Goupile et fournissent une première base de travail. Si vous êtes déjà familier avec la conception d'un eCRF avec Goupile, vous pouvez cliquer sur « non ». Après avoir saisi les différents champs, cliquer sur « Créer » [1].

<div class="screenshot">
    <img src="{{ ASSET static/help/admin_project_new2.webp }}" style="height: 320px;" alt=""/>
    <img src="{{ ASSET static/help/admin_project_new3.webp }}" style="height: 320px;" alt=""/>
    <img src="{{ ASSET static/help/admin_project_new4.webp }}" style="height: 320px;" alt=""/>
</div>

Une fois votre projet créé, plusieurs menus sont disponibles via le panneau de configuration « Projets » : « Diviser » [1], « Droits » [2], « Configurer » [3] et « accès » [4].

<div class="screenshot"><img src="{{ ASSET static/help/admin_project_actions.webp }}" alt=""/></div>

Par défaut, les projets sont mono-centriques. Pour transformer un projet mono-centrique en projet multi-centrique, vous pouvez utiliser l'option « Diviser ».

L'option « Diviser » vous permet de subdiviser votre projet initial en différents sous-projets. Une nouvelle fenêtre apparait « Division de *votre clé de projet* ». Vous devez alors renseigner une clé de sous-projet (« Clé du sous-projet ») et un nom de sous-projet (« Nom ») selon les mêmes contraintes que celles mentionnées pour la clé et le nom du projet. Après avoir saisi les différents champs, cliquer sur « Créer » [1].

<div class="screenshot">
    <img src="{{ ASSET static/help/admin_project_split1.webp }}" style="height: 320px;" alt=""/>
    <img src="{{ ASSET static/help/admin_project_split2.webp }}" style="height: 320px;" alt=""/>
    <img src="{{ ASSET static/help/admin_project_split3.webp }}" style="height: 320px;" alt=""/>
</div>

Une fois votre sous-projet créé, plusieurs menus sont disponibles via le panneau de configuration « Projets » : « Droits », « Configurer » et « accès ».

<div class="screenshot"><img src="{{ ASSET static/help/admin_project_sub.webp }}" alt=""/></div>

L'option « Droits » vous permet de créer un nouvel utilisateur (« Créer un utilisateur »), modifier les paramètres d'un utilisateur (« Modifier ») et assigner des droits à un utilisateur (« Assigner ») pour votre projet (ou sous-projet selon la sélection réalisée).

<div class="screenshot"><img src="{{ ASSET static/help/admin_project_permissions.webp }}" alt=""/></div>

L'option « Configurer » vous permet de modifier les paramètres de votre projet (« Modifier ») ou supprimer votre projet (« Supprimer »).

L'onglet « Modifier » vous permet de modifier le nom de votre projet, activer (ou non) l'utilisation hors-ligne (par défaut l'option n'est pas activée), modifier le mode de synchronisation (par défaut le mode de synchronisation est « En ligne ») et définir la session par défaut. L'utilisation hors-ligne permet à l'application de fonctionner sans connexion internet. Le mode de synchronisation « En ligne » correspond à une copie sur le serveur des données, le mode de synchronisation « Hors ligne » correspond à une copie uniquement locale (sur votre machine) des données sans copie sur le serveur et le mode de synchronisation « Miroir » permet une copie en ligne des données et une recopie des données sur chaque machine utilisée. La session par défaut permet d'afficher la session d'un utilisateur donné lors de la connexion.

L'onglet « Supprimer » vous permet de supprimer votre projet.

<div class="screenshot">
    <img src="{{ ASSET static/help/admin_project_edit.webp }}" style="height: 560px;" alt=""/>
    <img src="{{ ASSET static/help/admin_project_delete.webp }}" style="height: 200px;" alt=""/>
</div>

# Gestion des utilisateurs

Pour créer un nouvel utilisateur, il faut se connecter sur l'interface administrateur, afficher le panneau de configuration « Utilisateurs » [1] puis cliquer sur « Créer un utilisateur » [2].

<div class="screenshot"><img src="{{ ASSET static/help/admin_user_new1.webp }}" alt=""/></div>

Une nouvelle fenêtre apparaît (« Création d'un utilisateur »). Vous devez y définir obligatoirement un nom d'utilisateur (« Nom d'utilisateur »), un mot de passe de connexion (« Mot de passe ») et son statut administrateur ou non (« Administrateur »). Vous pouvez compléter ces informations avec une adresse de courriel (« Courriel ») et un numéro de téléphone (« Téléphone »).

Le nom d'utilisateur correspond au nom de l'utilisateur. Il peut être de format numérique ou alphanumérique. Les majuscules et les caractères spéciaux (à l'exception du tiret court ‘-‘, de l'underscore ‘_' et du point ‘.') ne sont pas autorisés. Nous vous conseillons un nom d'utilisateur au format : « prenom.nom ».

Le mot de passe doit contenir au moins 8 caractères de 3 classes différentes (numériques, alphanumériques, symboles spéciaux). Le statut administrateur permet l'accès à l'interface administrateur. Par défaut, le statut administrateur n'est pas activé.

Après avoir saisi les différents champs, cliquer sur « Créer » [1].

<div class="screenshot">
    <img src="{{ ASSET static/help/admin_user_new2.webp }}" style="height: 400px;" alt=""/>
    <img src="{{ ASSET static/help/admin_user_new3.webp }}" style="height: 400px;" alt=""/>
    <img src="{{ ASSET static/help/admin_user_new4.webp }}" style="height: 400px;" alt=""/>
</div>

Une fois votre utilisateur créé, un menu est disponible via le panneau de configuration « Utilisateurs » : « Modifier ».

L'onglet « Modifier » vous permet de modifier le nom d'utilisateur, le mot de passe, le courriel, le téléphone et le statut administrateur de l'utilisateur.

L'onglet « Supprimer » vous permet de supprimer l'utilisateur.

<div class="screenshot">
    <img src="{{ ASSET static/help/admin_user_edit.webp }}" style="height: 560px;" alt=""/>
    <img src="{{ ASSET static/help/admin_user_delete.webp }}" style="height: 200px;" alt=""/>
</div>

Pour affecter à un utilisateur des droits sur un projet donné, il faut afficher le panneau de configuration « Projets » [1] et cliquer sur l'option « Droits » du projet d'intérêt [2].

<div class="screenshot"><img src="{{ ASSET static/help/admin_user_assign1.webp }}" alt=""/></div>

Le panneau de configuration « Utilisateurs » s'affiche alors à droite du panneau de configuration « Projets ». Vous pouvez affecter des droits à un utilisateur donné via le menu « Assigner » de l'utilisateur d'intérêt [1].

<div class="screenshot">
    <img src="{{ ASSET static/help/admin_user_assign2.webp }}" style="height: 66px;" alt=""/>
    <img src="{{ ASSET static/help/admin_user_assign3.webp }}" style="height: 200px;" alt=""/>
</div>

Une nouvelle fenêtre s'ouvre (« Droits de *votre utilisateur* sur *votre clé de projet* »). Vous pouvez affecter des droits de développement ou d'enregistrements à votre utilisateur.

Les droits de développement comprennent :

Droit | Explication
----- | -----------
*Develop* | Modification des formulaires
*Publish* | Publication des formulaires modifiés
*Configure* | Configuration du projet et des centres (multi-centrique)
*Assign* | Modification des droits des utilisateurs sur le projet

Droit | Explication
----- | -----------
*Load* | Lecture des enregistrements
*Save* | Modifications des enregistrement
*Export* | Export facile des données (CSV, XLSX, etc.)
*Batch* | Recalcul de toutes les variables calculées sur tous les enregistrements
*Message* | Envoi de mails et SMS automatisés

Il est à noter que les droits d'enregistrements ne sont configurables qu'après avoir préalablement édité une première version de l'eCRF.

# Archivage des données

<div class="screenshot"><img src="{{ ASSET static/help/admin_archives.webp }}" alt=""/></div>

Le panneau de configuration « Archives » vous permet de créer une nouvelle archive (« Créer une archive »), restaurer (« Restaurer »), supprimer (« Supprimer ») ou charger (« Uploader une archive ») une archive.

Une archive enregistre l'état des (formulaires et données) au moment où elle est créée. Une fois l'archive créée, vous pouvez télécharger le fichier créé (avec l'extension .goupilebackup) et le stocker à l'abri par un moyen adéquat.

> [!WARNING]
> Attention, pour pouvoir restaurer ce fichier, vous devez **conserver la clé de restauration qui vous a été communiquée lors de la création du domaine**. Sans cette clé, la restauration est impossible et les données sont perdues. Par ailleurs, il nous est strictement **impossible de récupérer cette clé** si vous la perdez.
