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
    <p>Lorsque quelque chose de grave ou d'effrayant se produit, les gens peuvent avoir des pensées ou des sentiments différents. Vous nous avez expliqué un peu ce qui vous arrivé.
    <p>Ce questionnaire est destiné à évaluer :
    <ol>
        <li>Ce que vous pensiez et ressentez <u>AU MOMENT DE CE QUI VOUS EST ARRIVÉ</u> <i>(première partie)</i>
        <li>Ce que vous pensez et ressentez <u>MAINTENANT</u> <i>(deuxième partie)</i>
    </ol>
`;

function run(form, values) {
    const passe = html`
        <p>Les éléments suivants concernent ce que vous pensiez et ressentiez <u>AU MOMENT DE CE QUI VOUS EST ARRIVÉ</u>.
        <p>Dites-nous dans quelle mesure chacun d'entre eux est vrai pour vous.
    `
    const actuel = html`
        <p>Les éléments suivants concernent comment vous vous sentez <u>MAINTENANT</u>.
        <p>Dites-nous dans quelle mesure chacun d'entre eux est vrai pour vous.
    `

    form.block(() => {
        form.output(passe)
        q(2, "Mon cerveau s'est vidé")
        q(3, "J'ai fait des choses dont je n'avais pas conscience")
    })

    form.block(() => {
        form.output(passe)
        q(4, "Les choses semblaient se dérouler très lentement")
        q(5, "Les choses semblaient se dérouler très rapidement")
    })

    form.block(() => {
        form.output(passe)
        q(6, "Ce qui se passait me semblait irréel - comme si j'étais dans un rêve ou que je regardais un film")
        q(7, "J'avais l'impression de ne pas être là, que je ne faisais pas partie de ce qui se passait")
    })


    form.block(() => {
        form.output(passe)
        q(8, "Je me suis senti(e) confus(e)")
        q(9, "Je me suis senti(e) engourdi(e) - comme si je n'avais plus aucun sentiment")
    })

    form.block(() => {
        form.output(passe)
        q(10, "Des personnes comme ma famille ou mes amis semblaient être des étrangers")
        q(11, "Tout semblait bizarre, pas normal")
    })

    form.block(() => {
        form.output(passe)
        q(12, "Par moments, je n'étais pas sûr(e) de l'endoit où je me trouvais ni de l'heure qu'il était")
        q(13, "Il y a eu des moments où je n'ai pas ressenti de douleur, même là où j'étais blessé(e)")
    })

    form.block(() => {
        form.output(passe)
        q(14, "J’avais très peur")
        // q(15, "Je voulais que ça s'arrête mais je n'y arrivais pas")
        q(16, "Je me sens mal parce que ce qui se passait me semblait si horrible")
    })

    form.block(() => {
        form.output(actuel)
        q(17, "Je ne me souviens pas de certaines parties de ce qui s'est passé")
        q(18, "Je n'arrête pas de penser à ce qui s'est passé")
    })

    form.block(() => {
        form.output(actuel)
        q(19, "Je ne veux pas penser à ce qui s'est passé")
        q(20, "Je me sens nerveux(se)")
    })

    form.block(() => {
        form.output(actuel)
        q(21, "Mes sentiments sont engourdis - je me sens 'coupé(e)' de mes émotions")
        q(22, "Quand je pense à ce qui s'est passé, je me sens vraiment boulversé(e)")
    })

    form.block(() => {
        form.output(actuel)
        q(23, "J'essaie de ne pas me souvenir ou de ne pas penser à ce qui m'est arrivé")
        q(24, "J'ai du mal à me concentrer ou à prêter attention")
    })

    form.block(() => {
        form.output(actuel)
        q(25, "Je me sens dans l'espace ou déconnecté(e) du monde qui m'entoure")
        q(26, "Des images ou des sons de ce qui s'est passé me reviennent sans cesse à l'esprit")
    })

    form.block(() => {
        form.output(actuel)
        q(27, "Je suis perturbé(e) lorsque quelque chose me rappelle ce qui s'est passé")
        q(28, "Je me sens 'hyper' ou comme si je ne peux pas rester immobile")
    })

    function q(idx, label) {
        form.enum("*q" + idx, label, [
            [0, "Pas vrai"],
            [1, "Un peu ou parfois vrai"],
            [2, "Souvent ou très souvent vrai"]
        ])
    }
}

export {
    intro,
    run
}
