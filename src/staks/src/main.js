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

import { Util, Log } from '../../web/core/common.js';
import { AppRunner } from '../../web/core/runner.js';
import { init, update, draw } from './game.js';
import { loadAssets } from './assets.js';

import '../../../vendor/opensans/OpenSans.css';

let canvas = document.querySelector('#game');
let runner = new AppRunner(canvas);

// Shortcuts
let ctx = canvas.getContext('2d');
let mouse_state = runner.mouseState;
let pressed_keys = runner.pressedKeys;

async function checkSupport() {
    // Test let and async support
    eval('let test = async function() {}');

    // Test WebP support
    {
        let urls = [
            'data:image/webp;base64,UklGRjIAAABXRUJQVlA4ICYAAACyAgCdASoCAAEALmk0mk0iIiIiIgBoSygABc6zbAAA/v56QAAAAA==',
            'data:image/webp;base64,UklGRh4AAABXRUJQVlA4TBEAAAAvAQAAAAfQ//73v/+BiOh/AAA='
        ];

        for (let url of urls) {
            let img = new Image();

            await new Promise((resolve, reject) => {
                img.onload = () => resolve();
                img.onerror = () => reject(new Error('Browser does not support WebP image format'));
                img.src = url;
            });
        }
    }
}

async function start() {
    await loadAssets();

    await init();

    runner.updateFrequency = 120;
    runner.onUpdate = update;
    runner.onDraw = draw;

    runner.start();

    document.body.classList.remove('loading');
}

start();

export {
    runner,
    canvas,
    ctx,
    mouse_state,
    pressed_keys
}
