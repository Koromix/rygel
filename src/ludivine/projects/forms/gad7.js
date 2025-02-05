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
    <p>Ce questionnaire est destiné à repérer l'existence de <b>problèmes révélateur d'un trouble anxieux</b> et d'en mesurer la sévérité.
    <p>Les questions portent sur les problèmes ressentis <u>au cours des deux dernières semaines</u>.
`;

function run(form, values) {
    form.block(() => {
        form.output(html`<p>Au cours des <u>deux dernières semaines</u>, à quelle fréquence avez-vous été dérangé(e) par les problèmes suivants ?`)
        q(1, "Un sentiment de nervosité, d’anxiété ou de tension ?")
        q(2, "Une incapacité à arrêter de s’inquiéter ou à contrôler ses inquiétudes ?")
    })

    form.block(() => {
        form.output(html`<p>Au cours des <u>deux dernières semaines</u>, à quelle fréquence avez-vous été dérangé(e) par les problèmes suivants ?`)
        q(3, "Une inquiétude excessive à propos de différentes choses ?")
        q(4, "Des difficultés à se détendre ?")
    })

    form.block(() => {
        form.output(html`<p>Au cours des <u>deux dernières semaines</u>, à quelle fréquence avez-vous été dérangé(e) par les problèmes suivants ?`)
        q(5, "Une agitation telle qu’il est difficile à tenir en place ?")
        q(6, "Une tendance à être facilement contrarié(e) ou irritable ?")
    })

    form.block(() => {
        form.output(html`<p>Au cours des <u>deux dernières semaines</u>, à quelle fréquence avez-vous été dérangé(e) par les problèmes suivants ?`)
        q(7, "Un sentiment de peur comme si quelque chose de terrible risquait de se produire ?")
    })

    function q(idx, label) {
        form.enum("*q" + idx, label, [
            [0, "Jamais"],
            [1, "Plusieurs jours"],
            [2, "Plus de la moitié du temps"],
            [3, "Presque tous les jours"]
        ])
    }
}

export {
    intro,
    run
}
