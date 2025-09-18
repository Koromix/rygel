# Gestion des projets

Accédez au module d'administration pour gérer les projets, à l'aide du panneau de configuration « Projets » [1]. Cliquez sur « Créer un projet » [2] pour créer votre premier projet.

<div class="screenshot"><img src="{{ ASSET static/help/admin/project_new1.webp }}" alt=""/></div>

Une nouvelle fenêtre apparaît (« Création d'un projet »). Vous devez y définir une clé de projet (« Clé du projet »), un nom de projet (« Nom ») et vous aurez le choix pour ajouter (ou non) des pages par défaut (« Ajouter les pages par défaut »).

La clé du projet apparaîtra dans l'URL de connexion au projet. Son format doit être alphanumérique et ne peut excéder 24 caractères. Les majuscules, les accents et les caractères spéciaux ne sont pas autorisés (sauf le tiret court ou tiret du 6, ‘-‘). Le nom du projet correspond au nom que vous souhaitez donner à votre projet. Son format peut être numérique ou alphanumérique. Les majuscules et les caractères spéciaux sont autorisés. Les « pages par défaut » permettent l'ajout de quelques pages d'exemple vous permettant de vous familiariser avec la conception d'un eCRF avec Goupile et fournissent une première base de travail. Si vous êtes déjà familier avec la conception d'un eCRF avec Goupile, vous pouvez cliquer sur « non ». Après avoir saisi les différents champs, cliquer sur « Créer » [1].

<div class="screenshot">
    <img src="{{ ASSET static/help/admin/project_new2.webp }}" style="height: 320px;" alt=""/>
    <img src="{{ ASSET static/help/admin/project_new3.webp }}" style="height: 320px;" alt=""/>
    <img src="{{ ASSET static/help/admin/project_new4.webp }}" style="height: 320px;" alt=""/>
</div>

Une fois votre projet créé, plusieurs menus sont disponibles via le panneau de configuration « Projets » : « Diviser » [1], « Droits » [2], « Configurer » [3] et « accès » [4].

<div class="screenshot"><img src="{{ ASSET static/help/admin/project_actions.webp }}" alt=""/></div>

Par défaut, les projets sont mono-centriques. Pour transformer un projet mono-centrique en projet multi-centrique, vous pouvez utiliser l'option « Diviser ».

L'option « Diviser » vous permet de subdiviser votre projet initial en différents sous-projets. Une nouvelle fenêtre apparait « Division de *votre clé de projet* ». Vous devez alors renseigner une clé de sous-projet (« Clé du sous-projet ») et un nom de sous-projet (« Nom ») selon les mêmes contraintes que celles mentionnées pour la clé et le nom du projet. Après avoir saisi les différents champs, cliquer sur « Créer » [1].

<div class="screenshot">
    <img src="{{ ASSET static/help/admin/project_split1.webp }}" style="height: 320px;" alt=""/>
    <img src="{{ ASSET static/help/admin/project_split2.webp }}" style="height: 320px;" alt=""/>
    <img src="{{ ASSET static/help/admin/project_split3.webp }}" style="height: 320px;" alt=""/>
</div>

Une fois votre sous-projet créé, plusieurs menus sont disponibles via le panneau de configuration « Projets » : « Droits », « Configurer » et « accès ».

<div class="screenshot"><img src="{{ ASSET static/help/admin/project_sub.webp }}" alt=""/></div>

L'option « Droits » vous permet de créer un nouvel utilisateur (« Créer un utilisateur »), modifier les paramètres d'un utilisateur (« Modifier ») et assigner des droits à un utilisateur (« Assigner ») pour votre projet (ou sous-projet selon la sélection réalisée).

<div class="screenshot"><img src="{{ ASSET static/help/admin/project_permissions.webp }}" alt=""/></div>

L'option « Configurer » vous permet de modifier les paramètres de votre projet (« Modifier ») ou supprimer votre projet (« Supprimer »).

L'onglet « Modifier » vous permet de modifier le nom de votre projet, activer (ou non) l'utilisation hors-ligne (par défaut l'option n'est pas activée), modifier le mode de synchronisation (par défaut le mode de synchronisation est « En ligne ») et définir la session par défaut. L'utilisation hors-ligne permet à l'application de fonctionner sans connexion internet. Le mode de synchronisation « En ligne » correspond à une copie sur le serveur des données, le mode de synchronisation « Hors ligne » correspond à une copie uniquement locale (sur votre machine) des données sans copie sur le serveur et le mode de synchronisation « Miroir » permet une copie en ligne des données et une recopie des données sur chaque machine utilisée. La session par défaut permet d'afficher la session d'un utilisateur donné lors de la connexion.

L'onglet « Supprimer » vous permet de supprimer votre projet.

<div class="screenshot">
    <img src="{{ ASSET static/help/admin/project_edit.webp }}" style="height: 560px;" alt=""/>
    <img src="{{ ASSET static/help/admin/project_delete.webp }}" style="height: 200px;" alt=""/>
</div>

# Archivage des données

<div class="screenshot"><img src="{{ ASSET static/help/admin/archives.webp }}" alt=""/></div>

Le panneau de configuration « Archives » vous permet de créer une nouvelle archive (« Créer une archive »), restaurer (« Restaurer »), supprimer (« Supprimer ») ou charger (« Uploader une archive ») une archive.

Une archive enregistre l'état des (formulaires et données) au moment où elle est créée. Une fois l'archive créée, vous pouvez télécharger le fichier créé (avec l'extension .goarch) et le stocker à l'abri par un moyen adéquat.

> [!WARNING]
> Attention, pour pouvoir restaurer ce fichier, vous devez **conserver la clé de restauration qui vous a été communiquée lors de la création du domaine**.
>
> Sans cette clé, la restauration est impossible et les données sont perdues. Par ailleurs, il nous est strictement **impossible de récupérer cette clé** si vous la perdez.
