data.makeFormHeader("PCLS", page)
form.output(html`
    <p>Voici une liste de problèmes que les gens éprouvent parfois suite à une expérience vraiment stressante.
Veuillez lire chaque énoncé attentivement et cocher la case pour indiquer dans quelle mesure ce problème vous a
affecté dans le dernier mois.</p>

`)

form.pushOptions({mandatory: true, missingMode: 'disable'})

let reponses = [
    [1, "Pas du tout"],
    [2, "Un peu"],
    [3, "Modérément"],
    [4, "Beaucoup"],
    [5, "Extrêmement"]
]

form.enum("a", "1. Des souvenirs répétés, pénibles et involontaires de l’expérience stressante ?", reponses)
form.enum("b", "2. Des rêves répétés et pénibles de l’expérience stressante ?", reponses)
form.enum("c", "3. Se sentir ou agir soudainement comme si vous viviez à nouveau l’expérience stressante ?", reponses)
form.enum("d", "4. Se sentir mal quand quelque chose vous rappelle l’événement ?", reponses)
form.enum("e", "5. Avoir de fortes réactions physiques lorsque quelque chose vous rappelle l’événement (accélération cardiaque, difficulté respiratoire, sudation) ?", reponses)
form.enum("f", "6. Essayer d’éviter les souvenirs, pensées, et sentiments liés à l’événement ?", reponses)
form.enum("g", "7. Essayer d’éviter les personnes et les choses qui vous rappellent l’expérience stressante (lieux, personnes, activités, objets) ?", reponses)
form.enum("h", "8. Des difficultés à vous rappeler des parties importantes de l’événement ?", reponses)
form.enum("i", "9. Des croyances négatives sur vous-même, les autres, le monde (des croyances comme : je suis mauvais, j’ai quelque chose qui cloche, je ne peux avoir confiance en personne, le monde est dangereux) ?", reponses)
form.enum("j", "10. Vous blâmer ou blâmer quelqu’un d’autre pour l’événement ou ce qui s’est produit ensuite ?", reponses)
form.enum("k", "11. Avoir des sentiments négatifs intenses tels que peur, horreur, colère, culpabilité, ou honte ?", reponses)
form.enum("l", "12. Perdre de l’intérêt pour des activités que vous aimiez auparavant ?", reponses)
form.enum("m", "13. Vous sentir distant ou coupé des autres ?", reponses)
form.enum("n", "14. Avoir du mal à éprouver des sentiments positifs (par exemple être incapable de ressentir de la joie ou de l’amour envers vos proches) ?", reponses)
form.enum("o", "15. Comportement irritable, explosions de colère, ou agir agressivement ?", reponses)
form.enum("p", "16. Prendre des risques inconsidérés ou encore avoir des conduites qui pourraient vous mettre en danger ?", reponses)
form.enum("q", "17. Être en état de « super-alerte », hyper vigilant ou sur vos gardes ?", reponses)
form.enum("r", "18. Sursauter facilement ?", reponses)
form.enum("s", "19. Avoir du mal à vous concentrer ?", reponses)
form.enum("t", "20. Avoir du mal à trouver le sommeil ou à rester endormi ?", reponses)

let score = form.value("a") +
            form.value("b") +
            form.value("c") +
            form.value("d") +
            form.value("e") +
            form.value("f") +
            form.value("g") +
            form.value("h") +
            form.value("i") +
            form.value("j") +
            form.value("k") +
            form.value("l") +
            form.value("m") +
            form.value("n") +
            form.value("o") +
            form.value("p") +
            form.value("q") +
            form.value("r") +
            form.value("s") +
            form.value("t")
form.calc("score", "Score total", score)

data.makeFormFooter(nav, page)
