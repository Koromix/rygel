// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

import { html } from 'vendor/lit-html/lit-html.bundle.js'
import { ASSETS } from '../assets/assets.js'

const PROJECTS = [
    {
        index: 1,
        key: 'sociotrauma',
        title: 'SocioTrauma',
        description: html`<p>L'étude SocioTrauma <b>explore l'impact des événements particulièrement difficiles sur le bien-être psychologique et les relations sociales</b>. À cette fin, plusieurs questionnaires sont proposés à différents moments au cours d’une période d’un an.`,
        picture: ASSETS['pictures/sociotrauma'],
        bundle: BUNDLES['sociotrauma.js']
    }
];

export { PROJECTS }
