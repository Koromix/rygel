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

function build(form, values, start) {
    form.values = values

    form.intro = html`
        <p>Les individus qui ont vécu un ou plusieurs événements stressants souffrent parfois de <b>réactions différentes de celles des autres</b>. C'est ce qui va nous intéresser dans les questions qui suivent.
    `

    form.part(() => {
        form.binary("pre1", `Avez-vous continué à parler de l’évènement qui vous a amené ici aux personnes auxquelles vous avez fait référence dans le bilan initial du ${start.toLocaleString()} ?`)
        form.binary("pre2", "Avez-vous reçu des réactions négatives de leur part ?")

        if (values.pre2) {
            form.slider("pre3", "Comment estimez-vous l'impact que ces réactions négatives ont eu sur vous ?", {
                min: 0, max: 10,
                prefix: "Aucun impact", suffix: "Impact maximum",
                help: "Placez le curseur sur la barre avec la souris ou votre doigt"
            })
        }
    })

    form.part(() => {
        form.binary("pre4", `Avez-vous parlé à de nouvelles personnes de cet évènement depuis le bilan initial du ${start.toLocaleString()} ?`)

        if (values.pre4 == 1) {
            let keys = Object.keys(PERSON_KINDS)

            form.output(html`De quelle <b>sphère de votre vie</b> les personnes à qui vous en avez parlé font-elles partie ?`)

            for (let key of keys) {
                let info = PERSON_KINDS[key]
                let label = `Combien dans la sphère « ${info.text.toLowerCase()} » ?`

                form.enumButtons("pre5" + key, label, [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, [10, '10 personnes ou plus'], [99, 'Non applicable']])
            }

            form.binary("pre6", "Avez-vous reçu des réactions négatives après en avoir parlé ?")

            if (values.pre6 == 1) {
                form.slider("pre7", "Comment estimez-vous l'impact que ces réactions négatives ont eu sur vous ?", {
                    min: 0, max: 10,
                    prefix: "Aucun impact", suffix: "Impact maximum",
                    help: "Placez le curseur sur la barre avec la souris ou votre doigt"
                })
            }
        }
    })

    form.part(() => {
        form.binary("post1", "Depuis le bilan initial, avez-vous rencontré de nouvelles personnes faisant partie du domaine médical ?")

        if (values.post1 == 1) {
            form.slider("post1b", "Comment avez-vous perçu vos interactions avec ces nouvelles personnes ?", {
                min: -10, max: 10,
                prefix: "🙁", suffix: "🙂",
                help: "Placez le curseur sur la barre avec la souris ou votre doigt"
            })
        }

        form.binary("post2", "Depuis le bilan initial, avez-vous rencontré de nouvelles personnes faisant partie du domaine judiciaire ?")

        if (values.post2 == 1) {
            form.slider("post2b", "Comment avez-vous perçu vos interactions avec ces nouvelles personnes ?", {
                min: -10, max: 10,
                prefix: "🙁", suffix: "🙂",
                help: "Placez le curseur sur la barre avec la souris ou votre doigt"
            })
        }

        form.binary("post3", "Depuis le bilan initial, avez-vous rencontré de nouvelles personnes faisant partie du domaine de l'aide psychologique ?")

        if (values.post3 == 1) {
            form.slider("post3b", "Comment avez-vous perçu vos interactions avec ces nouvelles personnes ?", {
                min: -10, max: 10,
                prefix: "🙁", suffix: "🙂",
                help: "Placez le curseur sur la barre avec la souris ou votre doigt"
            })
        }
    })

    form.part(() => {
        form.binary("ia1", "Vous est-il arrivé de communiquer avec une IA sur l’évènement qui vous amène ici ?")

        if (values.ia1) {
            form.slider("ia2", "Comment avez-vous perçu cette interaction ?", {
                min: -10, max: 10,
                prefix: "Très négative", suffix: "Très positive",
                help: "Placez le curseur sur la barre avec la souris ou votre doigt"
            })
            form.binary("ia3", "Avez-vous appris des informations utiles sur le trauma et/ou les ressentis associés ?")
            form.binary("ia4", "Cela vous a-t-il apporté un soulagement ?")
        }
    })
}

export default build
