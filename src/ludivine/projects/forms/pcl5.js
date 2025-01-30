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
    <p>Ce questionnaire est destiné à rechercher les <b>expériences de dissociation</b> que vous auriez pu ressentir pendant l'événement traumatique, et au cours des quelques heures suivantes.
    <p>Veuillez répondre aux énoncés suivants en cochant le choix de réponse qui décrit le mieux vos expériences et réactions durant l’événement et immédiatement après. Si une question ne s’applique pas à votre expérience, <u>cochez « Pas du tout vrai »</u>.
    <p>Considérez uniquement les expériences <u>au cours du dernier mois</u> !
`;

function run(form, values) {
    let choices = [
        [0, "Pas du tout"],
        [1, "Un peu"],
        [2, "Modérément"],
        [3, "Beaucoup"],
        [4, "Extrêmement"]
    ]

    form.section(() => {
        form.enum("*pcl1", "1. Des souvenirs répétés, pénibles et involontaires de l’expérience stressante ?", choices)
        form.enum("*pcl2", "2. Des rêves répétés et pénibles de l’expérience stressante ?", choices)
    })

    form.section(() => {
        form.enum("*pcl3", "3. Se sentir ou agir soudainement comme si vous viviez à nouveau l’expérience stressante ?", choices)
        form.enum("*pcl4", "4. Se sentir mal quand quelque chose vous rappelle l’événement ?", choices)
    })

    form.section(() => {
        form.enum("*pcl5", "5. Avoir de fortes réactions physiques lorsque quelque chose vous rappelle l’événement (accélération cardiaque, difficulté respiratoire, sudation) ?", choices)
        form.enum("*pcl6", "6. Essayer d’éviter les souvenirs, pensées, et sentiments liés à l’événement ?", choices)
    })

    form.section(() => {
        form.enum("*pcl7", "7. Essayer d’éviter les personnes et les choses qui vous rappellent l’expérience stressante (lieux, personnes, activités, objets) ? ", choices)
        form.enum("*pcl8", "8. Des difficultés à vous rappeler des parties importantes de l’événement ?", choices)
    })

    form.section(() => {
        form.enum("*pcl9", "9. Des croyances négatives sur vous-même, les autres, le monde (des croyances comme : je suis mauvais, j’ai quelque chose qui cloche, je ne peux avoir confiance en personne, le monde est dangereux) ?", choices)
        form.enum("*pcl10", "10. Vous blâmer ou blâmer quelqu’un d’autre pour l’événement ou ce qui s’est produit ensuite ?", choices)
    })

    form.section(() => {
        form.enum("*pcl11", "11. Avoir des sentiments négatifs intenses tels que peur, horreur, colère, culpabilité, ou honte ?", choices)      
        form.enum("*pcl12", "12. Perdre de l’intérêt pour des activités que vous aimiez auparavant ?", choices)  
    })

    form.section(() => {
        form.enum("*pcl13", "13. Vous sentir distant ou coupé des autres ?", choices)   
        form.enum("*pcl14", "14. Avoir du mal à éprouver des sentiments positifs (par exemple être incapable de ressentir de la joie ou de l’amour envers vos proches) ?", choices)  
    })

    form.section(() => {
        form.enum("*pcl15", "15. Comportement irritable, explosions de colère, ou agir agressivement ?", choices)  
        form.enum("*pcl16", "16. Prendre des risques inconsidérés ou encore avoir des conduites qui pourraient vous mettre en danger ?", choices)
    })

    form.section(() => {
        form.enum("*pcl17", "17. Être en état de « super-alerte », hyper vigilant ou sur vos gardes ?", choices)   
        form.enum("*pcl18", "18. Sursauter facilement ?", choices)  
    })

    form.section(() => {
        form.enum("*pcl19", "19. Avoir du mal à vous concentrer ?", choices)  
        form.enum("*pcl20", "20. Avoir du mal à trouver le sommeil ou à rester endormi ?", choices)  
    })
}

export {
    intro,
    run
}
