# Installation initiale

Une fois le serveur Goupile déployé, vous pouvez effectuer la configuration initiale en ouvrant l'URL de l'application (par exemple `http://localhost:8889`) et en cliquant sur le lien d'administration.

<div class="screenshot"><img src="{{ ASSET static/help/admin/root.webp }}" height="240" alt=""/></div>

Un **domaine Goupile** correspond à l'ensemble des projets et des utilisateurs Goupile. Lorsque vous installez un domaine, vous devez commencer par accéder à l'écran d'installation afin de configurer Goupile, créer le premier compte super-administrateur, et récupérer la clé d'archive :

<div class="screenshot"><img src="{{ ASSET static/help/admin/install.webp }}" height="500" alt=""/></div>

Parmi les réglages initiaux figurent :

- Le *nom du domaine* Goupile, qui doit contenir uniquement des caractères minuscules et ne doit pas contenir d'espaces. Par convention il est pratique de le faire correspondre au nom de domaine réel (par exemple `beta.goupile.fr`) mais ce n'est pas obligatoire.
- Le *titre du domaine* Goupile, affiché dans le panneau d'administration et dans certains contextes hors projet (comme par exemple le libellé TOTP pour les utilisateurs qui activent l'authentification à 2 facteurs).

Vous devez également créer le *premier utilisateur*, qui aura les droits de super-administrateur, avec lequel vous pourrez vous connecter au panneau d'administration après installation.

Enfin, enregistrez bien la **clé d'archive** dans un lieu sécurité (par exemple votre gestionnaire de mots de passe). Cette clé sera nécessaire à la restauration des archives Goupile si vous utilisez cette fonctionnalité. Si elle est perdue, elle ne peut pas être et récupérée les archives existantes ne pourront pas être restaurées.

<div class="screenshot"><img src="{{ ASSET static/help/admin/key.webp }}" height="100" alt=""/></div>

> [!NOTE]
> Cependant, si vous la perdez, une nouvelle clé (différente) peut être paramétrée et les archives créées après le changement pourront être restaurées avec cette nouvelle clé.

# Configuration additionnelle

Une fois Goupile installé, des options supplémentaires sont disponibles.

Ouvrez le panneau de configuration global en cliquant avec le lien "Configurer le domaine" indiqué sur la capture ci-dessous :

<div class="screenshot"><img src="{{ ASSET static/help/admin/config.webp }}" height="160" alt=""/></div>

## SMTP

Certaines fonctionnalités nécessitent l'envoi de mails (par exemple le système d'inscription personnalisé). Vous devez configurer un serveur SMTP pour permettre l'envoi de mails par Goupile.

<div class="screenshot"><img src="{{ ASSET static/help/admin/smtp.webp }}" height="300" alt=""/></div>
