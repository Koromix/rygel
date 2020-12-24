// Retirer le commentaire de la ligne suivante pour afficher les
// champs (texte, numérique, etc.) à droite du libellé.
// page.pushOptions({compact: true})

page.output(html`
    <p>Cette page introduit les <i>conditions</i>, les <i>erreurs</i>, les <i>variables calculées</i> ainsi que quelques éléments de <i>mise en page</i>. Ces explications seront données au fur et à mesure.
`)

page.section("Conditions", () => {
    page.text("*num_inclusion", "N° d'inclusion", {
        help: "L'étoile au début du nom de la variable rend la saise obligatoire; cette étoile ne sera pas présente dans le nom de la variable par la suite. L'option mandatory (utilisée pour le champ date_inclusion ci-après) est équivalente."
    })

    page.date("date_inclusion", "Date d'inclusion", {
        value: dates.today(),
        help: "On peut tester la validité d'une ou plusieurs valeurs à l'aide de la fonction à l'aide du code. Ici, une valeur postérieure à la date actuelle entraine une erreur. Des conditions très complexes peuvent être programmées en cas de besoin.."
    })
    if (page.value("date_inclusion") > dates.today()) {
        page.error("date_inclusion", "La date d'inclusion ne peut pas être postérieure à la date actuelle")
    }

    page.number("age", "Âge", {
        min: 0,
        max: 120,
        mandatory: true,
        suffix: value => value > 1 ? "ans" : "an",
        help: "Ce champ utilise l'option mandatory plutôt que l'étoile pour rendre la saisie obligatoire. De plus, il utilise un suffixe dynamique qui est mis à jour en fonction de la valeur (1 an / 3 ans)."
    })

    page.enum("tabagisme", "Tabagisme", [
        ["actif", "Tabagisme actif"],
        ["sevre", "Tabagisme sevré"],
        ["non", "Non fumeur"]
    ], {
        help: "La valeur Tabagisme Actif déclenche l'apparition d'un autre champ."
    })

    if (page.value("tabagisme") == "actif") {
        page.number("tabagisme_cig", "Nombre de cigarettes par jour")
    }

    page.binary("hyperchol", "Hypercholestérolémie")
    page.sameLine(); page.binary("hypertension", "Hypertension artérielle")
    page.sameLine(); page.binary("diabete", "Diabète", {
        help: "La fonction page.sameLine() permet d'afficher le champ sur la même ligne."
    })
})

page.section("Poids et taille", () => {
    page.number("poids", "Poids", {
        min: 20,
        max: 400,
        suffix: "kg"
    })
    page.number("taille", "Taille", {
        min: 1,
        max: 3,
        decimals: 2,
        suffix: "m",
        help: "Entrez un poids et une taille et les variables IMC et classe d'IMC seront calculées automatiquement."
    })

    let calcul_imc = page.value("poids") / (page.value("taille") ** 2)
    page.calc("imc", "IMC", calcul_imc, {
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
    page.sameLine(); page.calc("imc_classe", "Classe d'IMC", calcul_classe)
})

page.section("Mode de vie", () => {
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
})
page.sameLine(); page.section("Sommeil", () => {
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
