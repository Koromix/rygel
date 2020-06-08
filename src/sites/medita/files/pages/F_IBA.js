if (typeof data !== 'undefined')
    data.makeHeader("Inventaire de Beck pour l'anxiété", page)
route.id = page.text("id", "Patient", {value: route.id, mandatory: true, compact: true}).value

form.output(html`
    <p>Voici une liste de symptômes courants dus à l’anxiété. Veuillez lire chaque symptôme attentivement. Indiquez, en encerclant le chiffre approprié, à quel degré vous avez été affecté par chacun de ces symptômes au cours de la dernière semaine, aujourd’hui inclus.</p>


    <p>Au cours des 7 derniers jours, j’ai été affecté(e)
par…

`)

form.pushOptions({mandatory: true, missingMode: 'disable'})

let reponses = [
    [1, "Pas du tout"],
    [2, "Un peu. Cela ne m'a pas beaucoup dérangé(e)"],
    [3, "Modérément. C'était très déplaisant mais supportable"],
    [4, "Beaucoup. Je pouvaus à peine le supporter."],
]

form.enum("a", "1. Sensations d’engourdissement ou de picotement", reponses)
form.enum("b", "2. Bouffées de chaleur", reponses)
form.enum("c", "3. «Jambes molles», tremblements dans les jambes", reponses)
form.enum("d", "4. Incapacité de se détendre", reponses)
form.enum("e", "5. Crainte que le pire ne survienne", reponses)
form.enum("f", "6. Étourdissement ou vertige, désorientation", reponses)
form.enum("g", "7. Battements cardiaques marqués ou rapides", reponses)
form.enum("h", "8. Mal assuré(e), manque d’assurance dans mes mouvements", reponses)
form.enum("i", "9. Terrifié(e)", reponses)
form.enum("j", "10. Nervosité", reponses)
form.enum("k", "11. Sensation d’étouffement", reponses)
form.enum("l", "12. Tremblements des mains", reponses)
form.enum("m", "13. Tremblements, chancelant(e)", reponses)
form.enum("n", "14. Crainte de perdre le contrôle de soi", reponses)
form.enum("o", "15. Respiration difficile", reponses)
form.enum("p", "16. Peur de mourir", reponses)
form.enum("q", "17. Sensation de peur, «avoir la frousse»", reponses)
form.enum("r", "18. Indigestion ou malaise abdominal", reponses)
form.enum("s", "19. Sensation de défaillance ou d’évanouissement", reponses)
form.enum("t", "20. Rougissement du visage", reponses)
form.enum("u", "21. Transpiration (non associée à la chaleur)", reponses)


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
            form.value("t") +
            form.value("u")
form.calc("score", "Score total", score)


if (typeof data !== 'undefined')
    data.makeFormFooter(nav, page)
