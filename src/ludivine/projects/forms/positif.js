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
        let domaines = [
            [1, "Personnel", "évènements liés à votre vie personnelle"],
            [2, "Travail", "évènements liés à votre travail"],
            [3, "Éducation", "évènements liés au plan éducatif"],
            [4, "Famille", "évènements en lien avec votre famille"],
            [5, "Vie amoureuse", "évènements survenus dans votre vie amoureuse"],
            [6, "Autres relations sociales", "évènements liés à vos autres relations sociales"],
            [7, "En lien avec un animal de compagnie", "évènements en lien avec votre animal de compagnie"],
            [8, "Loisirs", "évènements liés à vos loisirs"],
            [9, "Argent / Finances", "évènements en lien avec l'argent et les finances"],
            [10, "Santé", "évènements en lien avec votre santé"],
            [11, "Logement / Habitat", "évènements relatifs à votre logement / habitat"],
            [99, "Autre", "autres évènements survenus"]
        ]

        form.binary("q1", html`<b>Depuis l’évènement qui vous a amené ici</b>, avez-vous vécu des évènements particulièrement bons pour vous ? Quelque chose qui vous a fait du bien ?`)

        // Domaines
        {
            let props = domaines.map(domaine => [domaine[0], domaine[1]])

            form.multiCheck("q2", "Si oui, dans quel(s) domaine(s) de votre vie ces événements se sont-ils produits ?", props, {
                disabled: values.q1 != 1,
                help: "Plusieurs réponses possibles"
            })
        }

        if (values.q2?.includes?.(99))
            form.text("?q2_prec", "Précisez si vous le souhaitez :", { help: "Non obligatoire" })

        // Effet sur la vie
        {
            let q2 = values.q2 ?? []
            let props = domaines.filter(domaine => q2.includes(domaine[0])).map(domaine => [domaine[0], domaine[2]])

            for (let prop of props) {
                let key = "?q3_" + prop[0]
                let label = `À quel point avez-vous ressenti les ${prop[1]} comme positifs ?`

                form.slider(key, label, {
                    min: 0, max: 10,
                    prefix: "Assez positif", suffix: "Intensément positif",
                    help: "Placez le curseur sur la barre avec la souris ou votre doigt (non obligatoire)"
                })
            }
        }
    })
}

export default build
