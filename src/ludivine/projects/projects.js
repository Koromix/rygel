// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import { html } from 'vendor/lit-html/lit-html.bundle.js';
import { ASSETS } from '../assets/assets.js';

const PROJECTS = [
    {
        index: 1,
        key: 'sociotrauma',
        title: 'SocioTrauma',
        description: html`
            <p>L'étude SocioTrauma <b>explore l'impact des événements particulièrement difficiles sur le bien-être psychologique et les relations sociales</b>. À cette fin, plusieurs questionnaires sont proposés à différents moments au cours d’une période d’un an.
            <p>Retrouvez les <a href="/etudes#etude-sociotrauma" target="_blank">informations plus détaillées</a> sur le site de Lignes de Vie ! Pour rappel, cette étude s’adresse à toute personne majeure (18 ans et plus) <b>estimant avoir été exposée à un évènement potentiellement traumatique au cours du mois précédent</b> et n’étant pas actuellement sous mesure de protection de justice (sous tutelle, curatelle ou toute autre forme de protection judiciaire).
        `,
        picture: ASSETS['pictures/sociotrauma'],
        bundle: BUNDLES['sociotrauma.js']
    }
];

export { PROJECTS }
