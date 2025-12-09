// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import { html } from 'vendor/lit-html/lit-html.bundle.js'

function build(form, meta, since = 'le recueil précédent') {
    let values = form.values

    form.intro = html`
        <p>Nous allons vous poser une série de questions qui vont nous permettre de comprendre ce qui vous est arrivé. Nous avons conscience que cela peut être difficile, et nous vous sommes reconnaissants de cet effort.
    `

    form.section(() => {
        form.binary("nouveau", `Depuis que vous avez complété ${since}, avez-vous vécu de nouveaux évènements particulièrement difficiles ?`)
    })

    if (values.nouveau !== 0) {
        form.section(() => {
            form.enumRadio("q4", "Comment l’avez-vous vécu ?", [
                [1, "Cela m’est arrivé directement"],
                [2, "J’en ai été témoin"],
                [3, "J’ai appris qu’un membre de la famille ou un ami proche l’a vécu"],
                [4, "J’y ai été exposé à plusieurs reprises dans le cadre de mon travail (par exemple, ambulancier, police, militaire, ou autre premier répondant)"],
                [99, "Autre"]
            ])

            if (values.q4 == 99)
                form.text("?q4a", "Précisez si vous le souhaitez :", { help: "Non obligatoire" })
        })

        form.section(() => {
            form.enumButtons("q5", "La vie de quelqu’un était-elle en danger ?", [
                [1, "Oui, la mienne"],
                [2, "Oui, la vie de quelqu'un d'autre"],
                [0, "Non"]
            ])

            form.enumButtons("q6", "Quelqu’un a-t-il été gravement blessé ou tué ?", [
                [1, "Oui, j’ai été gravement blessé"],
                [2, "Oui, quelqu’un d’autre a été gravement blessé ou tué"],
                [0, "Non"]
            ])
        })

        form.section(() => {
            form.binary("q7", "L’évènement impliquait-il une agression sexuelle ?")
            form.binary("q8", "L’événement comportait-il une violence (physique ou psychologique) intentionnelle exercée par autrui à votre encontre ?")
        })

        form.section(() => {
            form.enumRadio("q9", "Si l’évènement a entrainé la mort d’un membre de la famille ou d’un ami proche, était-ce dû à un type d’accident ou de violence, ou était-ce dû à des causes naturelles ?", [
                [1, "Un accident ou de la violence"],
                [2, "Causes naturelles"],
                [0, "Ne s’applique pas (l’évènement n’a pas entrainé la mort d’un membre de la famille ou d’un ami proche)"]
            ])

            form.enumButtons("q10", "Combien de fois est-ce que cela s’est produit ?", [
                [1, "Seulement une fois"],
                [2, "Plus d’une fois"]
            ])
        })

        form.section(() => {
            form.enumRadio("q11", "Lors de cet événement, étiez-vous la seule victime ou d’autres personnes ont-elles également été affectées ?", [
                [1, "J’étais la seule victime"],
                [2, "D’autres personnes étaient également victimes"],
                [3, "Je ne sais pas"]
            ])

            if (values.q11 == 2) {
                form.binary("q12", "Connaissiez vous les autres victimes ?")

                if (values.q12 == 1) {
                    form.multiCheck("q12a", "Qui sont ces victimes ?", [
                        [1, "Cercle familial"],
                        [2, "Amis"],
                        [3, "Collègues"],
                        [99, "Autre"]
                    ], { help: "Plusieurs réponses possibles" })

                    if (values.q12a?.includes?.(99))
                        form.text("?q12b", "Précisez si vous le souhaitez :", { help: "Non obligatoire" })
                }
            }
        })

        form.section(() => {
            form.enumButtons("q13", "Cet évènement impliquait-il un ou plusieurs agresseur(s) ?", [
                [1, "Un agresseur"],
                [2, "Plusieurs agresseurs"],
                [0, "Non applicable"]
            ], {
                help: "Sélectionnez « Non applicable » si l’évènement n’impliquait pas d’agresseurs"
            })
        })

        if (values.q13 != 0) {
            form.section(() => {
                let singular = (values.q13 == 1)

                form.binary("q14", `Cet évènement impliquait-il des témoins qui ont délibérément laisser agir ${singular ? "l’agresseur" : "les agresseurs"} ?`)

                if (values.q14 == 1) {
                    form.binary("q15", "Connaissiez-vous ces témoins ?")

                    if (values.q15 == 1) {
                        form.multiCheck("q15a", "Qui sont ces témoins ?", [
                            [1, "Cercle familial"],
                            [2, "Amis"],
                            [3, "Collègues"],
                            [99, "Autre"]
                        ], { help: "Plusieurs réponses possibles" })

                        if (values.q15a?.includes?.(99))
                            form.text("?q15b", "Précisez si vous le souhaitez :", { help: "Non obligatoire" })
                    }
                }
            })

            form.section(() => {
                let singular = (values.q13 == 1)

                form.binary("q17", `Connaissiez-vous ${singular ? "l’agresseur" : "les agresseurs"} ?`)

                if (values.q17 == 1) {
                    form.multiCheck("q17a", `De quel cercle de connaissance ${singular ? "faisait-il" : "faisaient-ils"} partie ?`, [
                        [1, "Cercle familial"],
                        [2, "Amis"],
                        [3, "Collègues"],
                        [99, "Autre"]
                    ], { help: "Plusieurs réponses possibles" })

                    if (values.q17a?.includes?.(99))
                        form.text("?q17b", "Précisez si vous le souhaitez :", { help: "Non obligatoire" })
                }
            })
        }

        form.section(() => {
            form.binary("q16", "Cet évènement impliquait-il des témoins qui vous ont porté secours ?")
        })

        form.section(() => {
            form.binary("b1", "Avez-vous, au moment de l’évènement, ressenti un sentiment de peur intense ?")
            form.binary("b2", "Avez-vous, au moment de l’évènement, ressenti un sentiment d’impuissance ?")
            form.binary("b3", "Avez-vous, au moment de l’évènement, ressenti un sentiment de terreur ?")
        })
    }
}

export default build
