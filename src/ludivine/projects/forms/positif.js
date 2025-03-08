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

function build(form, values) {
    form.values = values

    form.intro = ''

    form.part(() => {
        form.binary("q1", html`<b>Depuis l’évènement qui vous a amené ici</b>, avez-vous vécu des évènements particulièrement bons pour vous ? Quelque chose qui vous a fait du bien ?`)

        form.multiCheck("q2", "Si oui, dans quel(s) domaine(s) de votre vie ces événements se sont-ils produits ?", [
            [1, "Personnel"],
            [2, "Travail"],
            [3, "Éducation"],
            [4, "Famille"],
            [5, "Vie amoureuse"],
            [6, "Autres relations sociales"],
            [7, "En lien avec un animal de compagnie"],
            [8, "Loisirs"],
            [9, "Argent / Finances"],
            [10, "Santé"],
            [11, "Logement / Habitat"],
            [12, "Autre (Précisez si vous le souhaitez)"]
        ], {
            disabled: values.q1 != 1,
            help: "Plusieurs réponses possibles"
        })

        if (values.q2 != null) {
            form.slider("?q3", "Quantifiez l'effet de ces domains sur votre vie :", {
                min: 0, max: 10,
                prefix: "Assez positif", suffix: "Intensément positif",
                help: "Non obligatoire"
            })
        }
    })
}

export default build
