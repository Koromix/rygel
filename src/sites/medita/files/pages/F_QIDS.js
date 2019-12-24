data.makeFormHeader("QIDS", page)

form.pushOptions({mandatory: true, missingMode: 'disable'})

form.section("Partie 1", () => {
    form.enumRadio("endormissement", "Endormissement :", [
        [0, "Je ne mets jamais plus de 30 minutes à m’endormir."],
        [1, "Moins d'une fois sur deux, je mets au moins 30 minutes à m’endormir."],
        [2, "Plus d'une fois sur deux, je mets au moins 30 minutes à m’endormir."],
        [3, "Plus d'une fois sur deux, je mets plus d’une heure à m’endormir."]
    ])

    form.enumRadio("sommeilPendantLaNuit", "Sommeil pendant la nuit :", [
        [0, "Je ne me réveille pas la nuit"],
        [1, "J’ai un sommeil agité, léger et quelques réveils brefs chaque nuit."],
        [2, "Je me réveille au moins une fois par nuit, mais je me rendors facilement."],
        [3, "Plus d'une fois sur deux, je me réveille plus d’une fois par nuit et reste éveillé(e) 20 minutes ou plus."]
    ])

    form.enumRadio("reveilAvantHeurePrevu", "Réveil avant l'heure prévue :", [
        [0, "La plupart du temps, je me réveille 30 minutes ou moins avant le moment où je dois me lever."],
        [1,"Plus d'une fois sur deux, je me réveille plus de 30 minutes avant le moment où je dois me lever."],
        [2, "Je me réveille presque toujours une heure ou plus avant le moment où je dois me lever, mais je finis par me rendormir."],
        [3, "Je me réveille au moins une heure avant le moment où je dois me lever et je n’arrive pas à me rendormir."]
    ])

    form.enumRadio("sommeilexcessif", "Sommeil excessif :", [
        [0, "Je ne dors pas plus de 7 à 8 heures par nuit, et je ne fais pas de sieste dans la journée."],
        [1, "Je ne dors pas plus de 10 heures sur un jour entier de 24 heures, siestes comprises."],
        [2, "Je ne dors pas plus de 12 heures sur un jour entier de 24 heures, siestes comprises."],
        [3, "Je dors plus de 12 heures sur un jour entier de 24 heures, siestes comprises."]
    ])

    form.enumRadio("tristesse", "Tristesse :", [
        [0, "Je ne me sens pas triste."],
        [1, "Je me sens triste moins de la moitié du temps. "],
        [2, "Je me sens triste plus de la moitié du temps."],
        [3, "Je me sens triste presque tout le temps."]
    ])

    form.enumRadio("diminutionApetit", "Diminution de l'apétit :", [
        [0, "J’ai le même appétit que d’habitude"],
        [1, "Je mange un peu moins souvent ou en plus petite quantité que d’habitude."],
        [2, "Je mange beaucoup moins que d’habitude et seulement en me forçant."],
        [3, "Je mange rarement sur un jour entier de 24 heures et seulement en me forçant énormément ou quand on me persuade de manger."]
    ])

    form.enumRadio("augmentationApetit", "Augmentation de l'apétit :", [
        [0, "J’ai le même appétit que d’habitude"],
        [1, "J'éprouve le besoin de manger plus souvent que d’habitude."],
        [2, "Je mange régulièrement plus souvent et/ou en plus grosse quantité que d’habitude."],
        [3, "J'éprouve un grand besoin de manger plus que d'habitude pendant et entre les repas."]
    ])

    form.enumRadio("perteDePoids", "Perte de poids (au cours des 15 derniers jours) :", [
        [0, "Mon poids n’a pas changé."],
        [1, "J’ai l’impression d’avoir perdu un peu de poids."],
        [2, "J’ai perdu 1 kg ou plus."],
        [3, "J’ai perdu plus de 2 kg."]
    ])

    form.enumRadio("priseDePoids", "Prise de poids (au cours des 15 derniers jours) :", [
        [0, "Mon poids n’a pas changé."],
        [1, "J’ai l’impression d’avoir pris un peu de poids."],
        [2, "J’ai pris 1 kg ou plus."],
        [3, "J’ai pris plus de 2 kg."]
    ])
})

