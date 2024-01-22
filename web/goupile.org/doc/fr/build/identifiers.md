# Identifiants uniques

Chaque enregistrement dans Goupile dispose de deux identifiants uniques :

- L'identifiant ULID, il s'agit d'un identifiant interne généré aléatoirement.
- L'identifiant HID (pour *Human ID*), il doit être renseigné par votre script.

Par exemple ici, nous affectons la valeur du champ num_inclusion au HID. De cette manière, ce numéro d'inclusion sera visible comme identifiant dans le tableau de suivi.

Si on veut que l'ID sont un autre champ ceci est accessible via la variable ```meta.hid```

```js
form.number("num_inclusion", "Numero d'inclusion")
meta.hid = values.num_inclusion
```

Chaque enregistrement a aussi un numero auto-incremental `sequence` du formulaire une sequence interne auto-incrémentale.
