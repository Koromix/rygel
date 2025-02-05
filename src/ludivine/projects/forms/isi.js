// Copyright (C) 2024  Niels Martignène <niels.martignene@protonmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

import { html } from '../../../../vendor/lit-html/lit-html.bundle.js';

let intro = html`
    <p>Ce questionnaire est destiné à <b>évaluer votre sommeil</b> selon deux axes : la qualité de votre sommeil et de vos nuits d'un part, et la satisfaction liée à votre sommeil d'autre part.
    <p>Ces questions portent sur votre <u>sommeil actuel c'est  dire au cours des derniers mois</u> et non avant.
`;

function run(form, values) {
    form.block(() => {
        form.output(html`
            <p>Veuillez estimer la <u>sévérité actuelle (derniers mois)</u> de vos difficultés de sommeil :
        `)

        q("diff1", "Difficultés à s'endormir :")
        q("diff2", "Difficultés à rester endormi(e) :")
        q("diff3", "Problèmes de réveil trop tôt le matin :")
    })

    form.block(() => {
        form.output(html`
            <p>Veuillez estimer la <u>satisfaction actuelle (derniers mois)</u> que vous retirez de votre sommeil :
        `)

        q("satisf1", html`Jusqu'à quel point êtes-vous <u>SATISFAIT(E)</u> de votre sommeil actuel ?`)
        q("satisf2", html`Jusqu'à quel point considérez-vous que vos difficultés de sommeil <u>PERTURBENT</u> votre fonctionnement quotidien ?`, {
            help: "Par exemple fatigue, concentration, mémoire, humeur ?"
        })
        q("satisf3", html`À quel point considérez-vous que vos difficultés de sommeil sont <u>APPARENTES</u> pour les autres en termes de détérioration de la qualité de votre vie ?`)
        q("satisf4", html`Jusqu’à quel point êtes-vous <u>INQUIET(ÈTE)</u>/préoccupé(e) à propos de vos difficultés de sommeil ?`)
    })

    function q(key, title) {
        form.enum("*" + key, title, [
            [0, "Aucune"],
            [1, "Légère"],
            [2, "Moyenne"],
            [3, "Très"],
            [4, "Extrêmement"]
        ])
    }
}

export {
    intro,
    run
}
