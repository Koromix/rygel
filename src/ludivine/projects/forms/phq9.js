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
    <p>Au cours des <u>deux dernières semaines</u>, à quelle fréquence avez-vous été dérangé(e) par les problèmes suivants ?
`;

function run(form, values) {
    let choices = [
        [0, "Jamais"],
        [1, "Plusieurs jours"],
        [2, "Plus de sept jours"],
        [3, "Presque tous les jours"]
    ]

    form.block(() => {
        form.enum("*q1", "Peu d’intérêt ou de plaisir à faire les choses", choices)
        form.enum("*q2", "Être triste, déprimé(e) ou désespéré(e)", choices)
    })

    form.block(() => {
        form.enum("*q3", "Difficultés à s’endormir ou à rester endormi(e), ou dormir trop", choices)
        form.enum("*q4", "Se sentir fatigué(e) ou manquer d’énergie", choices)
    })

    form.block(() => {
        form.enum("*q5", "Avoir peu d’appétit ou manger trop", choices)
        form.enum("*q6", "Avoir une mauvaise opinion de soi-même, ou avoir le sentiment d’être nul(le), ou d’avoir déçu sa famille ou s’être déçu(e) soi-même", choices)
    })

    form.block(() => {
        form.enum("*q7", "Avoir du mal à se concentrer, par exemple, pour lire le journal ou regarder la télévision", choices)
        form.enum("*q8", "Bouger ou parler si lentement que les autres auraient pu le remarquer. Ou au contraire, être si agité(e) que vous avez eu du mal à tenir en place par rapport à d’habitude", choices)
    })

    form.block(() => {
        form.enum("*q9", "Penser qu’il vaudrait mieux mourir ou envisager de vous faire du mal d’une manière ou d’une autre", choices)
    })

    form.block(() => {
        let any = Array.from(Array(9).keys()).reduce((acc, idx) => {
            if (acc)
                return true
            if (values["q" + idx] !== 0)
                return true

            return false
        }, false)

        form.enumRadio("q10", "Dans quelle mesure ce(s) problème(s) a-t-il (ont-ils) rendu difficile(s) votre travail, vos tâches à la maison ou votre capacité à bien vous entendre avec les autres ?", [
            [1, "Pas du tout difficile(s)"],
            [2, "Plutôt difficile(s)"],
            [3, "Très difficile(s)"],
            [4, "Extrêmement difficile(s)"]
        ], { disabled: !any })
    })
}

export {
    intro,
    run
}
