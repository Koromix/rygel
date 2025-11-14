// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import { render, html } from 'vendor/lit-html/lit-html.bundle.js';
import { Util, Log , LocalDate } from 'lib/web/base/base.js';
import * as UI from 'lib/web/base/ui.js';

import './color.css';

function ColorPicker() {
    let self = this;

    let change_handler = (color) => {};

    let current_color = null;
    let h = null, s = null, v = null;

    let sv = null;
    let hue = null;

    Object.defineProperties(this, {
        changeHandler: { get: () => change_handler, set: handler => { change_handler = handler; }, enumerable: true },

        color: { get: () => color, enumerable: true }
    });

    this.pick = async function(e, color) {
        current_color = color;
        [h, s, v] = RGBtoHSV(...parseRGB(color));

        sv = document.createElement('canvas');
        hue = document.createElement('canvas');

        sv.className = 'col_sv';
        sv.width = 360;
        sv.height = 360;
        hue.className = 'col_hue';
        hue.width = 40;
        hue.height = 360;

        sv.addEventListener('click', handleSV);
        sv.addEventListener('mousemove', handleSV);
        sv.addEventListener('touchmove', handleSV, { capture: true });
        hue.addEventListener('click', handleHue);
        hue.addEventListener('mousemove', handleHue);
        hue.addEventListener('touchmove', handleHue, { capture: true });

        redraw();

        await UI.popup(e, (close) => html`
            <div class="col_wrapper">
                ${sv}
                ${hue}
            </div>

            <div class="actions">
                <button type="button" class="secondary" @click=${UI.insist(close)}>Annuler</button>
                <button type="submit">Confirmer</button>
            </div>
        `);

        return current_color;
    }

    function redraw() {
        // Draw saturation/value gradient
        {
            let data = new Uint8ClampedArray(sv.width * sv.height * 4);

            for (let x = 0; x < sv.width; x++) {
                for (let y = 0; y < sv.height; y++) {
                    let i = (x + y * sv.width) * 4;
                    let [r, g, b] = HSVtoRGB(h, x / sv.width, 1 - y / sv.height);

                    data[i + 0] = Math.round(r * 255);
                    data[i + 1] = Math.round(g * 255);
                    data[i + 2] = Math.round(b * 255);
                    data[i + 3] = 255;
                }
            }

            let ctx = sv.getContext('2d');
            let img = new ImageData(data, sv.width, sv.height);

            ctx.putImageData(img, 0, 0);
        }

        // Draw hue gradient
        {
            let data = new Uint8ClampedArray(hue.width * hue.height * 4);

            for (let y = 0; y < hue.height; y++) {
                let start = y * hue.width * 4;
                let end = start + hue.width * 4;

                let [r, g, b] = HSVtoRGB((y / hue.height) * 360, 1, 1);

                for (let i = start; i < end; i += 4) {
                    data[i + 0] = Math.round(r * 255);
                    data[i + 1] = Math.round(g * 255);
                    data[i + 2] = Math.round(b * 255);
                    data[i + 3] = 255;
                }
            }

            let ctx = hue.getContext('2d');
            let img = new ImageData(data, hue.width, hue.height);

            ctx.putImageData(img, 0, 0);
        }

        // Draw saturation/value selection
        {
            let ctx = sv.getContext('2d');
            let x = s * sv.width;
            let y = (1 - v) * sv.height;

            ctx.strokeStyle = 'black';
            ctx.lineWidth = 4;

            ctx.beginPath();
            ctx.moveTo(x - 12, y - 12);
            ctx.lineTo(x, y);
            ctx.lineTo(x - 12, y + 12);
            ctx.lineTo(x, y);
            ctx.lineTo(x + 12, y + 12);
            ctx.lineTo(x, y);
            ctx.lineTo(x + 12, y - 12);
            ctx.lineTo(x, y);
            ctx.stroke();
        }

        // Draw hue selection
        {
            let ctx = hue.getContext('2d');
            let y = (h * hue.height / 360);

            ctx.fillStyle = 'black';

            ctx.beginPath();
            ctx.moveTo(0, y - 10);
            ctx.lineTo(10, y);
            ctx.lineTo(0, y + 10);
            ctx.closePath();
            ctx.fill();

            ctx.beginPath();
            ctx.moveTo(hue.width, y - 10);
            ctx.lineTo(hue.width - 10, y);
            ctx.lineTo(hue.width, y + 10);
            ctx.closePath();
            ctx.fill();
        }
    }

    function handleSV(e) {
        if (e.type == 'mousemove' && !(e.buttons & 1))
            return;
        if (e.type == 'touchmove' && e.touches.length != 1)
            return;

        let rect = e.currentTarget.getBoundingClientRect();
        let x = e.clientX ?? e.touches[0].clientX;
        let y = e.clientY ?? e.touches[0].clientY;

        s = Util.clamp((x - rect.left) / rect.width, 0, 1);
        v = Util.clamp(1 - (y - rect.top) / rect.height, 0, 1);

        redraw();
        handleChange();
    }

    function handleHue(e) {
        if (e.type == 'mousemove' && !(e.buttons & 1))
            return;
        if (e.type == 'touchmove' && e.touches.length != 1)
            return;

        let rect = e.currentTarget.getBoundingClientRect();
        let y = e.clientY ?? e.touches[0].clientY;

        h = Util.clamp((y - rect.top) / rect.height * 360, 0, 360);

        redraw();
        handleChange();
    }

    function handleChange() {
        let [r, g, b] = HSVtoRGB(h, s, v);

        r = Math.round(r * 255);
        g = Math.round(g * 255);
        b = Math.round(b * 255);

        current_color = `#${r.toString(16).padStart(2, '0')}${g.toString(16).padStart(2, '0')}${b.toString(16).padStart(2, '0')}`;

        change_handler(current_color);
    }
}

function parseRGB(color) {
    if (!color.match(/^#[0-9a-fA-F]{6}$/))
        throw new Error('Invalid hexadecimal color code');

    let r = parseInt(color.substr(1, 2), 16);
    let g = parseInt(color.substr(3, 2), 16);
    let b = parseInt(color.substr(5, 2), 16);

    return [r / 255, g / 255, b / 255];
}

function HSVtoRGB(h, s, v) {
    let [r, g, b] = [f(5), f(3), f(1)];

    function f(n) {
        let k = (n + h / 60) % 6;
        return v - v * s * Math.max(Math.min(k, 4 - k, 1), 0);
    }

    return [r, g, b];
}

function RGBtoHSV(r, g, b) {
    let v = Math.max(r, g, b);
    let c = v - Math.min(r, g, b);

    let h = c && ((v == r) ? (g - b) / c : (v == g ? 2 + (b - r) / c : 4 + (r - g) / c));
    let s = v && c / v;

    h = 60 * (h < 0 ? h + 6 : h);

    return [h, s, v];
}

export { ColorPicker }
