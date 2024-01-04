// Retirer le commentaire de la ligne suivante pour afficher les
// champs (texte, numérique, etc.) à droite du libellé.
// form.pushOptions({ compact: true })

form.output(html`
    <p>Cette page introduit les <i>conditions</i>, les <i>erreurs</i>, les <i>variables calculées</i> ainsi que quelques éléments de <i>mise en page</i>. Ces explications seront données au fur et à mesure.
`)

form.section("Conditions", () => {
    form.date("date_inclusion", "Date d'inclusion", {
        value: LocalDate.today(),
        help: "On peut tester la validité d'une ou plusieurs valeurs à l'aide de la fonction à l'aide du code. Ici, une valeur postérieure à la date actuelle entraine une erreur. Des conditions très complexes peuvent être programmées en cas de besoin."
    })
    if (form.value("date_inclusion") > LocalDate.today()) {
        form.error("date_inclusion", "La date d'inclusion ne peut pas être postérieure à la date actuelle")
    }

    form.number("age", "Âge", {
        min: 0,
        max: 120,
        mandatory: true,
        suffix: value => value > 1 ? "ans" : "an",
        help: "Ce champ utilise l'option mandatory plutôt que l'étoile pour rendre la saisie obligatoire. De plus, il utilise un suffixe dynamique qui est mis à jour en fonction de la valeur (1 an / 3 ans)."
    })

    form.enum("tabagisme", "Tabagisme", [
        ["actif", "Tabagisme actif"],
        ["sevre", "Tabagisme sevré"],
        ["non", "Non fumeur"]
    ], {
        help: "La valeur Tabagisme Actif déclenche l'apparition d'un autre champ."
    })

    if (form.value("tabagisme") == "actif") {
        form.number("tabagisme_cig", "Nombre de cigarettes par jour")
    }

    form.binary("hyperchol", "Hypercholestérolémie")
    form.sameLine(); form.binary("hypertension", "Hypertension artérielle")
    form.sameLine(); form.binary("diabete", "Diabète", {
        help: "form.sameLine(); affiche le champ sur la même ligne."
    })
})

form.section("Poids et taille", () => {
    form.number("poids", "Poids", {
        min: 20,
        max: 400,
        suffix: "kg"
    })
    form.number("taille", "Taille", {
        min: 1,
        max: 3,
        decimals: 2,
        suffix: "m",
        help: "Entrez un poids et une taille et les variables IMC et classe d'IMC seront calculées automatiquement."
    })

    let calcul_imc = form.value("poids") / (form.value("taille") ** 2)
    form.calc("imc", "IMC", calcul_imc, {
        suffix: "kg/m²",
        help: ""
    })

    let calcul_classe;
    if (calcul_imc >= 30) {
        calcul_classe = "Obésité"
    } else if (calcul_imc >= 25) {
        calcul_classe = "Surpoids"
    } else if (calcul_imc >= 18.5) {
        calcul_classe = "Normal"
    } else if (calcul_imc > 0) {
        calcul_classe = "Poids insuffisant"
    }
    form.sameLine(); form.calc("imc_classe", "Classe d'IMC", calcul_classe)
})

form.section("Mode de vie", () => {
    form.enumDrop("csp", "Catégorie socio-professionnelle", [
        [1, "Agriculteur exploitant"],
        [2, "Artisan, commerçant ou chef d'entreprise"],
        [3, "Cadre ou profession intellectuelle supérieure"],
        [4, "Profession intermédiaire"],
        [5, "Employé"],
        [6, "Ouvrier"],
        [7, "Retraité"],
        [8, "Autre ou sans activité professionnelle"]
    ])

    form.enumRadio("lieu_vie", "Lieu de vie", [
        ["maison", "Maison"],
        ["appartement", "Appartement"]
    ])
})
form.sameLine(); form.section("Sommeil", () => {
    form.multiCheck("sommeil", "Trouble(s) du sommeil", [
        [1, "Troubles d’endormissement"],
        [2, "Troubles de maintien du sommeil"],
        [3, "Réveil précoce"],
        [4, "Sommeil non récupérateur"],
        [null, "Aucune de ces réponses"]
    ])

    form.slider("eva", "Qualité du sommeil", {
        min: 1,
        max: 10,
        help: "Evaluez la qualité du sommeil avec un score entre 0 (médiocre) et 10 (très bon sommeil)"
    })
})
