if (data.makeHeader)
    data.makeHeader("échelle brève d'évaluation du fonctionnement du patient (FAST)", page)
route.id = page.text("id", "Patient", {value: route.id, mandatory: true, compact: true,
                                       hidden: goupile.isLocked()}).value

let cotation = [[0, "Aucune difficulté"], [1, "Difficulté légère"],[2, "Difficulté modérée"],[3, "Difficulté sévère"]]

form.pushOptions({mandatory: true, missingMode: 'disable'})

form.section("AUTONOMIE", () => {
form.enum("Q1", "1. Prendre des responsabilités au sein de la maison",cotation)
form.enum("Q2", "2. Vivre seul(e)",cotation)
form.enum("Q3", "3. Faire les courses",cotation)
form.enum("Q4", "4. Prendre soin de soi (aspect physique, hygiène…)",cotation)
})

form.section("ACTIVITE PROFESSIONNELLE", () => {
form.enum("Q5", "5. Avoir un emploi rémunéré",cotation)
form.enum("Q6", "6. Terminer les tâches le plus rapidement possible",cotation)
form.enum("Q7", "7. Travailler dans un champ correspondant à votre formation",cotation)
form.enum("Q8", "8. Recevoir le salaire que vous méritez",cotation)
form.enum("Q9", "9. Gérer correctement la somme de travail",cotation)
})

form.section("FONCTIONNEMENT COGNITIF", () => {

form.enum("Q10", "10. Capacité à se concentrer devant un film, un livre…",cotation)
form.enum("Q11", "11. Capacité au calcul mental",cotation)
form.enum("Q12", "12. Capacité à résoudre des problèmes correctement",cotation)
form.enum("Q13", "13. Capacité à se souvenir des noms récemment appris",cotation)
form.enum("Q14", "14. Capacité à apprendre de nouvelles informations",cotation)
})

form.section("FINANCES", () => {

form.enum("Q15", "15. Gérer votre propre argent",cotation)
form.enum("Q16", "16. Dépenser de façon équilibrée",cotation)
})

form.section("Relations interpersonnelles", () => {
form.enum("Q17", "17. Conserver des amitiés",cotation)
form.enum("Q18", "18. Participer à des activités sociales",cotation)
form.enum("Q19", "19. Avoir de bonnes relations avec vos proches",cotation)
form.enum("Q20", "20. Habiter avec votre famille",cotation)
form.enum("Q21", "21. Avoir des relations sexuelles satisfaisantes",cotation)
form.enum("Q22", "22. Etre capable de défendre vos intérêts",cotation)
})

form.section("Loisirs", () => {

form.enum("Q23", "23. Faire de l’exercice ou pratiquer un sport",cotation)
form.enum("Q244", "24. Avoir des loisirs",cotation)
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
            form.value("Q18") +
            form.value("Q19") +
            form.value("Q20") +
            form.value("Q21") +
            form.value("Q22") +
            form.value("Q23") +
            form.value("Q24")
form.calc("score", "Score total", score, {hidden: goupile.isLocked()})

if (data.makeHeader)
    data.makeFormFooter(nav, page)
