data.makeHeader("Aide", page)

page.output(`L'aide n'est pas encore disponible, désolé !`)

form.output(html`
    <br/>
    <button class="md_button" @click=${e => go("Accueil")}>Retour à l'accueil</button>
`)
data.makeFooter(page)
