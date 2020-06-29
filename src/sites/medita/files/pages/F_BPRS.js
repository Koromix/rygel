if (shared.makeHeader)
    shared.makeHeader("Echelle abrégée d'évaluation psychiatrique (BPRS)", page)
route.id = page.text("id", "Patient", {value: route.id, mandatory: true, compact: true}).value

form.pushOptions({mandatory: true, missingMode: 'disable'})

let dataQ = [[0, "Absent"], [1, "Très peu"],[2, "Peu"], [3, "Moyen"], [4, "Assez important"], [5, "Important"], [6, "Extrêmement important"]]


form.section("Préoccupations somatiques. Tristesse exprimée", () => {
    form.enumRadio("Q1", "Intensité des préoccupations actuelles sur la santé physique. Estimer combien sa santé physique préoccupe le patient, quel que soit le bienfondé des plaintes.", dataQ)
})


form.section("2. Anxiété.", () => {
    form.enumRadio("Q2", "Inquiétude, crainte ou préoccupations exagérées concernant le présent ou l'avenir. Ne considérer que ce que le malade rapporte lui-même de ses expériences subjectives. Ne pas déduire l'anxiété de signes physiques ou de système de défenses névrotiques.", dataQ)
})

form.section("3. Retrait affectif", () => {
    form.enumRadio("Q3", "Manque de contact avec l'interlocuteur, inadaptation à la situation d'entretien. A quel degré le patient donne-t-il l'impression de ne pouvoir établir un contact affectif au cours de l'entretien ?", dataQ)

})

form.section("4. Désorganisation conceptuelle", () => {
    form.enumRadio("Q4", "Degré de confusion, d'incohérence, de désorganisation des processus idéiques. Estimer les troubles au niveau de la production verbale. Ne pas se baser sur l'impression que le malade peut avoir du niveau de son fonctionnement mental.", dataQ)

})

form.section("5. Sentiments de culpabilité.", () => {
    form.enumRadio("Q5", "Préoccupations exagérées ou remords à propos d'une conduite passée. Faire l'estimation d'après les expériences subjectives de culpabilité, celle que le malade décrit et dans un contexte affectif approprié. Ne pas déduire l'existence de sentiments de culpabilité d'une symptomatologie dépressive, anxieuse ou de défenses névrotiques.", dataQ)

})

form.section("6. Tension.", () => {
    form.enumRadio("Q6", "Manifestations physiques et motrices de la tension, 'nervosité' et fébrilité. Faire l'estimation seulement d'après les signes somatiques et le comportement moteur. Ne pas se baser sur les sentiments de tension que le malade dit ressentir.", dataQ)

})

form.section("7. Maniérisme et attitude.", () => {
    form.enumRadio("Q7", "Comportement moteur inhabituel, du type de ceux qui font remarquer un malade mental dans un groupe de gens normaux. Estimer seulement la bizarrerie des mouvements. Ne pas tenir compte ici d'une simple hyperactivité motrice.", dataQ)

})

form.section("8. Mégalomanie.", () => {
    form.enumRadio("Q8", "Surestimation de soi-même, conviction d'être extraordinairement doué et puissant. Faire l'estimation seulement d'après ce que le malade déclare, soit de son propre statut, soit de sa position par rapport aux autres. Ne pas le déduire de son comportement au cours de l'entretien.", dataQ)

})

form.section("9. Tendances dépressives.", () => {
    form.enumRadio("Q9", "Découragement, tristesse. Estimer seulement l'importance du découragement. Ne pas le déduire d'un ralentissement global ou de plaintes hypocondriaques.", dataQ)

})

form.section("10. Hostilité.", () => {
    form.enumRadio("Q10", "Animosité, mépris, agressivité, dédain pour les autres en dehors de la situation d'examen. Faire l'estimation seulement d'après ce que dit le malade de ses sentiments et de son comportement envers les autres. Ne pas déduire l'hostilité des défenses névrotiques, de l'anxiété ou de plaintes somatiques. (L'attitude envers l'interlocuteur sera notée sous la rubrique 'non coopération').", dataQ)

})

form.section("11. Méfiance.", () => {
    form.enumRadio("Q11", "Croyance (délirante ou autre) que des gens ont, ou ont eu dans le passé, des intentions, ou mauvaises, ou de rejet envers le malade. Ne faire porter l'estimation que sur les soupçons que le malade, d'après ses dires, entretient actuellement, que ces soupçons concernent des circonstances présentes ou passées.", dataQ)

})

form.section("12. Comportement hallucinatoire.", () => {
    form.enumRadio("Q12", "Perceptions sans objet. Ne faire porter l'estimation que sur les expériences survenues au cours de la semaine écoulée, signalées comme telles par le malade, et décrites comme étant nettement différentes de la pensée et de l'imagination normales.", dataQ)

})

form.section("13. Ralentissement moteur.", () => {
    form.enumRadio("Q13", "Baisse de la sthénie apparaissant dans la lenteur du mouvement et du débit du discours, dans une réduction du tonus, dans la rareté du geste. Faire l'estimation seulement d'après l'observation du comportement du malade. Ne pas tenir compte de l'idée que le sujet a de sa propre sthénie.", dataQ)
})

form.section("14. Non-coopération.", () => {
    form.enumRadio("Q14", "Signes manifestes de résistance, d'inimitié, de ressentiment et de manque d'empressement à coopérer avec l'interlocuteur. Faire l'estimation seulement d'après l'attitude et les réponses du malade par rapport à l'interlocuteur et pendant l'entretien. Ne pas tenir compte du mécontentement ou du refus de coopérer se manifestant en dehors de l'entretien.", dataQ)
})

form.section("15. Pensées inhabituelles.", () => {
    form.enumRadio("Q15", "Idées insolites, singulières, étranges ou bizarres. Estimer l'étrangeté. Ne pas tenir compte de la désorganisation du cours de la pensée.", dataQ)
})

form.section("16. Emoussement affectif.", () => {
    form.enumRadio("Q16", "Réduction du tonus émotionnel, impression d'un manque de sensibilité ou de participation affective.", dataQ)
})

form.section("17. Excitation.", () => {
    form.enumRadio("Q17", "Elévation de la tonalité émotionnelle, agitation, réactions plus vives. Tenir compte d'une précipitation excessive dans le débit des paroles et de l'élévation du ton.", dataQ)
})

form.section("18. Désorientation.Désorientation.", () => {
    form.enumRadio("Q18", "Confusion entre personnes, lieux et successions d'événements. Tenir compte des impressions d'irréalité, de peur diffuse, et des difficultés de compréhension d'une situation banale.", dataQ)
})

let score = form.value("Q1") +
            form.value("Q2") +
            form.value("Q3") +
            form.value("Q4") +
            form.value("Q5") +
			form.value("Q6") +
            form.value("Q7") +
            form.value("Q8") +
            form.value("Q9") +
            form.value("Q10") +
			form.value("Q11") +
            form.value("Q12") +
            form.value("Q13") +
            form.value("Q14") +
            form.value("Q15") +
			form.value("Q16") +
            form.value("Q17") +
            form.value("Q18")

form.calc("score", "Score total", score);

if (shared.makeHeader)
    shared.makeFormFooter(nav, page)
