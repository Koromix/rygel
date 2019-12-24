data.makeFormHeader("PCLS", page)
form.output(html`
    <p>Veuillez trouver ci-dessous une liste de problèmes et de symptômes fréquents à la suite d’un épisode de vie traumatique. Veuillez lire chaque problème avec soin puis déplacer le curseur de 1 à 5 pour indiquer à quel point vous avez été perturbé par ce problème dans le mois précédent.</p>
    <p>1 => pas du tout ; 2=> un peu ; 3 => parfois ; 4 =>souvent ; 5 => très souvent</p>
`)

form.pushOptions({mandatory: true, missingMode: 'disable'})

let reponses = [
    [1, "Pas du tout"],
    [2, "Un peu"],
    [3, "Parfois"],
    [4, "Souvent"],
    [5, "Très souvent"]
]

form.enum("a", "1) Etre perturbé(e) par des souvenirs, des pensées ou des images en relation avec cet épisode stressant.", reponses)
form.enum("b", "2) Etre perturbé(e) par des rêves  répétés en relation avec cet événement.", reponses)
form.enum("c", "3) Brusquement agir ou sentir comme si l’épisode stressant se reproduisait (comme si vous étiez en train de le revivre).", reponses)
form.enum("d", "4) Se sentir très bouleversé(e) lorsque quelque chose vous rappelle l’épisode stressant.", reponses)
form.enum("e", "5) Avoir des réactions physiques, par exemple, battements de cœur, difficultés à respirer, sueurs lorsque quelque chose vous a rappelé l’épisode stressant.", reponses)
form.enum("f", "6) Eviter de penser ou de parler de votre épisode stressant ou éviter des sentiments qui sont en relation avec lui.", reponses)
form.enum("g", "7) Eviter des activités ou des situations parce qu’elles vous rappellent votre épisode stressant.", reponses)
form.enum("h", "8) Avoir des difficultés à se souvenir de parties importantes de l’expérience stressante.", reponses)
form.enum("i", "9) Perte d’intérêt dans des activités qui habituellement vous faisaient plaisir.", reponses)
form.enum("j", "10) Se sentir distant ou coupé(e) des autres personnes.", reponses)
form.enum("k", "11) Se sentir comme si votre avenir était en quelque sorte raccourci.", reponses)
form.enum("l", "12) Avoir des difficultés pour vous endormir ou rester endormi(e).", reponses)
form.enum("m", "13) Se sentir irritable ou avoir des bouffées de colère.", reponses)
form.enum("n", "14) Avoir des difficultés à vous concentrer", reponses)
form.enum("o", "15) Etre en état de super-alarme, sur la défensive, ou sur vos gardes.", reponses)
form.enum("p", "16) Se sentir énervé(e) ou sursauter facilement.", reponses)

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
            form.value("p")
form.calc("score", "Score total", score)

data.makeFormFooter(page)
