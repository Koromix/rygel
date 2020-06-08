if (typeof data !== 'undefined')
    data.makeHeader("AUTO-QUESTIONNAIRE DE ANGST", page)
route.id = page.text("id", "Patient", {value: route.id, mandatory: true, compact: true}).value

form.output(html`
    <p>Cocher les cases vrai/faux en pensant aux derniers épisodes durant lesquels vous vous êtes senti "bien dans votre peau", heureux, agité ou irritable.

`)

form.pushOptions({mandatory: true, missingMode: 'disable'})

form.binary("Q1", "1. Moins d'heures de sommeil.")
form.binary("Q2", "2. Davantage d'énergie et de résistance physique.")
form.binary("Q3", "3. Davantage de confiance en soi.")
form.binary("Q4", "4. Davantage de plaisir à faire plus de travail.")
form.binary("Q5", "5. Davantage d'activités sociales (plus d'appels téléphoniques, plus de visites…).")
form.binary("Q6", "6. Plus de déplacements et voyages ; davantage d'imprudences au volant.")
form.binary("Q7", "7. Dépenses d'argent excessives.")
form.binary("Q8", "8. Comportements déraisonnables dans les affaires.")
form.binary("Q9", "9. Surcroît d'activité (y compris au travail).")
form.binary("Q10", "10. Davantage de projets et d'idées créatives.")
form.binary("Q11", "11. Moins de timidité, moins d'inhibition.")
form.binary("Q12", "12. Plus bavard que d'habitude.")
form.binary("Q13", "13. Plus d'impatience ou d'irritabilité que d'habitude.")
form.binary("Q14", "14. Attention facilement distraite.")
form.binary("Q15", "15. Augmentation des pulsions sexuelles.")
form.binary("Q16", "16. Augmentation de la consommation de café et de cigarettes.")
form.binary("Q17", "17. Augmentation de la consommation d'alcool.")
form.binary("Q18", "18. Exagérément optimiste, voire euphorique.")
form.binary("Q19", "19. Augmentation du rire (farces, plaisanteries, jeux de mots, calembours).")
form.binary("Q20", "20. Rapidité de la pensée, idées soudaines, calembours.")

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
            form.value("Q20")

form.calc("score", "Score total", score);

if (typeof data !== 'undefined')
    data.makeFormFooter(nav, page)
