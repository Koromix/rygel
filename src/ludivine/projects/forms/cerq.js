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
import { PERSON_KINDS } from '../../client/network/constants.js'

function build(form, values) {
    form.values = values

    form.intro = html`
        <p>Nous vous demandons maintenant <b>ce que vous pensez généralement</b> lorsque vous vivez des événements négatifs ou désagréables.
    `

    form.part(() => {
        form.output(html`<p><i>Lorsque je vis des événements négatifs ou désagréables...</i>`)
        q(1, "J’ai le sentiment que je suis celui/celle à blâmer pour ce qui s’est passé :")
        q(2, "Je pense que je dois accepter que cela se soit passé :")
        q(3, "Je pense souvent à ce que je ressens par rapport à ce que j’ai vécu :")
    })

    form.part(() => {
        form.output(html`<p><i>Lorsque je vis des événements négatifs ou désagréables...</i>`)
        q(4, "Je pense à des choses plus agréables que celles que j’ai vécues :")
        q(5, "Je pense à la meilleure façon de faire :")
        q(6, "Je pense pouvoir apprendre quelque chose de la situation :")
    })

    form.part(() => {
        form.output(html`<p><i>Lorsque je vis des événements négatifs ou désagréables...</i>`)
        q(7, "Je pense que tout cela aurait pu être bien pire :")
        q(8, "Je pense souvent que ce que j’ai vécu est bien pire que ce que d’autres ont vécu :")
        q(9, "J’ai le sentiment que les autres sont à blâmer pour ce qui s’est passé :")
    })

    form.part(() => {
        form.output(html`<p><i>Lorsque je vis des événements négatifs ou désagréables...</i>`)
        q(10, "J’ai le sentiment que je suis responsable de ce qui s’est passé :")
        q(11, "Je pense que je dois accepter la situation :")
        q(12, "Je suis préoccupé(e) par ce que je pense et ce que je ressens concernant ce que j’ai vécu :")
    })

    form.part(() => {
        form.output(html`<p><i>Lorsque je vis des événements négatifs ou désagréables...</i>`)
        q(13, "Je pense à des choses agréables qui n’ont rien à voir avec ce que j’ai vécu :")
        q(14, "Je pense à la meilleure manière de faire face à la situation :")
        q(15, "Je pense pouvoir devenir une personne plus forte suite à ce qui s’est passé :")
    })

    form.part(() => {
        form.output(html`<p><i>Lorsque je vis des événements négatifs ou désagréables...</i>`)
        q(16, "Je pense que d’autres passent par des expériences bien pires :")
        q(17, "Je repense sans cesse au fait que ce que j’ai vécu est terrible :")
        q(18, "J’ai le sentiment que les autres sont responsables de ce qui s’est passé :")
    })

    form.part(() => {
        form.output(html`<p><i>Lorsque je vis des événements négatifs ou désagréables...</i>`)
        q(19, "Je pense aux erreurs que j’ai commises par rapport à ce qui s’est passé :")
        q(20, "Je pense que je ne peux rien changer à ce qui s’est passé :")
        q(21, "Je veux comprendre pourquoi je me sens ainsi à propos de ce que j’ai vécu :")
    })

    form.part(() => {
        form.output(html`<p><i>Lorsque je vis des événements négatifs ou désagréables...</i>`)
        q(22, "Je pense à quelque chose d’agréable plutôt qu’à ce qui s’est passé :")
        q(23, "Je cherche Je pense à la manière de changer la situation :")
        q(24, "Je pense que la situation a aussi des côtés positifs :")
    })

    form.part(() => {
        form.output(html`<p><i>Lorsque je vis des événements négatifs ou désagréables...</i>`)
        q(25, "Je pense que cela ne s’est pas trop mal passé en comparaison :")
        q(26, "Je pense souvent que ce que j’ai vécu est le pire qui puisse arriver à quelqu’un :")
        q(27, "Je pense aux erreurs que les autres ont commises par rapport à ce qui s’est passé :")
    })

    form.part(() => {
        form.output(html`<p><i>Lorsque je vis des événements négatifs ou désagréables...</i>`)
        q(28, "Je pense qu’au fond je suis la cause de ce qui s’est passé :")
        q(29, "Je pense que je dois apprendre à vivre avec ce qui s’est passé :")
        q(30, "Je pense sans cesse aux sentiments que la situation a suscités en moi :")
    })

    form.part(() => {
        form.output(html`<p><i>Lorsque je vis des événements négatifs ou désagréables...</i>`)
        q(31, "Je pense à des expériences agréables :")
        q(32, "Je pense à un plan concernant la meilleure façon de faire :")
        q(33, "Je cherche les aspects positifs de la situation :")
    })

    form.part(() => {
        form.output(html`<p><i>Lorsque je vis des événements négatifs ou désagréables...</i>`)
        q(34, "Je me dis qu’il y a pire dans la vie :")
        q(35, "Je pense continuellement à quel point la situation a été horrible :")
        q(36, "J’ai le sentiment qu’au fond les autres sont la cause de ce qui s’est passé :")
    })

    function q(idx, label) {
        form.enumButtons("q" + idx, label, [
            [1, "Presque jamais"],
            [2, "Parfois"],
            [3, "Souvent"],
            [4, "Régulièrement"],
            [5, "Presque toujours"]
        ])
    }
}

export default build
