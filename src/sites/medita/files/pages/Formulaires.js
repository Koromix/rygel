data.makeHeader("Formulaires", page)
route.id = page.text("id", "Patient", {value: route.id, mandatory: true, compact: true}).value

util.mapRLE(data.echelles, echelle => echelle.category, (category, offset, len) => {
    page.section(category, () => {
        form.buttons(util.mapRange(offset, offset + len, idx => {
            let echelle = data.echelles[idx];
            return [echelle.name, () => go(echelle.form)]
        }))
    })
})

form.output(html`
    <br/>
    <button class="md_button" @click=${e => go("Suivi")}>Suivi Patient / Graphique</button>
    <button class="md_button" @click=${e => go("Accueil")}>Retour à l'accueil</button>
`)
