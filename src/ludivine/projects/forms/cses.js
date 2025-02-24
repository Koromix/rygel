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
import { PERSON_KINDS } from '../../client/network/constants.js'

let intro = html`
    <p>Les personnes ayant vécu le genre d'événement comme celui qui vous a amené ici <b>différent considérablement dans leur façon d'en parler</b> aux autres. Nous aimerions savoir ce que vous avez divulgué à la <b>personne dont vous êtes le ou la plus proche</b> et à quelle fréquence (y compris si vous ne lui avez rien dit).
`

function run(form, values) {
    form.part(() => {
        q(1, "Je lui ai parlé de mon expérience")
        q(2, "Il y a des aspects de mon expérience que je lui ai volontairement cachés")
    })

    form.part(() => {
        q(3, "Il y a des aspects de mon expérience que je ne lui dirai pas")
        q(4, "J'ai l'intention de lui cacher tout ou partie de mon expérience")
    })

    form.part(() => {
        q(5, "Je lui ai parlé des images, des sons et/ou des odeurs liés à mon expérience")
        q(6, "Je lui ai parlé des détails graphiques de mon expérience")
    })

    form.part(() => {
        q(7, "Je lui ai parlé de mes pensées et de mes sentiments à propos de mon expérience")
        q(8, "Je lui ai parlé des effets de mon expérience sur ma façon de penser et de me sentir")
    })

    form.part(() => {
        let types = Object.keys(PERSON_KINDS).map(kind => [kind, PERSON_KINDS[kind].text])

        form.enumRadio("sphere", "Dans quelle sphère de votre vie se situe la personne à laquelle vous pensez ?", types)

        if (values.pshere == "other")
            form.text("?sphere_pec", "Précisez (non obligatoire) :")
    })

    function q(idx, label) {
        form.enumButtons("q" + idx, label, [
            [0, "Pas du tout"],
            [1, "Rarement"],
            [2, "Un petit peu"],
            [3, "Modérément"],
            [4, "Assez souvent"],
            [5, "Souvent"],
            [6, "Beaucoup"]
        ])
    }
}

export {
    intro,
    run
}
