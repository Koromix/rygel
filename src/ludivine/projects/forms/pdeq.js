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
        <p>Ce questionnaire est destiné à rechercher les <b>expériences de dissociation</b> que vous auriez pu ressentir pendant l'événement traumatique, et au cours des quelques heures suivantes.
        <p>Veuillez répondre aux énoncés suivants en cochant le choix de réponse qui décrit le mieux vos expériences et réactions durant l’événement et immédiatement après. Si une question ne s’applique pas à votre expérience, <b>cochez « Pas du tout vrai »</b>.
        <p>Pour répondre à ces questions, considérez uniquement le <b>moment de l'évènement et les premières heures</b> qui ont suivi !
    `

    form.part(() => {
        form.output(html`
            <p>Pour les questions suivantes, nous aimerions que vous gardiez en tête l’évènement (ou la situation traumatique) décrite <b>dans la partie « L’évènement qui vous a amené ici »</b>.
        `)

        form.binary("b1", "Avez-vous, au moment de l’évènement, ressenti un sentiment de peur intense ?")
        form.binary("b2", "Avez-vous, au moment de l’évènement, ressenti un sentiment d’impuissance ?")
        form.binary("b3", "Avez-vous, au moment de l’évènement, ressenti un sentiment de terreur ?")
    })

    form.part(() => {
        form.output(html`
            <p>Nous allons maintenant explorer les expériences de dissociation (moments où vous « sentiez détaché de vous-même »).
            <p>Nous vous invitons à répondre aux énoncés suivants en sélectionnant la réponse qui <b>décrit le mieux vos expériences et réactions durant l’événement</b> et immédiatement après. Si une question ne s’applique pas à votre expérience, cochez  Pas du tout vrai ».
        `)

        q(1, "Il y a eu des moments où j’ai perdu le fil de ce qui se passait – j’étais complètement déconnecté(e) ou, d’une certaine façon, j’avais l'impression de ne pas faire partie de ce qui se passait :")
        q(2, "Je me suis senti(e) en « pilote automatique » – je me suis mis(e) à faire des choses que, je l’ai réalisé plus tard, je n’avais pas activement décidé de faire :")
    })

    form.part(() => {
        q(3, "Ma perception du temps a changé – les choses avaient l’air de se dérouler au ralenti.")
        q(4, "Ce qui se passait me semblait irréel, comme si j’étais dans un rêve ou au cinéma, ou en train de jouer un rôle.")
    })

    form.part(() => {
        q(5, "C’est comme si j’étais le (ou la) spectateur(trice) de ce qui m’arrivait, comme si je flottais au dessus de la scène et l’observait de l’extérieur.")
        q(6, "Il y a eu des moments où la perception de mon propre corps était distordue ou changée. Je me sentais déconnecté(e) de mon propre corps, ou bien il me semblait plus grand ou plus petit que d’habitude.")
    })

    form.part(() => {
        q(7, "J’avais l’impression que les choses qui arrivaient aux autres m’arrivaient à moi aussi – comme par exemple être en danger même si je ne l'étais pas.")
        q(8, "J’ai été surpris(e) de constater après coup que je n'ai pas perçu certains évènements qui se sont produits, alors que je les aurais habituellement remarqués.")
    })

    form.part(() => {
        q(9, "J’étais confus(e), j’avais par moment de la difficulté à comprendre ce qui se passait vraiment.")
        q(10, "J’étais désorienté(e), j’étais par moment incertain(e) de l’endroit où je me trouvais.")
    })

    function q(idx, label) {
        form.enumButtons("q" + idx, label, [
            [1, "Pas du tout vrai"],
            [2, "Un peu vrai"],
            [3, "Plutôt vrai"],
            [4, "Très vrai"],
            [5, "Extrêmement vrai"]
        ])
    }
}

export default build
