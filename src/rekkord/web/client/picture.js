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

import { render, html, live } from '../../../../vendor/lit-html/lit-html.bundle.js';
import { Util, Log, Net } from '../../../web/core/base.js';
import * as UI from './ui.js';

if (typeof T == 'undefined')
    T = {};

Object.assign(T, {
    apply: 'Apply',
    browse_for_image: 'Browse for image',
    cancel: 'Cancel',
    clear_picture: 'Clear picture',
    drag_paste_or_browse_image: 'Drop an image, paste it or use the \'Browse\' button below',
    upload_image: 'Upload image',
    zoom: 'Zoom'
});

function PictureCropper(title, size) {
    let self = this;

    let refresh_func = null;
    let canvas = makeCanvas(size, size);

    let image_format = 'image/png';
    let default_url = null;

    Object.defineProperties(this, {
        imageFormat: { get: () => image_format, set: format => { image_format = format; }, enumerable: true },
        defaultURL: { get: () => default_url, set: url => { default_url = url; }, enumerable: true }
    });

    let img;

    let init_url;
    let apply_func;
    let is_default;
    let zoom;
    let offset;

    let move_pointer = null;
    let prev_offset = null;

    this.run = async function(url = null, func = null) {
        await load(url);

        init_url = url;
        apply_func = func;
        is_default = true;

        let ret = await UI.dialog({
            open: () => {
                canvas.addEventListener('pointerdown', react);
                canvas.addEventListener('wheel', react);
                canvas.addEventListener('dragenter', e => e.preventDefault());
                canvas.addEventListener('dragover', drop);
                canvas.addEventListener('drop', UI.wrap(drop));

                window.addEventListener('paste', drop);
            },

            close: () => {
                window.removeEventListener('paste', drop);
            },

            run: (render, close) => {
                refresh_func = render;

                redraw();

                return html`
                    <style>
                        .pic_cropper {
                            margin: 0 auto 0.5em auto;
                            position: relative;
                            border: 1px solid #ededf0;
                            overflow: hidden;
                            cursor: grab;
                        }
                        .pic_cropper.default { cursor: inherit !important; }

                        .pic_legend {
                            text-align: center;
                            font-size: 0.9em;
                            font-style: italic;
                            filter: opacity(0.75);
                        }
                    </style>

                    <div class="title">
                        ${title}
                        <div style="flex: 1;"></div>
                        <button type="button" class="secondary" @click=${UI.wrap(close)}>✖\uFE0E</button>
                    </div>

                    <div class="main">
                        <div class=${is_default ? 'pic_cropper default' : 'pic_cropper'}
                             style=${`width: ${size}px; height: ${size}px; min-height: ${size}px`}>${canvas}</div>
                        <div class="pic_legend">${T.drag_paste_or_browse_image}</div>
                        <label>
                            <span>${T.zoom}</span>
                            <input type="range" min="-32" max="32"
                                   .value=${live(zoom)} ?disabled=${is_default}
                                   @input=${UI.wrap(e => changeZoom(parseInt(e.target.value, 10) - zoom))} />
                        </label>
                        <label>
                            <span>${T.upload_image}</span> 
                            <input type="file" name="file" style="display: none;" accept="image/*"
                                   @change=${UI.wrap(e => load(e.target.files[0]))} />
                            <button type="button" @click=${e => { e.target.parentNode.click(); e.preventDefault(); }}>${T.browse_for_image}</button>
                        </label>
                        <label>
                            <span></span>
                            <button type="button" class="secondary"
                                    ?disabled=${is_default && init_url == null}
                                    @click=${UI.insist(e => load(null))}>${T.clear_picture}</button>
                        </label>
                    </div>

                    <div class="footer">
                        <button type="button" class="secondary" @click=${UI.insist(close)}>${T.cancel}</button>
                        <button type="submit">${T.apply}</button>
                    </div>
                `;
            },

            submit: apply
        });

        return ret;
    };

    async function load(file) {
        if (file != null) {
            img = await loadImage(file);
        } else if (default_url != null) {
            img = await loadImage(default_url);
        } else {
            img = null;
        }

        is_default = (file == null);
        init_url = null;
        zoom = 0;
        offset = { x: 0, y: 0 };

        if (refresh_func != null)
            refresh_func();
    }

    function react(e) {
        if (is_default)
            return;

        if (e.type == 'pointerdown') {
            if (move_pointer != null)
                return;

            move_pointer = e.pointerId;
            prev_offset = Object.assign({}, offset);

            canvas.setPointerCapture(move_pointer);
            canvas.style.cursor = 'grabbing';

            canvas.addEventListener('pointermove', react);
            canvas.addEventListener('pointerup', react);
            document.body.addEventListener('keydown', react);
        } else if (e.type == 'pointermove') {
            offset.x += e.movementX;
            offset.y += e.movementY;

            let rect = computeRect(img, zoom, offset);
            offset = fixEmptyArea(offset, rect);

            refresh_func();
        } else if (e.type == 'pointerup') {
            release(e);
        } else if (e.type == 'keydown') {
            offset = prev_offset;
            refresh_func();

            release(e);
        } else if (e.type == 'wheel') {
            let rect = canvas.getBoundingClientRect();
            let at = { x: e.clientX - rect.left, y: e.clientY - rect.top };

            if (e.deltaY < 0) {
                changeZoom(1, at);
            } else if (e.deltaY > 0) {
                changeZoom(-1, at);
            }

            e.preventDefault();
        }

        function release(e) {
            canvas.releasePointerCapture(move_pointer);
            canvas.style.cursor = null;

            canvas.removeEventListener('pointermove', react);
            canvas.removeEventListener('pointerup', react);
            document.body.removeEventListener('keydown', react);

            move_pointer = null;
            prev_offset = null;
        }
    }

    function drop(e) {
        let dt = e.dataTransfer || e.clipboardData;

        let src = null;
        let found = false;

        for (let i = 0; i < dt.items.length; i++) {
            let item = dt.items[i];

            if (item.kind == 'string' && item.type == 'text/uri-list') {
                let url = dt.getData('URL') || null;

                if (url != null)
                    src = url;

                found = true;
            } else if (item.kind == 'file' && item.type.startsWith('image/')) {
                let file = item.getAsFile();

                if (file != null)
                    src = file;

                found = true;
                break;
            }
        }

        if (e.type == 'dragover') {
            dt.dropEffect = found ? 'move' : 'none';
        } else if (e.type == 'drop' || e.type == 'paste') {
            if (src != null)
                load(src);
        }

        e.preventDefault();
    }

    function changeZoom(delta, at = null) {
        if (at == null)
            at = { x: size / 2, y: size / 2 };

        // Centered zoom
        {
            let rect = computeRect(img, zoom, offset);

            // Convert to relative coordinates (-1 to 1) in image space
            let center = {
                x: -2 * (rect.x1 - at.x) / (rect.x2 - rect.x1) - 1,
                y: -2 * (rect.y1 - at.y) / (rect.y2 - rect.y1) - 1
            };

            let new_zoom = Math.min(Math.max(zoom + delta, -32), 32);

            // Compute displacement caused by zoom effect
            let displace = Math.max(size / img.width, size / img.height) *
                           (Math.pow(2, new_zoom / 8) - Math.pow(2, zoom / 8));

            zoom = new_zoom;
            offset.x -= center.x * displace * img.width / 2;
            offset.y -= center.y * displace * img.height / 2;
        }

        let rect = computeRect(img, zoom, offset);
        offset = fixEmptyArea(offset, rect);

        refresh_func();
    }

    function redraw() {
        let ctx = canvas.getContext('2d');

        ctx.clearRect(0, 0, size, size);
        draw(ctx);

        ctx.fillStyle = '#f8f8f8dd';
        ctx.beginPath();
        ctx.arc(size / 2, size / 2, size / 2, 0, 2 * Math.PI);
        ctx.rect(size, 0, -size, size);
        ctx.fill();
    }

    function draw(ctx) {
        if (img == null)
            return;

        let rect = computeRect(img, zoom, offset);
        ctx.drawImage(img, rect.x1, rect.y1, rect.x2 - rect.x1, rect.y2 - rect.y1);
    }

    async function apply() {
        if (!is_default) {
            let blob = await new Promise((resolve, reject) => {
                let canvas = makeCanvas(size, size);

                // Draw clipped picture
                {
                    let ctx = canvas.getContext('2d');

                    ctx.beginPath();
                    ctx.arc(size / 2, size / 2, size / 2, 0, 2 * Math.PI);
                    ctx.clip();

                    draw(ctx);
                }

                canvas.toBlob(resolve, image_format, 1);
            });

            if (apply_func != null)
                await apply_func(blob);

            return blob;
        } else {
            if (apply_func != null)
                await apply_func(init_url);

            return init_url;
        }
    }

    function makeCanvas(width, height) {
        let canvas = document.createElement('canvas');

        canvas.width = width;
        canvas.height = height;

        return canvas;
    }

    function computeRect(img, zoom, offset) {
        let factor = Math.max(size / img.width, size / img.height) * Math.pow(2, zoom / 8);
        let width = img.width * factor;
        let height = img.height * factor;

        let x = size / 2 - width / 2 + offset.x;
        let y = size / 2 - height / 2 + offset.y;

        let rect = {
            x1: x,
            y1: y,
            x2: x + width,
            y2: y + height
        };

        return rect;
    }

    function fixEmptyArea(offset, rect) {
        offset = Object.assign({}, offset);

        if (rect.x1 > size) {
            offset.x -= rect.x1 - size;
        } else if (rect.x2 < 0) {
            offset.x -= rect.x2;
        }
        if (rect.y1 > size) {
            offset.y -= rect.y1 - size;
        } else if (rect.y2 < 0) {
            offset.y -= rect.y2;
        }

        return offset;
    }
}

function loadImage(url) {
    if (typeof url == 'string') {
        return new Promise((resolve, reject) => {
            let img = new Image();

            img.addEventListener('load', () => resolve(img));
            img.addEventListener('error', e => { console.log(e); reject(new Error('Failed to load image')); });

            img.src = url;
        });
    } else if (url instanceof Blob) {
        return new Promise((resolve, reject) => {
            let reader = new FileReader();

            reader.onload = async () => {
                try {
                    let img = await loadImage(reader.result);
                    resolve(img);
                } catch (err) {
                    reject(err);
                }
            };
            reader.onerror = () => reject(new Error('Failed to load image'));

            reader.readAsDataURL(url);
        });
    } else {
        throw new Error('Cannot load image from value of type ' + typeof url);
    }
}

export { PictureCropper }
