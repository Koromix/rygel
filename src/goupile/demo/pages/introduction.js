// Retirer le commentaire de la ligne suivante pour afficher les
// champs (texte, numérique, etc.) à droite du libellé.
// page.pushOptions({compact: true})

page.output(html`
    <p>Une fonction est composée d'un nom et de plusieurs paramètres et permet de proposer un <b>champ de saisie</b> (champ texte, menu déroulant ...).
    <p>Exemple : la fonction <span style="color: #24579d;">page.text("num_inclusion", "N° d'inclusion")</span> propose un champ de saisie texte intitulé <i>N° d'inclusion</i> et le stocke dans la variable <i>num_inclusion</i>.
    <p>De nombreux <b>autres champs</b> peuvent être utilisés : <span style="color: #24579d;">page.date(), page.number(), page.enum(), page.binary(), page.enumDrop(), page.enumRadio(), page.multiCheck(), page.slider()...</span><br/>
    <p>Ces champs peuvent être configurés à l'aide de <b>quelques options</b> : <i>value</i> (valeur par défaut), <i>min/max</i> (pour les valeurs numériques et échelles), <i>compact</i> (pour afficher le libellé et le champ sur la même ligne), <i>help</i> (pour donner des précisions), <i>prefix/suffix</i>, etc.
`)

page.section("Exemples", () => {
    page.text("*num_inclusion", "N° d'inclusion")

    page.date("*date_inclusion", "Date d'inclusion")

    page.number("*age", "Âge", {
        min: 0,
        max: 120
    })

    page.enum("tabagisme", "Tabagisme", [
        ["actif", "Tabagisme actif"],
        ["sevre", "Tabagisme sevré"],
        ["non", "Non fumeur"]
    ])

    page.binary("hypertension", "Hypertension artérielle")

    page.enumDrop("csp", "Catégorie socio-professionnelle", [
        [1, "Agriculteur exploitant"],
        [2, "Artisan, commerçant ou chef d'entreprise"],
        [3, "Cadre ou profession intellectuelle supérieure"],
        [4, "Profession intermédiaire"],
        [5, "Employé"],
        [6, "Ouvrier"],
        [7, "Retraité"],
        [8, "Autre ou sans activité professionnelle"]
    ])

    page.enumRadio("lieu_vie", "Lieu de vie", [
        ["maison", "Maison"],
        ["appartement", "Appartement"]
    ])

    page.multiCheck("sommeil", "Trouble(s) du sommeil", [
        [1, "Troubles d’endormissement"],
        [2, "Troubles de maintien du sommeil"],
        [3, "Réveil précoce"],
        [4, "Sommeil non récupérateur"],
        [null, "Aucune de ces réponses"]
    ])

    page.slider("eva", "Qualité du sommeil", {
        min: 1,
        max: 10,
        help: "Evaluez la qualité du sommeil avec un score entre 0 (médiocre) et 10 (très bon sommeil)"
    })
})

page.output(html`
    <p>Passez à la <b>page « Avancé »</b> à l'aide de l'onglet en haut de la page pour continuer.
`)
