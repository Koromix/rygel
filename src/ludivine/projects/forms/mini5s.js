// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import { html } from '../../../../vendor/lit-html/lit-html.bundle.js'

function build(form, values) {
    form.values = values

    form.intro = html`
        <p>Considérez le <b>dernier mois écoulé</b> pour répondre aux questions de cette partie.
    `

    form.part(() => {
        form.binary("c1", "Au cours du mois écoulé, avez-vous pensé qu’il vaudrait mieux que vous soyez mort(e), ou souhaité être mort(e) ?")
        form.binary("c2", "Au cours du mois écoulé, avez-vous vous faire du mal ?")
    })

    form.part(() => {
        form.binary("c3", "Au cours du mois écoulé, avez-vous pensé à vous faire du mal ?")
        form.binary("c4", "Au cours du mois écoulé, avez-vous réfléchi à la façon dont vous pourriez vous suicider ?")
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
