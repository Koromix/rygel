// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import { html } from 'vendor/lit-html/lit-html.bundle.js'
import { PERSON_KINDS } from '../../client/network/constants.js'

function build(form, values) {
    form.values = values

    form.intro = html`
        <p>Les personnes ayant vécu le genre d'événement comme celui qui vous a amené ici <b>différent considérablement dans leur façon d'en parler</b> aux autres.
        <p>Nous aimerions savoir ce que vous avez divulgué à la <b>personne dont vous êtes le ou la plus proche</b> (y compris si vous ne lui avez rien dit).
    `

    form.part(() => {
        q(1, "Je lui ai parlé de mon expérience :")
        q(2, "Il y a des aspects de mon expérience que je lui ai volontairement cachés :")
        q(3, "Il y a des aspects de mon expérience que je ne lui dirai pas :")
    })

    form.part(() => {
        q(4, "J'ai l'intention de lui cacher tout ou partie de mon expérience :")
        q(5, "Je lui ai parlé des images, des sons et/ou des odeurs liés à mon expérience :")
        q(6, "Je lui ai parlé des détails visuels de mon expérience :")
    })

    form.part(() => {
        q(7, "Je lui ai parlé de mes pensées et de mes sentiments à propos de mon expérience :")
        q(8, "Je lui ai parlé des effets de mon expérience sur ma façon de penser et de me sentir :")
    })

    form.part(() => {
        let types = Object.keys(PERSON_KINDS).map(kind => [kind, PERSON_KINDS[kind].text])

        form.enumRadio("sphere", "Dans quelle sphère de votre vie se situe la personne à laquelle vous pensez ?", types)

        if (values.sphere == "other")
            form.text("?sphere_prec", "Précisez si vous le souhaitez :", { help: "Non obligatoire" })
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

export default build
