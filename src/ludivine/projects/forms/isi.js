// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import { html } from 'vendor/lit-html/lit-html.bundle.js'

function build(form) {
    form.intro = html`
        <p>Ce questionnaire est destiné à <b>évaluer votre sommeil</b> selon deux axes : la qualité de votre sommeil et de vos nuits d'un part, et la satisfaction liée à votre sommeil d'autre part.
        <p>Ces questions portent sur votre <b>sommeil actuel c'est à dire au cours des derniers mois</b> et non avant.
    `

    form.section(() => {
        form.output(html`
            <p><i>Veuillez estimer la <b>sévérité actuelle (derniers mois)</b> de vos difficultés de sommeil.</i>
        `)

        q("diff1", "Difficultés à vous endormir :")
        q("diff2", "Difficultés à rester endormi(e) :")
        q("diff3", "Problèmes de réveil trop tôt le matin :")
    })

    form.section(() => {
        form.output(html`
            <p><i>Veuillez estimer la <b>satisfaction actuelle (derniers mois)</b> que vous retirez de votre sommeil</i>
        `)

        q("satisf1", html`Jusqu'à quel point êtes-vous <b>SATISFAIT(E)</b> de votre sommeil actuel ?`)
        q("satisf2", html`Jusqu'à quel point considérez-vous que vos difficultés de sommeil <b>PERTURBENT</b> votre fonctionnement quotidien ?`, {
            help: "Par exemple :fatigue, concentration, mémoire, humeur"
        })
        q("satisf3", html`À quel point considérez-vous que vos difficultés de sommeil sont <b>APPARENTES</b> pour les autres en termes de détérioration de la qualité de votre vie ?`)
        q("satisf4", html`Jusqu’à quel point êtes-vous <b>INQUIET(ÈTE)</b>/préoccupé(e) à propos de vos difficultés de sommeil ?`)
    })

    function q(key, title) {
        form.enumButtons(key, title, [
            [0, "Aucune"],
            [1, "Légère"],
            [2, "Moyenne"],
            [3, "Très"],
            [4, "Extrêmement"]
        ])
    }
}

export default build
