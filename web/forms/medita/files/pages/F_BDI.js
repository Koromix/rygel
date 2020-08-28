if (shared.makeHeader)
    shared.makeHeader("Inventaire de dépression de Beck (BDI)", page)
route.id = page.text("id", "Patient", {value: route.id, mandatory: true, compact: true,
                                       hidden: goupile.isLocked()}).value

form.output(html`
	<p>Ce questionnaire comporte plusieurs séries de quatre propositions. Pour chaque série, lisez les quatre propositions, puis choisissez celle qui décrit le mieux votre état actuel.
	<p>Cochez la case qui correspond à la proposition choisie. Si, dans une série, plusieurs propositions paraissent convenir, cochez les cases correspondantes.
</p>
    `)


form.pushOptions({mandatory: true, missing_mode: 'disable'})

form.section("A", () => {
    form.multiCheck("A", "", [
        [0, "0 : Je ne me sens pas triste."],
        [1, "1 : Je me sens cafardeux ou triste."],
        [2, "2 : Je me sens tout le temps cafardeux ou triste, et je n'arrive pas à en sortir."],
        [3, "3 : Je suis si triste et si malheureux que je ne peux pas le supporter."],
    ])
})

form.section("B", () => {
    form.multiCheck("B", "", [
        [0, "0 : Je ne suis pas particulièrement découragé ni pessimiste au sujet de l'avenir."],
        [1, "1 : J'ai un sentiment de découragement au sujet de l'avenir."],
        [2, "2 : Pour mon avenir, je n'ai aucun motif d'espérer."],
        [3, "3 : Je sens qu'il n'y a aucun espoir pour mon avenir, et que la situation ne peut s'améliorer."],
    ])
})

form.section("C", () => {

    form.multiCheck("C", "", [
        [0, "0 : Je n'ai aucun sentiment d'échec de ma vie."],
        [1, "1 : J'ai l'impression que j'ai échoué dans ma vie plus que la plupart des gens."],
        [2, "2 : Quand je regarde ma vie passée, tout ce que j'y découvre n'est qu'échecs."],
        [3, "3 : J'ai un sentiment d'échec complet dans toute ma vie personnelle (dans mes relations avec mes parents, mon mari, ma femme, mes enfants)."],
    ])
})

form.section("D", () => {
    form.multiCheck("D", "", [
        [0, "0 : Je ne me sens pas particulièrement insatisfait."],
        [1, "1 : Je ne sais pas profiter agréablement des circonstances."],
        [2, "2 : Je ne tire plus aucune satisfaction de quoi que ce soit."],
        [3, "3 : Je suis mécontent de tout."],
    ])
})

form.section("E", () => {
    form.multiCheck("E", "", [
        [0, "0 : Je ne me sens pas coupable."],
        [1, "1 : Je me sens mauvais ou indigne une bonne partie du temps."],
        [2, "2 : Je me sens coupable."],
        [3, "3 : Je me juge très mauvais et j'ai l'impression que je ne vaux rien."],
    ])
})

form.section("F", () => {
    form.multiCheck("F", "", [
        [0, "0 : Je ne suis pas déçu par moi-même."],
        [1, "1 : Je suis déçu par moi-même."],
        [2, "2 : Je me dégoûte moi-même."],
        [3, "3 : Je me hais."],
    ])
})

form.section("G", () => {
    form.multiCheck("G", "", [
        [0, "0 : Je ne pense pas à me faire du mal."],
        [1, "1 : Je pense que la mort me libérerait."],
        [2, "2 : J'ai des plans précis pour me suicider."],
        [3, "3 : Si je le pouvais, je me tuerais."],
    ])
})

form.section("H", () => {
    form.multiCheck("H", "", [
        [0, "0 : Je n'ai pas perdu l'intérêt pour Ies autres gens."],
        [1, "1 : Maintenant, je m'intéresse moins aux autres gens qu'autrefois."],
        [2, "2 : J'ai perdu tout l'intérêt que je portais aux autres gens, et j'ai peu de sentiments pour eux."],
        [3, "3 : J'ai perdu tout intérêt pour les autres, et ils m'indiffèrent totalement."],
    ])
})

form.section("I", () => {
    form.multiCheck("I", "", [
        [0, "0 : Je suis capable de me décider aussi facilement que de coutume."],
        [1, "1 : J'essaie de ne pas avoir à prendre de décision."],
        [2, "2 : J'ai de grandes difficultés à prendre des décisions."],
        [3, "3 : Je ne suis plus capable de prendre la moindre décision."],
    ])
})

form.section("J", () => {
    form.multiCheck("J", "", [
        [0, "0 : Je n'ai pas le sentiment d'être plus laid qu'avant."],
        [1, "1 : J'ai peur de paraître vieux ou disgracieux."],
        [2, "2 : J'ai l'impression qu'il y a un changement permanent dans mon apparence physique qui me fait paraître disgracieux."],
        [3, "3 : J'ai l'impression d'être laid et repoussant."],
    ])
})

form.section("K", () => {
    form.multiCheck("K", "", [
        [0, "0 : Je travaille aussi facilement qu'auparavant."],
        [1, "1 : Il me faut faire un effort supplémentaire pour commencer à faire quelque chose."],
        [2, "2 : Il faut que je fasse un très grand effort pour faire quoi que ce soit."],
        [3, "3 : Je suis incapable de faire le moindre travail."],
     ])
})

form.section("L", () => {
    form.multiCheck("L", "", [
        [0, "0 : Je ne suis pas plus fatigué que d'habitude."],
        [1, "1 : Je suis fatigué plus facilement que d'habitude."],
        [2, "2 : Faire quoi que ce soit me fatigue."],
        [3, "3 : Je suis incapable de faire le moindre travail."],
    ])
})

form.section("M", () => {
    form.multiCheck("M", "", [
        [0, "0 : Mon appétit est toujours aussi bon."],
        [1, "1 : Mon appétit n'est pas aussi bon que d'habitude."],
        [2, "2 : Mon appétit est beaucoup moins bon maintenant."],
        [3, "3 : Je n'ai plus du tout d'appétit."],
    ])
})

let score = form.value("A") +
            form.value("B") +
            form.value("C") +
            form.value("D") +
            form.value("E") +
            form.value("F") +
            form.value("G") +
            form.value("H") +
            form.value("I") +
            form.value("J") +
			form.value("K") +
			form.value("L") +
			form.value("M")
form.calc("score", "Score total", score, {hidden: goupile.isLocked()})

if (shared.makeHeader)
    shared.makeFormFooter(nav, page)
