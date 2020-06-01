data.makeFormHeader("Echelle d’évaluation de la dépression psychotique (PDAS)", page)
form.output(html`
    <p><b>Pour les instructions concernant l’entretien, veuillez vous reporter au guide associé.</p>

`)

form.pushOptions({mandatory: true, missingMode: 'disable'})

form.section("1- Symptômes somatiques – généraux", () => {
form.enumRadio("symptomesSomatiques", "", [
    [1, "1: Absent"],
    [2 ,"2: Sentiment incertain ou très vague de fatigue ou de douleurs musculaires"],
    [3 ,"3: Fatigue ou douleurs musculaires plus clairement présentes mais sans retentissement sur les activités du quotidien"],
    [4 ,"4: Fatigue ou douleurs musculaires tellement importantes qu’elles interfèrent significativement dans la vie du quotidien"],
    [5 ,"5: Fatigue ou douleurs musculaires sévères hautement invalidantes"],
])
})

form.section("2- Travail et activités", () => {
form.enumRadio("travailEtActivites", "", [
    [1, "1: Pas de difficulté"],
    [2 ,"2: Problèmes légers dans les activités habituelles du quotidien comme le travail ou les loisirs (à la maison ou à l’extérieur)"],
    [3 ,"3: Perte d’intérêt pour le travail ou les loisirs – rapportée directement par le patient ou indirectement, montrée par un manque d’énergie une indécision et une hésitation (doit se forcer pour finir les choses)"],
    [4 ,"4: Problèmes dans la gestion des tâches du quotidien qui ne peuvent être effectuées qu’avec un effort majeur. Signes clairs de désarroi"],
    [5 ,"5: Totalement incapable d’effectuer les tâches du quotidien sans aide. Désarroi extrême"],
])
})

form.section("3- Humeur dépressive", () => {
form.enumRadio("humeurDepressive", " ", [
    [1, "1: Absente"],
    [2 ,"2: Légère tendance au découragement ou à la tristesse"],
    [3 ,"3: Manifestations claires d’une humeur abaissée. Le patient est modérément déprimé mais le désespoir n’est pas présent"],
    [4 ,"4: Humeur significativement abaissée avec un sentiment occasionnel de désespoir. Il peut y avoir des manifestations non-verbales d’humeur dépressive (par exemple: des pleurs)"],
    [5 ,"5: Humeur dépressive sévère avec sentiment permanent de désespoir. Il peut y avoir des idées délirantes dépressives (par exemple: aucun espoir de rétablissement)"],
])
})

form.section("4- Anxiété psychique", () => {
form.enumRadio("anxietePsychique", "", [
    [1, "1: Absente"],
    [2 ,"2: Seulement une inquiétude, une tension ou une peur légères"],
    [3 ,"3: Inquiétudes par rapport à des sujets mineurs. Encore capable de contrôler l’anxiété"],
    [4 ,"4: L’anxiété et l’inquiétude sont si prononcées qu’il est difficile pour le patient de les contrôler. Les symptômes ont un impact sur les activités du quotidien"],
    [5 ,"5: L’anxiété et l’inquiétude sont hautement invalidantes et le patient est incapable de contrôler les symptômes"],
])
})

form.section("5- Sentiments de culpabilité", () => {
form.enumRadio("sentimentsDeCulpabilite", "", [
    [1, "1: Absent"],
    [2 ,"2: Estime de soi abaissée dans les relations avec la famille, les amis ou les collègues. Le patient a le sentiment d’être un fardeau pour les autres"],
    [3 ,"3: Sentiments de culpabilité plus prononcés. Le patient est préoccupé par des incidents du passé (omissions ou échecs mineurs)"],
    [4 ,"4: Sentiments de culpabilité plus sévères et déraisonnables. Le patient peut ressentir que sa dépression actuelle es"],
    [5 ,"5: Sentiments de culpabilité délirants, le patient ne peut être convaincu qu’ils sont déraisonnables"],
])
})

form.section("6- Ralentissement psychomoteur", () => {
form.enumRadio("ralentissementPsychomoteur", " ", [
    [1, "1: Absent"],
    [2 ,"2: Le niveau d’activité motrice habituel du patient est légèrement réduit"],
    [3 ,"3: Ralentissement psychomoteur plus prononcé, par exemple : gestes modérément réduits, marche et discours ralentis"],
    [4 ,"4: Le ralentissement psychomoteur est évident et l’entretien est clairement prolongé à cause de la lenteur des réponses"],
    [5 ,"5: L’entretien est à peine réalisable à cause du ralentissement psychomoteur. Un état de stupeur dépressif peut être presentables"],
])
})

