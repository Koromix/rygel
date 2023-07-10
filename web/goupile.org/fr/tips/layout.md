# Apparence et mise en page

## Disposition des widgets

*XXX*

## Style visuel (CSS)

### Portée globale ou page actuelle

Si le CSS est mis dans l'onglet application, alors il s'applique à toutes les pages.

```js
app.head(html`
    <style>
        .fm_legend { background: red; }
    </style>
`)
```

En revanche, il ne s'applique qu'à la page en cours de cette manière :

```js
form.output(html`
    <style>
        .fm_legend { background: green; }
    </style>
`)
```

### Couleur des sections de la page

```js
form.output(html`
    <style>
        .fm_legend { background: green; }
    </style>
`)
```

### Couleur d'une section

```js
form.section("Amines", () => {
    form.binary("key", "traitement")
}, { color: "red" })
```

### Couleur des boutons

```js
form.output(html`
    <style>
        body {
            --anchor_color: #00ff00;
            --anchor_color_n1: #00ee00;
            --anchor_color_n2: #00dd00;
            --anchor_color_n3: #00cc00;

            --page_color: #ff0000;
            --page_color_n1: #0f7ddfb8;
            --page_color_n2: #dd0000;
            --page_color_n3: #cc0000;
        }
        .fm_legend { background: green; }
    </style>
`)
```
