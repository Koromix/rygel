if (typeof data !== 'undefined')
    data.makeHeader("Echelle d’évaluation des hallucinations auditives (AHRS)", page)
route.id = page.text("id", "Patient", {value: route.id, mandatory: true, compact: true}).value

form.output(html`
	<p>Evaluation des hallucinations des 24 dernières heures. (D’après Hoffman et al., 2003)
    `)


form.pushOptions({mandatory: true, missingMode: 'disable'})

form.section("1. Fréquence", () => {
    form.enumRadio("frequence", "Quelle est la fréquence des voix ?", [
        [0, "0 : Absentes (stoppées)"],
        [1, "1 : rares (1-5 fois par 24h)"],
        [2, "2 : occasionnelles (6-10 fois par 24h)."],
        [3, "3 : occasionnelles (environ 1-2 fois par heure) "],
        [4, "4 : fréquentes  (3-6  fois par heure)"],
        [5, "5 : fréquentes (7-10   fois par heure)"],
        [6, "6 : très fréquentes (11-20 fois par heure)"],
        [7, "7 : très fréquentes  (21-50 fois par heure)"],
        [8, "8 : rapides (1 par minute)"],
        [6, "9 : presque ininterrompues"]
    ])
})

form.section("2. Réalité", () => {
    form.enumRadio("realite", "A quel point les voix vous semblent-elles réelles ?", [
        [0, "0 : indistincte de vos propres pensées"],
        [1, "1 : imaginaires"],
        [2, "2 : vague"],
        [3, "3 : comme un rêve"],
        [4, "4 : assez réelle"],
        [5, "5 : très réelle"]
        ])
})

form.section("3. Intensité", () => {
    form.enumRadio("intensite", "En général, quelle est l’intensité de la voix prédominante ?", [
        [0, "0 : trop discrète pour être clairement entendue"],
        [1, "1 : chuchotée mais claire"],
        [2, "2 : douce "],
        [3, "3 : voix d’intensité normale (comme celle d’interlocuteur qui vous parle)"],
        [4, "4 : forte "],
        [5, "5 : criante, hurlante "]
    ])
})

form.section("4. Nombre de voix", () => {
    form.enumRadio("nombredevoix", "Combien de voix différentes entendez-vous dont vous pouvez distinguer les mots ?", [
        [0, "0"],
        [1, "1"],
        [2, "2"],
        [3, "3"],
        [4, "4"],
        [5, "5"],
        [6, "6 ou plus"],
    ])
})

form.section("5. Longueur du contenu", () => {
    form.enumRadio("longueurcontenu", "Quelle est la 'taille du contenu de la voix prédominante ?", [
        [0, "0 : aucun mot entendu )"],
        [1, "1 : de simples mots "],
        [2, "2 : des phrases"],
        [3, "3 : des phrases entières"],
        [4, "4 : des phrases multiples"]
    ])
})

form.section("6. Saillance attentionnelle", () => {
    form.enumRadio("saillanceattentionnelle", "A quel point vos voix affectent-t-elles ce que vous pensez, ce que vous ressentez et ce que vous faites ?", [
        [0, "0 : aucune voix "],
        [1, "1 : elles ne m’affectent pas du tout"],
        [2, "2 : elles me perturbent occasionnellement"],
        [3, "3 : je suis brièvement perturbé, distrait transitoirement quand les voix  apparaissent"],
        [4, "4 : je dois la plus part du temps être attentif aux voix que j’entends"],
        [5, "5 : quand j’entends une voix cela altère souvent ce que je fais, je dis ou je pense"],
        [6, "6 : quand j’entends une voix cela altère complètement ce que je fais, je dis ou je pense"],
        [7, "7 : la seule chose qui est importante est d’être attentif à mes voix"]
    ])
})

form.section("7. Niveau de stress engendré", () => {
    form.enumRadio("stress", "À quel point vos voix sont stressantes ?", [
        [0, "0 : aucune voix "],,
        [1, "1 : pas de stress du tout"],
        [2, "2 : légèrement stressante"],
        [3, "3 : modérément stressante "],
        [4, "4 : parfois elles sont responsables d’une anxiété significative"],
        [5, "5 : elles sont souvent responsables  de peur ou d’anxiété"]
    ])
})


let score = form.value("frequence") +
            form.value("realite") +
            form.value("intensite") +
            form.value("nombredevoix") +
            form.value("longueurcontenu") +
            form.value("saillanceattentionnelle") +
            form.value("stress")
form.calc("score", "Score total", score)

if (typeof data !== 'undefined')
    data.makeFormFooter(nav, page)
