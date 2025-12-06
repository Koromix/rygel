// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import { html } from 'vendor/lit-html/lit-html.bundle.js'
import { PERSON_KINDS } from '../../client/network/constants.js'

function build(form) {
    form.intro = html`
        <p>Lorsque nous ressentons du stress, nous essayons d'y <b>faire face par diverses actions et pensées</b>. Les exemples suivants décrivent des situations de gestion du stress.
        <p>Veuillez indiquer comment ces situations peuvent s’appliquer à vous en choisissant l'une des options suivantes pour chaque situation : « S’applique tout à fait », « S’applique », « S’applique un peu », « Ne s’applique pas ».
    `

    form.section(() => {
        q(1, "Lorsqu'une situation stressante ne s'améliore pas, j'essaie de trouver d'autres moyens pour y faire face :")
        q(2, "Je n'utilise que certains moyens pour faire face au stress :")
        q(3, "Lorsque je suis stressé(e), j'utilise plusieurs moyens pour faire face à la situation et l'améliorer :")
    })

    form.section(() => {
        q(4, "Lorsque je n'ai pas réussi à faire face à une situation stressante, j'utilise d'autres moyens pour y faire face :")
        q(5, "Si une situation stressante ne s'améliore pas, j'utilise d'autres moyens pour y faire face :")
        q(6, "Je suis conscient(e) de la réussite ou de l'échec de mes tentatives de faire face au stress :")
    })

    form.section(() => {
        q(7, "Je ne me rends pas compte lorsque je ne parviens pas à faire face au stress :")
        q(8, "Si je sens que je n'ai pas réussi à faire face au stress, je change ma façon de gérer le stress :")
        q(9, "Après avoir fait face au stress, je réfléchis à l'efficacité ou inefficacité des moyens que j'ai utilisé pour le faire :")
    })

    form.section(() => {
        q(10, "Si je n'ai pas réussi à faire face au stress, je pense à d'autres moyens de le faire :")
    })

    function q(idx, label) {
        form.enumButtons("q" + idx, label, [
            [1, "Ne s'applique pas"],
            [2, "S'applique un peu"],
            [3, "S'applique"],
            [4, "S'applique tout à fait"]
        ])
    }
}

export default build
