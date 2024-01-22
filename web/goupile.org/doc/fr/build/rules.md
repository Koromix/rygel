# Erreurs et contrôles

## Gestion des erreurs

Min et max présent sur le widget form.number sont simplement des raccourcis pratiques.

Prenons l'exemple du widget de text : ```form.text("*num_inclusion", "N° d'inclusion")```

Celui-ci est une chaine de caractère JavaScript. On peut donc appeler l'ensemble des méthodes présentes ici :  https://developer.mozilla.org/fr/docs/Web/JavaScript/Reference/Global_Objects/String

```js
if (values.num_inclusion && !values.num_inclusion.match(/^[0-9]{5}$/))
    form.error("num_inclusion", "Format incorrect (5 chiffres attendus)")
```

ou

```js
if (values.inclusion_num && !values.inclusion_num.match(/^(REN|GRE|CFD|LRB|STE)[A-Z][a-z][A-Z]$/))
    form.error("inclusion_num", html`Format incorrect.<br/> Renseigner le code centre (REN, GRE, CFD, LRB, STE)  puis les 2 premières lettres du Nom puis la première lettre du Prénom.<br/>Exemple : Mr Philippe DURAND à Rennes : 'RENDuP'`)
```

Il faut juste toujous penser à vérifier que la chaine de caractères n'est pas vide (`null`).

Si on veut que l'erreur soit retardée à la validation de la page il faut rajouter true en troisième paramètres de `form.error()`.

```js
// Le true à la fin de form.error() indique que l'erreur est retardée (delayed) à la validation
if (values.num_inclusion && !values.num_inclusion.match(/^[0-9]{5}$/))
    form.error("num_inclusion", "Format incorrect (5 chiffres attendus)", true)
```

Pour tester des expressions régulières en live : https://regex101.com/

## Contrôles de cohérence

*XXX*
