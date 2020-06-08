if (typeof data !== 'undefined')
    data.makeHeader("Échelle de cotation de catatonie de Bush-Francis", page)
route.id = page.text("id", "Patient", {value: route.id, mandatory: true, compact: true}).value

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

form.section("8 - STÉRÉOTYPIES", () => {
    form.enumRadio("stereotypies", " Activité motrice répétitive, sans but précis (ex. : joue avec les doigts, se touche de façon répétée, se frotte ou se tapote), le caractère anormal n’est pas lié à la nature du geste mais du fait de sa répétition.", [
        [0, "0 : Absentes"],
        [1 ,"1 : Occasionnelles"],
        [2 ,"2 : Fréquentes"],
        [3 ,"3 : Constantes"],
    ])
})

form.section("9 - MANIÉRISME", () => {
    form.enumRadio("manierisme", " Mouvements bizarres mais orientés vers un but (ex : sauter ou marcher sur la pointe des pieds, salut des passants, mouvements banals exagérés). Le caractère anormal est lié à la nature du mouvement.", [
        [0, "0 : Absentes"],
        [1 ,"1 : Occasionnelles"],
        [2 ,"2 : Fréquentes"],
        [3 ,"3 : Constantes"],
    ])
})

form.section("10 - VERBIGÉRATION", () => {
    form.enumRadio("verbigeration", " Répétition d’expressions ou de phrases (comme un disque rayé).", [
        [0, "0 : Absente"],
        [1 ,"1 : Occasionnelle"],
        [2 ,"2 : Fréquente"],
        [3 ,"3 : Constante"],
    ])
})

form.section("11 - RIGIDITÉ", () => {
    form.enumRadio("rigidite", " Maintien d’une posture rigide en dépit d’efforts de mobilisation. Exclure si présence d’une roue dentée ou d’un tremblement.", [
        [0, "0 : Absente"],
        [1 ,"1 : Résistance légère"],
        [2 ,"2 : Résistance modérée"],
        [3 ,"3 : Résistance sévère, ne peut pas être repositionné"],
    ])
})

form.section("12 - NÉGATIVISME", () => {
    form.enumRadio("negativisme", " Résistance sans motivation apparente aux instructions ou tentatives de mobilisation ou d’examen du patient. Comportement d’opposition, fait exactement le contraire de ce qui est demandé.", [
        [0, "0 : Absent"],
        [1 ,"1 : Résistance légère et/ou opposition occasionnelle"],
        [2 ,"2 : Résistance modérée et/ou opposition fréquente"],
        [3 ,"3 : Résistance sévère et/ou opposition constante"],
    ])
})

form.section("13 - FLEXIBILITÉ CIREUSE", () => {
    form.enumRadio("flexibilite_cireuse", "Pendant les changements de postures exercés sur le patient, le patient présente une résistance initiale avant de se laisser repositionner, comme si on pliait une bougie.", [
        [0, "0 : Absente"],
        [3 ,"3 : Présente"],
    ])
})

form.section("14 - ATTITUDE DE RETRAIT", () => {
    form.enumRadio("attitude_de_retrait", " Refus de manger, de boire et/ou de maintenir un contact visuel.", [
        [0, "0 : Absente"],
        [1 ,"1 : Alimentation/interaction minimale(s) depuis moins d’une journée"],
        [2 ,"2 : Alimentation/interaction minimale(s) depuis plus d’une journée"],
        [3 ,"3 : Absence totale d’alimentation/interaction pendant au moins un jour"],
    ])
})

form.section("15 - IMPULSIVITÉ", () => {
    form.enumRadio("impulsivite", " Le patient s’engage brutalement dans un comportement inapproprié (ex : court dans tous les sens, crie, enlève ses vêtements) sans évènement déclenchant. Après il ne peut pas donner d’explication, ou alors une explication superficielle.", [
        [0, "0 : Absente"],
        [1 ,"1 : Occasionnelle"],
        [2 ,"2 : Fréquente"],
        [3 ,"3 : Constante ou non modifiable"],
    ])
})

