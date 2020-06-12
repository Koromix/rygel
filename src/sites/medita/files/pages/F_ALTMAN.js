if (data.makeHeader)
    data.makeHeader("Questionnaire de Altman (ASRM)", page)
route.id = page.text("id", "Patient", {value: route.id, mandatory: true, compact: true,
                                       hidden: goupile.isLocked()}).value

form.output(html`
	<p>Choisir la proposition dans chaque groupe qui correspond le mieux à la manière dont vous vous êtes
senti(e) la semaine dernière.
	<p>Veuillez noter : <p>« parfois » utilisé ici signifie une ou deux fois,
<p>« Souvent » signifie plusieurs,
<p>« Fréquemment » signifie la plupart du temps
</p>
    `)


form.pushOptions({mandatory: true, missingMode: 'disable'})

form.section("QUESTION 1 ", () => {
    form.enumRadio("Q1", "", [
        [0, "0. Je ne me sens pas plus heureux (se) ou plus joyeux (se) que d’habitude."],
        [1, "1. Je me sens parfois plus heureux (se) ou plus joyeux (se) que d’habitude."],
        [2, "2. Je me sens souvent plus heureux (se) ou plus joyeux (se) que d’habitude."],
        [3, "3. Je me sens plus heureux (se) ou plus joyeux (se) que d’habitude la plupart du temps."],
        [4, "4. Je me sens plus heureux (se) ou plus joyeux (se) que d’habitude tout le temps."],
          ])
})

form.section("QUESTION 2", () => {
    form.enumRadio("Q2", "", [
         [0, "0. Je ne me sens pas plus sûr(e) de moi que d’habitude."],
        [1, "1. Je me sens parfois plus sûr(e) de moi que d’habitude."],
        [2, "2. Je me sens souvent plus sûr(e) de moi que d’habitude."],
        [3, "3. Je me sens plus sûr(e) de moi que d’habitude la plupart du temps."],
        [4, "4. Je me sens extrêmement sûr de moi tout le temps."],
          ])
})

form.section("QUESTION 3", () => {

    form.enumRadio("Q3", "", [
         [0, "0. Je n’ai pas besoin de moins de sommeil que d’habitude."],
        [1, "1. J’ai parfois besoin de moins de sommeil que d’habitude."],
        [2, "2. J’ai souvent besoin de moins de sommeil que d’habitude."],
        [3, "3. J’ai fréquemment besoin de moins de sommeil que d’habitude."],
        [4, "4. Je peux passer toute la journée et toute la nuit sans dormir et ne toujours pas être fatigué(e)."],
          ])
})

form.section("QUESTION 4", () => {
    form.enumRadio("Q4", "", [
  [0, "0. Je ne parle pas plus que d’habitude."],
        [1, "1. Je parle parfois plus que d’habitude."],
        [2, "2. Je parle souvent plus que d’habitude."],
        [3, "3. Je parle fréquemment plus que d’habitude."],
        [4, "4. Je parle sans arrêt et je ne peux être interrompu(e)."],
          ])
})

form.section("QUESTION 5", () => {
    form.enumRadio("Q5", "", [
         [0, "0. Je n’ai pas été plus actif (ve) (que ce soit socialement, sexuellement, au travail, à la maison ou à l’école) que d’habitude."],
        [1, "1. J’ai parfois été plus actif (ve) que d’habitude."],
        [2, "2. J’ai souvent été plus actif (ve) que d’habitude."],
        [3, "3. J’ai fréquemment été plus actif (ve) que d’habitude."],
        [4, "Je suis constamment actif (ve), ou en mouvement tout le temps."],
          ])
})



let score = form.value("Q1") +
            form.value("Q2") +
            form.value("Q3") +
            form.value("Q4") +
            form.value("Q5")
form.calc("score", "Score total =", score, {hidden: goupile.isLocked()})

if (data.makeHeader)
    data.makeFormFooter(nav, page)
