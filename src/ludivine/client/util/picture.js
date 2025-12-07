// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import { render, html, svg, live } from 'vendor/lit-html/lit-html.bundle.js';
import { Util, Log } from 'lib/web/base/base.js';
import * as UI from 'lib/web/ui/ui.js';
import { loadImage } from '../core/misc.js';
import { ColorPicker } from './color.js';

import './picture.css';

if (typeof T == 'undefined')
    T = {};

Object.assign(T, {
    browse_for_image: 'Browse for image',
    cancel: 'Cancel',
    clear_picture: 'Clear picture',
    drag_paste_or_browse_image: 'Drop an image, paste it or use the \'Browse\' button',
    save: 'Save',
    upload_image: 'Upload image',
    zoom: 'Zoom',

    notion_categories: {
        face: 'Face',
        hair: 'Hair',
        eyes: 'Eyes',
        eyebrows: 'Eyebrows',
        glasses: 'Glasses',
        nose: 'Nose',
        mouth: 'Mouth',
        beard: 'Beard',
        accessories: 'Accessories'
    }
});

const NOTION_DEFAULTS = {
    parts: {
        face: 0,
        hair: 1,
        eyes: 0,
        eyebrows: 0,
        glasses: 0,
        nose: 0,
        mouth: 0,
        beard: 0
    },
    colors: {
        face: '#faddbc',
        hair: '#7b3f00',
        beard: '#7b3f00'
    }
};

const NOTION_PALETTES = {
    face: [
        '#faddbc',
        '#e0ba95',
        '#bf8e68',
        '#9b643d',
        '#594439'
    ],
    hair: [
        '#fffacd',
        '#c19a6b',
        '#7b3f00',
        '#2f1e0e',
        '#000000',
        '#954535',
        '#b06500',
        '#d3d3d3',
        '#ffffff'
    ],
    beard: [
        '#fffacd',
        '#c19a6b',
        '#7b3f00',
        '#2f1e0e',
        '#000000',
        '#954535',
        '#b06500',
        '#d3d3d3',
        '#ffffff'
    ]
}

