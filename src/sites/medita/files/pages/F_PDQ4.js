data.makeFormHeader("PDQ4 (À finir)", page)
form.output(html`
    <p>Le but de ce questionnaire est de vous aider à décrire le genre de personne que vous êtes. Pour répondre aux questions, pensez à la manière dont vous avez eu tendance à ressentir les choses, à penser et à agir durant ces dernières années. Afin de vous rappeler cette consigne, chaque page du questionnaire commence par la phrase : « Depuis plusieurs années... ».</p>
`)

form.pushOptions({mandatory: true, missingMode: 'disable'})

form.binary("a", "J'évite de travailler avec des gens qui pourraient me critiquer.")
form.binary("b", "Je ne peux prendre aucune décision sans le conseil ou le soutien des autres.")
form.binary("c", "Je me perds souvent dans les détails et n'ai plus de vision d'ensemble..")
form.binary("d", "J'ai besoin d'être au centre de l'attention générale.")
form.binary("e", "J'ai accompli beaucoup plus de choses que ce que les autres me reconnaissent..")
form.binary("f", "Je ferais n'importe quoi pour éviter que ceux qui me sont chers ne me quittent..")
form.binary("g", "Les autres se sont plaint que je ne sois pas à la hauteur professionnellement ou que je ne tienne pas mes engagements..")
form.binary("h", "J'ai eu des problèmes avec la loi à plusieurs reprises (ou j'en aurais eu si j'avais été pris(e))..")
form.binary("i", "Passer du temps avec ma famille ou avec des amis ne m'intéresse pas vraiment.")
form.binary("j", "Je reçois des messages particuliers de ce qui se passe autour de moi.")
form.binary("k", "Je sais que, si je les laisse faire, les gens vont profiter de moi ou chercher à me tromper..")
form.binary("l", "Parfois, je me sens bouleversé(e).")
form.binary("m", "Je ne me lie avec les gens que lorsque je suis sur qu'ils m'aiment.")
form.binary("n", "Je suis habituellement déprimé(e.")
form.binary("o", "Je préfère que ce soit les autres qui soient responsables pour moi.")
form.binary("p", "Je perds du temps à m'efforcer de tout faire parfaitement.")
form.binary("q", "Je suis plus «sexy» que la plupart des gens.")
form.binary("r", "Je me surprends souvent à penser à la personne importante que je suis ou que je vais devenir un jour.")
form.binary("s", "Je me bagarre beaucoup physiquement.")
form.binary("t", "Je sens très bien que les autres ne comprennent pas ou ne m'apprécient pas.")
form.binary("u", "J'aime mieux faire les choses tout(e) seul(e) qu'avec les autres.")
form.binary("v", "Je suis capable de savoir que certaines choses vont se produire avant qu'elles n'arrivent")
form.binary("w", "Je me demande souvent si les gens que je connais sont dignes de confiance")
form.binary("x", "Je suis capable de savoir que certaines choses vont .")
form.binary("y", "Parfois, je parle des gens dans leur dos.")
form.binary("z", "Je suis inhibé(e) dans mes relations intimes parce que j'ai peur d'être ridiculisé.")

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
            form.value("u") +
            form.value("v") +
            form.value("w") +
            form.value("x") +
            form.value("y") +
            form.value("z")
form.calc("score", "Score total", score)

data.makeFormFooter(page)
