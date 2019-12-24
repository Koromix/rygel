data.makeFormHeader("ECHELLE D'ÉVALUATION GLOBALE DU FONCTIONNEMENT (EGF)", page)
form.output(html`
    <p>Evaluer le fonctionnement psychologique, social et professionnel sur un continuum hypothétique allant de la santé mentale à la maladie. Ne pas tenir compte d'un handicap du fonctionnement dû à des facteurs limitant d'ordre physique ou environnemental.</p>
    <p><b>N.B :</b> Utiliser des codes intermédiaires lorsque cela est justifié : p. ex. 45, 68, 72</p>
`)

form.pushOptions({mandatory: true, missingMode: 'disable'})

form.section("De 91 à 100", () => {
    form.output(html`Niveau supérieur de fonctionnement dans une grande variété d’activités. N’est jamais débordé par les problèmes rencontrés. Est recherché par autrui en raison de ses nombreuses qualités.Absence de symptômes.`)
})

form.section("De 81 à 90", () => {
    form.output(html`Symptômes absents ou minimes (p. ex. anxiété légère avant un examen), fonctionnement satisfaisant dans tous les domaines, intéressé et impliqué dans une grande variété d'activités, socialement efficace, en général satisfait de la vie, pas plus de problèmes ou de préoccupations que les soucis de tous les jours (p.ex. conflit occasionnel avec des membres de la famille).`)
})

form.section("De 71 à 80", () => {
    form.output(html`Si des symptômes sont présents, ils sont transitoires et il s'agit de réactions prévisibles à des facteurs de stress (p. ex. des difficultés de concentration après une dispute familiale) ; pas plus qu'une altération légère du fonctionnement social, professionnel ou scolaire (p. ex. retard temporaire du travail scolaire).`)
})

form.section("De 61 à 70", () => {
    form.output(html`Quelques symptômes légers (p. ex. humeur dépressive et insomnie légère) ou une certaine difficulté dans le fonctionnement social, professionnel ou scolaire (p. ex. école buissonnière épisodique ou vol en famille) mais fonctionne assez bien de façon générale et entretient plusieurs relations interpersonnelles positives.`)
})

form.section("De 51 à 60", () => {
    form.output(html`Symptômes d'intensité moyenne (p. ex. émoussement affectif, prolixité circonlocutoire, attaques de panique épisodiques) ou difficultés d'intensité moyenne dans le fonctionnement social, professionnel ou scolaire (p. ex. peu d'amis, conflits avec les camarades de classe ou les collègues de travail).`)
})

form.section("De 41 à 50", () => {
    form.output(html`Symptômes importants (p. ex. idéation suicidaire, rituels obsessionnels sévères, vols répétés dans les grands magasins) ou altération importante dans le fonctionnement social, professionnel ou scolaire (p. ex. absence d'amis, incapacité à garder un emploi).`)
})

form.section("De 31 à 40", () => {
    form.output(html`Le comportement est notablement influencé par des idées délirantes ou des hallucinations ou trouble grave de la communication ou de jugement (par ex. parfois incohérent, actes grossièrement inadaptés, préoccupation suicidaire) ou incapable de fonctionner dans presque tous les domaines (par ex. reste au lit toute la journée, absence de travail, de foyer ou d'amis).`)
})

form.section("De 21 à 30", () => {
    form.output(html`Si des symptômes sont présents, ils sont transitoires et il s'agit de réactions prévisibles à des facteurs de stress (p. ex. des difficultés de concentration après une dispute familiale) ; pas plus qu'une altération légère du fonctionnement social, professionnel ou scolaire (p. ex. retard temporaire du travail scolaire).`)
})

form.section("De 11 à 20", () => {
    form.output(html`Existence d'un certain danger d'auto ou d'hétéro-agression (p. ex. tentative de suicide sans attente précise de la mort, violence fréquente, excitation maniaque) ou incapacité temporaire à maintenir une hygiène corporelle minimum (p. ex. se barbouille d'excréments) ou altération massive de la communication (p. ex. incohérence indiscutable ou mutisme).`)
})

form.section("De 1 à 10", () => {
    form.output(html`Danger persistant d’auto ou d'hétéro-agression grave (p. ex. accès répétés de violence) Ou incapacité durable à maintenir une hygiène corporelle minimum ou geste suicidaire avec attente précise de la mort.`)
})

form.section("0", () => {
    form.output(html`Information inadéquate.`)
})

form.slider("score", "Entrez votre score ici :", {min: 0, max: 100, wide: true});

data.makeFormFooter(page)