form.section("Partie 2", () => {
    form.enumRadio("concentrationPriseDeDecision", "Concentration/Prise de décisions :", [
        [0, " Il n’y a aucun changement dans ma capacité habituelle à me concentrer ou à prendre des décisions."],
        [1, "Je me sens parfois indécis(e) ou je trouve parfois que ma concentration est limitée."],
        [2, "La plupart du temps, j'ai du mal à me concentrer ou à prendre des décisions."],
        [3, "Je n’arrive pas me concentrer assez pour lire ou je n’arrive pas à prendre des décisions même si elles sont insignifiantes."]
    ])

    form.enumRadio("opinionDeMoiMeme", "Opinion de moi-même :", [
        [0, "Je considère que j'ai autant de valeur que les autres et que je suis aussi méritant(e) que les autres."],
        [1, "Je me critique plus que d’habitude."],
        [2, "Je crois fortement que je cause des problèmes aux autres."],
        [3, "Je pense presque tout le temps à mes petits et mes gros défauts."]
    ])

    form.enumRadio("ideeDeMortSuicide", "Idée de mort ou Suicide :", [
        [0, "Je ne pense pas au suicide ni à la mort"],
        [1, "Je pense que la vie est sans intérêt ou je me demande si elle vaut la peine d'être vécue."],
        [2, "Je pense au suicide ou à la mort plusieurs fois par semaine pendant plusieurs minutes."],
        [3, "Je pense au suicide ou à la mort plusieurs fois par jours en détail, j’ai envisagé le suicide de manière précise ou j’ai réellement tenté de mettre fin à mes jours."]
    ])

    form.enumRadio("enthousiasmeGeneral", "Enthousiasme Général :", [
        [0, "Il n’y pas de changement par rapport à d’habitude dans la manière dont je m’intéresse aux gens ou à mes activités."],
        [1, "Je me rends compte que je m’intéresse moins aux gens et à mes activités."],
        [2, "Je me rends compte que je n’ai d’intérêt que pour une ou deux des activités que j'avais auparavant."],
        [3, "Je n’ai pratiquement plus d'intérêt pour les activités que j'avais auparavant."]
    ])

    form.enumRadio("energie", "Energie :", [
        [0, "J'ai autant d'énergie que d'habitude."],
        [1, "Je me fatigue plus facilement que d’habitude."],
        [2, "Je dois faire un gros effort pour commencer ou terminer mes activités quotidiennes (par exemple, faire les courses, les devoirs, la cuisine ou aller au travail)."],
        [3, "Je ne peux vraiment pas faire mes activités quotidiennes parce que je n’ai simplement plus d’énergie."]
    ])

    form.enumRadio("impressionDeRalentissement", "Impression de ralentissement :", [
        [0, "Je pense, je parle et je bouge aussi vite que d’habitude."],
        [1, "Je trouve que je réfléchis plus lentement ou que ma voix est étouffée ou monocorde."],
        [2, "Il me faut plusieurs secondes pour répondre à la plupart des questions et je suis sûr(e) que je réfléchis plus lentement."],
        [3, "Je suis souvent incapable de répondre aux questions si je ne fais pas de gros efforts."]
    ])

    form.enumRadio("impressionDAgitation", "Impression d'agitation :", [
        [0, "Je ne me sens pas agité(e)."],
        [1, "Je suis souvent agité(e), je me tords les mains ou j’ai besoin de changer de position quand je suis assis(e)."],
        [2, "J'éprouve le besoin soudain de bouger et je suis plutôt agité(e)."],
        [3, "Par moments, je suis incapable de rester assis(e) et j’ai besoin de faire les cent pas."]
    ])
})

let score = form.value("impressionDAgitation") +
            form.value("impressionDeRalentissement") +
            form.value("energie") +
            form.value("enthousiasmeGeneral") +
            form.value("ideeDeMortSuicide") +
            form.value("opinionDeMoiMeme") +
            form.value("concentrationPriseDeDecision") +
            form.value("priseDePoids") +
            form.value("perteDePoids") +
            form.value("augmentationApetit") +
            form.value("diminutionApetit") +
            form.value("tristesse") +
            form.value("sommeilexcessif") +
            form.value("reveilAvantHeurePrevu") +
            form.value("sommeilPendantLaNuit") +
            form.value("endormissement")
form.calc("score", "Score total", score)

data.makeFormFooter(page)
