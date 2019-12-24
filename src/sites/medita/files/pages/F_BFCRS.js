data.makeFormHeader("Échelle de cotation de catatonie de Bush-Francis", page)
form.output(html`<p>Ne côter que les items bien définis. En cas de doute sur la présence d'un item, côter 0.</p>`)

form.pushOptions({mandatory: true, missingMode: 'disable'})

form.section("1 - AGITATION", () => {
    form.enumRadio("agitation", "Hyperactivité extrême, agitation motrice constante qui semble sans but. Ne pas attribuer à de l'akathisie ou à une agitation dirigée.", [
        [0, "0 : Absente"],
        [1 ,"1 : Mouvement excessif, intermittent."],
        [2 ,"2 : Semble découragé mais peut se dérider sans difficulté."],
        [3 ,"3 : Agitation catatonique caractérisée, activité motrice frénétique sans fin."],
    ])
})

form.section("2 - IMMOBILITE/STUPEUR ", () => {
    form.enumRadio("immobiliteStupeur", "Hypoactivité extrême, immobilité, faible réponse aux stimuli.", [
        [0, "0 : Absente"],
        [1 ,"1 : Position anormalement fixe, peut interagir brièvement."],
        [2 ,"2 : Pratiquement aucune interaction avec le monde extérieur."],
        [3 ,"3 : Stupeur, pas de réaction aux stimuli douloureux."],
    ])
})

form.section("3 - MUTISME ", () => {
    form.enumRadio("mutisme", "Peu ou pas de réponses verbales.", [
        [0, "0 : Absent"],
        [1 ,"1 : Absence de réponse à la majorité des questions, chuchotement incompréhensible."],
        [2 ,"2 : Prononce moins de 20 mots en 5 minutes."],
        [3 ,"3 : Aucune parole."],
    ])
})

form.section("4 - FIXITÉ DU REGARD ", () => {
    form.enumRadio("fixiteDuRegard", " Regard fixe, peu ou pas d'exploration visuelle de l'environnement, rareté du clignement.", [
        [0, "0 : Absente"],
        [1 ,"1 : Contact visuel pauvre, périodes de fixité du regard inférieures à 20 secondes, diminution du clignement des paupières."],
        [2 ,"2 : Fixité du regard supérieure à 20 secondes, changement de direction du regard occasionnelle."],
        [3 ,"3 : Regard fixe non réactif."],
    ])
})

form.section("5 - PRISE DE POSTURE/CATALEPSIE ", () => {
    form.enumRadio("priseDePosture", "Maintien de posture(s) spontanée(s), comprenant les postures banales (ex : rester assis ou debout pendant de longues périodes sans réagir).", [
        [0, "0 : Absente"],
        [1 ,"1 : Moins de 1  minute."],
        [2 ,"2 : Plus d'1 minute, moins de 15 min."],
        [3 ,"3 : Posture bizarre, ou postures courantes maintenues plus de 15 minutes."],
    ])
})

form.section("6 - GRIMACES ", () => {
    form.enumRadio("grimace", " Maintien d'expressions faciales bizarres :", [
        [0, "0 : Absente"],
        [1 ,"1 : Moins de 10 secondes."],
        [2 ,"2 : Moins de 1  minute."],
        [3 ,"3 : Expression bizarre maintenue plus d'1 minute."],
    ])
})

form.section("7 - ÉCHOPRAXIE/ÉCHOLALIE ", () => {
    form.enumRadio("echopraxie", " Imitations des mouvements ou des propos de l'examinateur.", [
        [0, "0 : Absente"],
        [1 ,"1 : Occasionnelle"],
        [2 ,"2 : Fréquente"],
        [3 ,"3 : Constante"],
    ])
})

let score = form.value("agitation") +
            form.value("immobiliteStupeur") +
            form.value("mutisme") +
            form.value("fixiteDuRegard") +
            form.value("priseDePosture") +
            form.value("grimace") +
            form.value("echopraxie")
form.calc("score", "Score total", score)

data.makeFormFooter(page)
