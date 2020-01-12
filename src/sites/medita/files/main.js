app.form("Accueil")
app.form("Aide")
app.form("Formulaires")
app.form("Suivi")

data.echelles = [
    {category: "Catatonie", name: "Évaluation de la catatonie", form: "F_CATATONIE", keys: ["score"]},
    {category: "Catatonie", name: "Échelle de Bush-Francis (BFCRS)", form: "F_BFCRS", keys: ["score"]},

    {category: "Troubles de l'humeur", name: "Échelle de dépression Montgomery-Åsberg (MADRS)", form: "F_MADRS", keys: ["score"]},
    {category: "Troubles de l'humeur", name: "Questionnaire sur les troubles de l'humeur (MDQ)", form: "F_MDQ", keys: ["score"]},
    {category: "Troubles de l'humeur", name: "État de stress post-traumatique (PCLS)", form: "F_PCLS", keys: ["score"]},
    {category: "Troubles de l'humeur", name: "Échelle de dépression QIDS", form: "F_QIDS", keys: ["score"]},

    {category: "Autres", name: "Chilhood Trauma Questionnaire (CTQ)", form: "F_CTQ",
        keys: ["physicalAbuse", "emotionalNeglect", "physicalNeglect",
               "sexualAbuse", "emotionalAbuse", "score"]},
    {category: "Autres", name: "Évaluation globale du fonctionnement (EGF)", form: "F_EGF", keys: ["score"]},
    {category: "Autres", name: "Évaluation de la dépression psychotique (PDAS)", form: "F_PDAS", keys: ["score"]},
    {category: "Autres", name: "Diagnostic de la personnalité (PQD4)", form: "F_PDQ4", keys: ["score"]}
]
for (let echelle of data.echelles)
    app.form(echelle.form)

data.makeHeader = function(title, page) {
    page.output(html`
        <a href="Accueil"><img src="${env.base_url}favicon.png" height="48" style="float: right;"></a>
        <h1>${title}</h1><br/>
    `)
}
data.makeFormHeader = function(title, page) {
    data.makeHeader(title, page)
    route.id = page.text("id", "Patient", {value: route.id, mandatory: true, compact: true}).value
}

data.makeFooter = function(page) {
    page.output(html`
        <br/><br/>
        <div class="md_footer">
            <p style="text-align: left;">Alpha</p>
            <a style="text-align: center;" href="Aide">Aide</a>
            <a style="text-align: right;" href="mailto:xxxx@xxx.fr">Contact</a>
        </div>
    `)
}
data.makeFormFooter = function(page) {
    page.output(html`
        <br/><br/>
        <button class="md_button" ?disabled=${!page.isValid()} @click=${page.submit}>Enregistrer</button>
        <button class="md_button" @click=${e => goupile.go("Formulaires")}>Formulaires</button>
        <button class="md_button" @click=${e => goupile.go("Suivi")}>Suivi Patient / Graphique</button>
        <button class="md_button" @click=${e => goupile.go("Accueil")}>Retour à l'accueil</button>
    `)
    data.makeFooter(page)
}
