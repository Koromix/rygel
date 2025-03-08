// Copyright (C) 2024  Niels Martignène <niels.martignene@protonmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTYform.part without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

import { html } from '../../../../vendor/lit-html/lit-html.bundle.js'

function build(form, values) {
    form.values = values

    form.intro = html`
        <p>Ce questionnaire nous permet d'évaluer la <b>présence d'idées suicidaires</b>. En cas d'urgence, n'hésitez surtout pas à <a href="https://3114.fr" target="_blank"><span class="ui_tag" style="background: #db0a0aform.part">demander de l'aide !</span></a>
    `

    form.part(() => {
        form.enumButtons("q1", "Comment évaluez-vous votre désir de vivre ?", [
            [0, "Moyen à fort"],
            [1, "Faible désir"],
            [2, "Nul / aucun désir"]
        ])
        form.enumButtons("q2", "Comment évaluez-vous votre désir de mourir ?", [
            [0, "Nul / aucun désir"],
            [1, "Faible désir"],
            [2, "Moyen à fort"]
        ])
    })

    form.part(() => {
        form.enumRadio("q3", "Comment situez-vous la balance entre raisons de vivre et raisons mourir", [
            [0, "Raisons de vivre plus fortes que celles de mourir"],
            [1, "Elles sont égales"],
            [2, "Raisons de mourir plus fortes que celles de vivre"]
        ])
        form.enumButtons("q4", "Avez-vous le désir de poser un geste suicidaire ?", [
            [0, "Nul / aucun désir"],
            [1, "Faible désir"],
            [2, "Moyen à fort"]
        ])
    })

    form.part(() => {
        form.enumRadio("q5", "Désir de l'idéation/désir suicidaire", [
            [0, "Prendrait les précautions nécéssaires pour vous garder en vie"],
            [1, "Laisserait le hasard décider de sa vie ou de sa mort"],
            [2, "Éviterait de prendre les précautions nécessaires pour se sauver ou se maintenir en vie"]
        ])
    })

    let five = values.q1 + values.q2 + values.q3 + values.q4 + values.q5

    if (five === 0)
        return

    form.part(() => {
        form.enumRadio("q6", "Comment de temps durent vos désirs ou idéations suicidaires ?", [
            [0, "Bref rapide comme un éclair"],
            [1, "Pendant de plus longues périodes"],
            [2, "Continuellement ou presque continuellement"]
        ])
        form.enumRadio("q7", "À quelle fréquence surviennent votre désirs ou idéations suicidaires ?", [
            [0, "Rarement, occasionnellement"],
            [1, "De façon intermittente"],
            [2, "Persistant ou continuellement"]
        ])
    })

    form.part(() => {
        form.enumRadio("q8", "Attitude face à l'idéation/désir suicidaire", [
            [0, "Rejet de l'idéation"],
            [1, "Ambivalence/indifférence"],
            [2, "Acceptation de l'idéation"]
        ])
        form.enumRadio("q9", "Contrôle de l'action suicidaire/du désir de passage à l'acte", [
            [0, "A une impression de contrôle"],
            [1, "N'est pas certain(e) de son contrôle"],
            [2, "N'a pas d'impression de contrôle"]
        ])
    })

    form.part(() => {
        form.enumRadio("q10", "Motifs particuliers qui retiennent de poser un geste (famille, religion, irréversibilité du geste) ?", [
            [0, "Il y a au moins un motif qui vous empêche de vous suicider"],
            [1, "Certaines inquiétudes font que vous n'êtes pas certain(e)"],
            [2, "Rien ou presque rien ne vous empêche de vous suicider"]
        ])
        form.enumRadio("q11", "Raisons qui incident à poser un geste suicidaire", [
            [0, "Pour obtenir l'attention de l'entourage, se venger"],
            [2, "Pour fuir, résoudre des problèmes"],
            [1, "Les deux à la fois"],
            [9, "Pas de tentative considérée activement"]
        ])
    })

    form.part(() => {
        form.enumRadio("q12", "Méthode : Planifier les mesures prises, le moyen planifié du geste suicidaire", [
            [0, "Vous n'y avez pas pensé"],
            [1, "Oui, mais quelques détails restent à régler"],
            [2, "Oui, très bien planifié"],
            [9, "Pas de tentative considérée attentivement"]
        ])
        form.enumRadio("q13", "Conditions propices", [
            [0, "Le moyen n'est pas disponibleform.part le moment est inopportun"],
            [1, "Le moyen demande du temps et de l'énergieform.part le contexte ne s'y prête pas actuellement"],
            ["2A", "Le moyen est accessible et le contexte est favorable maintenant"],
            ["2B", "Le moyen et le contexte seront favorable d'ici peu"]
        ])
    })

    form.part(() => {
        form.enumRadio("q14", "Sentiment de culpabilité de poser un geste suicidaire", [
            [0, "Vous avez peur de poser un geste pour vous suicider"],
            [1, "Vous n'êtes pas certain(e) d'être capable de poser un geste pour vous suicider"],
            [2, "Vous êtes certain(e) que vous pouvez poser un geste pour vous suicider"],
        ])
        form.enumButtons("q15", "Anticipation/attente du geste suicidaire", [
            [0, "Non"],
            [1, "Incertain(e)"],
            [2, "Oui"]
        ])
    })

    form.part(() => {
        form.enumButtons("q16", "Avez-vous fait des préparations en vue d'un passage à l'acte ?", [
            [0, "Non"],
            [1, "Partiellement"],
            [2, "Complètement"]
        ])
        form.enumRadio("q17", "Avez-vous écrit un message d'adieu ?", [
            [0, "Non"],
            [1, "Vous y avez pensé, il est commencé mais pas terminé"],
            [2, "Oui"]
        ])
    })

    form.part(() => {
        form.enumRadio("q18", "Avez-vous fait des préparations finales en prévision de la mort (assurance, testament) ?", [
            [0, "Non"],
            [1, "Vous y avez pensé, il est commencé mais pas terminé"],
            [2, "Oui"]
        ])
        form.enumRadio("q19", "Avez-vous partagé votre désir de mourir ?", [
            [0, "Vous avez révélé ouvertement vos intentions"],
            [1, "Vous y avez fait allusion"],
            [2, "Vous avez essayé de dissimuler votre intention, de cacher ce désir ou de mentir"],
            [9, "Pas de tentative considérée attentivement"]
        ])
    })
}

export default build
