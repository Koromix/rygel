form.pushOptions({compact: true})

form.number("age", "Quel est votre âge ?", {min: 0, max: 120, mandatory: true,
                                            suffix: value => value > 1 ? "ans" : "an"})

let sexe = form.enum("sexe", "Quel est votre sexe ?", [["M", "Homme"], ["F", "Femme"]])

form.enumDrop("csp", "Quelle est votre CSP ?", [
    [1, "Agriculteur exploitant"],
    [2, "Artisan, commerçant ou chef d'entreprise"],
    [3, "Cadre ou profession intellectuelle supérieure"],
    [4, "Profession intermédiaire"],
    [5, "Employé"],
    [6, "Ouvrier"],
    [7, "Retraité"],
    [8, "Autre ou sans activité professionnelle"]
])

form.enumRadio("lieu_vie", "Quel est votre lieu de vie ?", [
    ["maison", "Maison"],
    ["appartement", "Appartement"]
])

form.multiCheck("sommeil", "Présentez-vous un trouble du sommeil ?", [
    [1, "Troubles d’endormissement"],
    [2, "Troubles de maintien du sommeil"],
    [3, "Réveil précoce"],
    [4, "Sommeil non récupérateur"],
    [null, "Aucune de ces réponses"]
])

if (sexe.value == "F") {
    form.binary("enceinte", "Êtes-vous enceinte ?")
}

form.section("Alcool", () => {
    let alcool = form.binary("alcool", "Consommez-vous de l'alcool ?")

    if (alcool.value && form.value("enceinte")) {
        alcool.error("Pensez au bébé...")
        alcool.error("On peut mettre plusieurs erreurs")
        form.error("alcool", "Et de plein de manières différentes !")
    }

    if (alcool.value) {
        form.number("alcool_qt", "Combien de verres par semaine ?", {min: 1, max: 30})
    }
})

form.section("Autres", () => {
    form.number("enfants", "Combien avez-vous d'enfants ?", {min: 0, max: 30})
    form.binary("frites", "Aimez-vous les frites ?",
                {help: "Si si, c'est important, je vous le jure !"})
})

form.output(html`On peut aussi mettre du <b>HTML directement</b> si on veut...`)
form.output("Ou bien encore mettre du <b>texte brut</b>.")
