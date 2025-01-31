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

import { html } from '../../../vendor/lit-html/lit-html.bundle.js';
import { ASSETS } from '../assets/assets.js';

const consent = html`
    <p>Dans le cadre de cette étude nous allons vous questionner sur votre situation actuelle, vos relations avec vos proches et la société et votre bien-être psychologique.  Il vous sera également demandé quelques informations sur l’évènement difficile que vous avez vécu. Nous vous inviterons à construire votre sociogramme qui est une représentation visuelle des liens sociaux que vous entretenez avec autrui. Il s’agit d’un dessin graphique qui représente la structure de vos relations interpersonnelles.
    <p>Vous serez soutenus dans la réalisation de chacune de ces tâches : toutes les consignes de participation seront présentées à chaque étape.
    <p>Cette étude se déroule en 6 étapes. Nous vous proposerons de réaliser plusieurs fois les mêmes exercices, à quelques exceptions près. Vous recevrez, pour ce faire, des rappels par mail et/ou notification via l’application. Un calendrier est disponible sur votre tableau de bord. 
    <p>Cette étude se déroule entièrement en ligne. Vous n’aurez pas de contact direct avec les autres participants et avec les responsables de l’étude. Vous pouvez néanmoins, de votre initiative, entrer en contact par mail avec la responsable de cette étude pour toute question: Wivine Blekic, sociotrauma@ldv-recherche.fr.
    <p>Nous vous invitons à prendre connaissance de la lettre d’information de l’étude SocioTrauma. Celle-ci a pour objectif de répondre aux questions que vous pourriez vous poser avant de prendre la décision de participer à la recherche. En bas de page, nous vous demanderons d’indiquer que vous avez pris connaissance de la lettre d’information.
`;

