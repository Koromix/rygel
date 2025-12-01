// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import { html } from 'vendor/lit-html/lit-html.bundle.js'

function build(form, meta, since = 'le recueil précédent') {
    let values = form.values

    form.intro = html`
        <p>Nous allons maintenant vous poser des questions sur les <b>autres évènements que vous avez pu vivre</b> depuis ${since}, qu’ils soient positifs ou négatifs, et qui font actuellement partie de votre réalité.
        <p>Parmi les évènements de vie stressants indiqués ci-dessous, indiquez ceux qui vous sont arrivés <b>depuis ${since}</b>.
    `

    form.part(() => {
        let types = [
            [1, "Divorce / séparation"],
            [2, "Conflits familiaux"],
            [3, "Conflits au travail"],
            [4, "Conflits avec le voisinage"],
            [5, "Maladie d’un être cher"],
            [6, "Décès d’un être cher"],
            [7, "Adaptation à la retraite"],
            [8, "Chômage"],
            [9, "Trop / pas assez de travail"],
            [10, "Pression pour respecter des délais / pression du temps"],
            [11, "Emménager dans un nouveau logement"],
            [12, "Problèmes financiers"],
            [13, "Propre maladie grave"],
            [14, "Accident grave"],
            [15, "Agression"],
            [16, "Fin d’une activité de loisir importante"],
            [17, "Confinement suite à une épidémie"],
            [99, "Tout autre évènement de vie stressant"]
        ]

        form.multiCheck("evts", "Indiquez le ou les évènements qui s’appliquent à vous :", types)

        if (values.evts?.includes?.(99))
            form.text("?evts_prec", "Précisez si vous le souhaitez :", { help: "Non obligatoire" })

        let choices = types.filter(type => values.evts?.includes?.(type[0]))

        if (choices.length)
            form.multiCheck("main", "Indiquez ci-dessous quel(s) évènement(s) a(ont) été les plus éprouvants :", choices)
    })

    form.intro = html`
        <p>Merci d'avoir identifié ces évènements. Ils peuvent avoir de nombreuses conséquences sur votre bien-être et comportement. Nous vous demanderons de préciser <b>à quelle fréquence les différentes affirmations s’appliquent</b> à vous (de « Jamais » à « Souvent »).
    `

    form.part(() => {
        q(1, "Depuis les évènements stressants identifiés, je me sens déprimé·e et triste :")
        q(2, "Je pense à ces évènements stressants de manière répétée :")
    })

    form.part(() => {
        q(3, "J’essaie d’éviter de parler des évènements stressants identifiés chaque fois que cela est possible :")
        q(4, "Je pense beaucoup à ces évènements stressants et cela est un poids pour moi :")
    })

    form.part(() => {
        q(5, "Je fais rarement les activités qui me plaisaient avant :")
        q(6, "Si je pense aux évènements stressants identifiés, je ressens un réel état d’anxiété :")
    })

    form.part(() => {
        q(7, "J’évite certaines choses qui peuvent me rappeler les évènements stressants identifiés :")
        q(8, "Je suis nerveux.se et agité·e depuis ces évènements stressants :")
    })

    form.part(() => {
        q(9, "Depuis les évènements stressants identifiés, je me mets en colère bien plus rapidement qu'auparavant, même pour de petites choses :")
        q(10, "Depuis ces évènements stressants, je trouve difficile de me concentrer sur certaines choses :")
    })

    form.part(() => {
        q(11, "J’essaie de sortir ces évènements stressants de ma mémoire :")
        q(12, "J’ai remarqué que je deviens plus irritable à cause de ces évènements stressants :")
    })

    form.part(() => {
        q(13, "J’ai constamment des souvenirs de ces évènements stressants et je ne peux rien faire pour les arrêter :")
        q(14, "J’essaie de supprimer mes émotions car elles sont un poids pour moi :")
    })

    form.part(() => {
        q(15, "Mes pensées tournent souvent autour de tout ce qui est relié aux évènements stressants identifiés :")
        q(16, "Depuis ces évènements stressants, j’ai peur de faire certaines choses ou de me retrouver dans certaines situations :")
    })

    form.part(() => {
        q(17, "Depuis ces évènements stressants, je n’aime pas aller au travail ou faire les tâches quotidiennes nécessaires :")
        q(18, "Je me sens découragé·e depuis ces évènements stressants et j’ai peu d’espoir dans l’avenir :")
    })

    form.part(() => {
        q(19, "Depuis ces évènements stressants identifiés, je ne peux plus dormir correctement :")
        q(20, "Dans l’ensemble, ces évènements stressants causent une détérioration importante de ma vie sociale et professionnelle, de mon temps de loisirs, et des autres domaines importants de fonctionnement :")
    })

    function q(idx, label) {
        let intf = form.enumButtons("q" + idx, label, [
            [1, "Jamais"],
            [2, "Rarement"],
            [3, "Parfois"],
            [4, "Souvent"]
        ])

        if (intf.value >= 2) {
            form.enumButtons("f" + idx, "Depuis combien de temps avez-vous eu ce ressenti ?", [
                [1, "< 1 mois"],
                [2, "1 à 6 mois"],
                [3, "6 mois à 2 ans"],
            ], { help: "Cela peut ne pas être très simple à indiquer, mais essayez de donner une estimation approximative de la durée de votre ressenti" })
        }
    }
}

export default build
