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

function build(form, values) {
    form.values = values

    form.intro = html`
        <p>Les informations recueuilles sur cette page nous aident et vous aident à identifier <b>le(s) évènement(s) possiblement traumatiques</b> survenus lorsque vous étiez plus jeune.
        <p>Ces questions concernent donc votre expérience <b>pendant l'enfance et l'adolescence</b>. Essayez de répondre de la manière la plus honnête possible.
    `

    form.part(() => {
        q(1, "Il m’est arrivé de ne pas avoir assez à manger :")
        q(2, "Je savais qu’il y avait quelqu’un pour prendre soin de moi et me protéger :")
        q(3, "Des membres de ma famille me disaient que j’étais « stupide » ou « paresseux » ou « laid » :")
    })

    form.part(() => {
        q(4, "Mes parents étaient trop saouls ou « pétés » pour s’occuper de la famille :")
        q(5, "Il y avait quelqu'un dans ma famille qui m’aidait à sentir que j’étais important ou particulier :")
        q(6, "Je devais porter des vêtements sales :")
    })

    form.part(() => {
        q(7, "Je me sentais aimé(e) :")
        q(8, "Je pensais que mes parents n’avaient pas souhaité ma naissance :")
        q(9, "J’ai été frappé(e) si fort par un membre de ma famille que j’ai dû consulter un docteur ou aller à l’hôpital :")
    })

    form.part(() => {
        q(10, "Je n’aurais rien voulu changer à ma famille :")
        q(11, "Un membre de ma famille m’a frappé(e) si fort que j’ai eu des bleus ou des marques :")
        q(12, "J’étais puni(e) au moyen d’une ceinture, d’un bâton, d’une corde ou de quelque autre objet dur :")
    })

    form.part(() => {
        q(13, "Les membres de ma famille étaient attentifs les uns aux autres :")
        q(14, "Les membres de ma famille me disaient des choses blessantes ou insultantes :")
        q(15, "Je pense que j’ai été physiquement maltraité(e) :")
    })

    form.part(() => {
        q(16, "J’ai eu une enfance parfaite :")
        q(17, "J’ai été frappé(e) ou battu(e) si fort que quelqu’un l’a remarqué (par ex. un professeur, un voisin ou un docteur) :")
        q(18, "J’avais le sentiment que quelqu’un dans ma famille me détestait :")
    })

    form.part(() => {
        q(19, "Les membres de ma famille se sentaient proches les uns des autres :")
        q(20, "Quelqu’un a essayé de me faire des attouchements sexuels ou de m’en faire faire :")
        q(21, "Quelqu’un a menacé de me blesser ou de raconter des mensonges à mon sujet si je ne faisais pas quelque chose de nature sexuelle avec lui :")
    })

    form.part(() => {
        q(22, "J’avais la meilleure famille du monde :")
        q(23, "Quelqu’un a essayé de me faire faire des actes sexuels ou de me faire regarder de tels actes :")
        q(24, "J’ai été victime d’abus sexuels :")
    })

    form.part(() => {
        q(25, "Je pense que j’ai été maltraité affectivement :")
        q(26, "Il y avait quelqu’un pour m’emmener chez le docteur si j’en avais besoin :")
        q(27, "Je pense qu’on a abusé de moi sexuellement :")
    })

    form.part(() => {
        q(28, "Ma famille était une source de force et de soutien :")
    })

    function q(idx, label) {
        form.enumButtons("q" + idx, label, [
            [1, "Jamais"],
            [2, "Rarement"],
            [3, "Quelquefois"],
            [4, "Souvent"],
            [5, "Très souvent"]
        ])
    }
}

export default build
