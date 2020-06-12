if (data.makeHeader)
    data.makeHeader("Questionnaire sur les troubles de l'humeur (Mood Disorder QUESTIONNAIRE", page)
route.id = page.text("id", "Patient", {value: route.id, mandatory: true, compact: true,
                                       hidden: goupile.isLocked()}).value

form.pushOptions({mandatory: true, missingMode: 'disable'})

form.section("1- Y a-t-il jamais eu des périodes de temps où vous n’étiez pas comme d’habitude et où…", ()=> {
    form.binary("a", "Vous vous sentiez si bien et si remonté que d’autres personnes pensaient que vous n’étiez pas comme d’habitude ou que vous étiez si remonté que vous alliez vous attirer des ennuis.")
    form.binary("b", "Vous étiez si irritable que vous criez après les gens ou que vous provoquiez des bagarres ou des disputes.")
    form.binary("c", "Vous vous sentiez beaucoup plus sûr(e) de vous que d’habitude.")
    form.binary("d", "Vous dormiez beaucoup moins que d’habitude et trouviez que cela ne vous manquait pas vraiment.")
    form.binary("e", "Vous étiez beaucoup plus bavard(e) et parliez beaucoup plus vite que d’habitude")
    form.binary("f", "Des pensées traversaient rapidement votre tête et vous ne pouviez pas les ralentir.")
    form.binary("g", "Vous étiez si facilement distrait(e) par ce qui vous entourait que vous aviez des difficultés à vous concentrer ou à poursuivre la même idée")
    form.binary("h", "Vous aviez beaucoup plus d’énergie que d’habitude.")
    form.binary("i", "Vous étiez beaucoup plus actif(ve) ou vous faisiez beaucoup plus de choses que d’habitude.")
    form.binary("j", "Vous étiez beaucoup plus sociable ou extraverti(e) que d’habitude, par exemple, vous téléphoniez à vos amis au milieu de la nuit.")
    form.binary("k", "Vous étiez beaucoup plus intéressé(e) par le sexe que d’habitude.")
    form.binary("l", "Vous faisiez des choses inhabituelles pour vous ou que d’autres gens pensaient être excessives, imprudentes ou risquées.")
    form.binary("m", "Vous dépensiez de l’argent de manière si inadaptée que cela vous attirait des ennuis ou à votre famille.")
})

form.binary("n", "2- Si vous avez coché « oui » à plus d’une des questions précédentes, est-ce que plusieurs d’entre elles sont apparues durant la même période de temps ?")

form.enumRadio("o", "3- A quel point, une de ces questions a été pour vous un problème au point de ne plus travailler, d’avoir des difficultés familiales, légales, d’argent, de vous inciter à des bagarres ou des disputes ?", [
    [0,  "Pas de problème"],
    [1 , "Problème mineur"],
    [2 , "Problème moyen"]
])

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
            form.value("o")
form.calc("score", "Score total", score, {hidden: goupile.isLocked()})

if (data.makeHeader)
    data.makeFormFooter(nav, page)
