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

const PROJECTS = [
    {
        index: 1,
        key: 'sociotrauma',
        title: 'SocioTrauma',
        description: html`
            <p>L'étude SocioTrauma explore l'impact des événements particulièrement difficiles sur le bien-être psychologique et les relations sociales. À cette fin, plusieurs questionnaires sont proposés à différents moments au cours d’une période d’un an.
            <p>Retrouvez les <a href="/etudes#etude-sociotrauma" target="_blank">informations plus détaillées</a> sur le site de Lignes de Vie !
        `,
        picture: ASSETS['pictures/sociotrauma'],
        bundle: BUNDLES['sociotrauma.js']
    }
];

export { PROJECTS }
