if (typeof data !== 'undefined')
    data.makeHeader("Echelle de mesure de l'observance médicamenteuse (MARS)", page)
route.id = page.text("id", "Patient", {value: route.id, mandatory: true, compact: true}).value

form.output(html`
    <p>Ce questionnaire consiste à mieux comprendre les difficultés liées à la prise de médicaments.
    <p>Veuillez répondre aux questions suivantes en cochant la réponse qui correspond le mieux à votre comportement ou attitude vis-à-vis du traitement que vous preniez durant la semaine avant votre hospitalisation.
`)

form.pushOptions({mandatory: true, missingMode: 'disable'})

form.binary("Q1", "1. Vous est-t-il parfois arrivé d'oublier de prendre vos médicaments ?")
form.binary("Q2", "2. Négligez-vous parfois l'heure de prise d'un de vos médicaments ?")
form.binary("Q3", "3. Lorsque vous vous sentez mieux, interrompez-vous parfois votre traitement ?")
form.binary("Q4", "4. Vous est-t-il arrivé d'arrêter le traitement parce que vous vous sentiez moins bien en le prenant ?")
form.binary("Q5", "5. Je ne prends les médicaments que lorsque je me sens malade.")
form.binary("Q6", "6. Ce n'est pas naturel pour mon corps et pour mon esprit d'être équilibré par des médicaments.")
form.binary("Q7", "7. Mes idées sont plus claires avec les médicaments.")
form.binary("Q8", "8. En continuant à prendre les médicaments, je peux éviter de tomber à nouveau malade.")
form.binary("Q9", "9. Avec les médicaments je me sens bizarre comme un « zombie ».")
form.binary("Q10", "10. Les médicaments me rendent lourd(e) et fatigué(e).")

let score = form.value("Q1") +
            form.value("Q2") +
            form.value("Q3") +
            form.value("Q4") +
            form.value("Q5") +
	        form.value("Q6") +
            form.value("Q7") +
            form.value("Q8") +
            form.value("Q9") +
            form.value("Q10")

form.calc("score", "Score total", score);

if (typeof data !== 'undefined')
    data.makeFormFooter(nav, page)
