if (shared.makeHeader)
    shared.makeHeader("Evalue la gravité des symptômes de dépression", page)
route.id = page.text("id", "Patient", {value: route.id, mandatory: true, compact: true,
                                       hidden: goupile.isLocked()}).value

form.pushOptions({mandatory: true, missing_mode: 'disable'})

form.section("1. Tristesse apparente", () => {
    form.enumRadio("tristesseApparente", "Correspond au découragement, à la dépression et au désespoir (plus qu'un simple cafard passager) reflétés par la parole, la mimique et la posture. Coter selon la profondeur et l'incapacité à se dérider.", [
        [0, "0 : Pas de tristesse."],
        [1, "1"],
        [2, "2 : Semble découragé mais peut se dérider sans difficulté."],
        [3, "3"],
        [4, "4 : Paraît triste et malheureux la plupart du temps."],
        [5, "5"],
        [6, "6 : Semble malheureux tout le temps. Extrêmement découragé."]
    ])
})

form.section("2. Tristesse exprimée", () => {
    form.enumRadio("tristesseExprimee", "Correspond à l'expression d'une humeur dépressive, que celle-ci soit apparente ou non. Inclut le cafard, le découragement ou le sentiment de détresse sans espoir. Coter selon l'intensité, la durée à laquelle l'humeur est dite être influencée par les événements.", [
        [0, "0 : Tristesse occasionnelle en rapport avec les circonstances."],
        [1, "1"],
        [2, "2 : Triste ou cafardeux, mais se déride sans difficulté."],
        [3, "3"],
        [4, "4 : Sentiment envahissant de tristesse ou de dépression ; l'humeur est encore influencée par les circonstances extérieures."],
        [5, "5"],
        [6, "6 : Tristesse, désespoir ou découragement permanents ou sans fluctuation."]
    ])
})

form.section("3. Tension intérieure", () => {

    form.enumRadio("tensionInterieure", "Correspond aux sentiments de malaise mal défini, d'irritabilité, d'agitation intérieure, de tension nerveuse allant jusqu'à la panique, l'effroi ou l'angoisse. Coter selon l'intensité, la fréquence, la durée, le degré de réassurance nécessaire.", [
        [0, "0 : Calme. Tension intérieure seulement passagère."],
        [1, "1"],
        [2, "2 : Sentiments occasionnels d'irritabilité et de malaise mal défini."],
        [3, "3"],
        [4, "4 : Sentiments continuels de tension intérieure ou panique intermittente que le malade ne peut maîtriser qu'avec difficulté."],
        [5, "5"],
        [6, "6 : Effroi ou angoisse sans relâche. Panique envahissante."]
    ])
})

form.section("4. Réduction du sommeil", () => {
    form.enumRadio("reductionDuSommeil", "Correspond à une réduction de la durée ou de la profondeur du sommeil par comparaison avec le sommeil du patient lorsqu'il n'est pas malade.", [
        [0, "0 : Dort comme d'habitude."],
        [1, "1"],
        [2, "2 : Légère difficulté à s'endormir ou sommeil légèrement réduit, léger ou agité."],
        [3,"3"],
        [4, "4 : Sommeil réduit ou interrompu au moins deux heures."],
        [5, "5"],
        [6, "6 : Moins de deux ou trois heures de sommeil."]
    ])
})

form.section("5. Réduction de l'appétit", () => {
    form.enumRadio("reductionDeLAppetit", "Correspond au sentiment d'une perte de l'appétit comparé à l'appétit habituel. Coter l'absence de désir de nourriture ou le besoin de se forcer pour manger.", [
        [0, "0 : Appétit normal ou augmenté."],
        [1, "1"],
        [2, "2 : Appétit légèrement réduit."],
        [3, "3"],
        [4, "4 : Pas d'appétit. Nourriture sans goût."],
        [5, "5"],
        [6, "6 : Ne mange que si on le persuade."]
    ])
})

form.section("6. Difficulté de concentration ", () => {
    form.enumRadio("difficulteDeConcentration", "Correspond aux difficultés à rassembler ses pensées allant jusqu'à l'incapacité à se concentrer. Coter l'intensité, la fréquence et le degré d'incapacité.", [
        [0, "0 : Pas de difficulté de concentration."],
        [1, "1"],
        [2, "2 : Difficultés occasionnelles à rassemblée ses pensées."],
        [3, "3"],
        [4, "4 : Difficultés à se concentrer et à maintenir son attention, ce qui réduit la capacité à lire ou à soutenir une conversation."],
        [5, "5"],
        [6, "6 : Incapable de lire ou de converser sans grande difficulté."]
    ])
})

form.section("7. Lassitude", () => {
    form.enumRadio("lassitude", "Correspond à une difficulté à se mettre en train ou une lenteur à commencer et à accomplir les activités quotidiennes.", [
        [0, "0 : Guère de difficultés à se mettre en route ; pas de lenteur."],
        [1, "1"],
        [2, "2 : Difficultés à commencer des activités."],
        [3, "3"],
        [4, "4 : Difficultés à commencer des activités routinières qui sont poursuivies avec effort."],
        [5, "5"],
        [6, "6 : Grande lassitude. Incapable de faire quoi que ce soit sans aide."]
    ])
})

form.section("8. Incapacité à ressentir", () => {
    form.enumRadio("incapaciteARessentir", "Correspond à l'expérience subjective d'une réduction d'intérêt pour le monde environnant, ou les activités qui donnent normalement du plaisir. La capacité à réagir avec une émotion appropriée aux circonstances ou aux gens est réduite.", [
        [0, "0 : Intérêt normal pour le monde environnant et pour les gens."],
        [1, "1"],
        [2 , "2 : Capacité réduite à prendre plaisir à ses intérêts habituels."],
        [3 , "3"],
        [4 , "4 : Perte d'intérêt pour le monde environnant. Perte de sentiment pour les amis et les connaissances."],
        [5 , "5"],
        [6 , "6 : Sentiment d'être paralysé émotionnellement, incapacité à ressentir de la colère, du chagrin ou du plaisir, et impossibilité complète ou même douloureuse de ressentir quelque chose pour les proches, parents et amis."]
    ])
})

form.section("9. Pensée pessimiste", () => {
    form.enumRadio("penseePessimiste", "Correspond aux idées de culpabilité, d'infériorité, d'auto-accusation, de pêché, de remords ou de ruine.", [
        [0, "0 : Pas de pensée pessimiste."],
        [1 , "1"],
        [2 , "2 : Idées intermittentes d'échec, d'auto-accusation ou d'autodépreciation."],
        [3 , "3"],
        [4 , "4 : Auto-accusations  persistantes ou idées de culpabilité ou péché précises, mais encore rationnelles. Pessimisme croissant à propos du futur."],
        [5 , "5"],
        [6 , "6 : Idées délirantes de ruine, de remords ou péché inexpiable. Auto-accusations absurdes ou inébranlables."]
    ])
})

form.section("10. Idée de suicide", () => {
    form.enumRadio("ideeDeSuicide", "Correspond au sentiment que la vie ne vaut pas le peine d'être vécue, qu'une mort naturelle serait la bienvenue, idées de suicide et préparatifs au suicide. Les tentatives de suicide ne doivent pas, en elles-mêmes, influencer la cotation.", [
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
form.calc("score", "Score total", score, {hidden: goupile.isLocked()})

if (shared.makeHeader)
    shared.makeFormFooter(nav, page)
