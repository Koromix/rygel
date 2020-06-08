// Echelles

data.echelles = [

    {category: "Général", name: "Observation", form: "F_OBSERVATION", keys: ["score"]},

    {category: "AUTO-QUESTIONNAIRES", name: "Inventaire de dépression de Beck (BDI)", form: "F_BDI", keys: ["score"]},
    {category: "AUTO-QUESTIONNAIRES", name: "Échelle de dépression QIDS", form: "F_QIDS", keys: ["score"]},
    {category: "AUTO-QUESTIONNAIRES", name: "Questionnaire de trouble de l'humeur (MDQ)", form: "F_MDQ", keys: ["score"]},
    {category: "AUTO-QUESTIONNAIRES", name: "Auto-questionnaire de Angst", form: "F_ANGST", keys: ["score"]},
    {category: "AUTO-QUESTIONNAIRES", name: "Inventaire de Beck pour l'anxiété", form: "F_IBA", keys: ["score"]},
    {category: "AUTO-QUESTIONNAIRES", name: "Chilhood Trauma Questionnaire (CTQ)", form: "F_CTQ",
        keys: ["physicalAbuse", "emotionalNeglect", "physicalNeglect",
               "sexualAbuse", "emotionalAbuse", "score"]},
    {category: "AUTO-QUESTIONNAIRES", name: "Echelle de mesure de l'observance médicamenteuse (MARS)", form: "F_MARS", keys: ["score"]},
    {category: "AUTO-QUESTIONNAIRES", name: "Questionnaire de fonctionnement social", form: "F_QFS", keys: ["score"]},
    {category: "AUTO-QUESTIONNAIRES", name: "PQD4", form: "F_PDQ4", keys: ["score"]},
    {category: "AUTO-QUESTIONNAIRES", name: "Symptômes psychotiques", form: "F_symptomespsychotiquesMINI", keys: ["score"]},




    {category: "Catatonie", name: "Catatonie : Critères diagnostiques", form: "F_CATATONIE", keys: ["score"]},
    {category: "Catatonie", name: "Échelle de Bush-Francis (BFCRS)", form: "F_BFCRS", keys: ["score"]},


    {category: "ESPPER", name: "MADRS", form: "F_MADRS", keys: ["score"]},
    {category: "ESPPER", name: "Historique des traitements", form: "F_Chimiogramme", keys: ["score"]},
    {category: "ESPPER", name: "État de stress post-traumatique (PCLS)", form: "F_PCLS", keys: ["score"]},
    {category: "ESPPER", name: "Évaluation de la dépression psychotique (PDAS)", form: "F_PDAS", keys: ["score"]},
    {category: "ESPPER", name: "Évaluation globale du fonctionnement (EGF)", form: "F_EGF", keys: ["score"]},
    {category: "ESPPER", name: "Echelle de Calgary (CDS)", form: "F_CDSS", keys: ["score"]},
    {category: "ESPPER", name: "Echelle abrégée d'évaluation psychiatrique (BPRS)", form: "BPRS", keys: ["score"]},
    {category: "ESPPER", name: "Questionnaire de Altman (ASRM)", form: "F_ALTMAN", keys: ["score"]},





    {category: "Personnalité", name: "Echelle SAPAS (Standardised Assessment of Personality – Abbreviated Scale) REVOIR SCORE", form: "SAPAS", keys: ["score"]},

    {category: "Fonctionnement", name: "Échelle brève d'évaluation du fonctionnement du patient (FAST)", form: "FAST", keys: ["score"]},
    {category: "Fonctionnement", name: "Évaluation globale du fonctionnement (EGF)", form: "F_EGF", keys: ["score"]},

    {category: "Neuropsy", name: "MOCA", form: "F_MOCA",
        keys: ["score"]},
    {category: "Neuropsy", name: "Barnes Akathisia Rating Scale", form: "F_BARS",
        keys: ["score"]},
    {category: "Neuropsy", name: "Historique des traitements", form: "F_Chimiogramme_neuropsy", keys: ["score"]},

 {category: "CNEP/Troubles somatoformes", name: "PQD4", form: "F_PDQ4", keys: ["score"]},
 {category: "CNEP/Troubles somatoformes", name: "Chilhood Trauma Questionnaire (CTQ)", form: "F_CTQ",keys: ["physicalAbuse", "emotionalNeglect", "physicalNeglect", "sexualAbuse", "emotionalAbuse", "score"]},
 {category: "CNEP/Troubles somatoformes", name: "État de stress post-traumatique (PCLS)", form: "F_PCLS", keys: ["score"]},
]



// Déclaration des pages

app.form("Accueil", null, {actions: false})
app.form("Aide", null, {actions: false})
app.form("Formulaires", null, {actions: false})
app.form("Suivi", null, {actions: false})

{
    let handled_set = new Set;

    for (let echelle of data.echelles) {
        if (handled_set.has(echelle.form))
            continue;

        app.form(echelle.form, echelle.name, {actions: false})
        handled_set.add(echelle.form)
    }
}

// Templates

data.makeHeader = function(title, page) {
    page.output(html`
        <a href=${env.base_url}><img src="${env.base_url}favicon.png" height="48" style="float: right;"></a>
        <h1>${title}</h1><br/>
    `)
}

data.makeFormFooter = function(nav, page) {
    page.output(html`
        <br/><br/>
        <button class="md_button" ?disabled=${!page.isValid()}
                @click=${async e => {await page.submit(); nav.new();}}>Enregistrer</button>
        <button class="md_button" @click=${e => go("Formulaires")}>Formulaires</button>
        <button class="md_button" @click=${e => go("Suivi")}>Suivi Patient / Graphique</button>
        <button class="md_button" @click=${e => go("Accueil")}>Retour à l'accueil</button>
    `)
}
