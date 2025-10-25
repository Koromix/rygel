// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import { html } from '../../../../vendor/lit-html/lit-html.bundle.js'

function build(form, values) {
    form.values = values

    form.intro = html`
        <p>Ce questionnaire est destiné à rechercher les <b>expériences de dissociation</b> que vous auriez pu ressentir pendant l'événement traumatique, et au cours des quelques heures suivantes.
        <p>Veuillez répondre aux énoncés suivants en cochant le choix de réponse qui décrit le mieux vos expériences et réactions durant l’événement et immédiatement après. Si une question ne s’applique pas à votre expérience, <b>cochez « Pas du tout vrai »</b>.
        <p>Considérez uniquement les expériences <b>au cours du dernier mois</b> !
    `

    let choices = [
        [0, "Pas du tout"],
        [1, "Un peu"],
        [2, "Modérément"],
        [3, "Beaucoup"],
        [4, "Extrêmement"]
    ]

    form.part(() => {
        form.enumButtons("pcl1", "Des souvenirs répétés, pénibles et involontaires de l’expérience stressante ?", choices)
        form.enumButtons("pcl2", "Des rêves répétés et pénibles de l’expérience stressante ?", choices)
        form.enumButtons("pcl3", "Se sentir ou agir soudainement comme si vous viviez à nouveau l’expérience stressante ?", choices)
    })

    form.part(() => {
        form.enumButtons("pcl4", "Se sentir mal quand quelque chose vous rappelle l’événement ?", choices)
        form.enumButtons("pcl5", "Avoir de fortes réactions physiques lorsque quelque chose vous rappelle l’événement (accélération cardiaque, difficulté respiratoire, sudation) ?", choices)
        form.enumButtons("pcl6", "Essayer d’éviter les souvenirs, pensées, et sentiments liés à l’événement ?", choices)
    })

    form.part(() => {
        form.enumButtons("pcl7", "Essayer d’éviter les personnes et les choses qui vous rappellent l’expérience stressante (lieux, personnes, activités, objets) ? ", choices)
        form.enumButtons("pcl8", "Des difficultés à vous rappeler des parties importantes de l’événement ?", choices)
        form.enumButtons("pcl9", "Des croyances négatives sur vous-même, les autres, le monde (des croyances comme : je suis mauvais, j’ai quelque chose qui cloche, je ne peux avoir confiance en personne, le monde est dangereux) ?", choices)
    })

    form.part(() => {
        form.enumButtons("pcl10", "Vous blâmer ou blâmer quelqu’un d’autre pour l’événement ou ce qui s’est produit ensuite ?", choices)
        form.enumButtons("pcl11", "Avoir des sentiments négatifs intenses tels que peur, horreur, colère, culpabilité, ou honte ?", choices)
        form.enumButtons("pcl12", "Perdre de l’intérêt pour des activités que vous aimiez auparavant ?", choices)
    })

    form.part(() => {
        form.enumButtons("pcl13", "Vous sentir distant ou coupé des autres ?", choices)
        form.enumButtons("pcl14", "Avoir du mal à éprouver des sentiments positifs (par exemple être incapable de ressentir de la joie ou de l’amour envers vos proches) ?", choices)
        form.enumButtons("pcl15", "Comportement irritable, explosions de colère, ou agir agressivement ?", choices)
    })

    form.part(() => {
        form.enumButtons("pcl16", "Prendre des risques inconsidérés ou encore avoir des conduites qui pourraient vous mettre en danger ?", choices)
        form.enumButtons("pcl17", "Être en état de « super-alerte », hyper vigilant ou sur vos gardes ?", choices)
        form.enumButtons("pcl18", "Sursauter facilement ?", choices)
    })

    form.part(() => {
        form.enumButtons("pcl19", "Avoir du mal à vous concentrer ?", choices)
        form.enumButtons("pcl20", "Avoir du mal à trouver le sommeil ou à rester endormi ?", choices)
    })
}

export default build
