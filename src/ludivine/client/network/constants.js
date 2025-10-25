// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import { html } from '../../../../vendor/lit-html/lit-html.bundle.js';

const PERSON_KINDS = {
    love: { text: 'Relation amoureuse', icon: 'network/love' },
    family: { text: 'Famille', icon: 'network/family' },
    friend: { text: 'Amitié', icon: 'network/friend' },
    work: { text: 'Collègues / milieu professionnel', icon: 'network/work' },
    help: { text: 'Professionnel (santé, juridique, social, ...) ', icon: 'network/help' },
    online: { text: 'Membre d’une communauté (en ligne)', icon: 'network/online' },
    community: { text: 'Membre d’une communauté (hors ligne)', icon: 'network/community' },
    org: { text: 'Association d’aide aux victimes', icon: 'network/org' },
    animal: { text: 'Animal', icon: 'network/animal' },
    other: { text: 'Autre', icon: 'network/other' }
};

const PROXIMITY_LEVELS = [
    {
        text: null,
        description: null,
        size: 0.6
    },

    {
        text: 'Contacts rares',
        description: 'Vous rencontrez ou vous êtes rarement en contact avec ces personnes',
        size: 0.8
    },
    {
        text: 'Contacts réguliers',
        description: 'Il s\'agit de personnes que vous cotoyez régulièrement par le travail, ou des activités, ou des amis',
        size: 1
    },
    {
        text: 'Vie quotidienne',
        description: 'Ces personnes vivent sous le même toit ou proches de vous, vous les fréquentez donc très souvent',
        size: 1.2
    }
];

// Keep in sync with quality widget CSS (network.css)
const QUALITY_COLORS = {
    start: 0xb13737,
    middle: 0xefbe48,
    end: 0x3fa26b
};

const USER_GUIDES = {
    addSubjects: html`Bienvenue sur votre sociogramme ! Commencez par ajouter les personnnes de votre entourage sur le sociogramme en cliquant sur le <b>bouton d'ajout en bas à gauche</b> de l'écran.`,
    movePersons: html`
        <p>Classez les personnes ajoutées dans le <b>cercle de proximité qui leur correspond le mieux</b> :
        <ul>
            <li><i>Vie quotidienne</i> : celles que vous voyez ou contactez tous les jours ou presque.
            <li><i>Régulier</i> : celles que vous voyez ou contactez fréquemment, mais pas quotidiennement.
            <li><i>De temps en temps</i> : celles que vous voyez ou contactez occasionnellement.
        </ul>
        <p>Déplacez chaque personne <b>dans le cercle approprié</b> en fonction de votre niveau d'interaction avec elle.
    `,
    editPerson: html`Une fois les personnes classées dans leur cercle respectif, <b>cliquez sur chaque personne ajoutée pour évaluer la relation que vous entretenez avec elle</b>. Indiquez comment vous percevez cette relation.`,
    addMore: html`Complétez le sociogramme avec <b>des amis, les membres de votre famille, des collègues, des soignants, des animaux</b>, et tout autre personne à laquelle vous pensez. N'hésitez pas !`,
    createLinks: html`Vous pouvez également indiquer comment les personnes s'entendent à l'aide du mode Liens. Cliquez sur le <b>bouton « Liens »</b> pour relier vos proches entre eux/elles en traçant les liens avec votre souris ou à l'aide de l'écran tactile.`,
    editLink: html`Cliquez sur le <b>petit cercle au centre de chaque lien</b> pour évaluer la relation entre les personnes concernées.`,
    peopleMode: html`Utilisez le <b>bouton « Relations »</b> pour quitter le mode Liens et ajouter d'autres personnes.`
};

export {
    PROXIMITY_LEVELS,
    PERSON_KINDS,
    QUALITY_COLORS,
    USER_GUIDES
}
