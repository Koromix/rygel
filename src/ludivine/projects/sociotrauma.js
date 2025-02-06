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

import * as adnm20 from './forms/adnm20.js';
import * as cses from './forms/cses.js';
import * as ctqsf from './forms/ctqsf.js';
import * as gad7 from './forms/gad7.js';
import * as isi from './forms/isi.js';
import * as isrc from './forms/isrc.js';
import * as lec5 from './forms/lec5.js';
import * as mhqol from './forms/mhqol.js';
import * as pcl5 from './forms/pcl5.js';
import * as pdeq from './forms/pdeq.js';
import * as phq9 from './forms/phq9.js';
import * as profil from './forms/profil.js';
import * as ptci from './forms/ptci.js';
import * as rds from './forms/rds.js';
import * as sni from './forms/sni.js';
import * as sps10 from './forms/sps10.js';
import * as ssi from './forms/ssi.js';
import * as ssq6 from './forms/ssq6.js';
import * as substances from './forms/substances.js';

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
            <div class="help">
                <img src=${ASSETS['pictures/help1']} alt="" />
                <div>
                    <p>Sélectionnez la <b>prochaine étape de l’étude</b> pour commencer à remplir le questionnaire !
                </div>
            </div>
        `;

        build.module('profil', 'Profil', mod => {
            build.form('profil', 'Profil', profil);
        });

        build.module('initial', 'Bilan initial', mod => {
            mod.level = 'Module';
            mod.help = html`
                <div class="help right">
                    <div>
                        <p>Nous vous conseillons de répondre aux différents questionnaires <b>dans l’ordre</b> ci-dessus.
                        <p>Commencez par votre <b>entourage</b> puis abordez le module sur les <b>évènements</b>.
                    </div>
                    <img src=${ASSETS['pictures/help2']} alt="" />
                </div>
            `;

            let options = { schedule: start };

            build.module('entourage', 'Entourage', () => {
                build.form('ssq6', 'Soutien social', ssq6, options)
                build.form('sps10', 'Provisions sociales', sps10, options)
                build.form('sni', 'Réseau proche', sni, options)
            });

            build.module('evenement', 'Évènement', () => {
                build.form('lec5', 'Situations stressantes', lec5, options)
                build.form('isrc', 'Pensées et ressenti', isrc, options)
                build.form('pdeq', 'Réactions pendant l\'évènement', pdeq, options)
                build.form('adnm20', 'Évènements de vie', adnm20, options)
            });

            build.module('qualite', 'Qualité de vie', () => {
                build.form('mhqol', 'Qualité de vie', mhqol, options)
            });

            build.module('divulgation', 'Divulgation', () => {
                build.form('cses', 'Communication à l\'entourage', cses, options)
                build.form('rds', 'Réactions des proches', rds, options)
            });
        });

        build.module('s6', '6 semaines', mod => {
            mod.level = 'Module';
            mod.help = html`
                <div class="help right">
                    <div>
                        <p>Nous vous conseillons de répondre aux différents questionnaires <b>dans l’ordre</b> ci-dessus.
                        <p>Commencez par votre <b>entourage</b> puis abordez le module sur les <b>évènements</b>.
                    </div>
                    <img src=${ASSETS['pictures/help2']} alt="" />
                </div>
            `;

            let options = { schedule: start.plus(6 * 7) };

            build.module('entourage', 'Entourage', () => {
                build.form('ssq6', 'Soutien social', ssq6, options)
                build.form('sps10', 'Provisions sociales', sps10, options)
                build.form('sni', 'Réseau proche', sni, options)
            });

            build.module('evenement', 'Évènement', () => {
                build.form('pcl5', 'Problèmes liés à l\'évènement', pcl5, options)
                build.form('adnm20', 'Évènements de vie', adnm20, options)
                build.form('ctqsf', 'Enfance', ctqsf, options)
                build.form('ptci', 'Pensées suite à l\'évènement', ptci, options)
            });

            build.module('sante', 'Santé mentale', () => {
                build.form('phq9', 'Manifestations dépressives', phq9, options)
                build.form('gad7', 'Difficultés anxieuses', gad7, options)
                build.form('ssi', 'Idées suicidaires', ssi, options)
                build.form('substances', 'Consommations', substances, options)
                build.form('isi', 'Qualité du sommeil', isi, options)
            });

            build.module('qualite', 'Qualité de vie', () => {
                build.form('mhqol', 'Qualité de vie', mhqol, options)
            });

            build.module('divulgation', 'Divulgation', () => {
                build.form('cses', 'Communication à l\'entourage', cses, options)
                build.form('rds', 'Réactions des proches', rds, options)
            });
        });

        build.module('m3', '3 mois', mod => {
            mod.level = 'Module';
            mod.help = html`
                <div class="help right">
                    <div>
                        <p>Nous vous conseillons de répondre aux différents questionnaires <b>dans l’ordre</b> ci-dessus.
                        <p>Commencez par votre <b>entourage</b> puis abordez le module sur les <b>évènements</b>.
                    </div>
                    <img src=${ASSETS['pictures/help2']} alt="" />
                </div>
            `;

            let options = { schedule: start.plusMonths(3) };

            build.module('entourage', 'Entourage', () => {
                build.form('ssq6', 'Soutien social', ssq6, options)
                build.form('sps10', 'Provisions sociales', sps10, options)
                build.form('sni', 'Réseau proche', sni, options)
            });

            build.module('evenement', 'Évènement', () => {
                build.form('pcl5', 'Problèmes liés à l\'évènement', pcl5, options)
                build.form('adnm20', 'Évènements de vie', adnm20, options)
            });

            build.module('sante', 'Santé mentale', () => {
                build.form('phq9', 'Manifestations dépressives', phq9, options)
                build.form('gad7', 'Difficultés anxieuses', gad7, options)
                build.form('ssi', 'Idées suicidaires', ssi, options)
                build.form('substances', 'Consommations', substances, options)
                build.form('isi', 'Qualité du sommeil', isi, options)
            });

            build.module('qualite', 'Qualité de vie', () => {
                build.form('mhqol', 'Qualité de vie', mhqol, options)
            });

            build.module('divulgation', 'Divulgation', () => {
                build.form('cses', 'Communication à l\'entourage', mhqol, options)
                build.form('rds', 'Réactions des proches', rds, options)
            });
        });

        build.module('m6', '6 mois', mod => {
            mod.level = 'Module';
            mod.help = html`
                <div class="help right">
                    <div>
                        <p>Nous vous conseillons de répondre aux différents questionnaires <b>dans l’ordre</b> ci-dessus.
                        <p>Commencez par votre <b>entourage</b> puis abordez le module sur les <b>évènements</b>.
                    </div>
                    <img src=${ASSETS['pictures/help2']} alt="" />
                </div>
            `;

            let options = { schedule: start.plusMonths(6) };

            build.module('entourage', 'Entourage', () => {
                build.form('ssq6', 'Soutien social', ssq6, options)
                build.form('sps10', 'Provisions sociales', sps10, options)
                build.form('sni', 'Réseau proche', sni, options)
            });

            build.module('evenement', 'Évènement', () => {
                build.form('pcl5', 'Problèmes liés à l\'évènement', pcl5, options)
                build.form('adnm20', 'Évènements de vie', adnm20, options)
            });

            build.module('sante', 'Santé mentale', () => {
                build.form('phq9', 'Manifestations dépressives', phq9, options)
                build.form('gad7', 'Difficultés anxieuses', gad7, options)
                build.form('ssi', 'Idées suicidaires', ssi, options)
                build.form('substances', 'Consommations', substances, options)
                build.form('isi', 'Qualité du sommeil', isi, options)
            });

            build.module('qualite', 'Qualité de vie', () => {
                build.form('mhqol', 'Qualité de vie', mhqol, options)
            });

            build.module('divulgation', 'Divulgation', () => {
                build.form('cses', 'Communication à l\'entourage', cses, options)
                build.form('rds', 'Réactions des proches', rds, options)
            });
        });

        build.module('m12', '1 an', mod => {
            mod.level = 'Module';
            mod.help = html`
                <div class="help right">
                    <div>
                        <p>Nous vous conseillons de répondre aux différents questionnaires <b>dans l’ordre</b> ci-dessus, en commençant par votre <b>entourage</b> puis en abordant le module sur les <b>évènements</b>.
                    </div>
                    <img src=${ASSETS['pictures/help2']} alt="" />
                </div>
            `;

            let options = { schedule: start.plusMonths(12) };

            build.module('entourage', 'Entourage', () => {
                build.form('ssq6', 'Soutien social', ssq6, options)
                build.form('sps10', 'Provisions sociales', sps10, options)
                build.form('sni', 'Réseau proche', sni, options)
            });

            build.module('evenement', 'Évènement', () => {
                build.form('pcl5', 'Problèmes liés à l\'évènement', pcl5, options)
                build.form('adnm20', 'Évènements de vie', adnm20, options)
            });

            build.module('sante', 'Santé mentale', () => {
                build.form('phq9', 'Manifestations dépressives', phq9, options)
                build.form('gad7', 'Difficultés anxieuses', gad7, options)
                build.form('ssi', 'Idées suicidaires', ssi, options)
                build.form('substances', 'Consommations', substances, options)
                build.form('isi', 'Qualité du sommeil', isi, options)
            });

            build.module('qualite', 'Qualité de vie', () => {
                build.form('mhqol', 'Qualité de vie', mhqol, options)
            });

            build.module('divulgation', 'Divulgation', () => {
                build.form('cses', 'Communication à l\'entourage', cses, options)
                build.form('rds', 'Réactions des proches', rds, options)
            });
        });
    });
}

export {
    consent,
    init
}
