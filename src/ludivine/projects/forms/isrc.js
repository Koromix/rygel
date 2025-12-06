// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import { html } from 'vendor/lit-html/lit-html.bundle.js'

function build(form) {
    form.intro = html`
        <p>Lorsque quelque chose de grave ou d'effrayant se produit, les gens peuvent avoir des <b>pensées ou des sentiments différents</b>.
        <p>À nouveau, pour les questions suivantes, nous aimerions que vous gardiez en tête l’évènement (ou la situation traumatique) décrit <b>dans la partie « L’évènement qui vous a amené ici »</b>.
        <p>Les éléments suivants concernent ce que vous pensiez et ressentiez <b>AU MOMENT DE CE QUI VOUS EST ARRIVÉ</b>. Dites-nous dans quelle mesure chacun d'entre eux est vrai pour vous.
    `

    form.section(() => {
        q(2, "Mon cerveau s'est vidé :")
        q(3, "J'ai fait des choses dont je n'avais pas conscience :")
        q(4, "Les choses semblaient se dérouler très lentement :")
    })

    form.section(() => {
        q(5, "Les choses semblaient se dérouler très rapidement :")
        q(6, "Ce qui se passait me semblait irréel - comme si j'étais dans un rêve ou que je regardais un film :")
        q(7, "J'avais l'impression de ne pas être là, que je ne faisais pas partie de ce qui se passait :")
    })


    form.section(() => {
        q(8, "Je me suis senti(e) confus(e) :")
        q(9, "Je me suis senti(e) engourdi(e) - comme si je n'avais plus aucun sentiment :")
        q(10, "Des personnes comme ma famille ou mes amis semblaient être des étrangers :")
    })

    form.section(() => {
        q(11, "Tout semblait bizarre, pas normal :")
        q(12, "Par moments, je n'étais pas sûr(e) de l'endroit où je me trouvais ni de l'heure qu'il était :")
        q(13, "Il y a eu des moments où je n'ai pas ressenti de douleur, même là où j'étais blessé(e) :")
    })

    form.section(() => {
        q(14, "J’avais très peur :")
        // q(15, "Je voulais que ça s'arrête mais je n'y arrivais pas")
        q(16, "Je me sentais mal parce que ce qui se passait me semblait si horrible :")
    })

    form.intro = html`
        <p>Lorsque quelque chose de grave ou d'effrayant se produit, les gens peuvent avoir des <b>pensées ou des sentiments différents</b>.
        <p>Les éléments suivants concernent comment vous vous sentez <b>MAINTENANT</b>. Dites-nous dans quelle mesure chacun d'entre eux est vrai pour vous.
    `

    form.section(() => {
        form.output(html`<b>Changement de consigne</b> ! Nous vous demandons maintenant de répondre aux énoncés ci-dessous par rapport à comment vous vous sentez <b>MAINTENANT</b>. N’hésitez pas à regarder la consigne détaillée ci-dessus !`)

        q(17, "Je ne me souviens pas de certaines parties de ce qui s'est passé :")
        q(18, "Je n'arrête pas de penser à ce qui s'est passé :")
        q(19, "Je ne veux pas penser à ce qui s'est passé :")
    })

    form.section(() => {
        q(20, "Je me sens nerveux(se) :")
        q(21, "Mes sentiments sont engourdis - je me sens 'coupé(e)' de mes émotions :")
        q(22, "Quand je pense à ce qui s'est passé, je me sens vraiment boulversé(e) :")
    })

    form.section(() => {
        q(23, "J'essaie de ne pas me souvenir ou de ne pas penser à ce qui m'est arrivé :")
        q(24, "J'ai du mal à me concentrer ou à prêter attention :")
        q(25, "Je me sens dans l'espace ou déconnecté(e) du monde qui m'entoure :")
    })

    form.section(() => {
        q(26, "Des images ou des sons de ce qui s'est passé me reviennent sans cesse à l'esprit :")
        q(27, "Je suis perturbé(e) lorsque quelque chose me rappelle ce qui s'est passé :")
        q(28, "Je me sens 'hyper' ou comme si je ne peux pas rester immobile :")
    })

    function q(idx, label) {
        form.enumButtons("q" + idx, label, [
            [0, "Pas vrai"],
            [1, "Un peu ou parfois vrai"],
            [2, "Souvent ou très souvent vrai"]
        ])
    }
}

export default build
