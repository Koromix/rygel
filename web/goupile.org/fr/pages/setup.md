# Installation initiale

Une fois le serveur Goupile déployé, vous pouvez effectuer la configuration initiale en ouvrant l'URL de l'application dans un navigateur (par exemple `http://localhost:8889`) et en cliquant sur le lien d'administration.

<div class="screenshot"><img src="{{ ASSET static/help/admin/root.webp }}" width="350" alt=""/></div>

Un **domaine Goupile** correspond à l'ensemble des projets et des utilisateurs Goupile. Lorsque vous installez un domaine, vous devez commencer par accéder à l'écran d'installation afin de configurer Goupile, créer le premier compte super-administrateur, et récupérer la clé d'archive :

<div class="screenshot"><img src="{{ ASSET static/help/admin/install.webp }}" alt=""/></div>

Parmi les réglages initiaux figurent :

- Le *nom du domaine*, qui doit contenir uniquement des caractères minuscules et ne doit pas contenir d'espaces. Par convention il est pratique de le faire correspondre au nom de domaine réel (par exemple `beta.goupile.fr`) mais ce n'est pas obligatoire.
- Le *titre du domaine*, affiché dans le panneau d'administration et dans certains contextes hors projet (comme par exemple le libellé TOTP pour les utilisateurs qui activent l'authentification à 2 facteurs).

Vous devez également créer le *premier utilisateur*, qui aura les droits de super-administrateur, avec lequel vous pourrez vous connecter au panneau d'administration après installation.

Enfin, enregistrez bien la **clé d'archive** dans un lieu sécurisé (par exemple votre gestionnaire de mots de passe). Cette clé sera nécessaire à la restauration des archives Goupile, si vous utilisez cette fonctionnalité. Si elle est perdue, elle ne peut pas être récupérée et les archives existantes ne pourront pas être restaurées.

<div class="screenshot"><img src="{{ ASSET static/help/admin/key.webp }}" width="600" alt=""/></div>

> [!NOTE]
> Cependant, si vous la perdez, une nouvelle clé (différente) peut être paramétrée et les archives créées après le changement pourront être restaurées avec cette nouvelle clé.

# Configuration additionnelle

Une fois Goupile installé, des options supplémentaires sont disponibles.

Ouvrez le panneau de configuration global en cliquant avec le lien "Configurer le domaine" indiqué sur la capture ci-dessous :

<div class="screenshot"><img src="{{ ASSET static/help/admin/config.webp }}" alt=""/></div>

> [!NOTE]
> Certains des paramètres qui suivent peuvent être configurés dans le fichier `goupile.ini`. Quand c'est le cas, les valeurs issues du fichier sont prioritaires et il n'est pas possible de les modifier par le module d'administration.

## SMTP

Certaines fonctionnalités nécessitent l'envoi de mails (par exemple le système d'inscription personnalisé). Vous devez configurer un serveur SMTP pour permettre l'envoi de mails par Goupile.

<div class="screenshot"><img src="{{ ASSET static/help/admin/smtp.webp }}" width="350" alt=""/></div>

## Sécurité

L'onglet Sécurité vous permet de paramétrer la politique de mots de passe, en différenciant [trois classes d'utilisateurs](users#classes-d-utilisateurs) :

- Les *utilisateurs normaux*
- Les *utilisateurs administrateurs* ont des droits d'administration sur au moins un projet
- Les *super-administrateurs* ont tous les droits sur le domaine Goupile

<div class="screenshot"><img src="{{ ASSET static/help/admin/security.webp }}" width="350" alt=""/></div>

Pour chaque classe d'utilisateur vous pouvez exiger une complexité de mot de passe parmi 3 choix :

- La **complexité légère** impose des mots de passe avec au moins 10 caractères
- La **complexité modérée** impose des mots de passe avec des lettres, des chiffres et des symboles, ou bien un mot de passe de plus de 16 caractères
- La **complexité élevée** utilise un système de score et de pénalité en fonction des caractères utilisés

> [!WARNING]
> Cette complexité est imposée lors d'un changement de mot de passe par l'utilisateur, ou lors de chaque connexion pour les administrateurs et les super-administrateurs.
>
> En revanche, les administrateurs peuvent créer des utilisateurs dont le mot de passe échappe à ces contraintes.
