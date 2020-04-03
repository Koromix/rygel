// Echelles

data.echelles = [

    {category: "Général", name: "Anamnèse", form: "F_ANAMNESE", keys: ["score"]},


    {category: "Catatonie", name: "Catatonie : Critères DSM", form: "F_CATATONIE", keys: ["score"]},
    {category: "Catatonie", name: "Échelle de Bush-Francis (BFCRS)", form: "F_BFCRS", keys: ["score"]},


    {category: "Troubles de l'humeur ESPPER", name: "MADRS", form: "F_MADRS", keys: ["score"]},
    {category: "Troubles de l'humeur ESPPER", name: "Historique des traitements", form: "F_Chimiogramme", keys: ["score"]},
    {category: "Troubles de l'humeur ESPPER", name: "État de stress post-traumatique (PCLS)", form: "F_PCLS", keys: ["score"]},
    {category: "Troubles de l'humeur ESPPER", name: "Évaluation de la dépression psychotique (PDAS)", form: "F_PDAS", keys: ["score"]},
    {category: "Troubles de l'humeur ESPPER", name: "Évaluation globale du fonctionnement (EGF)", form: "F_EGF", keys: ["score"]},
    {category: "Troubles de l'humeur ESPPER", name: "Echelle de Calgary (CDS)", form: "F_CDSS", keys: ["score"]},


    {category: "Troubles de l'humeur ESPPER (autoQ)", name: "Mood Disorder Questionnaire (MDQ)", form: "F_MDQ", keys: ["score"]},
    {category: "Troubles de l'humeur ESPPER (autoQ)", name: "Inventaire de dépression de Beck (BDI)", form: "F_BDI", keys: ["score"]},
    {category: "Troubles de l'humeur ESPPER (autoQ)", name: "Auto-questionnaire de Angst", form: "F_ANGST", keys: ["score"]},
    {category: "Troubles de l'humeur ESPPER (autoQ)", name: "Questionnaire de Altman (ASRM)", form: "F_ALTMAN", keys: ["score"]},
     {category: "Troubles de l'humeur ESPPER (autoQ)", name: "Échelle de dépression QIDS", form: "F_QIDS", keys: ["score"]},
   {category: "Troubles de l'humeur ESPPER (autoQ)", name: "Chilhood Trauma Questionnaire (CTQ)", form: "F_CTQ",
        keys: ["physicalAbuse", "emotionalNeglect", "physicalNeglect",
               "sexualAbuse", "emotionalAbuse", "score"]},
    {category: "Troubles de l'humeur ESPPER (autoQ)", name: "PQD4", form: "F_PDQ4", keys: ["score"]},

    {category: "Troubles psychotiques", name: "Echelle abrégée d'évaluation psychiatrique (BPRS)", form: "BPRS", keys: ["score"]},


    {category: "Neuropsy", name: "MOCA", form: "F_MOCA",
        keys: ["score"]},
    {category: "Neuropsy", name: "Historique des traitements", form: "F_Chimiogramme_neuropsy", keys: ["score"]},




]

// Déclaration des pages

app.form("Accueil")
app.form("Aide")
app.form("Formulaires")
app.form("Suivi")

{
    let handled_set = new Set;

    for (let echelle of data.echelles) {
        if (handled_set.has(echelle.form))
            continue;

        app.form(echelle.form)
        handled_set.add(echelle.form)
    }
}

// Templates

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

data.makeFormFooter = function(page) {
    page.output(html`
        <br/><br/>
        <button class="md_button" ?disabled=${!page.isValid()} @click=${page.submit}>Enregistrer</button>
        <button class="md_button" @click=${e => go("Formulaires")}>Formulaires</button>
        <button class="md_button" @click=${e => go("Suivi")}>Suivi Patient / Graphique</button>
        <button class="md_button" @click=${e => go("Accueil")}>Retour à l'accueil</button>
    `)
}
