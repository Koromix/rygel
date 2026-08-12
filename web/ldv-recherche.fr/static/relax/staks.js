// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

document.body.classList.add('loading');

import { load, start } from '../../../../src/staks/src/game.js';
import './staks.css';

run();

async function run() {
    try {
        let div = document.querySelector('#game');

        await load('static/relax', 'fr', (value, total) => {
            let progress = Math.floor(value / total * 100);
            div.innerHTML = progress + '%';
        });
        div.innerHTML = '';

        await start(div);
    } catch (err) {
        console.error(err);

        document.body.innerHTML =
            '<div id="fatal"><div>' +
                '<span style="color: red;">⚠\uFE0E <b>Une erreur est survenue pendant le chargement</b></span><br/>' +
                err.message + '<br/><br/>' +
                window.SUPPORTED_BROWSERS +
            '</div></div>';
    } finally {
        document.body.classList.remove('loading');
    }
}
