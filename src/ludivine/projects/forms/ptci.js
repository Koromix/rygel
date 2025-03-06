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
        <p>Nous voulons en savoir plus sur les <b>pensées que vous pourriez avoir suite à une expérience très stressante</b>. Vous trouverez ci-dessous une liste d’énoncés qui peuvent être représentatifs ou non de ces pensées. Veuillez lire attentivement ces énoncés et indiquer votre niveau d’accord ou de désaccord avec chacun d’entre eux.
        <p>Chaque personne réagit de façon différente à un événement très stressant. Il <b>n’y a pas de bonnes ou de mauvaises réponses</b> à ces énoncés.
    `

    let choices = [
        [1, "Totalement en désaccord"],
        [2, "Fortement en désaccord"],
        [3, "Légèrement en désaccord"],
        [4, "Neutre (ni accord ni désaccord)"],
        [5, "Légèrement en accord"],
        [6, "Fortement en accord"],
        [7, "Totalement en accord"],
    ]

    form.part(() => {
        form.enumButtons("q1", "L’événement est arrivé à cause de la façon dont j’ai agi", choices)
        form.enumButtons("q2", "Je n’ai pas confiance que je ferai ce qui est juste et bon", choices)
    })

    form.part(() => {
        form.enumButtons("q3", "Je suis une personne faible", choices)
        form.enumButtons("q4", "Je ne serai pas capable de contrôler ma colère et je ferai quelque chose de terrible", choices)
    })

    form.part(() => {
        form.enumButtons("q5", "Je ne suis pas capable de gérer la moindre frustration", choices)
        form.enumButtons("q6", "Auparavant j’étais une personne heureuse, mais maintenant je suis toujours malheureux", choices)
    })

    form.part(() => {
        form.enumButtons("q7", "On ne peut pas faire confiance aux gens", choices)
        form.enumButtons("q8", "Je dois toujours être sur mes gardes", choices)
    })

    form.part(() => {
        form.enumButtons("q9", "Je me sens mort à l’intérieur", choices)
        form.enumButtons("q10", "On ne peut jamais savoir qui nous fera du mal", choices)
    })

    form.part(() => {
        form.enumButtons("q11", "Je dois être particulièrement vigilant parce qu’on ne sait jamais ce qui nous attend", choices)
        form.enumButtons("q12", "Je suis inadéquat", choices)
    })

    form.part(() => {
        form.enumButtons("q13", "Je ne serai pas capable de contrôler mes émotions et quelque chose de terrible va arriver", choices)
        form.enumButtons("q14", "Si je pense à l’événement, je ne serai pas capable de le gérer", choices)
    })

    form.part(() => {
        form.enumButtons("q15", "L’événement m’est arrivé à cause du type de personne que je suis", choices)
        form.enumButtons("q16", "Mes réactions depuis l’événement indiquent que je suis en train de devenir fou", choices)
    })

    form.part(() => {
        form.enumButtons("q17", "Je ne pourrai jamais ressentir des émotions normales de nouveau", choices)
        form.enumButtons("q18", "Le monde est un endroit dangereux", choices)
    })

    form.part(() => {
        form.enumButtons("q19", "Quelqu’un d’autre aurait été capable d’empêcher l’événement d’arriver", choices)
        form.enumButtons("q20", "J’ai changé pour le pire et je ne redeviendrai jamais normal", choices)
    })

    form.part(() => {
        form.enumButtons("q21", "Je me sens comme un objet plutôt que comme une personne", choices)
        form.enumButtons("q22", "Quelqu’un d’autre ne se serait pas mis dans cette situation", choices)
    })

    form.part(() => {
        form.enumButtons("q23", "Je ne peux pas compter sur les autres", choices)
        form.enumButtons("q24", "Je me sens isolé et mis à part des autres", choices)
    })

    form.part(() => {
        form.enumButtons("q25", "Je n’ai pas d’avenir", choices)
        form.enumButtons("q26", "Je ne peux pas empêcher que des mauvaises choses m’arrivent", choices)
    })

    form.part(() => {
        form.enumButtons("q27", "Les gens ne sont pas ce qu’ils semblent être", choices)
        form.enumButtons("q28", "Ma vie a été gâchée par l’événement", choices)
    })

    form.part(() => {
        form.enumButtons("q29", "Il y a quelque chose qui ne va pas en moi", choices)
        form.enumButtons("q30", "Mes réactions depuis l’événement prouvent que je n’ai pas la capacité d’y faire face", choices)
    })

    form.part(() => {
        form.enumButtons("q31", "Il y a quelque chose en moi qui a provoqué l’événement", choices)
        form.enumButtons("q32", "Je ne serai pas capable de tolérer mes pensées à propos de l’événement et je vais m’effondrer", choices)
    })

    form.part(() => {
        form.enumButtons("q33", "J’ai l’impression de ne plus me connaître", choices)
        form.enumButtons("q34", "On ne sait jamais quand quelque chose de terrible va arriver", choices)
    })

    form.part(() => {
        form.enumButtons("q35", "Je ne peux pas me faire confiance", choices)
        form.enumButtons("q36", "Plus rien de positif ne peut m’arriver", choices)
    })
}

export default build
