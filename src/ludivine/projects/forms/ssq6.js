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

import { html } from '../../../../vendor/lit-html/lit-html.bundle.js'

let intro = html`
    <p>Les questions suivantes concernent les <b>personnes de votre environnement qui vous procurent une aide ou un soutien</b>.
    <p>Chaque question est en deux parties :
    <ol>
        <li>Dans un premier temps, indiquez le <u>nombre de personnes</u> de 1 à 9 sur qui vous pouvez compter pour une aide ou un soutien.
        <li>Dans un second temps, indiquez quel est votre <u>degré de satisfaction</u> par rapport au soutien obtenu.
    </ol>
    <p>Si pour une question <u>vous n’avez personne, indiquez « 0 »</u>, mais évaluez quand même votre degré de satisfaction.
`

function run(form, values) {
    form.part(() => {
        q("q1", "Combien de personnes de votre entourage sont réellement disponibles quand vous avez besoin d’aide ?")
    })

    form.part(() => {
        q("q2", "Sur combien de personnes pouvez-vous réellement compter pour vous aider à vous sentir plus détendu(e) lorsque vous êtes sous pression ou crispé(e) ?")
    })

    form.part(() => {
        q("q3", "Combien de personnes vous acceptent tel(le) que vous êtes, c’est-à-dire avec vos bons et vos mauvais côtés ?")
    })

    form.part(() => {
        q("q4", "Sur combien de personnes pouvez-vous réellement compter pour s’occuper de vous quoi qu’il arrive ?")
    })

    form.part(() => {
        q("q5", "Sur combien de personnes pouvez-vous réellement compter pour vous aider à vous sentir mieux quand il vous arrive de broyer du noir ?")
    })

    form.part(() => {
        q("q6", "Sur combien de personnes pouvez-vous réellement compter pour vous consoler quand vous êtes bouleversé(e) ?")
    })

    function q(key, label) {
        let choices = [
            [1, "Très insatisfait"],
            [2, "Insatisfait"],
            [3, "Plutôt insatisfait"],
            [4, "Plutôt satisfait"],
            [5, "Satisfait"],
            [6, "Très satisfait"]
        ]

        form.enumButtons("*" + key + "a", label, [0, 1, 2, 3, 4, 5, 6, 7, 8, 9])
        form.enumButtons(key + "b", "Quel est votre degré de satisfaction par rapport au soutien obtenu ?", choices)
    }
}

export {
    intro,
    run
}
