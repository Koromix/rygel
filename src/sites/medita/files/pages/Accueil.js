form.output(html`
    <img src="${env.base_url}files/resources/logo.png" style="display: block; height: 160px; margin: 0 auto;">
    <br/><br/>
`)

form.output(html`
    <button class="md_button" @click=${e => go("Formulaires")}>Formulaires</button>
    <button class="md_button" @click=${e => go("Suivi")}>Suivi Patient / Graphique</button>
`)

data.makeFooter(page)
