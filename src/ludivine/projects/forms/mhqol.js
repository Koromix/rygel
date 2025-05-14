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
        <p>Les questions qui suivent portent sur votre état de santé, telle que vous la <b>ressentez AUJOURD'HUI</b>.
        <p>Ces informations nous permettront de savoir comment vous vous sentez dans votre vie de tous les jours.
    `

    form.part(() => {
        form.enumRadio("q1", "Quelle image avez-vous de vous-même ?", [
            [2, "J'ai une image très positive de moi-même"],
            [1, "J'ai une image positive de moi-même"],
            [-1, "J'ai une image négative de moi-même"],
            [-2, "J'ai une image très négative de moi-même"]
        ], { help: "On parle d'image de soi" })
    })

    form.part(() => {
        form.enumRadio("q2", "Quel est votre niveau d'indépendance ?", [
            [2, "Je suis très satisfait(e) de mon niveau d'indépendance"],
            [1, "Je suis satisfait(e) de mon niveau d'indépendance"],
            [-1, "Je suis insatisfait(e) de mon niveau d'indépendance"],
            [-2, "Je suis très insatisfait(e) de mon niveau d'indépendance"]
        ], { help: "Par exemple : liberté dans vos choix, vos finances, les co-décisions" })
    })

    form.part(() => {
        form.enumRadio("q3", "Comment est votre humeur ?", [
            [2, "Je ne me sens pas anxieux/se, triste ou déprimé(e)"],
            [1, "Je me sens un peu anxieux/se, triste ou déprimé(e)"],
            [-1, "Je me sens anxieux/se, triste ou déprimé(e)"],
            [-2, "Je me sens très anxieux/se, triste ou déprimé(e)"]
        ])
    })

    form.part(() => {
        form.enumRadio("q4", "Êtes-vous satisfait(e) de vos relations ?", [
            [2, "Je suis très satisfait(e) de mes relations"],
            [1, "Je suis satisfait(e) de mes relations"],
            [-1, "Je suis insatisfait(e) de mes relations"],
            [-2, "Je suis très insatisfait(e) de mes relations"]
        ], { help: "Par exemple : partenaire, enfants, famille, amis, collègues" })
    })

    form.part(() => {
        form.enumRadio("q5", "Êtes-vous satisfait(e) de vos activités quotidiennes ?", [
            [2, "Je suis très satisfait(e) de mes activités quotidiennes"],
            [1, "Je suis satisfait(e) de mes activités quotidiennes"],
            [-1, "Je suis insatisfait(e) de mes activités quotidiennes"],
            [-2, "Je suis très insatisfait(e) de mes activités     quotidiennes"]
        ], { help: "Par exemple : travail, études, ménage, loisirs" })
    })

    form.part(() => {
        form.enumRadio("q6", "Comment évaluez-vous votre santé physique ?", [
            [2, "Je n'ai aucun problème de santé physique"],
            [1, "J'ai quelques problèmes de santé physique"],
            [-1, "J'ai plusieurs problèmes de santé physique"],
            [-2, "J'ai beaucoup de problèmes de santé physique"]
        ])
    })

    form.part(() => {
        form.enumRadio("q7", "Comment voyez-vous votre avenir ?", [
            [2, "Je suis très optimiste quant à mon avenir"],
            [1, "Je suis optimiste quant à mon avenir"],
            [-1, "Je suis pessimiste quant à mon avenir"],
            [-2, "Je suis très pessimiste quant à mon avenir"]
        ])
    })

    form.part(() => {
        form.slider("q8", "Comment vous évaluez votre bien-être psychologique ?", {
            min: 0, max: 10,
            prefix: "Pire bien-être psychologique imaginable", suffix: "Meilleur bien-être psychologique imaginable",
            help: "Placez le curseur sur la barre avec la souris ou votre doigt"
        })
    })
}

export default build
