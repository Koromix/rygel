// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import '../../../lib/web/flat/static.js';
import * as AsciinemaPlayer from '../../../vendor/asciinema/asciinema-player.js';
import * as hljs from '../../../vendor/highlight.js/highlight.js';

import demo from './demo.cast';

window.addEventListener('load', e => {
    hljs.highlightAll();
    initDemo();
});

function initDemo() {
    let div = document.querySelector('#demo');

    if (div != null) {
        AsciinemaPlayer.create('static/' + demo, div, {
            autoPlay: true,
            loop: true
        });
    }
}
