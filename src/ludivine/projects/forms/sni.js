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
    form.values = values

    form.intro = html`
        <p>Ce questionnaire nous permet d'évaluer le <b>nombre de personnes que vous voyez ou à qui vous parlez régulièrement</b> : il peut s'agir de votre famille, d'amis, de collègues de travail, de voisins, etc.
    `

    let incoherent = "Cette réponse ne semble pas cohérente avec la précédédente, veuillez vérifier"

    form.part(() => {
        form.enumRadio("q1", "Lequel des qualificatifs suivants décrit le mieux votre état civil ?", [
            [1, "Actuellement marié(e) et vivant ensemble, ou vivant avec une personne dans une relation de type conjugal"],
            [2, "Jamais marié(e) et jamais vécu avec une personne dans une relation de type conjugal"],
            [3, "Séparé(e)"],
            [4, "Divorcé(e) ou ayant vécu précédemment avec une personne dans une relation de type conjugal"],
            [5, "Veuf/veuve"]
        ])
    })

    form.part(() => {
        form.enumButtons("q2", "Combien d’enfants avez-vous eu au total ?", [0, 1, 2, 3, 4, 5, 6, "7 ou plus"])
        form.enumButtons("q2a", "Combien de vos enfants voyez-vous ou avez-vous au téléphone au moins une fois toutes les deux semaines ?", [0, 1, 2, 3, 4, 5, 6, [7, "7 ou plus"]], { disabled: !values.q2 })

        if (values.q2 != null && values.q2a > values.q2)
            form.error("q2a", incoherent);
    })

    form.part(() => {
        form.enumButtons("q3", "L’un de vos parents est-il en vie ?", [
            [0, "Aucun des deux"],
            [1, "Mère uniquement"],
            [2, "Père uniquement"],
            [3, "Tous les deux"]
        ])
        form.enumButtons("q3a", "Voyez-vous ou parlez-vous au téléphone avec l'un de vos parents au moins une fois toutes les deux semaines ?", [
            [0, "Aucun des deux"],
            [1, "Mère uniquement"],
            [2, "Père uniquement"],
            [3, "Tous les deux"]
        ], { disabled: !values.q3 })

        if ((values.q3 === 1) && (values.q3a == 2 || values.q3a == 3))
            form.error("q3a", incoherent)
        if ((values.q3 === 2) && (values.q3a == 1 || values.q3a == 3))
            form.error("q3a", incoherent)
    })

    form.part(() => {
        form.enumButtons("q4", "L’un de vos beaux-parents (ou les parents de votre partenaire) est-il encore en vie ?", [
            [0, "Aucun des deux"],
            [1, "Mère uniquement"],
            [2, "Père uniquement"],
            [3, "Tous les deux"]
        ])
        form.enumButtons("q4a", "Voyez-vous ou parlez-vous au téléphone avec l'un des parents de votre partenaire au moins une fois toutes les deux semaines ?", [
            [0, "Aucun des deux"],
            [1, "Mère uniquement"],
            [2, "Père uniquement"],
            [3, "Tous les deux"]
        ], { disabled: !values.q4 })

        if ((values.q4 === 1) && (values.q4a == 2 || values.q4a == 3))
            form.error("q4a", incoherent)
        if ((values.q4 === 2) && (values.q4a == 1 || values.q4a == 3))
            form.error("q4a", incoherent)
    })

    form.part(() => {
        form.enumButtons("q5", "De combien d'autres membres de votre famille vous sentez-vous proche ?", [0, 1, 2, 3, 4, 5, 6, [7, "7 ou plus"]], {
            help: "Autres que votre conjoint, vos parents et vos enfants !"
        })
        form.enumButtons("q5a", "Combien de ces membres de votre famille voyez-vous ou avez-vous au téléphone au moins une fois toutes les deux semaines ?", [0, 1, 2, 3, 4, 5, 6, [7, "7 ou plus"]], { disabled: !values.q5 })

        if (values.q5 != null && values.q5a > values.q5)
            form.error("q5a", incoherent)
    })

    form.part(() => {
        form.enumButtons("q6", "Combien d’amis proches avez-vous ?", [0, 1, 2, 3, 4, 5, 6, [7, "7 ou plus"]], {
            help: "Cest-à-dire des personnes avec lesquelles vous vous sentez à l'aise, à qui vous pouvez parler de sujets privés et à qui vous pouvez demander de l'aide"
        })
        form.enumButtons("q6a", "Combien de ces amis voyez-vous ou avec combien parlez-vous au moins une fois toutes les deux semaines ?", [0, 1, 2, 3, 4, 5, 6, [7, "7 ou plus"]], { disabled: !values.q6 })

        if (values.q6 != null && values.q6a > values.q6)
            form.error("q6a", incoherent)
    })

    form.part(() => {
        form.binary("q7", "Appartenez-vous à une église, un temple ou à un autre groupe religieux ?")
        form.enumButtons("q7a", "Avec combien de membres de votre église ou groupe religieux parlez-vous au moins une fois toutes les deux semaines ?", [0, 1, 2, 3, 4, 5, 6, [7, "7 ou plus"]], {
            help: "Cela inclus les réunions de groupe et les services",
            disabled: !values.q7
        })
    })

    form.part(() => {
        form.binary("q8", "Suivez-vous régulièrement des cours ?", {
            help: "École, université, formation technique ou formation pour adultes"
        })
        form.enumButtons("q8a", "Avec combien de camarades de classe ou d'enseignants discutez-vous au moins une fois toutes les deux semaines ?", [0, 1, 2, 3, 4, 5, 6, [7, "7 ou plus"]], {
            help: "Cela inclus les réunions de classe",
            disabled: !values.q8
        })
    })

    form.part(() => {
        form.enumButtons("q9", "Avez-vous un emploi à temps plein ou à temps partiel ?", [
            [0, "Non"],
            [1, "Oui, en tant qu'indépendant"],
            [2, "Oui, en tant qu'employé(e)"]
        ])
        form.binary("q9a", "Avez-vous une activité d’encadrement (collaborateurs) ?", { disabled: !values.q9 })
        if (values.q9a == 1)
            form.enumButtons("q9b", "Combien de collaborateurs encadrez-vous ?", [1, 2, 3, 4, 5, 6, [7, "7 ou plus"]], { disabled: !values.q9 })
        form.enumButtons("q9c", "Avec combien de personnes au travail (autres que celles que vous encadrez) parlez-vous au moins une fois toutes les deux semaines ?", [0, 1, 2, 3, 4, 5, 6, [7, "7 ou plus"]], { disabled: !values.q9 })
    })

    form.part(() => {
        form.enumButtons("q10", "Combien de vos voisins voyez-vous ou appelez-vous au moins une fois toutes les deux semaines ?", [0, 1, 2, 3, 4, 5, 6, [7, "7 ou plus"]])
    })

    form.part(() => {
        form.binary("q11", "Êtes-vous actuellement engagé(e) dans un travail bénévole régulier ?")
        form.enumButtons("q11a", "Avec combien de personnes impliquées dans ce travail bénévole parlez-vous de questions liées au bénévolat au moins une fois toutes les deux semaines ?", [0, 1, 2, 3, 4, 5, 6, [7, "7 ou plus"]], { disabled: !values.q11 })
    })

    form.part(() => {
        form.binary("q12", "Faites-vous partie d’un groupe au sein duquel vous échangez avec un ou plusieurs membres sur des sujets liés au groupe au moins une fois toutes les deux semaines ?", {
            help: "Il peut s'agir par exemple de clubs sociaux, de groupes de loisirs, de syndicats, de groupes commerciaux, d'organisations professionnelles, de groupes s'occupant d'enfants comme l'association des parents d'élèves ou les scouts, de groupes s'occupant de travaux d'intérêt général, etc."
        })
    })

    if (values.q12 !== 0) {
        form.part(() => {
            form.output(html`
                <p>Pensez aux groupes dans lesquels vous parlez à un autre membre du groupe au moins une fois toutes les deux semaines.
                <p>Veuillez fournir les <b>informations suivantes pour chacun de ces groupes</b> : le nom ou le type de groupe et le nombre total de membres de ce groupe avec lesquels vous parlez au moins une fois toutes les deux semaines.
            `)

            let end = ([1, 2, 3, 4, 5].findLast(i => values["q12_" + i + "a"]) ?? 0) + 1

            for (let i = 1; i <= end; i++) {
                form.text("q12_" + i + "a", i < 2 ? "Nom/type de groupe" : null)
                form.enumButtons("q12_" + i + "b", i < 2 ? "Nombre de membres avec lesquels vous parlez au moins une fois toutes les deux semaines" : null, [0, 1, 2, 3, 4, 5, 6, [7, "7 ou plus"]])
            }
        })
    }
}

export default build
