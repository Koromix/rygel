form.pushOptions({large: true})

function var_bin_types(key, title1, title2, choices) {
    let bin = form.binary(key, title1);
    form.multi(key + "_type", title2, choices,
               {disable: !bin.value, missing: bin.missing});
}

form.section("Contexte de prise en charge", () => {
    form.text("date", "Date ?")
    form.enumRadio("typepec", "Type de prise en charge", [
        [0, "Consultation sans RDV Fontan"],
        [1, "Hospitalisation temps plein"],
        [2, "HPDD"],
        [3, "Consultation programmée"],
            ])
 form.text("precisioncontexte", "Précision ?", {mandatory: false})           
            
})

form.section("Données démographiques", () => {form.text("Prenom", "Prénom ?", {mandatory: false})
form.text("nom", "Nom ?", {mandatory: false})
let sexe = form.enum("sexe", "Sexe ?", [["Homme"], ["Femme"]])
if (sexe.value == "Femme") {
    form.binary("enceinte", "Êtes-vous enceinte ?");
}
form.text("ddn","Date de naissance ?")
form.number("age", "Age ?", {min: 0, max: 120,
                                            suffix: value => value > 1 ? "ans" : "an"})

let motif = form.enumDrop("motif", "Motif de consult ?", [
    [1, "Avis thérapeutique"],
    [2, "Avis diagnostique"],
    [3, "Avis"],
    [4, "Autre"],
    ])
if (motif.value == "4") {
    form.text("autre_motif", "Autre motif de consult");
}
let contexte = form.enumDrop("contexte", "Dans un contexte de ?", [
    [0, "idées suicidaires"]
    [1, "dépression résistante"],
    [2, "dépression résistante dans le cadre d'un trouble bipolaire"],
    [3, "syndrome catatonique"],
    [4, "schizophrénie résistante"],
    [5, "symptômes psychotiques résistants"],
    [6, "Autre"]
    ])
if (contexte.value == "6") {
    form.text("autre_contexte", "Autre contexte de consult");
}

});

form.section("Antécédents", () => {
     var_bin_types("psy", "Antécédents psychiatriques",
                  "Types d'antécédents psychiatriques", [
        [1, "Schizophrénie"],
        [2, "Catatonie"],
        [3, "Trouble bipolaire"],
        [4, "Dépression"],
        [5, "Trouble de personnalité"],
        [6, "Manie"],
        [7, "Hypomanie"],
        [9, "Autre"]
        ]);
    form.text("autreatcd_psy","Autres antécédents psychiatriques et/ou commentaires") 

 let atcd_hospit = form.binary("atcd_hospit", "Antécédents d'hospitalisation ?")
     if (atcd_hospit.value) {
        form.text("atcd_hospita", "Commentaires")
    }
 let atcd_ts = form.binary("atcd_ts", "Antécédents de tentative de suicide ?")
     if (atcd_ts.value) {
        form.text("atcd_tsu", "Commentaires")
    }
 

 // Variable cases à cocher multiples
    var_bin_types("neuro", "Antécédents neurologiques",
                  "Types d'antécédents neurologiques", [
        [1, "AVC ischémique"],
        [2, "Séquelles"],
        [3, "AVC hémorragique"],
        [4, "Hémorragie méningée"],
        [5, "Epilepsie"],
        [6, "Maladie de Parkinson ou syndrome parkinsonien"],
        [7, "Neuropathie ou polyneuropathie"],
        [9, "Autre"]
        ]);
    form.text("autreatcd_neuro","Autres antécédents neurologiques et/ou commentaires")    
 
     // Variable à choix unique (menu déroulant)
    var_bin_types("endoc", "Antécédents endocrinologiques",
                  "Types d'antécédents endocrinologiques", [
        [1, "Diabète de type 1"],
        [2, "Diabète de type 2"],
        [3, "Dyslipidémie"],
        [4, "Syndrome métabolique"],
        [5, "Hypothyroïdie"],
        [6, "Hyperthyroïdie"],
        [7, "Chirurgie de l'obésité"],
        [9, "Autre"]
    ]);

form.text("antecedents_familiaux_ts","Antécédents familiaux de suicide ou tentative de suicide")
form.text("antecedents_familiaux_maladie","Antécédents familiaux de dépression, schizophrénie,trouble bipolaire")
form.text("autres_fam", "Autres antécédents familiaux notables ?")
});

form.section("Biographie", () => {
    form.enumRadio("lieu_vie", "Quel est votre lieu de vie ?", [
    ["maison", "Maison"],
    ["appartement", "Appartement"]])

form.enumRadio("situation_perso", "Situation personnelle", [
    ["couple", "Couple"],
    ["marié", "Marié"],
    ["séparé", "Séparé"],
    ["veuf", "veuf"]
    ])
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
form.text("situation_pro","Situation professionnelle")
form.text("source_revenus","Source de revenus")
});





form.multi("sommeil", "Présentez-vous un trouble du sommeil ?", [
    [1, "Troubles d’endormissement"],
    [2, "Troubles de maintien du sommeil"],
    [3, "Réveil précoce"],
    [4, "Sommeil non récupérateur"],
    [null, "Aucune de ces réponses"]
])





form.section("Autres", () => {
    form.number("enfants", "Combien avez-vous d'enfants ?", {min: 0, max: 30})
    form.binary("frites", "Aimez-vous les frites ?",
                {help: "Si si, c'est important, je vous le jure !"})
});

form.output(html`On peut aussi mettre du <b>HTML directement</b> si on veut...
                 <button class="af_button" @click=${e => go("complicated_help")}>Afficher l'aide</button>`)
form.output("Ou bien encore mettre du <b>texte brut</b>.")

form.errorList()
form.buttons("save")
