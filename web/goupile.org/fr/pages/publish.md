# Environnement de conception

En mode Conception, les modifications apportées aux différentes pages **ne sont visibles qu'en mode Conception**. Les autres utilisateurs ne verront les modifications apportées qu'après publication du projet.

Vous pouvez utiliser le formulaire normalement pendant le développement, et même enregistrer des données.

Faites attention à ne pas mélanger des données de test et des données réelles si vous modifiez le formulaire après sa mise en production. Les données enregistrées en mode de conception utilisent un numéro de séquence, qui **ne sera pas réutilisé** même si l'enregistrement est supprimé !

> [!TIP]
> Une exception existe : si vous **supprimez tous les enregistrements** (par exemple après avoir terminé les tests de votre projet, avant la mise en production), le numéro de séquence est remis à sa valeur initiale et le premier enregistrement aura le numéro de séquence 1.

# Publication du projet

Une fois votre formulaire prêt, vous devez le publier pour le rendre accessible à tous les utilisateurs. Le code non publié n'est visible que par les utilisateurs qui utilisent le mode Conception.

Après publication, les utilisateurs pourront saisir des données sur ces formulaires.

Pour ce faire, cliquez sur le bouton Publier en haut à droite du panneau d'édition de code. Ceci affichera le panneau de publication (visible dans la capture à gauche).

<div class="screenshot"><img src="{{ ASSET static/help/dev/publish.webp }}" alt=""/></div>

Ce panneau récapitule les modifications apportées et les actions qu'engendrera la publication. Dans la capture à droite, on voit qu'une page a été modifiée localement (nommée « accueil ») et sera rendue publique après acceptation des modifications.

Validez les modifications, et **tous les utilisateurs auront accès** à la nouvelle version du formulaire.

# Ajout de fichiers et d'images

Il est possible d'intégrer des **images, des vidéos, des PDF et tous types de fichiers**, qui seront directement hébergés par Goupile. Pour cela, ouvrez le panneau de publication (accessible au-dessus de l'éditeur), puis cliquez sur le lien « Ajouter un fichier » [1].

<div class="screenshot">
    <img src="{{ ASSET static/help/dev/file1.webp }}" style="height: 240px;" alt=""/>
    <img src="{{ ASSET static/help/dev/file2.webp }}" style="height: 240px;" alt=""/>
</div>

Vous pouvez ensuite sélectionner un fichier à ajouter depuis votre ordinateur, et le renommer si vous le souhaitez. Vous pouvez également le mettre en arborescence en lui donnant un nom tel que « images/alpes/montblanc.png ».

Une fois le fichier ajouté, vous pouvez directement y faire référence dans vos pages à l'aide du widget `form.output`. Le code HTML suivant vous explique comment afficher le logo Apple, à partir d'un fichier `images/apple.png` ajouté au projet :

```js
form.output(html`
    <img src=${ENV.urls.files + 'images/apple.png'} alt="Logo Apple" />
`)
```

L'utilisation de `ENV.urls.files` pour construire l'URL vous garantit une URL qui changera à chaque publication du projet, pour éviter des problèmes liés au cache des navigateurs. Cependant, chaque fichier est aussi accessible via `/projet/files/images/alpes/montagne.png`, et cette URL est stable depuis l'extérieur de Goupile.

> [!NOTE]
> Le nom de fichier `favicon.png` est particulier. Si vous mettez une image nommée favicon.png elle remplacera la favicon affichée dans le navigateur, et l'icône affichée sur l'écran de connexion.
