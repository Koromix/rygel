// Retirer le commentaire de la ligne suivante pour afficher les
// champs (texte, numérique, etc.) à droite du libellé.
// page.pushOptions({compact: true})

page.output(html`
    <p>Une <b>fonction</b> est composée d'un <i>nom</i> et de plusieurs <i>paramètres</i> et permet de proposer un outil de saisie (champ texte, menu déroulant ...).
    <p>Exemple : la fonction page.text("num_patient", "Numéro de patient") propose un champ de saisie texte intitulé <i>Numéro de patient</i> et le stocke dans la variable <i>num_patient</i>.
`)


page.section("Nouvelle section", () => {
    page.output(html`
        <p>Vous pouvez par exemple copier les fonctions présentées dans la section <b>Exemples</b> dans cette section pour créer votre propre formulaire.
    `)
})

page.sameLine(true)
page.section("Exemples", () => {
    page.number("*age", "Quel est votre âge ?", {min: 0, max: 120})

    page.enum("sexe", "Quel est votre sexe ?", [["M", "Homme"], ["F", "Femme"]])

    page.enumDrop("csp", "Quelle est votre CSP ?", [
        [1, "Agriculteur exploitant"],
        [2, "Artisan, commerçant ou chef d'entreprise"],
        [3, "Cadre ou profession intellectuelle supérieure"],
        [4, "Profession intermédiaire"],
        [5, "Employé"],
        [6, "Ouvrier"],
        [7, "Retraité"],
        [8, "Autre ou sans activité professionnelle"]
    ])

    page.enumRadio("lieu_vie", "Quel est votre lieu de vie ?", [
        ["maison", "Maison"],
        ["appartement", "Appartement"]
    ])

    page.multiCheck("sommeil", "Présentez-vous un trouble du sommeil ?", [
        [1, "Troubles d’endormissement"],
        [2, "Troubles de maintien du sommeil"],
        [3, "Réveil précoce"],
        [4, "Sommeil non récupérateur"],
        [null, "Aucune de ces réponses"]
    ])
})
