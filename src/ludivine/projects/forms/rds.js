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
    <p>Les individus qui ont vécu un ou plusieurs événements stressants souffrent parfois de réactions différentes de celles des autres.
    <p>Donnez-nous, sur base des énoncés suivants, votre meilleure <b>estimation de la réaction de la personne dont vous êtes le plus proche</b> lorsque vous lui avez parlé de votre expérience la plus difficile.
`;

function run(form, values) {
    form.block(() => {
        q(1, "Il ou elle semblait comprendre ce que j'ai vécu")
        q(2, "Il ou elle a ressenti de la sympathie envers moi pour ce qui s'est passé")
    })

    form.block(() => {
        q(3, "Il ou elle ne pouvait pas comprendre, n'ayant pas vécu mon expérience")
        q(4, "Il ou elle n'a pas compris à quel point il est difficile de poursuivre une vie quotidienne « normale » après ce qui s'est passé")
    })

    form.block(() => {
        q(5, "Ses réactions m'ont été utiles")
        q(6, "Il ou elle a trouvé que ma réaction à ces expériences était excessive")
    })

    form.block(() => {
        q(7, "Il ou elle que ma réaction à ces expériences étaient excessives")
        q(8, "Il ou elle semblait me blâmer, douter, me juger ou me questionnaire sur cette expérience")
    })

    form.block(() => {
        q(9, "Il ou elle s'est montré(e) très compréhensif(ve) et m'a soutenu(e) lorsque nous en avons parlé")
        q(10, "Je pensais que lui en parler se passerait bien mais ça n'a pas été le cas")
    })

    //form.block(() => {
        // q(11, "Je pensais que parler avec il ou elle serait terrible, mais en fait cela s'est très bien passé")
    //})

    function q(idx, label) {
        form.enum("*q" + idx, label, [
            [0, "Pas du tout d'accord"],
            [1, "Pas d'accord"],
            [2, "Plutôt pas d'accord"],
            [3, "Ni d'accord ni pas d'accord"],
            [4, "Plutôt d'accord"],
            [5, "D'accord"],
            [6, "Tout à fait d'accord"]
        ])
    }
}

export {
    intro,
    run
}
