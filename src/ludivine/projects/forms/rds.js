// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import { html } from '../../../../vendor/lit-html/lit-html.bundle.js'
import { PERSON_KINDS } from '../../client/network/constants.js'

function build(form, values) {
    form.values = values

    form.intro = html`
        <p>Les individus qui ont vécu un ou plusieurs événements stressants souffrent parfois de réactions différentes de celles des autres. C'est ce qui va nous intéresser dans les questions qui suivent.
    `

    form.part(() => {
        form.binary("parle", "Avez-vous parlé à une ou plusieurs personnes de l’expérience qui vous a amené ici ?")
    })

    if (values.parle == 1) {
        form.part(() => {
            let keys = Object.keys(PERSON_KINDS)

            form.output(html`De quelle <b>sphère de votre vie</b> les personnes à qui vous en avez parlé font-elles partie ?`)

            for (let key of keys) {
                let info = PERSON_KINDS[key]
                let label = `Combien dans la sphère « ${info.text.toLowerCase()} » ?`

                form.enumButtons(key, label, [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, [10, '10 personnes ou plus'], [99, 'Non applicable']])
            }
        })

        form.part(() => {
            form.binary("negatif", "Avez-vous reçu des réactions négatives après en avoir parlé ?")

            if (values.negatif == 1) {
                form.slider("impact", "Comment estimez-vous l’impact que ces réactions négatives ont eu sur vous ?", {
                    min: 0, max: 10,
                    prefix: "Aucun impact", suffix: "Impact maximum",
                    help: "Placez le curseur sur la barre avec la souris ou votre doigt"
                })
            }
        })

        form.intro = html`
            <p>Les individus qui ont vécu un ou plusieurs événements stressants souffrent parfois de réactions différentes de celles des autres. C'est ce qui va nous intéresser dans les questions qui suivent.
            <p>Nous souhaiterions maintenant que vous pensiez spécifiquement à la <b>personne dont vous êtes le plus proche et à qui vous avez parlé</b> de l’évènement qui vous a amené ici. Donnez-nous, sur base des énoncés suivants, votre meilleure estimation de sa réaction lorsque vous lui avez parlé cet évènement.
        `

        form.part(() => {
            q(1, "Il ou elle semblait comprendre ce que j'ai vécu :")
            q(2, "Il ou elle a ressenti de la sympathie envers moi pour ce qui s'est passé :")
            q(3, "Il ou elle ne pouvait pas comprendre, n'ayant pas vécu mon expérience :")
        })

        form.part(() => {
            q(4, "Il ou elle n'a pas compris à quel point il est difficile de poursuivre une vie quotidienne « normale » après ce qui s'est passé :")
            q(5, "Ses réactions m'ont été utiles :")
            q(6, "Il ou elle a trouvé que ma réaction à ces expériences était excessive :")
        })

        form.part(() => {
            q(7, "Il ou elle a trouvé que ma réaction à ces expériences étaient excessives :")
            q(8, "Il ou elle semblait me blâmer, douter, me juger ou me questionner sur cette expérience :")
            q(9, "Il ou elle s'est montré(e) très compréhensif(ve) et m'a soutenu(e) lorsque nous en avons parlé :")
        })

        form.part(() => {
            q(10, "Je pensais que lui en parler se passerait bien mais ça n'a pas été le cas :")
        })
    }

    form.intro = html`
        <p>Les individus qui ont vécu un ou plusieurs événements stressants souffrent parfois de réactions différentes de celles des autres. C'est ce qui va nous intéresser dans les questions qui suivent.
        <p>Suite à l’évènement qui vous a amené ici, décrivez comment <b>avez-vous perçu vos interactions</b> avec le personnel médical, avec le personnel judiciaire et avec le personnel d'aide psychologique.
    `

    form.part(() => {
        form.binary("inter1a", "Avez-vous eu des interactions avec un personnel médical en lien avec cet évènement ?")

        if (values.inter1a == 1) {
            form.slider("inter1b", "Comment avez-vous perçu vos interactions avec le personnel médical ?", {
                min: -10, max: 10,
                prefix: "🙁", suffix: "🙂",
                help: "Placez le curseur sur la barre avec la souris ou votre doigt"
            })
        }

        form.binary("inter2a", "Avez-vous eu des interactions avec un personnel judiciaire en lien avec cet évènement ?")

        if (values.inter2a == 1) {
            form.slider("inter2b", "Comment avez-vous perçu vos interactions avec le personnel judiciaire ?", {
                min: -10, max: 10,
                prefix: "🙁", suffix: "🙂",
                help: "Placez le curseur sur la barre avec la souris ou votre doigt"
            })
        }

        form.binary("inter3a", "Avez-vous eu des interactions avec un personnel d'aide psychologique en lien avec cet évènement ?")

        if (values.inter3a == 1) {
            form.slider("inter3b", "Comment avez-vous perçu vos interactions avec le personnel d'aide psychologique ?", {
                min: -10, max: 10,
                prefix: "🙁", suffix: "🙂",
                help: "Placez le curseur sur la barre avec la souris ou votre doigt"
            })
        }
    })

    function q(idx, label) {
        form.enumButtons("q" + idx, label, [
            [0, "Pas du tout d'accord"],
            [1, "Pas d'accord"],
            [2, "Plutôt pas d'accord"],
            [3, "Ni d'accord ni pas d'accord"],
            [4, "Plutôt d'accord"],
            [5, "D'accord"],
            [6, "Tout à fait d'accord"]
        ])
    }
}

export default build
