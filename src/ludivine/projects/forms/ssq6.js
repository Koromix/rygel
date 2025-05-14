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
import { PERSON_KINDS } from '../../client/network/constants.js'

function build(form, values, mask) {
    form.values = values

    form.intro = html`
        <p>Nous cherchons dans un premier temps à avoir une représentation des <b>personnes de votre environnement</b> qui vous procurent de l’aide ou du soutien.
        <p>Chaque question comporte 4 étapes :
        <ol>
            <li>Indiquez le nombre de personnes (de 0 à 9) sur qui vous pouvez compter
            <li>Inscrivez le prénom ou le surnom de chaque personne
            <li>Décrivez le type de relation que vous avez avec chaque personne
            <li>Indiquez quel est votre degré de satisfaction par rapport au soutien obtenu
        </ol>
        <p>Si vous ne pensez à personne en particulier pour une question, indiquez « 0 », mais remplissez tout de même l’évaluation de satisfaction.
    `

    let anonymes = new Map;

    form.part(() => {
        q("q1", "Combien de personnes de votre entourage sont réellement disponibles quand vous avez besoin d’aide ?")
    })

    form.part(() => {
        q("q2", "Sur combien de personnes pouvez-vous réellement compter pour vous aider à vous sentir plus détendu(e) lorsque vous êtes sous pression ou crispé(e) ?")
    })

    form.part(() => {
        q("q3", "Combien de personnes vous acceptent tel(le) que vous êtes, c’est-à-dire avec vos bons et vos mauvais côtés ?")
    })

    form.part(() => {
        q("q4", "Sur combien de personnes pouvez-vous réellement compter pour s’occuper de vous quoi qu’il arrive ?")
    })

    form.part(() => {
        q("q5", "Sur combien de personnes pouvez-vous réellement compter pour vous aider à vous sentir mieux quand il vous arrive de broyer du noir ?")
    })

    form.part(() => {
        q("q6", "Sur combien de personnes pouvez-vous réellement compter pour vous consoler quand vous êtes bouleversé(e) ?")
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

        let types = Object.keys(PERSON_KINDS).map(kind => [kind, PERSON_KINDS[kind].text])

        form.enumButtons(key + "a", label, [0, 1, 2, 3, 4, 5, 6, 7, 8, 9])

        let count = values[key + "a"]

        for (let i = 0; i < count; i++) {
            let initiales = values[key + "b" + i]

            if (typeof initiales == 'string') {
                initiales = initiales.toUpperCase().replace(/[^A-Z]/g, '')
                values[key + "b" + i] = initiales

                let anon = anonymes.get(initiales)
                if (anon == null) {
                    anon = anonymes.size + 1
                    anonymes.set(initiales, anon)
                }
                mask(key + "b" + i, anon)
            }

            form.text(key + "b" + i, `Quelles sont les initiales de la personne n°${i + 1} ?`, {
                help: "Lettres majuscules uniquement, sans espaces ou autres caractères"
            })

            let name = values[key + "b" + i] || 'cette personne'
            let label = `Quelle est votre relation avec ${name} ?`

            form.enumDrop(key + "c" + i, label, types)
        }

        form.enumButtons(key + "d", "Quel est votre degré de satisfaction par rapport au soutien obtenu ?", choices)
    }
}

export default build
