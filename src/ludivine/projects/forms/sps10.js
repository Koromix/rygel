// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import { html } from 'vendor/lit-html/lit-html.bundle.js'

function build(form, values) {
    form.values = values

    form.intro = html`
        <p>Nous allons maintenant évaluer la <b>disponibilité de votre entourage</b>. Il n’y a pas de bonnes ou de mauvaises réponses.
        <p>Lorsque vous y répondrez, essayez de penser aux personnes qui vous entourent.
    `

    form.part(() => {
        q(1, "Il y a des personnes sur qui je peux compter pour m’aider en cas de réel besoin matériel :")
        q(5, "Il y a des personnes qui prennent plaisir aux mêmes activités sociales que moi :")
        q(8, "J’ai l’impression de faire partie d’un groupe de personnes qui partagent mes attitudes et mes croyances :")
    })

    form.part(() => {
        q(11, "J’ai des personnes proches de moi qui me procurent un sentiment de sécurité affective et de bien-être :")
        q(12, "Il y a quelqu’un avec qui je pourrais discuter de décisions importantes qui concernent ma vie :")
        q(13, "J’ai des relations où sont reconnus ma compétence et mon savoir-faire (confirmation de sa valeur) :")
    })

    form.part(() => {
        q(16, "Il y a une personne fiable à qui je pourrais faire appel pour me conseiller si j’avais des problèmes :")
        q(17, "Je ressens un lien affectif fort avec au moins une autre personne :")
        q(20, "Il y a des gens qui admirent mes talents et habiletés :")
    })

    form.part(() => {
        q(23, "Il y a des gens sur qui je peux compter en cas d’urgence matérielle :")
    })

    function q(idx, label) {
        form.enumButtons("q" + idx, label, [
            [1, "Fortement en désaccord"],
            [2, "En désaccord"],
            [3, "D’accord"],
            [4, "Fortement en accord"]
        ])
    }
}

export default build
