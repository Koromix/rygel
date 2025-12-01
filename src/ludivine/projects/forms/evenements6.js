// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import { html } from 'vendor/lit-html/lit-html.bundle.js'

function build(form, values, since) {
    form.values = values

    form.intro = html`
        <p>Nous allons vous demander de penser à l’évènement que vous avez <b>décrit dans le bilan initial</b>.
    `

    form.part(() => {
        form.binary("q1", "Est-ce que l’événement qui vous a conduit à participer à cette étude concerne une situation de longue durée ayant impliqué de la violence physique et/ou morale ?", {
            help: "Exemples : relation abusive, traite humaine, esclavagisme, secte, inceste, situations dans lesquels les secours ont mis plusieurs jours à intervenir, etc."
        })

        if (values.q1 == 1) {
            form.enumRadio("?q1a", "De quel évènement s'agit-il ?", [
                [1, "Relation abusive"],
                [2, "Traite humaine"],
                [3, "Esclavagisme"],
                [4, "Secte"],
                [5, "Inceste"],
                [6, "Situation dans laquelle les secours ont mis plusieurs jours à intervenir"],
                [99, "Autre"]
            ], {
                help: html`Non obligatoire. Cela peut être difficile, n’hésitez pas à appuyer sur le <b>bouton « SOS »</b> si vous en avez besoin, ou rapprochez-vous d’une personne ressource.`
            })

            if (values.q1a == 99)
                form.text("?q1b", "Précisez si vous le souhaitez :", { help: "Non obligatoire" })

            form.binary("q2", "Considérez-vous être à l’abri maintenant ?", {
                help: values.q2e === 0 ? html`N'hésitez pas à utiliser notre <b>bouton « SOS »</b> si vous en sentez le besoin` : ""
            })
        }
    })

    form.part(() => {
        form.multiCheck("q3", `Depuis ${since}, avez-vous :`, [
            [1, "Poursuivi un suivi psychologique débuté avant le bilan initial ?"],
            [2, "Arrêté un suivi psychologique en cours ?"],
            [3, "Débuté un suivi psychologique ?"]
        ])

        form.enumRadio("q4", "À la suite du bilan initial, avez-vous :", [
            [1, "Poursuivi un traitement médicamenteux en lien avec l'évènement ?"],
            [2, "Arrêté un suivi psychologique en cours ?"],
            [3, "Débuté un suivi psychologique ?"]
        ])
    })
}

export default build