form.section("7- Retrait émotionnel", () => {
form.enumRadio("retraitEmotionnel", " ", [
    [1, "1: Absent"],
    [2 ,"2: Manque d’implication émotionnelle montré par l’échec notable de faire des commentaires réciproques avec l’interviewer, ou manque de chaleur, mais le patient répond à l’interviewer lorsqu’il est approché"],
    [3 ,"3: Le contact émotionnel est absent pendant la majeure partie de l’entretien. Le patient ne précise pas les réponses, ne parvient pas à maintenir le contact oculaire, ou ne semble pas se soucier de savoir si l'intervieweur est à l'écoute"],
    [4 ,"4: Le patient évite activement la participation émotionnelle. Il ne répond pas souvent aux questions ou répond fréquemment par oui/non, avec un minimum d’affects (non seulement dû à des idées délirantes de persécution)"],
    [5 ,"5: Le patient évite constamment la participation émotionnelle. Il ne répond pas ou répond par oui/non (non seulement dû à des idées délirantes de persécution). Il peut quitter l’entretien"],
])
})

form.section("8- Méfiance", () => {
form.enumRadio("mefiance", " ", [
    [1, "1: Absente"],
    [2 ,"2: Le patient semble sur ses gardes. Décrit des incidents qui semblent plausibles au cours desquels d’autres personnes lui ont fait du mal ou ont voulu le faire. A le sentiment que les autres le regardent, rient de lui ou le critiquent en public, mais cela ne se produit que rarement. Il y a peu ou pas de préoccupation"],
    [3 ,"3: Le patient dit que d’autres personnes parlent de lui de façon malfaisante, avec des intentions négatives, ou qu’elles peuvent lui faire du mal (au-delà de la probabilité de vraisemblance ). La persécution perçue est associée à une certaine préoccupation"],
    [4 ,"4: Le patient est délirant et parle de complots contre lui, par exemple qu’on l’espionne chez lui, au travail ou à l’hôpital"],
    [5 ,"5: Identique à 3, mais les croyances sont plus préoccupantes et le patient a tendance à dévoiler ou à mettre en action les idées délirantes de persécution"],
])
})

form.section("9- Hallucinations", () => {
form.enumRadio("hallucinations", "", [
    [1, "1: Absente"],
    [2 ,"2: Le patient a occasionnellement des visions, entend des voix, des sons ou des chuchotements, sent des odeurs, ou toute autre perception sensorielle, en l’absence de stimuli externes. Il n’y a pas d’altération du fonctionnement"],
    [3 ,"3: Hallucinations visuelles, auditives, gustatives, olfactives, tactiles ou proprioceptives occasionnelles ou quotidiennes, avec un déficit fonctionnel léger"],
    [4 ,"4: Le patient a des hallucinations plusieurs fois par jour, ou certains domaines de fonctionnement sont perturbés par les hallucinations"],
    [5 ,"5: Le patient a des hallucinations persistantes pendant toute la journée ou la plupart des domaines de fonctionnement sont perturbés par les hallucinations"],
])
})

form.section("10- Pensées inhabituelles", () => {
form.enumRadio("penseesInhabituelles", "  ", [
    [1, "1: Absente"],
    [2 ,"2: Vagues idées de référence (des gens l’observent ou rient de lui). Idées de persécution Croyances inhabituelles à propos des pouvoirs psychiques, des esprits et des OVNI. Idées non raisonnables autour des maladies, de la pauvreté, etc. Le patient ne tient pas fortement à ces idées (non délirant)"],
    [3 ,"3: Les idées délirantes sont présentes avec une certaine préoccupation, ou certains domaines de fonctionnement sont perturbés par les idées délirantes"],
    [4 ,"4: Les idées délirantes sont présentes avec beaucoup de préoccupations, ou plusieurs domaines de fonctionnement sont perturbés par les idées délirantes"],
    [5 ,"5: Les idées délirantes sont présentes et constituent l’essentiel des préoccupations, ou la plupart des domaines de fonctionnement sont perturbés par les pensées délirantes"],
])
})

form.section("11- Émoussement des affects", () => {
form.enumRadio("emoussementDesAffects", " ", [
    [1, "1: Absent"],
    [2 ,"2: L’ensemble des réponses émotionnelles est légèrement diminué, atténué ou réservé. Le ton de la voix apparaît monocorde"],
    [3 ,"3: Les expressions émotionnelles sont très diminuées. Le patient ne montre pas d’émotion ou ne réagit pas avec émotion à des sujets pénibles, sauf de façon minimale. L’expression faciale varie peu. Le ton de voix est monocorde la plupart du temps"],
    [4 ,"4: Très peu d’expressions émotionnelles. Le discours et les gestes sont automatiques la plupart du temps. Les expressions faciales ne changent pas. Le ton de la voix est monocorde la plupart du temps"],
    [5 ,"5: Pas d’expression émotionnelle ou gestuelle. Le ton de la voix est très monocorde tout le temps"],
])
})

let score = form.value("symptomesSomatiques") +
            form.value("travailEtActivites") +
            form.value("humeurDepressive") +
            form.value("anxietePsychique") +
            form.value("sentimentsDeCulpabilite") +
            form.value("ralentissementPsychomoteur") +
            form.value("retraitEmotionnel") +
            form.value("mefiance") +
            form.value("hallucinations") +
            form.value("penseesInhabituelles") +
            form.value("emoussementDesAffects")
form.calc("score", "Score total", score)

data.makeFormFooter(nav, page)
