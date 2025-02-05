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

import { html } from '../../../../vendor/lit-html/lit-html.bundle.js';

let intro = html`
    <p>Les informations recueuilles sur cette page nous aident et vous aident à identifier <b>le(s) évènement(s) possiblement traumatiques</b> survenus lorsque vous étiez plus jeune.
    <p>Ces questions concernent donc votre expérience <u>pendant l'enfance et l'adolescence</u>. Essayez de répondre de la manière la plus honnête possible.
`;

function run(form, values) {
    form.block(() => {
        q(1, "Les membres de ma famille me traitaient de « stupide », « paresseux(se) » ou « laid(e) »")
        q(2, "J’ai eu le sentiment que mes parents n’avaient pas désiré ma naissance")
        q(3, "Mes parents me disaient des choses blessantes et/ou insultantes")
        q(4, "Je sentais qu’il y avait un membre de ma famille qui me haïssait")
        q(5, "Je croyais être abusé(e) émotionnellement")
    })

    form.block(() => {
        q(6, "Il y a eu un membre de ma famille qui m’a aidé à avoir une bonne estime de moi")
        q(7, "Je me sentais aimé(e)")
        q(8, "Il y avait beaucoup d’entraide entre les membres de ma famille")
        q(9, "Les membres de ma famille étaient proches les uns des autres")
        q(10, "Ma famille était source de force et de support")
    })

    form.block(() => {
        q(11, "J’ai été frappé(e) par un membre de ma famille à un point tel que j’ai dû consulter un médecin ou être hospitalisé(e)")
        q(12, "J’ai été battu(e) par les membres de ma famille au point d’en avoir des bleus ou des marques")
        q(13, "J’ai été battu(e) avec une ceinture, un bâton ou une corde (ou tout objet dur)")
        q(14, "J’ai été battu(e) au point qu’un professeur, un voisin ou un médecin s’en soit aperçu")
    })

    form.block(() => {
        q(15, "J’ai manqué de nourriture")
        q(16, "Il y avait quelqu’un pour prendre soin de moi et me protéger")
        q(17, "Mes parents étaient trop ivres ou drogués pour prendre soin des enfants")
        q(18, "J’ai dû porter des vêtements sales")
        q(19, "Il y avait quelqu’un pour m’amener consulter un médecin lorsque nécessaire")
    })

    form.block(() => {
        q(20, "Quelqu’un a tenté de me faire des attouchements sexuels ou tenté de m’amener à poser de tels gestes")
        q(21, "Un membre de ma famille me menaçait de blessures ou de mentir sur mon compte afin que j’aie des contacts sexuels avec lui/elle")
        q(22, "Quelqu’un a essayé de me faire poser des gestes sexuels ou de me faire voir des choses sexuelles")
        q(23, "Je croyais être abusé(e) sexuellement")
    })

    form.block(() => {
        q(24, "Il n’y avait rien que j’aurais voulu changer dans ma famille")
        q(25, "J’ai grandi dans un entourage idéal")
        q(26, "J’avais la meilleure famille au monde")
    })

    function q(idx, label) {
        form.enum("*q" + idx, label, [
            [1, "Jamais"],
            [2, "Rarement"],
            [3, "Parfois"],
            [4, "Souvent"],
            [5, "Sans arrêt"]
        ])
    }
}

export {
    intro,
    run
}
