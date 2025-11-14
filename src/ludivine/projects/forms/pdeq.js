// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import { html } from 'vendor/lit-html/lit-html.bundle.js'

function build(form, values) {
    form.values = values

    form.intro = html`
        <p>Pour les questions suivantes, nous aimerions que vous gardiez en tête l’évènement (ou la situation traumatique) décrit <b>dans la partie « L’évènement qui vous a amené ici »</b>.
        <p>Ce questionnaire est destiné à rechercher les <b>expériences de dissociation</b> (moments où vous « sentiez détaché(e) de vous-même ») que vous auriez pu ressentir pendant l'événement traumatique, et au cours des quelques heures suivantes.
    `

    form.part(() => {
        form.binary("b1", "Avez-vous, au moment de l’évènement, ressenti un sentiment de peur intense ?")
        form.binary("b2", "Avez-vous, au moment de l’évènement, ressenti un sentiment d’impuissance ?")
        form.binary("b3", "Avez-vous, au moment de l’évènement, ressenti un sentiment de terreur ?")
    })

    form.intro = html`
        <p>Pour les questions suivantes, nous aimerions que vous gardiez en tête l’évènement (ou la situation traumatique) décrit <b>dans la partie « L’évènement qui vous a amené ici »</b>.
        <p>Ce questionnaire est destiné à rechercher les <b>expériences de dissociation</b> (moments où vous « sentiez détaché(e) de vous-même ») que vous auriez pu ressentir pendant l'événement traumatique, et au cours des quelques heures suivantes.
        <p>Veuillez répondre aux énoncés suivants en cochant le choix de réponse qui décrit le mieux vos <b>expériences et réactions durant l’événement et immédiatement après</b>. Si une question ne s’applique pas à votre expérience, cochez « Pas du tout vrai ».
        <p>Pour répondre à ces questions, considérez uniquement le moment de l'évènement et les premières heures qui ont suivi !
    `

    form.part(() => {
        q(1, "Il y a eu des moments où j’ai perdu le fil de ce qui se passait – j’étais complètement déconnecté(e) ou, d’une certaine façon, j’avais l'impression de ne pas faire partie de ce qui se passait :")
        q(2, "Je me suis senti(e) en « pilote automatique » – je me suis mis(e) à faire des choses que, je l’ai réalisé plus tard, je n’avais pas activement décidé de faire :")
        q(3, "Ma perception du temps a changé – les choses avaient l’air de se dérouler au ralenti :")
    })

    form.part(() => {
        q(4, "Ce qui se passait me semblait irréel, comme si j’étais dans un rêve ou au cinéma, ou en train de jouer un rôle :")
        q(5, "C’est comme si j’étais le (ou la) spectateur(trice) de ce qui m’arrivait, comme si je flottais au dessus de la scène et l’observait de l’extérieur :")
        q(6, "Il y a eu des moments où la perception de mon propre corps était distordue ou changée. Je me sentais déconnecté(e) de mon propre corps, ou bien il me semblait plus grand ou plus petit que d’habitude :")
    })

    form.part(() => {
        q(7, "J’avais l’impression que les choses qui arrivaient aux autres m’arrivaient à moi aussi – comme par exemple être en danger même si je ne l'étais pas :")
        q(8, "J’ai été surpris(e) de constater après coup que je n'ai pas perçu certains évènements qui se sont produits, alors que je les aurais habituellement remarqués :")
        q(9, "J’étais confus(e), j’avais par moment de la difficulté à comprendre ce qui se passait vraiment :")
    })

    form.part(() => {
        q(10, "J’étais désorienté(e), j’étais par moment incertain(e) de l’endroit où je me trouvais :")
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
