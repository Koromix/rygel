data.makeFormHeader("Evalue la gravité des symptômes de dépression", page)

form.pushOptions({mandatory: true, missingMode: 'disable'})

form.section("Partie 1", () => {
    form.enumRadio("tristesseApparente", "Tristesse apparente :", [
        [0, "0 : Pas de tristesse."],
        [1, "1"],
        [2, "2 : Semble découragé mais peut se dérider sans difficulté."],
        [3, "3"],
        [4, "4 : Paraît triste et malheureux la plupart du temps."],
        [5, "5"],
        [6, "6 : Semble malheureux tout le temps. Extrêmement découragé."]
    ])

    form.enumRadio("tristesseExprimee", "Tristesse exprimée :", [
        [0, "0 : Tristesse occasionnelle en rapport avec les circonstances."],
        [1, "1"],
        [2, "2 : Triste ou cafardeux, mais se déride sans difficulté."],
        [3, "3"],
        [4, "4 : Sentiment envahissant de tristesse ou de dépression."],
        [5, "5"],
        [6, "6 : Tristesse, désespoir ou découragement permanents ou sans fluctuation."]
    ])

    form.enumRadio("tensionInterieure", "Tension intérieure :", [
        [0, "0 : Calme. Tension intérieure seulement passagère."],
        [1, "1"],
        [2, "2 : Sentiments occasionnels d'irritabilité et de malaise mal défini."],
        [3, "3"],
        [4, "4 : Sentiments continuels de tension intérieure ou panique intermittente que le malade ne peut maîtriser qu'avec difficulté."],
        [5, "5"],
        [6, "6 : Effroi ou angoisse sans relâche. Panique envahissante."]
    ])
})

form.section("Partie 2", () => {
    form.enumRadio("reductionDuSommeil", "Réduction du sommeil :", [
        [0, "0 : Dort comme d'habitude."],
        [1, "1"],
        [2, "2 : Légère difficulté à s'endormir ou sommeil légèrement réduit. Léger ou agité."],
        [3,"3"],
        [4, "4 : Sommeil réduit ou interrompu au moins deux heures."],
        [5, "5"],
        [6, "6 : Moins de deux ou trois heures de sommeil."]
    ])

    form.enumRadio("reductionDeLAppetit", "Réduction de l'appétit :", [
        [0, "0 : Appétit normal ou augmenté."],
        [1, "1"],
        [2, "2 : Appétit légèrement réduit."],
        [3, "3"],
        [4, "4 : Pas d'appétit. Nourriture sans goût."],
        [5, "5"],
        [6, "6 : Ne mange que si on le persuade."]
    ])

    form.enumRadio("difficulteDeConcentration", "Difficulté de concentration :", [
        [0, "0 : Pas de difficulté de concentration."],
        [1, "1"],
        [2, "2 : Difficultés occasionnelles à rassemblée ses pensées."],
        [3, "3"],
        [4, "4 : Difficultés à se concentrer et à maintenir son attention, ce qui réduit la capacité à lire ou à soutenir une conversation."],
        [5, "5"],
        [6, "6 : Grande lassitude. Incapable de faire quoi que ce soit sans aide."]
    ])

    form.enumRadio("lassitude", "Lassitude :", [
        [0, "0 : Guère de difficultés à se mettre en route ; pas de lenteur."],
        [1, "1"],
        [2, "2 : Difficultés à commencer des activités."],
        [3, "3"],
        [4, "4 : Difficultés à commencer des activités routinières qui sont poursuivies avec effort."],
        [5, "5"],
        [6, "6 : Grande lassitude. Incapable de faire quoi que ce soit sans aide."]
    ])
})

form.section("Partie 3", () => {
    form.enumRadio("incapaciteARessentir", "Incapacité à ressentir :", [
        [0, "0 : Intérêt normal pour le monde environnant et pour les gens."],
        [1, "1"],
        [2 , "2 : Capacité réduite à prendre plaisir à ses intérêts habituels."],
        [3 , "3"],
        [4 , "4 : Perte d'intérêt pour le monde environnant. Perte de sentiment pour les amis et les connaissances."],
        [5 , "5"],
        [6 , "6 : Sentiment d'être paralysé émotionnellement, incapacité à ressentir de la colère, du chagrin ou du plaisir, et impossibilité complète ou même douloureuse de ressentir quelque chose pour les proches, parents et amis."]
    ])

    form.enumRadio("penseePessimiste", "Pensée pessimiste :", [
        [0, "0 : Pas de pensées pessimistes."],
        [1 , "1"],
        [2 , "2 : Idées intermittentes d'échec, d'auto-accusation et d'autodépreciation."],
        [3 , "3"],
        [4 , "4 : Auto-accusations  persistantes ou idées de culpabilité ou péché précises, mais encore rationnelles. Pessimisme croissant à propos du futur."],
        [5 , "5"],
        [6 , "6 : Idées délirantes de ruine, de remords ou péché inexpiable. Auto-accusations absurdes et inébranlables."]
    ])

    form.enumRadio("ideeDeSuicide", "Idée de suicide :", [
        [0, "0 : Jouit de la vie ou la prend comme elle vient."],
        [1 , "1"],
        [2 , "2 : Fatigué de la vie, idées de suicide seulement passagères."],
        [3 , "3"],
        [4 , "4 : Il vaudrait mieux être mort. Les idées de suicide sont courantes et le suicide est considéré comme une solution possible, mais sans projet ou intention précis."],
        [5 , "5"],
        [6 , "6 : Projets explicites de suicide si l'occasion se présente. Préparatifs de suicide."]
    ])
})

let score = form.value("ideeDeSuicide") +
            form.value("penseePessimiste") +
            form.value("incapaciteARessentir") +
            form.value("lassitude") +
            form.value("difficulteDeConcentration") +
            form.value("reductionDeLAppetit") +
            form.value("reductionDuSommeil") +
            form.value("tensionInterieure") +
            form.value("tristesseExprimee") +
            form.value("tristesseApparente")
form.calc("score", "Score total", score)

data.makeFormFooter(page)
