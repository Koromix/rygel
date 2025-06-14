// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
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

import { html } from '../../../../vendor/lit-html/lit-html.bundle.js'

function build(form, values) {
    form.values = values

    form.intro = html`
        <p>Les questions ci-dessous reflètent des difficultés que vous pouvez rencontrer <b>au cours de votre vie</b>.
        <p>Au cours des <b>14 derniers jours</b>, à quelle fréquence avez-vous été dérangé(e) par les problèmes suivants ?
    `

    form.part(() => {
        q(1, "Un sentiment de nervosité, d’anxiété ou de tension :")
        q(2, "Une incapacité à arrêter de vous inquiéter ou à contrôler vos inquiétudes :")
        q(3, "Une inquiétude excessive à propos de différentes choses :")
    })

    form.part(() => {
        q(4, "Des difficultés à vous détendre :")
        q(5, "Une agitation telle qu’il est difficile à tenir en place :")
        q(6, "Une tendance à être facilement contrarié(e) ou irritable :")
    })

    form.part(() => {
        q(7, "Un sentiment de peur comme si quelque chose de terrible risquait de se produire :")
    })

    function q(idx, label) {
        form.enumButtons("q" + idx, label, [
            [0, "Jamais"],
            [1, "Plusieurs jours"],
            [2, "Plus de la moitié du temps"],
            [3, "Presque tous les jours"]
        ])
    }
}

export default build
