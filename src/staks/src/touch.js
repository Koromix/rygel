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

import { render, html } from '../../../vendor/lit-html/lit-html.bundle.js';
import { Util, Log } from '../../web/core/base.js';
import { AppRunner } from '../../web/core/runner.js';
import { loadAssets, assets } from './assets.js';
import * as rules from './rules.js';

function TouchInterface(runner) {
    let self = this;

    // Render and input API
    let ctx = runner.ctx;
    let mouse_state = runner.mouseState;
    let pressed_keys = runner.pressedKeys;

    // State
    let buttons = new Map;
    let prev_buttons = null;

    this.begin = function() {
        prev_buttons = buttons;
        buttons = new Map;
    };

    this.button = function(key, x, y, size) {
        let prev = prev_buttons.get(key);

        let img = assets.ui[key];

        let btn = {
            rect: {
                left: x - size / 2,
                top: y - size / 2,
                width: size,
                height: size,
            },
            pos: {
                x: x,
                y: y,
            },
            size: size,

            img: img,

            clicked: false,
            pressed: 0
        };

        if (isInsideRect(mouse_state.x, mouse_state.y, btn.rect)) {
            if (mouse_state.left == 1)
                btn.clicked = true;
            if (mouse_state.left >= 1)
                btn.pressed = (prev?.pressed ?? 0) + 1;

            runner.cursor = 'pointer';
            mouse_state.left = 0;
        }

        buttons.set(key, btn);

        let status = {
            clicked: prev?.clicked ?? false,
            pressed: prev?.pressed ?? 0
        };

        return status;
    };

    this.draw = function() {
        ctx.fillStyle = 'white';

        for (let btn of buttons.values()) {
            ctx.beginPath();
            ctx.arc(btn.pos.x, btn.pos.y, btn.size / 2, 0, 2 * Math.PI);
            ctx.fill();

            let size = 0.5 * btn.size;
            let left = btn.pos.x - size / 2;
            let top = btn.pos.y - size / 2;

            ctx.drawImage(btn.img, left, top, size, size);
        }
    };
}

function isInsideRect(x, y, rect) {
    let inside = (x >= rect.left &&
                  x <= rect.left + rect.width &&
                  y >= rect.top &&
                  y <= rect.top + rect.height);
    return inside;
}

export { TouchInterface }