form.section("16 - OBÉISSANCE AUTOMATIQUE", () => {
    form.enumRadio("obeissance_automatique", " Coopération exagérée avec les demandes de l’examinateur, ou poursuite  spontanée du mouvement demandé.", [
        [0, "0 : Absente"],
        [1 ,"1 : Occasionnelle"],
        [2 ,"2 : Fréquente"],
        [3 ,"3 : Constante"],
    ])
})

form.section("17 - MITGEHEN (obéissance passive)", () => {
    form.enumRadio("mitgehen","Élévation du bras en « lampe d’architecte » en réponse à une légère pression du doigt, en dépit d’instructions contraires.", [
		[0, "0 : Absent"],
        [3 ,"3 : Présent"],
    ])
})

form.section("18 - GEGENHALTEN (oppositionnisme, négativisme « musculaire »)", () => {
    form.enumRadio("gegenhalten","Résistance à un mouvement passif proportionnel à la force du stimulus, paraît plus automatique que volontaire.", [
		[0, "0 : Absent"],
        [3 ,"3 : Présent"],
    ])
})

form.section("19 - AMBITENDANCE", () => {
    form.enumRadio("ambitendance","Le patient paraît « coincé », sur le plan moteur, dans un mouvement indécis et hésitant.", [
		[0, "0 : Absent"],
        [3 ,"3 : Présent"],
    ])
})

form.section("20 - REFLEXE DE GRASPING", () => {
    form.enumRadio("grasping","Durant l’examen neurologique.", [
		[0, "0 : Absent"],
        [3 ,"3 : Présent"],
    ])
})

form.section("21 - PERSÉVÉRATION", () => {
    form.enumRadio("perseveration","Retour répétitif au même sujet de discussion ou persistance d’un mouvement.", [
		[0, "0 : Absent"],
        [3 ,"3 : Présent"],
    ])
})

form.section("22 - COMBATIVITÉ", () => {
    form.enumRadio("combativite","Habituellement non dirigée, avec peu ou pas d’explication par la suite.", [
		[0, "0 : Absente"],
        [1 ,"1 : Agitation ou coups occasionnels avec un faible risque de blessures"],
        [2 ,"2 : Agitation ou coups fréquents avec un risque modéré de blessures"],
        [3 ,"3 : Dangerosité pour autrui"],
    ])
})

form.section("23 - ANOMALIES NEUROVÉGÉTATIVES", () => {
    form.enumRadio("anomalies_neuroveget","Température,tension artérielle, fréquence cardiaque, fréquence respiratoire, hypersudation.", [
		[0, "0 : Absentes"],
        [1 ,"1 : Anomalie d’un paramètre (HTA pré-existante exclue)"],
        [2 ,"2 : Anomalie de 2 paramètres"],
        [3 ,"3 : Anomalie de 3 paramètres ou plus"],
    ])
})

let score = form.value("agitation") +
            form.value("immobiliteStupeur") +
            form.value("mutisme") +
            form.value("fixiteDuRegard") +
            form.value("priseDePosture") +
            form.value("grimace") +
            form.value("echopraxie") +
			form.value("stereotypies") +
            form.value("manierisme") +
            form.value("verbigeration") +
            form.value("rigidite") +
            form.value("negativisme") +
            form.value("flexibilite_cireuse") +
            form.value("attitude_de_retrait") +
			form.value("impulsivite") +
            form.value("obeissance_automatique") +
            form.value("mitgehen") +
            form.value("gegenhalten") +
            form.value("ambitendance") +
            form.value("grasping") +
            form.value("perseveration") +
			form.value("combativite") +
			form.value("anomalies_neuroveget")

form.calc("score", "Score total", score)

if (typeof data !== 'undefined')
    data.makeFormFooter(nav, page)
