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
        <p>Considérez le <b>dernier mois écoulé</b> pour répondre aux questions de cette partie.
    `

    form.part(() => {
        form.binary("c1", "Au cours du mois écoulé, avez-vous pensé qu’il vaudrait mieux que vous soyez mort(e), ou souhaité être mort(e) ?")
    })

    form.part(() => {
        form.binary("c2", "Au cours du mois écoulé, avez-vous vous faire du mal ?")
    })

    form.part(() => {
        form.binary("c3", "Au cours du mois écoulé, avez-vous pensé à vous faire du mal ?")
    })

    form.part(() => {
        form.binary("c4", "Au cours du mois écoulé, avez-vous réfléchi à la façon dont vous pourriez vous suicider ?")
    })

    form.part(() => {
        form.binary("c5", "Au cours du mois écoulé, avez-vous fait une tentative de suicide ?")
    })

    form.intro = html`
        <p>Répondez à cette dernière question en considérant votre <b>vie entière</b>.
    `

    form.part(() => {
        form.binary("c6", "Avez-vous déjà fait une tentative de suicide au cours de votre vie entière ?")
    })
}

export default build
