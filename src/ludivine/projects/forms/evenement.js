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
    form.intro = html`
        <p>Nous allons vous demander de penser à l’évènement que <b>vous considérez comme étant le pire</b>, c'est-à-dire pour ce questionnaire, celui qui vous dérange le plus actuellement.
        <p>Nous allons vous poser une série de questions qui vont nous permettre de comprendre ce qui vous est arrivé. Nous avons conscience que cela <b>peut être difficile</b>, et nous vous sommes reconnaissants de cet effort.
    `

    form.part(() => {
        form.binary("q1", "Est-ce que l’événement qui vous a conduit à participer à cette étude concerne une situation de longue durée ayant impliqué de la violence physique et / ou morale ?", {
            help: "Exemples : relation abusive, traite humaine, esclavagisme, secte, inceste, situations dans lesquels les secours ont mis plusieurs jours à intervenir, etc."
        })

        if (values.q1 == 1)
            form.text("?q1a", "Précisez si vous le souhaitez (cela peut être difficile, n’hésitez pas à appuyer sur le bouton SOS si vous en avez besoin, ou rapprochez-vous d’une personne ressource)")
    })

    form.part(() => {
        if (values.q1 == 1)
            console.log('XXX')

        if (!values.q1) {
            form.number("q3", "Quel âge aviez-vous au moment des faits ?", {
                suffix: value => value > 1 ? "ans" : "an",
            })
        }
    })

    form.part(() => {
        form.enumRadio("q4", "Comment l’avez-vous vécu ?", [
            [1, "Cela m’est arrivé directement"],
            [2, "J’en ai été témoin"],
            [3, "J’ai appris qu’un membre de la famille ou un ami proche l’a vécu"],
            [4, "J’y ai été́ exposé à plusieurs reprises dans le cadre de mon travail (par exemple, ambulancier, police, militaire, ou autre premier répondant)"],
            [99, "Autre"]
        ])

        if (values.q4 == 99)
            form.text("?q4a", "Précisez :", { help: "Non obligatoire" })
    })

    form.part(() => {
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

    form.part(() => {
        form.binary("q7", "L’évènement impliquait-il une agression sexuelle ?")
        form.binary("q8", "L’événement comportait-il une violence (physique ou psychologique) intentionnelle exercée par autrui à votre encontre ?")
    })

    form.part(() => {
        form.enumRadio("q9", "Si l’évènement a entrainé la mort d’un membre de la famille ou d’un ami proche, était-ce dû à un type d’accident ou de violence, ou était-ce dû à des causes naturelles ?", [
            [1, "Un accident ou de la violence"],
            [2, "Causes naturelles"],
            [0, "Ne s’applique pas (l’évènement n’a pas entrainé la mort d’un membre de la famille ou d’un ami proche)"]
        ])

        form.enumButtons("q10", "Combien de fois est-ce que cela s’est produit ?", [
            [1, "Seulement une fois"],
            [2, "Plus qu’une fois"]
        ])
    })

    form.part(() => {
        form.enumRadio("q11", "Lors de cet événement, étiez-vous la seule victime ou d’autres personnes ont-elles également été affectées ?", [
            [1, "J’étais la seule victime"],
            [2, "D’autres personnes étaient également victimes"],
            [3, "Je ne sais pas"]
        ])

        form.binary("q12", "Connaissiez vous les autres victimes ?")

        form.multiCheck("q12a", "Qui sont ces victimes ?", [
            [1, "Cercle familial"],
            [2, "Amis"],
            [3, "Collègues"],
            [99, "Autre"]
        ], { help: "Plusieurs réponses possibles" })

        if (values.q12a?.includes?.(99))
            form.text("?q12b", "Précisez :", { help: "Non obligatoire" })
    })

    form.part(() => {
        form.enumButtons("q13", "Cet évènement impliquait-il un ou plusieurs agresseur(s) ?", [
            [1, "Un agresseur"],
            [2, "Plusieurs agresseurs"]
        ])
    })

    form.part(() => {
        form.binary("q14", "Cet évènement impliquait-il des témoins qui ont délibérément laisser agir l’agresseur ?")

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
                    form.text("?q15b", "Précisez :", { help: "Non obligatoire" })
            }
        }
    })

    form.part(() => {
        form.binary("q16", "Cet évènement impliquait-il des témoins qui vous ont porté secours ?")
    })
}

export default build
