// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import { html } from 'vendor/lit-html/lit-html.bundle.js'
import { ASSETS } from '../../assets/assets.js'

function build(form) {
    let values = form.values

    form.intro = html`
        <p>Les questions qui suivent portent sur des <b>comportements que vous pourriez présenter</b> (ou non) dans votre vie quotidienne.
    `

    form.section(() => {
        form.enumButtons("tabagisme", "Êtes-vous consommateur/consommatrice de tabac ?", [
            ["actif", "Oui"],
            ["non", "Je n'ai jamais fumé"],
            ["sevre", "J'ai arrêté"]
        ])

        if (values.tabagisme == "actif") {
            form.binary("tabagisme_preex", "Était-ce déjà le cas avant l’évènement ?")

            if (values.tabagisme_preex) {
                form.number("tabagisme_qt", "Depuis combien de temps fumez-vous ?", {
                    min: 0,
                    suffix: value => value > 1 ? "années" : "année",
                    help: "Si vous fumez depuis moins d'un an, mettez 0"
                })
                form.binary("tabagisme_inc", "Considérez-vous que votre consommation ait augmenté suite à l’évènement ?")
            }
        }
    })

    form.intro = html`
        <p>Ce questionnaire interroge votre consommation d'alcool durant cette dernière année. Répondez donc en fonction de votre <b>consommation moyenne sur la dernière année</b> et non sur les dernières semaines.
    `

    form.section(() => {
        form.enumButtons("audit1", "À quelle fréquence vous arrive-t-il de consommer des boissons contenant de l'alcool ?", [
            [0, "Jamais"],
            [1, "Au moins une fois par mois"],
            [2, "2 à 4 fois par mois"],
            [3, "2 à 3 fois par semaine"],
            [4, "4 fois ou plus par semaine"]
        ])
    })

    form.section(() => {
        form.enumButtons("audit2", "Combien de verres standards buvez-vous au cours d'une journée ordinaire où vous buvez de l'alcool ?", [
            [0, "1 ou 2"],
            [1, "3 ou 4"],
            [2, "4 ou 5"],
            [3, "7 à 9"],
            [4, "10 ou plus"]
        ], { help: "Consultez le guide visuel ci-dessous pour vous aider à répondre" })

        form.output(html`
            <div style="text-align: center; margin-top: 2em;">
                <img src=${ASSETS['misc/verres_std']} width="583" height="273"/>
            </div>
        `)
    })

    form.section(() => {
        form.enumButtons("audit3", "Au cours d'une même occasion, combien de fois vous arrive-t-il de boire six verres standards ou plus ?", [
            [0, "Jamais"],
            [1, "Moins d'une fois par mois"],
            [2, "Une fois par mois"],
            [3, "Une fois par semaine"],
            [4, "Chaque jour ou presque"]
        ], { help: "Consultez le guide visuel ci-dessous pour vous aider à répondre" })

        form.output(html`
            <div style="text-align: center; margin-top: 2em;">
                <img src=${ASSETS['misc/verres_std']} width="583" height="273"/>
            </div>
        `)
    })

    form.section(() => {
        form.enumButtons("audit4", "Dans les 12 derniers mois, combien de fois avez-vous observé que vous n'étiez plus capable de vous arrêter de boire après avoir commencé ?", [
            [0, "Jamais"],
            [1, "Moins d'une fois par mois"],
            [2, "Une fois par mois"],
            [3, "Une fois par semaine"],
            [4, "Chaque jour ou presque"]
        ])

        form.enumButtons("audit5", "Dans les 12 derniers mois, combien de fois le fait d'avoir bu de l'alcool, vous-a-t-il empêché de faire ce qu'on attendait normalement de vous ?", [
            [0, "Jamais"],
            [1, "Moins d'une fois par mois"],
            [2, "Une fois par mois"],
            [3, "Une fois par semaine"],
            [4, "Chaque jour ou presque"]
        ], { help: "Par exemple : aller au travail, vous rendrez à un rendez-vous"})

        form.enumButtons("audit6", "Dans les 12 derniers mois, combien de fois, après une période de forte consommation, avez-vous du boire de l'alcool dès le matin pour vous remettre en forme ?", [
            [0, "Jamais"],
            [1, "Moins d'une fois par mois"],
            [2, "Une fois par mois"],
            [3, "Une fois par semaine"],
            [4, "Chaque jour ou presque"]
        ])
    })

    form.section(() => {
        form.enumButtons("audit7", "Dans les 12 derniers mois, combien de fois avez-vou eu un sentiment de culpabilité ou de regret après avoir bu ?", [
            [0, "Jamais"],
            [1, "Moins d'une fois par mois"],
            [2, "Une fois par mois"],
            [3, "Une fois par semaine"],
            [4, "Chaque jour ou presque"]
        ])

        form.enumButtons("audit8", "Dans les 12 derniers mois, combien de fois avez vous été incapable de vous souvenir de ce qui s'était passé la nuit précédente parce que vous aviez bu ?", [
            [0, "Jamais"],
            [1, "Moins d'une fois par mois"],
            [2, "Une fois par mois"],
            [3, "Une fois par semaine"],
            [4, "Chaque jour ou presque"]
        ])

        form.enumRadio("audit9", "Vous êtes-vous blessé ou avez-vous blessé quelqu'un parce que vous aviez bu ?", [
            [0, "Non"],
            [2, "Oui mais pas dans les 12 derniers mois"],
            [4, "Oui au cours des 12 derniers mois"]
        ])
    })

    form.section(() => {
        form.enumRadio("audit10", "Est-ce qu'un parent, un ami, un médecin ou un autre professionnel de santé s'est déjà préoccupé de votre consommation d'alcool et vous a conseillé de la diminuer ?", [
            [0, "Non"],
            [2, "Oui mais pas dans les 12 derniers mois"],
            [4, "Oui au cours des 12 derniers mois"]
        ])
    })

    form.intro = html`
        <p>Nous allons désormais nous intéresser aux <b>autres substances</b> que le tabac et l'alcool.
    `

    form.section(() => {
        form.binary("drogues1", "Êtes-vous consommateur ou consommatrice d'autres substances ?", {
            help: "Exemples : cannabis, champignons hallucinogènes, héroïne, crack, cocaïne, LSD, MDMA, ou autres substances apparentées"
        })

        form.multiCheck("drogues2", "Pouvez-vous cocher les substances que vous consommez ?", [
            [1, "Cannabis"],
            [2, "Champignons hallucinogènes"],
            [3, "Héroïne"],
            [4, "Crack"],
            [5, "Cocaïne"],
            [6, "LSD"],
            [7, "MDMA"],
            [99, "Autres substances apparentées"]
        ], { disabled: !values.drogues1 })

        if (values.drogues2?.includes?.(99))
            form.text("?drogues2b", "Précisez si vous le souhaitez :", { help: "Non obligatoire" })
        if (values.drogues1)
            form.binary("drogues3", "Était-ce déjà le cas avant l’évènement ?")
    })

    if (values.drogues1) {
        form.section(() => {
            form.number("drogues4", "Depuis combien de temps consommez-vous ces substances ?", {
                suffix: value => value > 1 ? "années" : "année",
                help: "Si vous consommez depuis moins d'un an, mettez 0"
            })
            form.binary("drogues5", "Considérez-vous que votre consommation ait augmenté suite à l’évènement ?")
        })
    }

    form.section(() => {
        form.enumButtons("automed1", "Vous arrive-t-il de prendre plus de comprimés ou de doses d’un médicament que ce qui est indiqué sur votre ordonnance ?", [
            [1, "Oui"],
            [0, "Non"],
            [99, "Non applicable"]
        ], { help: "Exemples : psychotropes, antidouleurs, anxiolytiques, ou autres substances apparentées" })

        if (values.automed1 == 1)
            form.textArea("?automed2", "Si vous l’acceptez, pouvez-vous nous partager les raisons pour lesquelles cela vous arrive ?", { help: "Non obligatoire" })
    })

    form.section(() => {
        form.enumButtons("suivi1", "Êtes-vous suivi par un médecin et/ou psychologue pour gérer votre consommation de tabac et/ou de substances ?", [
            [1, "Oui"],
            [0, "Non"],
            [99, "Non applicable"]
        ], { help: "Si vous ne consommez pas de tabac / substances, cochez « Non applicable »"})
    })
}

export default build
