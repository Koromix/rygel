// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import { render, html } from '../../../vendor/lit-html/lit-html.bundle.js';
import { Util, Log } from '../../core/web/base/base.js';
import { AppRunner } from '../../core/web/base/runner.js';

function TouchInterface(runner, assets) {
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
