# Trucs et astuces

## Variables disponibles

### meta

### profile

```js
profile.username
profile.userid
```

### forms

### app

Contient la structure de l'appli : formulaires, pages, etc

## Widgets

### Assignation / lecture

Sur une pages données les variables sont accesibles via `values.ma_variable`. Il est aussi possible de les assigner et de le créer avec `values.new_variable`.

```{note}
La syntaxe `form.value("")` est supportée mais n'est plus recommandée.
```

## Valeurs par défaut

*XXX*

## Liste non exhaustives des options

- **compact** (bool, show label and widget on same line)
- **mandatory** (bool)
- **help** (text)
- **wide** (bool)
- **prefix** (text or function taking value)
- **suffix** (text or function taking value)
- **columns** (number, for radio or checkboxes only)
- **placeholder** (text)
- **disabled** (bool)
- **readonly** (bool)
- **hidden** (bool)
- **decimals** (number)
- **value** (default value)
- **min** (number)
- **max** (number)
- **floating** (bool, for sliders only)
- **ticks** (bool or list, for sliders only)
- **text** (for calc only)
- **anchor** (give specific id, for sections only)
- **deploy** (bool, for sections only)
- **color** (hexa, for sections only)
- **tab** (default tab, for tabs only)
