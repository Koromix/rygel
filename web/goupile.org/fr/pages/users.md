# Gestion des utilisateurs

Pour créer un nouvel utilisateur, il faut se connecter sur l'interface administrateur, afficher le panneau de configuration « Utilisateurs » [1] puis cliquer sur « Créer un utilisateur » [2].

<div class="screenshot"><img src="{{ ASSET static/help/admin/user_new1.webp }}" alt=""/></div>

Une nouvelle fenêtre apparaît (« Création d'un utilisateur »). Vous devez y définir obligatoirement un nom d'utilisateur (« Nom d'utilisateur »), un mot de passe de connexion (« Mot de passe ») et son statut administrateur ou non (« Administrateur »). Vous pouvez compléter ces informations avec une adresse de courriel (« Courriel ») et un numéro de téléphone (« Téléphone »).

Le nom d'utilisateur correspond au nom de l'utilisateur. Il peut être de format numérique ou alphanumérique. Les majuscules et les caractères spéciaux (à l'exception du tiret court ‘-‘, de l'underscore ‘_' et du point ‘.') ne sont pas autorisés. Nous vous conseillons un nom d'utilisateur au format : « prenom.nom ».

La complexité requise pour les mots de passe dépend du type d'utilisateur :

- Les utilisateurs normaux ou ayant des droits d'administration sur un ou plusieurs projets doivent utiliser un mot de passe d'au moins 8 caractères avec 3 classes de caractère, ou plus de 16 caractères avec 2 classes différentes
- Les super-administrateurs doivent utiliser un mot de passe complexe, évalué par un score de complexité du mot de passe

Après avoir saisi les différents champs, cliquer sur « Créer » [1].

<div class="screenshot">
    <img src="{{ ASSET static/help/admin/user_new2.webp }}" style="height: 400px;" alt=""/>
    <img src="{{ ASSET static/help/admin/user_new3.webp }}" style="height: 400px;" alt=""/>
    <img src="{{ ASSET static/help/admin/user_new4.webp }}" style="height: 400px;" alt=""/>
</div>

Une fois votre utilisateur créé, un menu est disponible via le panneau de configuration « Utilisateurs » : « Modifier ».

L'onglet « Modifier » vous permet de modifier le nom d'utilisateur, le mot de passe, le courriel, le téléphone et le statut administrateur de l'utilisateur.

L'onglet « Supprimer » vous permet de supprimer l'utilisateur.

<div class="screenshot">
    <img src="{{ ASSET static/help/admin/user_edit.webp }}" style="height: 560px;" alt=""/>
    <img src="{{ ASSET static/help/admin/user_delete.webp }}" style="height: 200px;" alt=""/>
</div>

Pour affecter à un utilisateur des droits sur un projet donné, il faut afficher le panneau de configuration « Projets » [1] et cliquer sur l'option « Droits » du projet d'intérêt [2].

<div class="screenshot"><img src="{{ ASSET static/help/admin/user_assign1.webp }}" alt=""/></div>

Le panneau de configuration « Utilisateurs » s'affiche alors à droite du panneau de configuration « Projets ». Vous pouvez affecter des droits à un utilisateur donné via le menu « Assigner » de l'utilisateur d'intérêt [1].

<div class="screenshot">
    <img src="{{ ASSET static/help/admin/user_assign2.webp }}" style="height: 66px;" alt=""/>
    <img src="{{ ASSET static/help/admin/user_assign3.webp }}" style="height: 200px;" alt=""/>
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
*Read* | Liste et lecture des enregistrements
*Save* | Modifications des enregistrement
*Export* | Export facile des données (CSV, XLSX, etc.)
*Batch* | Recalcul de toutes les variables calculées sur tous les enregistrements
*Message* | Envoi de mails et SMS automatisés

Il est à noter que les droits d'enregistrements ne sont configurables qu'après avoir préalablement édité une première version de l'eCRF.
