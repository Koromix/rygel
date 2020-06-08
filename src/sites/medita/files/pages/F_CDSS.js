if (typeof data !== 'undefined')
    data.makeHeader("Echelle de Calgary CDSS", page)
route.id = page.text("id", "Patient", {value: route.id, mandatory: true, compact: true}).value

form.output(html`
	<p>Poser la première question telle qu'elle est écrite. Par la suite, vous pouvez utiliser d'autres questions d'exploration ou d'autres questions pertinentes à votre discrétion.
	<p>Le cadre temporel concerne les 2 dernières semaines à moins qu'il ne soit stipulé autrement.
</p>
    `)


form.pushOptions({mandatory: true, missingMode: 'disable'})

form.section("1. DÉPRESSION ", () => {
    form.enumRadio("depression", "Comment pourriez-vous décrire votre humeur durant les 2 dernières semaines : avez-vous pu demeurer raisonnablement gai ou est ce que vous avez été très déprimé ou plutôt triste ces derniers temps ? Combien de fois vous êtes-vous senti ainsi, tous les jours ? toute la journée ? ", [
        [0, "0 : Absente."],
        [1, "1 : Légère : le sujet exprime une certaine tristesse ou un certain découragement lorsqu'il est questionné."],
        [2, "2 : Modéré :  humeur dépressive distinctive est présente tous les jours."],
        [3, "3 : Sévère :  humeur dépressive marquée persistant tous les jours, plus de la moitié du temps, affectant le fonctionnement normal, psychomoteur et social."],
          ])
})

form.section("2. DÉSESPOIR", () => {
    form.enumRadio("desespoir", "Comment entrevoyez-vous le futur pour vous-même? Est ce que vous pouvez envisager un avenir pour vous? Ou  est-ce que la vie vous paraît plutôt sans espoir? Est ce que vous avez tout laissé tomber ou est ce qu'il vous paraît y avoir  encore  des raisons d'essayer? ", [
        [0, "0 : Absent."],
        [1, "1 : Léger :  à certains moments, le sujet s'est senti sans espoir au cours de la dernière semaine mais il a encore un certain degré d'espoir pour l'avenir."],
        [2, "2 : Modéré : perception persistante mais modérée de désespoir au cours de la dernière semaine. On peut cependant persuader le sujet d'acquiescer à la possibilité que les choses peuvent s'améliorer."],
        [3, "3 : Sévère : sentiment persistant et éprouvant de désespoir."],
           ])
})

form.section("3. AUTO-DÉPRÉCIATION ", () => {

    form.enumRadio("autodepreciation", "Quelle est votre opinion de vous-même, en comparaison avec d'autres personnes? Est ce que vous vous sentez meilleur ou moins bon, ou à peu près comparable aux autres personnes en général ? Vous sentez-vous inférieur ou même sans aucune valeur? Si oui quel serait le pourcentage de temps durant lequel vous ressentez ce sentiment?", [
        [0, "0: Absent."],
        [1, "1 : Légère : légère infériorité, n'atteint pas le degré de se sentir sans valeur."],
        [2, "2 : Modéré : le sujet se sent sans valeur mais moins de 50 % du temps."],
        [3, "3 : Sévère :  le sujet se sent sans valeur plus de 50 % du temps. Il peut être mis au défi de reconnaître un autre point de vue."],
            ])
})

form.section("4.  IDÉES DE RÉFÉRENCE ASSOCIÉES A LA CULPABILITÉ", () => {
    form.enumRadio("ideesrefculpabilite", "Avez-vous l'impression que l'on vous blâme pour certaines choses ou même qu'on vous accuse sans raison? A propos de quoi? (ne pas inclure ici des blâmes ou des accusations justifiés. Exclure les délires de culpabilité ou les propos des hallucinations en tant que tels).", [
        [0, "0 : Absente."],
        [1, "1 : Légère : le sujet se sent blâmé mais non accusé, moins de 50 % du temps."],
        [2, "2 : Modérée : sentiment persistant d'être blâmé et/ou sentiment occasionnel d'être accusé.Modérée : sentiment persistant d'être blâmé et/ou sentiment occasionnel d'être accusé."],
        [3,"3 : Sévère : sentiment persistant d'être accusé. Mais lorsqu'on le met en doute, le sujet reconnaît que cela n'est pas vrai"],
        ])
})

