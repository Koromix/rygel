// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import { html } from '../../../../vendor/lit-html/lit-html.bundle.js'

function build(form, values, since) {
    form.values = values

    form.intro = ''

    form.part(() => {
        let domaines = [
            [1, "Personnel", html`évènements liés <b>à votre vie personnelle</b>`],
            [2, "Travail", html`évènements liés <b>à votre travail</b>`],
            [3, "Éducation", html`évènements liés <b>au plan éducatif</b>`],
            [4, "Famille", html`évènements en lien <b>avec votre famille</b>`],
            [5, "Vie amoureuse", html`évènements survenus <b>dans votre vie amoureuse</b>`],
            [6, "Autres relations sociales", html`évènements liés à <b>vos autres relations sociales</b>`],
            [7, "En lien avec un animal de compagnie", html`évènements en lien <b>avec votre animal de compagnie</b>`],
            [8, "Loisirs", html`évènements liés <b>à vos loisirs</b>`],
            [9, "Argent / Finances", html`évènements en lien <b>avec l'argent et les finances</b>`],
            [10, "Santé", html`évènements en lien <b>avec votre santé</b>`],
            [11, "Logement / Habitat", html`évènements relatifs <b>à votre logement / habitat</b>`],
            [99, "Autre", html`<b>autres évènements survenus</b>`]
        ]

        form.binary("q1", html`<b>Depuis ${since}</b>, avez-vous vécu des évènements particulièrement bons pour vous ? Quelque chose qui vous a fait du bien ?`)

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
                let label = html`À quel point avez-vous ressenti les ${prop[1]} comme positifs ?`

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
