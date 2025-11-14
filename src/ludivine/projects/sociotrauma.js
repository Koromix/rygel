// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import { html } from 'vendor/lit-html/lit-html.bundle.js';
import { PERSON_KINDS } from '../client/network/constants.js';
import { ASSETS } from '../assets/assets.js';

import adnm20 from './forms/adnm20.js';
import adnm20s6 from './forms/adnm20s6.js';
import cerq from './forms/cerq.js';
import cfs from './forms/cfs.js';
import cses from './forms/cses.js';
import ctq from './forms/ctq.js';
import evenement0 from './forms/evenement0.js';
import evenement6 from './forms/evenement6.js';
import gad7 from './forms/gad7.js';
import isi from './forms/isi.js';
import isrc from './forms/isrc.js';
import lec5 from './forms/lec5.js';
import mhqol from './forms/mhqol.js';
import mini5s from './forms/mini5s.js';
import nouveaus6 from './forms/nouveaus6.js';
import pcl5 from './forms/pcl5.js';
import pdeq from './forms/pdeq.js';
import pensees6 from './forms/pensees6.js';
import phq9 from './forms/phq9.js';
import positif from './forms/positif.js';
import sociodemo from './forms/sociodemo.js';
import ptci from './forms/ptci.js';
import rds from './forms/rds.js';
import rdss6 from './forms/rdss6.js';
import sni from './forms/sni.js';
import sps10 from './forms/sps10.js';
import ssi from './forms/ssi.js';
import ssq6 from './forms/ssq6.js';
import substance from './forms/substance.js';

const consent = {
    text: html`
        <p>L'étude socio-trauma vise à étudier comment, à la suite directe d'un évènement difficile et potentiellement traumatique, vos relations à vos proches et votre bien être psychologique évolue. Cela implique que, pour participer, <b>vous devez avoir vécu un évènement que vous considérez comme difficile il y a moins d'un mois</b>.
        <p>Dans cette étude, nous vous poserons des questions sur votre situation actuelle, vos relations avec vos proches et la société, ainsi que sur votre bien-être psychologique. Il sera également demandé de fournir quelques informations sur l’évènement difficile que vous avez vécu.
        <p>Des consignes détaillées guideront chacune des 6 étapes de l’étude, avec des exercices à réaliser à plusieurs reprises. Des rappels par e-mail et/ou notifications via l’application vous aideront à respecter les échéances. Un calendrier est également disponible depuis votre tableau de bord.
        <p>L’étude se déroule <b>entièrement en ligne</b>, sans contact direct avec les autres participants ou les responsables de l’étude. Toutefois, l’investigatrice principale peut être contactée par e-mail pour toute question: Wivine Blekic, <a href="mailto:sociotrauma@ldv-recherche.fr">sociotrauma@ldv-recherche.fr</a>.
        <p>Nous vous invitons à <b>prendre connaissance de la lettre d’information avant de participer</b> à l’étude SocioTrauma. Celle-ci répond aux exigences de la recherche sur la personne humaine et a pour objectif de répondre aux questions que vous pourriez vous poser avant de prendre la décision de participer à la recherche.
    `,

    download: '/static/documents/SocioTrauma_Information.pdf',

    accept: (form, values) => {
        form.binary("consentement", "J’ai lu et je ne m’oppose pas à participer à l’étude SocioTrauma :");

        form.enumRadio("anciennete", "Je considère avoir vécu un évènement difficile il y a :", [
            [0, "Il y a quelques jours"],
            [1, "La semaine dernière"],
            [2, "Il y a 2 semaines"],
            [3, "Il y a 3 semaines"],
            [4, "Il y a 1 mois"],
            [5, "L’évènement s’est déroulé il y a plus d’un mois"]
        ]);

        form.binary("reutilisation", "J’accepte que mes données soient réutilisées dans le cadre d’autres études de Lignes de Vie :");

        return (values.consentement == 1);
    }
};