function PictureCropper(title, size) {
    let self = this;

    let refresh_func = null;
    let preview = makeCanvas(size, size);

    let image_format = 'image/png';
    let default_url = null;
    let notion_assets = null;

    Object.defineProperties(this, {
        imageFormat: { get: () => image_format, set: format => { image_format = format; }, enumerable: true },

        defaultURL: { get: () => default_url, set: url => { default_url = url; }, enumerable: true },
        notionAssets: { get: () => notion_assets, set: assets => { notion_assets = assets; }, enumerable: true }
    });

    let current_mode = null;

    let custom_img;
    let init_url;
    let is_default;
    let zoom;
    let offset;
    let move_pointer = null;
    let prev_offset = null;

    let notion = Util.assignDeep({}, NOTION_DEFAULTS);
    let notion_cat = 'face';

    let apply_func;

    this.change = async function(prev = null, func = null) {
        await load(prev);

        init_url = prev;
        apply_func = func;
        is_default = true;

        let blob = await UI.dialog({
            overlay: true,

            open: () => {
                preview.addEventListener('pointerdown', handleCustomEvent);
                preview.addEventListener('wheel', handleCustomEvent);
                preview.addEventListener('dragenter', e => e.preventDefault());
                preview.addEventListener('dragover', dropImage);
                preview.addEventListener('drop', UI.wrap(dropImage));

                window.addEventListener('paste', dropImage);
            },

            close: () => {
                window.removeEventListener('paste', dropImage);
            },

            run: (render, close) => {
                refresh_func = render;

                return html`
                    <div class="tabbar">
                        ${notion_assets != null ?
                            html`<a class=${current_mode == 'notion' ? 'active' : ''} @click=${UI.wrap(e => switchMode('notion'))}>Avatar virtuel</a>` : ''}
                        <a class=${current_mode == 'custom' ? 'active' : ''} @click=${UI.wrap(e => switchMode('custom'))}>Image personnalisée</a>
                    </div>

                    <div class="tab">
                        ${current_mode == 'notion' ? renderNotion() : ''}
                        ${current_mode == 'custom' ? renderCustom() : ''}

                        <div class="actions">
                            <button type="button" class="secondary" @click=${UI.insist(close)}>${T.cancel}</button>
                            <button type="submit">${T.save}</button>
                        </div>
                    </div>
                `;
            },

            submit: apply
        });

        return blob;
    }

    function switchMode(mode) {
        current_mode = mode;
        refresh_func();
    }

    function renderNotion() {
        drawPreview(size);

        return html`
            <div class="box">
                <div class="header">Avatar</div>

                <div class="row" style="justify-content: center; align-items: center;">
                    <div class="not_preview" style=${`--size: ${size}px`}>${preview}</div>

                    <div class="not_colors">
                        ${Object.keys(notion.colors).map(cat => {
                            if (cat != 'face' && !notion.parts[cat])
                                return '';

                            let palette = NOTION_PALETTES[cat] ?? [];
                            let other = palette.every(color => color != notion.colors[cat]);

                            return html`
                                <div class="widget">
                                    <label>${T.notion_categories[cat]}</label>
                                    <div class="not_palette">
                                        ${palette.map(color => html`
                                            <button type="button" class=${color == notion.colors[cat] ? 'active' : ''} @click=${UI.wrap(e => switchColor(cat, color))}>
                                                <div style=${'background: ' + color}></div>
                                            </button>
                                        `)}
                                        <button type="button" class=${'small' + (other ? ' active' : '')}
                                                @click=${UI.wrap(e => pickColor(e, cat))}>Autre couleur</button>
                                    </div>
                                </div>
                            `;
                        })}
                    </div>
                </div>
            </div>

            <div class="box">
                <div class="not_categories">
                    ${Object.keys(notion.parts).map(cat =>
                        html`<button type="button" class=${'small' + (notion_cat == cat ? ' active' : '')}
                                     @click=${UI.wrap(e => switchCategory(cat))}>${T.notion_categories[cat]}</button>`)}
                </div>

                <div class="not_parts">
                    ${notion_assets[notion_cat].map((xml, idx) => {
                        let active = (idx == notion.parts[notion_cat]);
                        let fill = active ? '#dddddd' : 'none';

                        return svg`
                            <svg viewBox="0 0 1180 1180" class=${active ? 'active' : ''} fill="none"
                                 @click=${UI.wrap(e => switchPart(notion_cat, idx))}>
                                ${renderPart(notion_cat, xml)}
                            </svg>
                        `;
                    })}
                </div>
            </div>
        `;
    }

    function switchCategory(cat) {
        notion_cat = cat;
        refresh_func();
    }

    function switchPart(cat, idx) {
        notion.parts[cat] = idx;
        refresh_func();
    }

    async function pickColor(e, cat) {
        let picker = new ColorPicker;

        picker.changeHandler = color => {
            notion.colors[cat] = color;
            refresh_func();
        };

        let prev = notion.colors[cat];

        try {
            await picker.pick(e, prev);
        } catch (err) {
            notion.colors[cat] = prev;
            refresh_func();

            throw err;
        }
    }

    function switchColor(cat, color) {
        notion.colors[cat] = color;
        refresh_func();
    }

    function renderCustom() {
        drawPreview(size);

        return html`
            <div class="row" style="justify-content: center;">
                <div class="box">
                    <div class="header">Avatar</div>
                    <div class=${is_default ? 'pic_preview' : 'pic_preview interactive'}
                         style=${`--size: ${size}px`}>${preview}</div>
                    <div class="pic_legend">${T.drag_paste_or_browse_image}</div>
                </div>

                <div class="box">
                    <div>
                        <label>
                            <span>${T.zoom}</span>
                            <input type="range" min="-32" max="32"
                                   .value=${live(zoom)} ?disabled=${is_default}
                                   @input=${UI.wrap(e => changeZoom(parseInt(e.target.value, 10) - zoom))} />
                        </label>
                        <label>
                            <span>${T.upload_image}</span>
                            <input type="file" name="file" style="display: none;"
                                   accept="image/png, image/jpeg, image/gif, image/webp"
                                   @change=${UI.wrap(e => load(e.target.files[0]))} />
                            <button type="button" @click=${e => { e.target.parentNode.click(); e.preventDefault(); }}>${T.browse_for_image}</button>
                        </label>
                        <label>
                            <button type="button" class="secondary"
                                    ?disabled=${is_default && init_url == null}
                                    @click=${UI.insist(e => load(null))}>${T.clear_picture}</button>
                        </label>
                    </div>
                </div>
            </div>
        `;
    }

    async function load(obj) {
        if (notion_assets != null && Util.isPodObject(obj)) {
            current_mode = 'notion';

            for (let key of ['parts', 'colors']) {
                let sub = obj[key];

                if (sub != null) {
                    for (let cat in sub) {
                        if (Object.hasOwn(notion[key], cat))
                            notion[key][cat] = sub[cat];
                    }
                }
            }

            is_default = true;
        } else {
            if (obj != null) {
                custom_img = await loadImage(obj);
                current_mode = 'custom';
            } else {
                custom_img = null;

                if (current_mode == null)
                    current_mode = 'notion';
            }

            is_default = (obj == null);
        }

        if (custom_img == null && default_url != null)
            custom_img = await loadImage(default_url);

        init_url = null;
        zoom = 0;
        offset = { x: 0, y: 0 };

        if (refresh_func != null)
            refresh_func();
    }

    function handleCustomEvent(e) {
        if (current_mode != 'custom' || is_default)
            return;

        if (e.type == 'pointerdown') {
            if (move_pointer != null)
                return;

            move_pointer = e.pointerId;
            prev_offset = Object.assign({}, offset);

            preview.setPointerCapture(move_pointer);
            preview.style.cursor = 'grabbing';

            preview.addEventListener('pointermove', handleCustomEvent);
            preview.addEventListener('pointerup', handleCustomEvent);
            document.body.addEventListener('keydown', handleCustomEvent);
        } else if (e.type == 'pointermove') {
            offset.x += e.movementX;
            offset.y += e.movementY;

            let rect = computeRect(custom_img, zoom, offset);
            offset = fixEmptyArea(offset, rect);

            refresh_func();
        } else if (e.type == 'pointerup') {
            release(e);
        } else if (e.type == 'keydown') {
            offset = prev_offset;
            refresh_func();

            release(e);
        } else if (e.type == 'wheel') {
            let rect = preview.getBoundingClientRect();
            let at = { x: e.clientX - rect.left, y: e.clientY - rect.top };

            if (e.deltaY < 0) {
                changeZoom(1, at);
            } else if (e.deltaY > 0) {
                changeZoom(-1, at);
            }

            e.preventDefault();
        }

        function release(e) {
            preview.releasePointerCapture(move_pointer);
            preview.style.cursor = null;

            preview.removeEventListener('pointermove', handleCustomEvent);
            preview.removeEventListener('pointerup', handleCustomEvent);
            document.body.removeEventListener('keydown', handleCustomEvent);

            move_pointer = null;
            prev_offset = null;
        }
    }

    function dropImage(e) {
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
            let rect = computeRect(custom_img, zoom, offset);

            // Convert to relative coordinates (-1 to 1) in image space
            let center = {
                x: -2 * (rect.x1 - at.x) / (rect.x2 - rect.x1) - 1,
                y: -2 * (rect.y1 - at.y) / (rect.y2 - rect.y1) - 1
            };

            let new_zoom = Math.min(Math.max(zoom + delta, -32), 32);

            // Compute displacement caused by zoom effect
            let displace = Math.max(size / custom_img.width, size / custom_img.height) *
                           (Math.pow(2, new_zoom / 8) - Math.pow(2, zoom / 8));

            zoom = new_zoom;
            offset.x -= center.x * displace * custom_img.width / 2;
            offset.y -= center.y * displace * custom_img.height / 2;
        }

        let rect = computeRect(custom_img, zoom, offset);
        offset = fixEmptyArea(offset, rect);

        refresh_func();
    }

    function computeRect(custom_img, zoom, offset) {
        let factor = Math.max(size / custom_img.width, size / custom_img.height) * Math.pow(2, zoom / 8);
        let width = custom_img.width * factor;
        let height = custom_img.height * factor;

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

        if (rect.x1 > size * 0.9) {
            offset.x -= Math.round(rect.x1 - size * 0.9);
        } else if (rect.x2 < size * 0.1) {
            offset.x -= Math.round(rect.x2 - size * 0.1);
        }
        if (rect.y1 > size * 0.9) {
            offset.y -= Math.round(rect.y1 - size * 0.9);
        } else if (rect.y2 < size * 0.1) {
            offset.y -= Math.round(rect.y2 - size * 0.1);
        }

        return offset;
    }

    function drawPreview(size) {
        let ctx = preview.getContext('2d');

        draw(ctx, size, () => {
            ctx.clearRect(0, 0, size, size);

            ctx.fillStyle = 'white';
            ctx.beginPath();
            ctx.arc(size / 2, size / 2, size / 2, 0, 2 * Math.PI);
            ctx.fill();
            ctx.clip();
        });
    }

    async function draw(ctx, size, prepare) {
        switch (current_mode) {
            case 'custom': {
                if (custom_img == null)
                    return;

                let rect = computeRect(custom_img, zoom, offset);

                prepare();
                ctx.drawImage(custom_img, rect.x1, rect.y1, rect.x2 - rect.x1, rect.y2 - rect.y1);
            } break;

            case 'notion': {
                let el = drawNotion();

                let xml = new XMLSerializer().serializeToString(el);
                let url = 'data:image/svg+xml;base64,' + btoa(xml);
                let img = await loadImage(url);

                prepare();
                ctx.drawImage(img, 0, 0, size, size);
            } break;
        }
    }

    async function apply() {
        if (current_mode == 'custom' && is_default) {
            if (apply_func != null)
                await apply_func(null, null);

            return;
        }

        let blob = await new Promise(async (resolve, reject) => {
            let canvas = makeCanvas(size, size);
            let ctx = canvas.getContext('2d');

            await draw(ctx, size, () => {
                ctx.beginPath();
                ctx.arc(size / 2, size / 2, size / 2, 0, 2 * Math.PI);
                ctx.clip();
            });

            canvas.toBlob(resolve, image_format, 1);
        });

        if (apply_func != null) {
            switch (current_mode) {
                case 'notion': { await apply_func(blob, notion); } break;
                default: { await apply_func(blob, null); } break;
            }
        }

        return blob;
    }

    function drawNotion() {
        let el = document.createElementNS('http://www.w3.org/2000/svg', 'svg');

        el.setAttribute('width', size);
        el.setAttribute('height', size);
        el.setAttribute('viewBox', '0 0 1180 1180');
        el.setAttribute('fill', 'none');

        render(svg`
            <g id="picture">
                ${Object.keys(notion_assets).map(cat => {
                    let idx = notion.parts[cat];
                    let xml = notion_assets[cat][idx];

                    return renderPart(cat, xml);
                })}
            </g>
        `, el);

        let g = el.querySelector('#picture');
        let scale = 1.2;

        // Determine bounding box, which requires DOM insertion
        let center = null;
        {
            el.style = `
                width: 0 !important;
                height: 0 !important;
                position: fixed !important;
                overflow: hidden !important;
                visibility: hidden !important;
                opacity: 0 !important;
            `;
            document.body.appendChild(el);

            let bbox = g.getBBox();

            center = [
                bbox.x + bbox.width / 2,
                bbox.y + bbox.height / 2
            ];

            document.body.removeChild(el);
            el.style = '';
        }

        let transform = [590 - center[0] * scale, 590 - center[1] * scale];
        g.setAttribute('transform', `
            scale(${scale} ${scale})
            translate(${transform[0] * 0.5} ${transform[1] * 0.5})
        `);

        return el;
    }

    function renderPart(cat, xml) {
        let parser = new DOMParser;

        let svg = parser.parseFromString(xml, 'image/svg+xml');
        let g = svg.querySelector('g');

        switch (cat) {
            case 'face': {
                let path = g.querySelector('path');

                path.setAttribute('fill', notion.colors.face);
                path.setAttribute('stroke-width', 18);
            } break;

            case 'hair': {
                let nodes = g.querySelectorAll('path, line');

                for (let node of nodes) {
                    node.setAttribute('fill', notion.colors.hair);
                    node.setAttribute('stroke', notion.colors.hair);
                }
            } break;

            case 'beard': {
                let nodes = g.querySelectorAll('path, line');

                for (let node of nodes) {
                    node.setAttribute('fill', notion.colors.beard);
                    node.setAttribute('stroke', notion.colors.beard);
                }
            } break;
        }

        return g;
    }

    function makeCanvas(width, height) {
        let canvas = document.createElement('canvas');

        canvas.width = width;
        canvas.height = height;

        return canvas;
    }
}

export { PictureCropper }
