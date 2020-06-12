if (data.makeHeader)
    data.makeHeader("Questionnaire de fonctionnement social (QFS)", page)
route.id = page.text("id", "Patient", {value: route.id, mandatory: true, compact: true,
                                       hidden: goupile.isLocked()}).value

let cotation1 = [[5, "Tous les jours"], [4, "Au moins deux fois par semaine"],[3, "au moins une fois par semaine"],[2, "Une fois tous les 15 jours"], [1, "Jamais"]]

let cotation2 = [[1, "Très satisfait(e)"], [2, "Plutôt satisfait(e)"],[3, "Moyennement satisfait(e)"],[4, "Plutôt insatisfait(e)"], [5, "Très insatisfait(e)"]]

form.pushOptions({mandatory: true, missingMode: 'disable'})

form.section("ACTIVITES", () => {
form.enum("Q1", "1. Au cours de ces 2 dernières semaines, à quelle fréquence avez-vous accompli l’une ou l’autre de ces activités (activités professionnelles, études, activités en atelier protégé ou dans une structure de soins, activités bénévoles, recherche d’emploi,…) ?",cotation1)
form.enum("Q2", "2. Êtes-vous satisfait(e) de la manière dont vous avez accompli ces activités au cours des 2 dernières semaines ?",cotation2)
})

form.section("TÂCHES DE LA VIE QUOTIDIENNE", () => {
form.enum("Q3", "1. Au cours de ces 2 dernières semaines, à quelle fréquence avez-vous réalisé l’une ou l’autre de ces tâches (ménage,achats, cuisine, éducation des enfants,…) ?",cotation1)
form.enum("Q4", "2. Êtes-vous satisfait(e) de la manière dont vous avez réalisé ces tâches au cours des 2 dernières semaines ?",cotation2)
})

form.section("LOISIRS", () => {

form.enum("Q5", "1. Au cours de ces 2 dernières semaines, à quelle fréquence avez-vous consacré du temps à vos loisirs (sport, activités artistiques ou culturelles, lecture, séance de cinéma,…) ??",cotation1)
form.enum("Q6", "2. Êtes-vous satisfait(e) des loisirs que vous avez eus au cours des 2 dernières semaines ?",cotation2)
})

form.section("RELATIONS FAMILIALES ET DE COUPLE", () => {

form.enum("Q7", "1. Au cours de ces 2 dernières semaines, à quelle fréquence avez-vous accompli l’une ou l’autre de ces activités (activités professionnelles, études, activités en atelier protégé ou dans une structure de soins, activités bénévoles, recherche d’emploi,…) ?",cotation1)
form.enum("Q8", "2. Êtes-vous satisfait(e) de la manière dont vous avez accompli ces activités au cours des 2 dernières semaines ?",cotation2)
})

form.section("RELATIONS EXTRAFAMILIALES", () => {
form.enum("Q9", "1. Durant ces 2 dernières semaines, à quelle fréquence avez-vous eu des relations avec des membres de votre famille (parents, conjoint ou concubin, enfants, fratrie, cousins,…) ?",cotation1)
form.enum("Q10", "Êtes-vous satisfait(e) des relations que vous avez eues avec ces personnes de votre entourage extrafamilial au cours des 2 dernières semaines ?",cotation2)
})

form.section("GESTION FINANCIÈRE ET ADMINISTRATIVE", () => {

form.enum("Q11", "1. Au cours de ces 2 dernières semaines, à quelle fréquence avez-vous veillé à votre gestion financière et administrative (paiements, versements, classement,…)?",cotation1)
form.enum("Q12", "2. Êtes-vous satisfait(e) de la manière dont vous avez veillé à votre gestion financière et administrative au cours des 2 dernières semaines ?",cotation2)
})

form.section("SANTÉ GÉNÉRALE", () => {

form.enum("Q13", "1. Au cours de ces 2 dernières semaines, à quelle fréquence avez-vous pris soin de votre santé générale (hygiène et présentation corporelle, alimentation, soins médicaux et dentaires de base,…) ?",cotation1)
form.enum("Q14", "2. Êtes-vous satisfait(e) de la façon dont vous avez pris soin de votre santé générale au cours des 2 dernières semaines ?",cotation2)
})

form.section("VIE COLLECTIVE ET INFORMATIONS", () => {

form.enum("Q15", "1. Au cours de ces 2 dernières semaines, à quelle fréquence vous êtes-vous tenu informé(e) et/ou avez-vous participé à la vie collective (participation à la vie politique, associative, culturelle de votre milieu de vie et information au sujet des actualités régionales et mondiales,…) ?",cotation1)
form.enum("Q16", "2. Êtes-vous satisfait(e) de la façon dont vous vous êtes tenu informé(e) et/ou avez participé à la vie collective au cours des 2 dernières semaines ?",cotation2)
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
            form.value("Q16")
form.calc("score", "Score total", score, {hidden: goupile.isLocked()})

if (data.makeHeader)
    data.makeFormFooter(nav, page)