function init(build, start, values) {
    build.summary = html`
        <p>Pour rappel, cette étude s’adresse à <b>toute personne majeure (18 ans et plus) estimant avoir été exposée à un évènement potentiellement traumatique</b> au cours du mois précédent et n’étant pas actuellement sous mesure de protection de justice (sous tutelle, curatelle ou toute autre forme de protection judiciaire).
        <p>Commencez par faire votre bilan initial. Vous pouvez <b>arrêter à tout moment</b> et recommencer plus tard !
    `;

    build.module('recueil', 'Recueil', mod => {
        let anciennete = values.anciennete ?? 0;

        if (values.anciennete >= 5) {
            mod.help = html`
                <p>L'étude SocioTrauma est actuellement limitée aux personnes ayant vécu un événement difficile au cours du dernier mois. Cette restriction est liée à l'objectif spécifique de l'étude, qui examine les changements <b>survenant immédiatement après un événement traumatisant</b> dans les relations sociales et la santé mentale.
                <p>Si votre expérience remonte à plus d'un mois, <b>nous vous demandons de ne pas participer à cette étude</b>. Cela ne diminue en rien l'importance de votre vécu ou de votre souffrance. C’est tout simplement que le contenu de cette étude ne correspond plus à votre situation actuelle. SocioTrauma est conçue pour capturer les changements qu’une personne réalise dans ses relations à la suite immédiate à un évènement difficile.
                <p>Nous vous encourageons à consulter les ressources disponibles sur le site du Centre national de ressources et de résilience (CN2R) pour obtenir de l'aide et du soutien. De plus, nous vous invitons à revenir régulièrement sur le site de Lignes de Vie car d’autres études arriveront prochainement. En fonction du profil de participants recherchés, vous pourrez y participer !
            `;

            return;
        }

        let avance = -Math.max(0, anciennete - 1);
        let debut = start.plus(avance * 7);

        mod.level = 'Temporalité';
        mod.help = (progress, total) => {
            if (progress == total) {
                return html`
                    <p><b>Merci d’avoir complété votre bilan initial !</b>
                    <p>Vous pourrez poursuivre les prochains questionnaires selon le calendrier prévu. Notre objectif est de mieux comprendre l’évolution de votre bien-être psychologique et de vos relations au fil du temps. Pour cela, <b>il est essentiel que chaque participant remplisse les questionnaires aux mêmes périodes</b> : 1 mois, 3 mois, 6 mois et 1 an après l’événement.
                    <p>Les dates de rappel ont été calculées en fonction de la période que vous avez indiquée lors de votre consentement à l’étude. <b>Nous vous recommandons de les ajouter à votre agenda</b> et de revenir sur le site aux dates prévues pour le suivi. Pas d’inquiétude, vous <b>recevrez également un email de rappel</b> afin de ne rien oublier !
                    <p>En attendant, vous pouvez :
                    <ul>
                        <li>Explorer les pages « Ressources » ou « Se détendre » pour des conseils et outils utiles.
                        <li>Écrire vos ressentis dans votre journal, accessible depuis votre page « Profil ».
                    </ul>
                    <p>Merci pour votre précieuse implication dans notre recherche !
                `;
            }

            return html`
                <p>La première étape consiste à <b>vous présenter puis à faire un bilan initial</b>.
                <p>Vous pourrez ensuite continuer <b>selon le calendrier prévu</b>. À tout moment, vous pouvez arrêter l'étude ou la mettre en pause et y revenir plus tard. Vous avez également la possibilité de consulter les pages « Ressources » ou « Se détendre ».
                <p>Vous êtes libre de compléter les questionnaires dans l’ordre qui vous convient. Toutefois, nous vous recommandons de <b>suivre l’ordre proposé</b>. Les transitions ont été pensées avec soin par notre équipe de recherche de Lignes de Vie afin d’accompagner votre cheminement de la manière la plus confortable possible.
                <p>Nous vous invitons à <b>répondre à toutes les questions</b>, même si vous n'êtes pas certain de votre réponse. Il n’y a pas de bonne ou de mauvaise réponse, et même incertaines, elles sont importantes. Dans Lignes de Vie, on se donne le droit à l’erreur, à la bêtise, à l’ignorance… Et pour les chercheurs aussi !
                <p>Dès lors, si une question <b>ne correspond pas à votre vécu</b> ou si vous souhaitez ajouter un commentaire, validez la page sans y répondre et apposez votre commentaire dans le champ qui apparait.
            `;
        }

        build.module('sociodemo', 'Présentation', mod => {
            build.form('sociodemo', 'Présentation', sociodemo);
        });

        build.module('initial', 'Bilan initial', mod => {
            mod.level = 'Module';
            mod.help = html`
                <p>Nous vous conseillons de répondre aux différents questionnaires <b>dans l’ordre</b> ci-dessus.
                <p>Commencez par votre <b>entourage</b> puis abordez le module sur les <b>évènements</b>.
            `;
            mod.step = html`
                <p>Le bilan initial est réalisé <b>dans le mois</b> qui suit l'évènement.
                <p>Retrouvez les <a href="/etudes#etude-sociotrauma">informations de cette étude</a> sur le site de Lignes de Vie.
            `;

            let options = { schedule: start };

            build.module('entourage', 'Entourage', mod => {
                mod.help = html`
                    <p>Dans ce module, nous visons à comprendre votre <b>environnement social</b> : À qui parlez-vous de manière régulière ? Qui est là pour vous ? Êtes-vous satisfaits du soutien que vous recevez ?
                    <p>À la fin de ce module, nous vous demanderons de décrire votre entourage précisément à l'aide d'un <abbr title="Outil permettant de représenter visuellement vos relations sociales">sociogramme</abbr> interactif.
                `;

                build.form('ssq6', 'Soutien social', ssq6, options)
                build.form('sni', 'Interactions sociales', sni, options)
                build.form('sps10', 'Disponibilité de votre entourage', sps10, options)
                build.network('network', 'Sociogramme', options)
            });

            build.module('evenement', 'Évènement', mod => {
                mod.help = html`
                    <p>Dans ce module, nous vous poserons des questions sur l’<b>événement qui vous a conduit à participer à cette étude</b>, ainsi que sur <b>tout autre événement, qu’il soit positif ou négatif</b>, qui fait actuellement partie de votre réalité.
                    <p>N'hésitez pas à interrompre l'application ou à cliquer sur le <b>bouton « SOS »</b> si cela s’avère nécessaire pour vous. Vous pouvez également arrêter à tout moment et revenir plus tard.
                `

                build.form('evenement', 'L’évènement qui vous a amené ici', evenement0, options)
                build.form('pensees', 'Pensées et ressentis', (form, values) => {
                    if (values.pdeq == null)
                        values.pdeq = {}
                    if (values.isrc == null)
                        values.isrc = {}

                    pdeq(form, values.pdeq)
                    isrc(form, values.isrc)
                }, options)
                build.form('autres', 'Autres évènements de vie', (form, values) => {
                    if (values.adnm20 == null)
                        values.adnm20 = {}
                    if (values.lec5 == null)
                        values.lec5 = {}
                    if (values.positif == null)
                        values.positif = {}

                    adnm20(form, values.adnm20)
                    lec5(form, values.lec5)
                    positif(form, values.positif, 'l’évènement qui vous a amené ici')
                }, options)
            });

            build.module('qualite', 'Qualité de vie', () => {
                build.form('mhqol', 'Qualité de vie', mhqol, options)
            });

            build.module('divulgation', 'Communication', mod => {
                mod.help = html`
                    <p>Dans ce module, nous allons nous intéresser à la façon dont vous <b>souhaitez – ou non – parler de votre expérience</b> à vos proches, ainsi qu’à leurs réactions (si vous leur en avez parlé).
                `

                build.form('cses', 'Témoignage à l\'entourage', cses, options)
                build.form('rds', 'Réactions des proches', rds, options)
            });
        });

        build.module('s6', '6 semaines', mod => {
            mod.level = 'Module';
            mod.help = html`
                <p>Nous vous remercions d'avoir rempli le premier questionnaire. Pour compléter notre étude, nous avons besoin que vous répondiez à un <b>deuxième questionnaire</b> sur ce que vous avez ressenti suite à l'événement qui vous a amené ici.
                <p>Vous remarquerez peut-être que certaines questions <b>semblent se répéter</b>. C'est normal et voulu ! Chaque question a son importance pour nous permettre d'analyser correctement les résultats.
                <p>Nous vous sommes très reconnaissants pour votre temps et votre patience. Vos <b>réponses sont précieuses</b> pour notre recherche !
            `;
            mod.step = html`
                <p>Bienvenue dans ce <b>premier suivi</b> de socio-trauma !
                <p>À tout moment, vous pouvez arrêter l'étude ou la mettre en pause et y revenir plus tard. Vous avez également la possibilité de consulter les pages « Ressources » ou « Se détendre ».
            `;

            let options = { schedule: debut.plus(6 * 7) };

            build.module('evenement', 'Évènement', mod => {
                mod.help = html`
                    <p>Dans ce module, nous vous poserons des questions sur l’<b>événement qui vous a conduit à participer à cette étude</b>, ainsi que sur <b>tout autre événement, qu’il soit positif ou négatif</b>, qui fait actuellement partie de votre réalité.
                    <p>N'hésitez pas à interrompre l'application ou à cliquer sur le <b>bouton « SOS »</b> si cela s’avère nécessaire pour vous. Vous pouvez également arrêter à tout moment et revenir plus tard.
                `

                build.form('evenement', 'L’évènement qui vous a amené ici', (form, values) => evenement6(form, values, start), options)
                build.form('pensees', 'Pensées et ressentis', pensees6, options)
                build.form('autres', 'Autres évènements de vie', (form, values) => {
                    if (values.adnm20 == null)
                        values.adnm20 = {}
                    if (values.nouveau == null)
                        values.nouveau = {}
                    if (values.positif == null)
                        values.positif = {}

                    form.part(() => {
                        form.binary('situation1', 'Votre situation familiale a-t-elle changé depuis le bilan initial ?')

                        if (values.situation1) {
                            form.enumRadio('situation2', 'Quelle est votre situation familiale actuelle ?', [
                                ['C', 'Célibataire'],
                                ['M', 'Marié(e)'],
                                ['P', 'Pacsé(e)'],
                                ['MG', 'En relation monogame'],
                                ['PG', 'En relation polyamoureuse'],
                                ['D', 'Divorcé(e)'],
                                ['V', 'Veuf/veuve']
                            ])
                        }
                    })

                    adnm20s6(form, values.adnm20)
                    nouveaus6(form, values.nouveau)
                    positif(form, values.positif, 'la fin du bilan initial')
                }, options)
                build.form('enfance', 'Enfance et adolescence', ctq, options)
            });

            build.module('qualite', 'Qualité de vie', mod => {
                mod.help = html`
                    <p>Ce module porte sur votre <b>état de santé</b>. Ces informations nous permettront de savoir comment vous vous sentez dans votre vie de tous les jours.
                    <p>N'hésitez pas à interrompre l'application ou à cliquer sur le <b>bouton « SOS »</b> si cela s’avère nécessaire pour vous. Vous pouvez également arrêter à tout moment et revenir plus tard.
                `

                build.form('mhqol', 'Qualité de vie', mhqol, options)
                build.form('substances', 'Comportements', substance, options)
                build.form('sommeil', 'Sommeil', isi, options)
                build.form('stress', 'Gestion du stress', (form, values) => {
                    if (values.cfs == null)
                        values.cfs = {}
                    if (values.cerq == null)
                        values.cerq = {}

                    cfs(form, values.cfs)
                    cerq(form, values.cerq)
                }, options)
                build.form('autres', 'Autres difficultés', (form, values) => {
                    if (values.phq9 == null)
                        values.phq9 = {}
                    if (values.gad7 == null)
                        values.gad7 = {}
                    if (values.mini5s == null)
                        values.mini5s = {}

                    phq9(form, values.phq9)
                    gad7(form, values.gad7)
                    mini5s(form, values.mini5s)
                }, options)
            });

            build.module('entourage', 'Entourage', mod => {
                mod.help = html`
                    <p>Dans ce module, nous visons à comprendre votre <b>environnement social</b> : À qui parlez-vous de manière régulière ? Qui est là pour vous ? Êtes-vous satisfaits du soutien que vous recevez ?
                    <p>À la fin de ce module, nous vous demanderons de décrire votre entourage précisément à l'aide d'un <abbr title="Outil permettant de représenter visuellement vos relations sociales">sociogramme</abbr> interactif.
                `;

                build.form('ssq6', 'Soutien social', ssq6, options)
                build.form('sni', 'Interactions sociales', sni, options)
                build.form('sps10', 'Disponibilité de votre entourage', (form, values) => {
                    sps10(form, values);

                    form.part(() => {
                        form.binary('sup1', 'Considérez-vous que l’évènement qui vous a amené ici a engendré une rupture avec votre vie d’avant ?')

                        if (values.sup1) {
                            let types = Object.keys(PERSON_KINDS).map(kind => [kind, PERSON_KINDS[kind].text])

                            form.multiCheck('sup2', 'Dans quelle(s) sphère(s) de votre vie ressentez-vous cette rupture ?', types, {
                                help: "Plusieurs réponses possibles"
                            })
                        }
                    })
                }, options)
                build.network('network', 'Sociogramme', options)
            });

            build.module('divulgation', 'Communication', mod => {
                mod.help = html`
                    <p>Dans ce module, nous allons nous intéresser à la façon dont vous <b>souhaitez – ou non – parler de votre expérience</b> à vos proches, ainsi qu’à leurs réactions (si vous leur en avez parlé).
                `

                build.form('cses', 'Témoignage à l\'entourage', (form, values) => {
                    cses(form, values)

                    form.part(() => {
                        form.enumButtons('meme', 'Est-ce que la personne à qui vous pensez est la même que celle à laquelle vous faisiez référence lors du bilan initial ?', [
                            [1, 'Oui'],
                            [0, 'Non'],
                            [-1, 'Je ne me souviens plus']
                        ])
                    })
                }, options)

                build.form('rds', 'Réactions des proches', (form, values) => rdss6(form, values, start), options)
            });
        });

        build.module('m3', '3 mois', mod => {
            mod.level = 'Module';

            mod.help = html`
                <p>Ce module n'est <b>pas prêt</b> ! Revenez plus tard :)
            `;
            return;

            let options = { schedule: debut.plusMonths(3) };

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
                build.form('substances', 'Consommations', substance, options)
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
                <p>Ce module n'est <b>pas prêt</b> ! Revenez plus tard :)
            `;
            return;

            let options = { schedule: debut.plusMonths(6) };

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
                build.form('substances', 'Consommations', substance, options)
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
                <p>Ce module n'est <b>pas prêt</b> ! Revenez plus tard :)
            `;
            return;

            let options = { schedule: debut.plusMonths(12) };

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
                build.form('substances', 'Consommations', substance, options)
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
