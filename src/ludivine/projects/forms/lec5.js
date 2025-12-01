// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import { html } from 'vendor/lit-html/lit-html.bundle.js'

function build(form) {
    let values = form.values

    form.intro = html`
        <p>Voici une liste de <b>situations potentiellement traumatiques</b> que vous pouvez avoir eu à traverser (vivre) depuis votre enfance.
        <p>Pour chaque situation, cochez <b>une ou plusieurs cases</b> pour indiquer que :
        <ul>
            <li>Vous avez vécu personnellement la situation
            <li>Une autre personne a vécu la situation et vous en avez été témoin
            <li>Vous avez appris qu’un de vos proches a vécu la situation
            <li>Vous avez été exposé(e) à la situation dans le cadre de votre travail (ex : secouriste, policier, militaire, autre intervenant de première ligne)
            <li>Vous n'êtes pas sûr(e) que la situation corresponde
            <li>La situation ne s’applique pas à vous
        </ul>
        <p><i>Il est tout à fait possible d’avoir vécu une situation type, par exemple, une catastrophe naturelle sous des angles différents. Une fois, j’ai été inondé ; une autre fois j’ai appris qu’un membre de ma famille a été inondé, et finalement par mon travail de sapeur-pompier, je suis intervenu dans une situation d’inondation. Dans ce cas hypothétique, vous aurez coché  Ça m'est arrivé », « Je l’ai appris » et « Dans mon travail ».</i>
    `

    let catastrophes = {
        1: "Catastrophe naturelle (inondation, ouragan, tornade, tremblement de terre, etc.)",
        2: "Incendie ou explosion",
        3: "Accident de la route (accident de voiture ou de bateau, déraillement de train, écrasement d’avion, etc.)",
        4: "Accident grave au travail, à domicile ou pendant des occupations de loisirs",
        5: "Exposition à une substance toxique (produits chimiques dangereux, radiation, etc.)",
        6: "Agression physique (avoir été attaqué, frappé, poignardé, battu, reçu des coups de pieds, etc.)",
        7: "Attaque à main armée (avoir été menacé ou blessé par une arme à feu, un couteau, une bombe, etc.)",
        8: "Agression sexuelle (viol, tentative de viol, accomplir tout acte sexuel par la force ou sous menaces)",
        9: "Autre expérience sexuelle non désirée et désagréable (abus sexuel dans l’enfance)",
        10: "Participation à un conflit armé ou présence dans une zone de guerre (dans l’armée ou comme civil)",
        11: "Captivité, kidnapping ou prise d'otage",
        12: "Maladie ou blessure mettant ma vie ou celle d'autrui en danger",
        13: "Souffrances humaines intenses",
        14: "Souffrances animales intenses",
        15: "Mort violente (homicide, suicide, etc.)",
        16: "Mort soudaine accidentelle",
        17: "Blessure grave, dommage ou mort causé par vous à quelqu’un",
        18: "Toute autre évènement ou expérience très stressant"
    }
    let indices = Object.keys(catastrophes)

    let choices = [
        [1, "Ça m'est arrivé"],
        [2, "J'en ai été témoin"],
        [3, "Je l'ai appris"],
        [4, "Dans mon travail"],
        [5, "Je ne suis pas sûr"],
        [null, "Ne s'applique pas"]
    ]

    let evts = 0

    for (let start = 0; start < indices.length; start += 2) {
        let end = Math.min(start + 2, indices.length)

        form.part(() => {
            for (let i = start; i < end; i++) {
                let idx = indices[i]
                let label = catastrophes[idx] + " :"

                let intf = form.multiButtons("evt" + idx, label, choices, {
                    help: "Plusieurs réponses possibles"
                })

                if (intf.value && intf.value.length)
                    evts++
            }
        })
    }

    // if (evts > 1) {
    //     form.part(() => {
    //         let props = indices.filter(idx => values["evt" + idx]?.length > 0).map(idx => [idx, catastrophes[idx]])
    //         form.enumRadio("evt_index", "Sélectionnez l'évènement qui a été le plus difficile pour vous :", props)
    //     })
    // }
}

export default build
