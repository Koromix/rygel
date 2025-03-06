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

import adnm20 from './forms/adnm20.js';
import cses from './forms/cses.js';
import ctqsf from './forms/ctqsf.js';
import evenement from './forms/evenement.js';
import gad7 from './forms/gad7.js';
import isi from './forms/isi.js';
import isrc from './forms/isrc.js';
import lec5 from './forms/lec5.js';
import mhqol from './forms/mhqol.js';
import pcl5 from './forms/pcl5.js';
import pdeq from './forms/pdeq.js';
import phq9 from './forms/phq9.js';
import positif from './forms/positif.js';
import sociodemo from './forms/sociodemo.js';
import ptci from './forms/ptci.js';
import rds from './forms/rds.js';
import sni from './forms/sni.js';
import sps10 from './forms/sps10.js';
import ssi from './forms/ssi.js';
import ssq6 from './forms/ssq6.js';
import substances from './forms/substances.js';

const consent = html`
    <p>Dans cette étude, des questions vous seront posées sur votre <b>situation actuelle, vos relations avec vos proches</b> et la société, ainsi que sur votre bien-être psychologique. Il sera également demandé de fournir quelques informations sur l’évènement difficile que vous avez vécu.
    <p>Une étape clé de cette étude consiste à construire un sociogramme, une <b>représentation visuelle des liens sociaux</b> que vous entretenez avec les personnes qui vous entourent. Ce graphique met en lumière la structure de vos relations interpersonnelles.
    <p>Des consignes détaillées guideront chacune des <b>6 étapes de l’étude</b>, avec des exercices à réaliser à plusieurs reprises. Des rappels par e-mail et/ou notifications via l’application vous aideront à respecter les échéances. Un calendrier est également disponible depuis votre tableau de bord.
    <p>L’étude se déroule <b>entièrement en ligne</b>, sans contact direct avec les autres participants ou les responsables de l’étude. Toutefois, l’investigatrice principale peut être contactée par e-mail pour toute question: Wivine Blekic, <a href="mailto:sociotrauma@ldv-recherche.fr">sociotrauma@ldv-recherche.fr</a>.
    <p>Nous vous invitons à <b>prendre connaissance de la lettre d’information</b> de l’étude SocioTrauma. Celle-ci répond aux exigences de la recherche sur la personne humaine et a pour objectif de répondre aux questions que vous pourriez vous poser avant de prendre la décision de participer à la recherche.

    <div class="actions">
        <a href=${ENV.urls.static + '/static/documents/SocioTrauma_Information.pdf'} download>Télécharger la lettre d'information</a>
    </div>
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
                    <p>La première étape consiste à <b>vous présenter puis à faire un bilan initial</b>.
                    <p>Vous pourrez ensuite continuer <b>selon le calendrier prévu</b>. À tout moment, vous pouvez arrêter l'étude ou la mettre en pause et y revenir plus tard. Vous avez également la possibilité de consulter les pages « Ressources » ou « Se détendre ».
                    <p>Vous êtes libre de compléter les questionnaires dans l’ordre qui vous convient. Toutefois, nous vous recommandons de <b>suivre l’ordre proposé</b>. Les transitions ont été pensées avec soin par notre équipe de recherche de Lignes de Vie afin d’accompagner votre cheminement de la manière la plus confortable possible.
                    <p>Nous vous invitons à <b>répondre à toutes les questions</b>, même si vous n'êtes pas certain de votre réponse. Il n’y a pas de bonne ou de mauvaise réponse, et même incertaines, elles sont importantes. Dans Lignes de Vie, on se donne le droit à l’erreur, à la bêtise, à l’ignorance… Et pour les chercheurs aussi !
                    <p>Dès lors, si une question <b>ne correspond pas à votre vécu</b> ou si vous souhaitez ajouter un commentaire, validez la page sans y répondre et apposez votre commentaire dans le champ qui apparait.
                </div>
            </div>
        `;

        build.module('sociodemo', 'Présentation', mod => {
            build.form('sociodemo', 'Présentation', sociodemo);
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

            build.module('entourage', 'Entourage', mod => {
                mod.help = html`
                    <div class="help left">
                        <img src=${ASSETS['pictures/help1']} alt="" />
                        <div>
                            <p>Dans ce module, nous visons à comprendre votre <b>environnement social</b> : À qui parlez-vous de manière régulière ? Qui est là pour vous ? Êtes-vous satisfaits du soutien que vous recevez ?
                            <p>À la fin de ce module, nous vous demanderons de décrire votre entourage précisément à l'aide d'un <abbr title="Outil permettant de représenter visuellement vos relations sociales">sociogramme</abbr> interactif.
                        </div>
                    </div>
                `;

                build.form('ssq6', 'Soutien social', ssq6, options)
                build.form('sni', 'Interactions sociales', sni, options)
                build.form('sps10', 'Disponibilité de votre entourage', sps10, options)
                build.network('network', 'Sociogramme', options)
            });

            build.module('evenement', 'Évènement', mod => {
                mod.help = html`
                    <div class="help left">
                        <img src=${ASSETS['pictures/help1']} alt="" />
                        <div>
                            <p>Dans ce module, nous vous poserons des questions sur l’<b>événement qui vous a conduit à participer à cette étude</b>, ainsi que sur tout autre événement, qu’il soit positif ou négatif, qui fait actuellement partie de votre réalité.
                            <p>N'hésitez pas à interrompre l'application ou à cliquer sur le <b>bouton « SOS »</b> si cela s’avère nécessaire pour vous. Vous pouvez également arrêter à tout moment et revenir plus tard.
                        </div>
                    </div>
                `

                build.form('evenement', 'L’évènement qui vous a amené ici', evenement, options)
                build.form('pensees', 'Pensées et ressentis', (form, values) => {
                    pdeq(form, values)
                    isrc(form, values)
                }, options)
                build.form('autres', 'Autres évènements de vie', (form, values) => {
                    adnm20(form, values)
                    lec5(form, values)
                    positif(form, values)
                }, options)
            });

            build.module('qualite', 'Qualité de vie', () => {
                build.form('mhqol', 'Qualité de vie', mhqol, options)
            });

            build.module('divulgation', 'Communication', mod => {
                mod.help = html`
                    <div class="help left">
                        <img src=${ASSETS['pictures/help1']} alt="" />
                        <div>
                            <p>Dans ce module, nous allons nous intéresser à la façon dont vous <b>souhaitez – ou non – parler de votre expérience</b> à vos proches, ainsi qu’à leurs réactions (si vous leur en avez parlé).
                        </div>
                    </div>
                `

                build.form('cses', 'Témoignage à l\'entourage', cses, options)
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
                build.form('sni', 'Interactions sociales', sni, options)
                build.form('sps10', 'Disponibilité de votre entourage', sps10, options)
                build.network('network', 'Sociogramme', options)
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
                build.form('sni', 'Interactions sociales', sni, options)
                build.form('sps10', 'Disponibilité de votre entourage', sps10, options)
                build.network('network', 'Sociogramme', options)
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
                build.form('sni', 'Interactions sociales', sni, options)
                build.form('sps10', 'Disponibilité de votre entourage', sps10, options)
                build.network('network', 'Sociogramme', options)
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
                build.form('sni', 'Interactions sociales', sni, options)
                build.form('sps10', 'Disponibilité de votre entourage', sps10, options)
                build.network('network', 'Sociogramme', options)
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