function init(build, start) {
    build.summary = html`
        <p>Retrouvez des <a href=${ENV.urls.static + '/etudes#etude-1-sociotrauma'}>informations sur cette étude</a> sur le site officiel de Ligne de Vie.
        <p>Commencez par faire votre bilan initial. Vous pouvez <b>arrêter à tout moment</b> et recommencer plus tard !
    `;

    build.module('recueil', 'Recueil', mod => {
        mod.level = 'Temporalité';
        mod.help = html`
            <img src=${ASSETS['pictures/help']} alt="" />
            <div>
                <p>Sélectionnez la <b>prochaine étape de l’étude</b> pour commencer à remplir le questionnaire !
            </div>
        `;

        build.module('initial', 'Bilan initial', mod => {
            mod.level = 'Module';
            mod.help = html`
                <img src=${ASSETS['pictures/help']} alt="" />
                <div>
                    <p>Nous vous conseillons de répondre aux différents questionnaires <b>dans l’ordre</b> ci-dessus, en commençant par votre <b>entourage</b> puis en abordant le module sur les <b>évènements</b>.
                </div>
            `;

            let options = { schedule: start };

            build.module('entourage', 'Entourage', () => {
                build.test('ssq6', 'Soutien social', options)
                build.test('sps10', 'Provisions sociales', options)
                build.test('sni', 'Réseau proche', options)
            });

            build.module('evenement', 'Évènement', () => {
                build.test('lec5', 'Situations stressantes', options)
                build.test('isrc', 'Pensées et ressenti', options)
                build.test('pdeq', 'Réactions pendant l\'évènement', options)
                build.test('adnm20', 'Évènements de vie', options)
            });

            build.module('qualite', 'Qualité de vie', () => {
                build.test('mhqol', 'Qualité de vie', options)
            });

            build.module('divulgation', 'Divulgation', () => {
                build.test('cses', 'Communication à l\'entourage', options)
                build.test('rds', 'Réactions des proches', options)
            });
        });

        build.module('s6', '6 semaines', mod => {
            mod.level = 'Module';
            mod.help = html`
                <img src=${ASSETS['pictures/help']} alt="" />
                <div>
                    <p>Nous vous conseillons de répondre aux différents questionnaires <b>dans l’ordre</b> ci-dessus, en commençant par votre <b>entourage</b> puis en abordant le module sur les <b>évènements</b>.
                </div>
            `;

            let options = { schedule: start.plus(6 * 7) };

            build.module('entourage', 'Entourage', () => {
                build.test('ssq6', 'Soutien social', options)
                build.test('sps10', 'Provisions sociales', options)
                build.test('sni', 'Réseau proche', options)
            });

            build.module('evenement', 'Évènement', () => {
                build.test('pcl5', 'Problèmes liés à l\'évènement', options)
                build.test('adnm20', 'Évènements de vie', options)
                build.test('ctqsf', 'Enfance', options)
                build.test('ptci', 'Pensées suite à l\'évènement', options)
            });

            build.module('sante', 'Santé mentale', () => {
                build.test('phq9', 'Manifestations dépressives', options)
                build.test('gad7', 'Difficultés anxieuses', options)
                build.test('ssi', 'Idées suicidaires', options)
                build.test('substances', 'Consommations', options)
                build.test('isi', 'Qualité du sommeil', options)
            });

            build.module('qualite', 'Qualité de vie', () => {
                build.test('mhqol', 'Qualité de vie', options)
            });

            build.module('divulgation', 'Divulgation', () => {
                build.test('cses', 'Communication à l\'entourage', options)
                build.test('rds', 'Réactions des proches', options)
            });
        });

        build.module('m3', '3 mois', mod => {
            mod.level = 'Module';
            mod.help = html`
                <img src=${ASSETS['pictures/help']} alt="" />
                <div>
                    <p>Nous vous conseillons de répondre aux différents questionnaires <b>dans l’ordre</b> ci-dessus, en commençant par votre <b>entourage</b> puis en abordant le module sur les <b>évènements</b>.
                </div>
            `;

            let options = { schedule: start.plusMonths(3) };

            build.module('entourage', 'Entourage', () => {
                build.test('ssq6', 'Soutien social', options)
                build.test('sps10', 'Provisions sociales', options)
                build.test('sni', 'Réseau proche', options)
            });

            build.module('evenement', 'Évènement', () => {
                build.test('pcl5', 'Problèmes liés à l\'évènement', options)
                build.test('adnm20', 'Évènements de vie', options)
            });

            build.module('sante', 'Santé mentale', () => {
                build.test('phq9', 'Manifestations dépressives', options)
                build.test('gad7', 'Difficultés anxieuses', options)
                build.test('ssi', 'Idées suicidaires', options)
                build.test('substances', 'Consommations', options)
                build.test('isi', 'Qualité du sommeil', options)
            });

            build.module('qualite', 'Qualité de vie', () => {
                build.test('mhqol', 'Qualité de vie', options)
            });

            build.module('divulgation', 'Divulgation', () => {
                build.test('cses', 'Communication à l\'entourage', options)
                build.test('rds', 'Réactions des proches', options)
            });
        });

        build.module('m6', '6 mois', mod => {
            mod.level = 'Module';
            mod.help = html`
                <img src=${ASSETS['pictures/help']} alt="" />
                <div>
                    <p>Nous vous conseillons de répondre aux différents questionnaires <b>dans l’ordre</b> ci-dessus, en commençant par votre <b>entourage</b> puis en abordant le module sur les <b>évènements</b>.
                </div>
            `;

            let options = { schedule: start.plusMonths(6) };

            build.module('entourage', 'Entourage', () => {
                build.test('ssq6', 'Soutien social', options)
                build.test('sps10', 'Provisions sociales', options)
                build.test('sni', 'Réseau proche', options)
            });

            build.module('evenement', 'Évènement', () => {
                build.test('pcl5', 'Problèmes liés à l\'évènement', options)
                build.test('adnm20', 'Évènements de vie', options)
            });

            build.module('sante', 'Santé mentale', () => {
                build.test('phq9', 'Manifestations dépressives', options)
                build.test('gad7', 'Difficultés anxieuses', options)
                build.test('ssi', 'Idées suicidaires', options)
                build.test('substances', 'Consommations', options)
                build.test('isi', 'Qualité du sommeil', options)
            });

            build.module('qualite', 'Qualité de vie', () => {
                build.test('mhqol', 'Qualité de vie', options)
            });

            build.module('divulgation', 'Divulgation', () => {
                build.test('cses', 'Communication à l\'entourage', options)
                build.test('rds', 'Réactions des proches', options)
            });
        });

        build.module('m12', '1 an', mod => {
            mod.level = 'Module';
            mod.help = html`
                <img src=${ASSETS['pictures/help']} alt="" />
                <div>
                    <p>Nous vous conseillons de répondre aux différents questionnaires <b>dans l’ordre</b> ci-dessus, en commençant par votre <b>entourage</b> puis en abordant le module sur les <b>évènements</b>.
                </div>
            `;

            let options = { schedule: start.plusMonths(12) };

            build.module('entourage', 'Entourage', () => {
                build.test('ssq6', 'Soutien social', options)
                build.test('sps10', 'Provisions sociales', options)
                build.test('sni', 'Réseau proche', options)
            });

            build.module('evenement', 'Évènement', () => {
                build.test('pcl5', 'Problèmes liés à l\'évènement', options)
                build.test('adnm20', 'Évènements de vie', options)
            });

            build.module('sante', 'Santé mentale', () => {
                build.test('phq9', 'Manifestations dépressives', options)
                build.test('gad7', 'Difficultés anxieuses', options)
                build.test('ssi', 'Idées suicidaires', options)
                build.test('substances', 'Consommations', options)
                build.test('isi', 'Qualité du sommeil', options)
            });

            build.module('qualite', 'Qualité de vie', () => {
                build.test('mhqol', 'Qualité de vie', options)
            });

            build.module('divulgation', 'Divulgation', () => {
                build.test('cses', 'Communication à l\'entourage', options)
                build.test('rds', 'Réactions des proches', options)
            });
        });
    });
}

export {
    consent,
    init
}
