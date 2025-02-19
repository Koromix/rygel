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

let intro = html`
    <p>Ce questionnaire est destiné à <b>déterminer la disponibilité du réseau social</b> autour de vous.
    <p>Évaluez votre <u>niveau d’accord ou de désaccord</u> avec chacun des énoncés. Il n’ y a pas de bonnes ou de mauvaises réponses; lorsque vous y répondrez, essayez de penser aux personnes qui vous entourent.
`

function run(form, values) {
    form.part(() => {
        q(1, "Il y a des personnes sur qui je peux compter pour m’aider en cas de réel besoin matériel")
        q(5, "Il y a des personnes qui prennent plaisir aux mêmes activités sociales que moi")
    })

    form.part(() => {
        q(8, "J’ai l’impression de faire partie d’un groupe de personnes qui partagent mes attitudes et mes croyances")
        q(11, "J’ai des personnes proches de moi qui me procurent un sentiment de sécurité affective et de bien-être")
    })

    form.part(() => {
        q(12, "Il y a quelqu’un avec qui je pourrais discuter de décisions importantes qui concernent ma vie")
        q(13, "J’ai des relations où sont reconnus ma compétence et mon savoir-faire (confirmation de sa valeur)")
    })

    form.part(() => {
        q(16, "Il y a une personne fiable à qui je pourrais faire appel pour me conseiller si j’avais des  problèmes")
        q(17, "Je ressens un lien affectif fort avec au moins une autre personne")
    })

    form.part(() => {
        q(20, "Il y a des gens qui admirent mes talents et habiletés")
        q(23, "Il y a des gens sur qui je peux compter en cas d’urgence matérielle")
    })

    function q(idx, label) {
        form.enumButtons("*q" + idx, label, [
            [1, "Fortement en désaccord"],
            [2, "En désaccord"],
            [3, "D’accord"],
            [4, "Fortement en accord"]
        ])
    }
}

export {
    intro,
    run
}