form.section("5. CULPABILITÉ PATHOLOGIQUE", () => {
    form.enumRadio("culpabilitepatho", "Avez-vous tendance à vous blâmer vous-même pour des petites choses que vous pourriez avoir faites dans le passé? Pensez-vous que vous méritez d'être aussi préoccupé de cela?", [
        [0, "0 : Absente."],
        [1, "1 : Légère : le sujet se sent coupable de certaines peccadilles mais moins de 50 % du temps."],
        [2, "2 : Modérée : le sujet se sent coupable habituellement (plus de 50 % du temps) à propos d'actes dont il exagère la signification."],
        [3, "3 : Sévère : le sujet ressent habituellement qu'il est à blâmer pour tout ce qui va mal même lorsque ce n'est pas de sa faute"],
        ])
})

form.section("6. DÉPRESSION MATINALE", () => {
    form.enumRadio("depressionmatinale", "Lorsque vous vous êtes senti déprimé au cours des deux dernières semaines, avez-vous remarqué que la dépression était pire à certains moments de la journée?", [
        [0, "0 : Absente."],
        [1, "1 : Légère : dépression présente mais sans variation diurne"],
        [2, "2 : Modérée : le sujet mentionne spontanément que la dépression est pire le matin."],
        [3, "3 : Sévère : la dépression est, de façon marquée, pire le matin, avec un fonctionnement perturbé qui s'améliore l'après midi."],
        ])
})

form.section("7. ÉVEIL PRÉCOCE", () => {
    form.enumRadio("eveilprecoce", "Vous réveillez-vous plus tôt le matin qu'à l'accoutumée? Combien de fois par semaine cela vous arrive-t-il?", [
        [0, "0 : Absent : pas de réveil précoce."],
        [1, "1 : Léger : à l'occasion s'éveille (jusqu'à 2 fois par semaine) 1 heure ou plus avant le moment normal de s'éveiller ou l'heure fixée à son réveille-matin."],
        [2, "2 : Modéré : s'éveille fréquemment de façon hâtive (jusqu'à 5 fois par semaine) 1 heure ou plus avant son heure habituelle d'éveil ou l'heure fixée par son réveil matin "],
        [3, "3 : Sévère : s'éveille tous les jours 1 heure ou plus avant l'heure normale d'éveil."],
        ])
})

form.section("8. SUICIDE", () => {
    form.enumRadio("suicide", " Avez-vous déjà eu l'impression que la vie ne valait pas la peine d'être vécue? Avez-vous déjà pensé mettre fin à tout cela? Qu'est ce que vous pensez que vous auriez pu faire? Avez-vous effectivement essayé?", [
        [0, "0 : Absent."],
        [1, "1 : Léger : le sujet a l'idée qu'il serait mieux mort ou des idées suicidaires occasionnelles.."],
        [2, "2 : Modéré : il a planifié son suicide mais sans faire de tentative."],
        [3, "3 : Sévère : tentative de suicide apparemment conçue pour se terminer par la mort (c'est-à-dire : découverte accidentelle ou par un moyen qui s'est avéré inefficace)"],
        ])
})

form.section("9. DÉPRESSION OBSERVÉE", () => {
    form.enumRadio("depressionobservee", "Se baser sur l'ensemble de l'entretien. La question 'est-ce que vous ressentez parfois l'envie de pleurer?' utilisée à un moment approprié durant l'entretien, peut susciter l'émergence d'informations utiles à cette observation.", [
        [0, "0 : Absente."],
        [1, "1 : Légère : le sujet apparaît triste et sur le point de pleurer même durant des parties de l'entretien touchant des sujets effectivement neutres."],
        [2, "2 : Modérée : le sujet apparaît triste, près des larmes durant tout l'entretien avec une voix monotone et mélancolique, extériorise des larmes ou est près des larmes à certains moments"],
        [3, "3 : Sévère : le patient s'étrangle lorsqu'il évoque des sujets générant de la détresse, soupire profondément, fréquemment et pleure ouvertement, ou est de façon persistante bloqué dans un état de souffrance."],
        ])
})

let score = form.value("depression") +
            form.value("desespoir") +
            form.value("autodepreciation") +
            form.value("ideesrefculpabilite") +
            form.value("culpabilitepatho") +
            form.value("depressionmatinale") +
            form.value("eveilprecoce") +
            form.value("suicide") +
            form.value("depressionobservee")
form.calc("score", "Score total =", score)

if (typeof data !== 'undefined')
    data.makeFormFooter(nav, page)
