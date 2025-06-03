// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
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

import { render, html } from '../../node_modules/lit/html.js';
import { unsafeHTML } from '../../node_modules/lit/directives/unsafe-html.js';
import MarkdownIt from '../../node_modules/markdown-it/dist/markdown-it.js';
import { Util, Log, Net } from '../../../web/core/base.js';
import { UI } from '../lib/ui.js';
import * as parse from '../lib/parse.js';
import { AppRunner } from '../../../web/core/runner.js';
import { TileMap } from '../lib/tilemap.js';

let provider = null;

let canvas;
let ctx;

let runner;
let map;
let map_markers = [];
let flash_pos = null;

let profile = null;

let markdown = null;

let complete_timer = null;
let complete_id = 0;
let complete_div = null;

let edit_changes = new Set;
let edit_key = null;

async function start(prov, options = {}) {
    Log.pushHandler(UI.notifyHandler);

    provider = prov;
    await provider.loadMap();

    canvas = document.querySelector('#map');
    ctx = canvas.getContext('2d');

    runner = new AppRunner(canvas);
    map = new TileMap(runner);

    ctx.imageSmoothingEnabled = true;
    ctx.imageSmoothingQuality = 'high';

    new ResizeObserver(syncSize).observe(document.body);
    syncSize();

    // Set up map
    map.init(ENV.map);
    map.onClick = handleClick;
    if (provider.styleCluster != null)
        map.styleCluster = provider.styleCluster;
    if (provider.clusterTreshold != null)
        map.clusterTreshold = provider.clusterTreshold;
    map.setMarkers('entries', map_markers);
    map.move(options.latitude, options.longitude, options.zoom);

    runner.onUpdate = updateMap;
    runner.onDraw = drawMap;
    runner.idleTimeout = 1000;
    runner.start();

    profile = await Net.get('api/admin/profile') || {};

    // Init MarkDown converter
    {
        markdown = MarkdownIt({
            breaks: true,
            linkify: true
        });

        let prev_render = markdown.renderer.rules.link_open || ((tokens, idx, options, env, self) => self.renderToken(tokens, idx, options));

        // Open links in new tab
        markdown.renderer.rules.link_open = (tokens, idx, options, env, self) => {
            let attr_idx = tokens[idx].attrIndex('target');

            if (attr_idx >= 0) {
                tokens[idx].attrs[attr_idx][1] = '_blank';
            } else {
                tokens[idx].attrPush(['target', '_blank']);
            }

            return prev_render(tokens, idx, options, env, self);
        };
    }

    renderMenu();

    // Add event handlers
    for (let input of document.querySelectorAll('#filters input'))
        input.addEventListener('change', refreshMap);
    document.querySelector('#search input').addEventListener('input', e => completeAddress(e, showAddress));
    document.body.addEventListener('click', closeAddress);

    refreshMap();

    document.body.classList.remove('loading');
}

function syncSize() {
    let rect = document.body.getBoundingClientRect();
    if (!rect.width && !rect.height)
        return;
    runner.resize(rect.width, rect.height, window.devicePixelRatio);
}

function zoom(delta) {
    map.zoom(delta);
}

function updateMap() {
    map.update();

    if (flash_pos != null) {
        let pos = map.coordToScreen(flash_pos.latitude, flash_pos.longitude);

        flash_pos.x = pos.x;
        flash_pos.y = pos.y;

        let t = (runner.updateCounter - flash_pos.start) / 240;
        let t1 = Util.clamp(t, 0, 1);
        let t2 = Util.clamp(t - 4, 0, 1);

        flash_pos.speed = 0.5 + 4 * Math.max(0, 1 - t1);
        flash_pos.radius = 10 + 390 * (1 - easeInOutSine(t1));
        flash_pos.opacity = 1 - t2;

        if (flash_pos.opacity <= 0)
            flash_pos = null;

        runner.busy();
    }
}

function easeInOutSine(t) {
    return -(Math.cos(Math.PI * t) - 1) / 2;
}

function drawMap() {
    map.draw();

    if (flash_pos != null) {
        ctx.save();

        ctx.fillStyle = '#ff660066';
        ctx.strokeStyle = '#ff6600';

        ctx.lineWidth = 4;
        ctx.lineDashOffset = flash_pos.speed * -runner.drawCounter;
        ctx.setLineDash([6, 6]);
        ctx.globalAlpha = flash_pos.opacity;

        ctx.beginPath();
        ctx.arc(flash_pos.x, flash_pos.y, flash_pos.radius, 0, Math.PI * 2, false);
        ctx.fill();
        ctx.stroke();

        ctx.restore();
    }
}

function renderMenu() {
    let menu_el = document.querySelector('#menu');

    menu_el.classList.toggle('admin', isConnected());

    render(html`
        <div id="search">
            <label>
                ${navigator.geolocation != null ?
                    html`<a @click=${geolocalize}>
                             <img src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABgAAAAYCAYAAADgdz34AAACDElEQVRIia3V32vPYRQH8BcbMkxMKYSmYUwt86MUEqWWCyntSiMXpLgQCzeUXUgoWtnFcqFcuPA7RexCcSFGwtLKzzAWofwBLs6z9mlt3+/ni/fVOc9znvN+znnOOQ/5UYFaLMNslJVwtiAacQu/0I2HeIM+dKDmbx1PxA28QBPGDdqfgUOJ6CBGlOK8Es9wGuVFbKeJqM6UQnAZbYPWyrEE6zBniAu9RHMe52vRg1GZtR34gqe4gw/owvKMTT0+YXwxgkvYltFb8QQLBtltxleszqxdUSSKMlEtk5PegI+oSntbcQyrMiRvMTrpW3CxEMH05LAfbWhJcgc6cQDvsTGt38f6zIUeFyKYh1cZvRNrxHv8Fs0Gm3A9ySexN8k14v2GxRR8y+g3sUHUeJ+Bd2jBuSS3Y2eSV+BBIYKR+ClyTjRSe5KbEkkXXqM6RfYOdcmmGRcKERBl2JjkKvSKNPXrDRiTojqOa5mz7dhdjOAwTmX01aIcj2AuJolU3BblW5Wx7cHiYgR1opGys2UWzorUfMcj7MPYjE29GIK5ZlK3gVrPixOiR3Jhl5hHeTFBVF913gMVIu8Lc9rvV6SDh8Ie3M1hN1WUb22pBGWi5rcXsBmBqzhaqvN+zMdnrBxmvxX3RF/8NVaJB9wlOp34Ts/juYHJ+09YJG7aK3rgh+jayv/hPIuZWCoiyIU/yGpi++3oSLIAAAAASUVORK5CYII="
                                  alt="Localisation GPS"  style="vertical-align: top;"/>
                         </a>` : ''}
                <input type="search" autocomplete="off" placeholder="Entrez votre adresse..."/>
            </label>
        </div>

        <div style="flex: 1;"></div>

        ${provider.renderFilters()}
    `, menu_el);
}

async function login() {
    await UI.dialog({
        submit_on_return: false,

        run: (render, close) => html`
            <div class="title">
                Se connecter
                <div style="flex: 1;"></div>
                <button type="button" class="secondary" @click=${UI.wrap(close)}>✖\uFE0E</button>
            </div>

            <label>
                <span>Utilisateur</span>
                <input name="username" type="text" />
            </label>
            <label>
                <span>Mot de passe</span>
                <input name="password" type="password" />
            </label>
            <button type="submit">Se connecter</button>
        `,

        submit: async (elements) => {
            profile = await Net.post('api/admin/login', {
                username: elements.username.value,
                password: elements.password.value
            });

            renderMenu();
        }
    });
}

async function logout() {
    profile = await Net.post('api/admin/logout') || {};
    renderMenu();
}

function geolocalize() {
    navigator.geolocation.getCurrentPosition(pos => {
        document.querySelector('#search input').value = '';
        map.move(pos.coords.latitude, pos.coords.longitude, 12);
    });
}

function completeAddress(e, func) {
    let addr = e.target.value;
    let id = ++complete_id;

    if (addr.length > 3) {
        if (complete_timer != null)
            clearTimeout(complete_timer);

        complete_timer = setTimeout(async () => {
            complete_timer = null;

            try {
                let results = await Net.post('api/admin/geocode', { address: addr });

                if (complete_id == id) {
                    if (results.length) {
                        let target = e.target;
                        let rect = e.target.getBoundingClientRect();

                        if (complete_div == null) {
                            complete_div = document.createElement('div');

                            complete_div.id = 'suggestions';
                            complete_div.addEventListener('click', e => { e.stopPropagation(); });

                            document.body.appendChild(complete_div);
                        }

                        complete_div.style.left = rect.left + 'px';
                        complete_div.style.top = rect.bottom + 'px';

                        render(results.map(result => html`<a @click=${() => { func(e, result); closeAddress(); }}>${result.address}</a>`), complete_div);
                    }
                }
            } catch (err) {
                Log.error(err);
                closeAddress();
            }
        }, 200);
    } else {
        closeAddress();
    }
}

function closeAddress() {
    if (complete_div == null)
        return;

    document.body.removeChild(complete_div);
    complete_div = null;
}

function showAddress(e, result) {
    e.target.value = result.address || '';

    flash_pos = {
        start: runner.updateCounter,

        latitude: result.latitude,
        longitude: result.longitude,

        x: null,
        y: null,

        speed: 1,
        radius: 480,
        opacity: 1
    };

    map.move(result.latitude, result.longitude, 14);
}

function refreshMap() {
    map_markers.length = 0;

    // Run filters
    let filters = [];
    {
        let groups = document.querySelectorAll('#filters > .group');

        for (let group of groups) {
            let inputs = Array.from(group.querySelectorAll('input:checked'));
            let tests = inputs.map(input => new Function('return ' + input.dataset.filter));

            filters.push(it => tests.some(test => test.call(it)));
        }
    }

    let markers = provider.fillMap(filters);
    map_markers.push(...markers);

    map.refresh();

    runner.busy();
}

async function handleClick(markers, clickable) {
    if (markers.length > 1) {
        let zoomed = map.reveal(markers, true);

        if (!zoomed) {
            if (!clickable)
                return;

            await UI.dialog({
                run: (render, close) => html`
                    <div class="title">
                        Plusieurs entrées disponibles
                        <div style="flex: 1;"></div>
                        <button type="button" class="secondary" @click=${UI.wrap(close)}>✖\uFE0E</button>
                    </div>

                    <div @click=${handlePopupClick}>
                        ${markers.map(marker => html`
                            <a @click=${UI.wrap(e => { handleClick([marker], true); close(); })}>${marker.entry.name}</a><br>
                        `)}
                    </div>
                `
            });
        }
    } else if (provider.renderEntry != null) {
        if (!clickable)
            return;

        let marker = markers[0];
        let entry = isConnected() ? Object.assign({}, marker.entry) : marker.entry;

        edit_key = null;

        try {
            await UI.dialog({
                run: (render, close) => html`
                    <div class="title">
                        ${edit_key == 'name' ? makeField(entry, 'name', 'markdown') : ''}
                        ${edit_key != 'name' ? html`<div style="max-width: 660px;">${makeField(entry, 'name', 'markdown')}</div>` : ''}
                        <div style="flex: 1;"></div>
                        <button type="button" class="secondary" @click=${UI.wrap(close)}>✖\uFE0E</button>
                    </div>

                    <div @click=${handlePopupClick}>
                        ${provider.renderEntry(entry, edit_key)}
                    </div>

                    ${isConnected() ? html`
                        <div class="footer">
                            <button type="button" class="danger"
                                    @click=${UI.confirm('Supprimer cet établissement', e => deleteEntry(entry.id).then(close))}>Supprimer</button>
                            <div style="flex: 1;"></div>
                            <button @click=${closeOrSubmit} type="submit">Modifier</button>
                            <button type="button" class="secondary" @click=${UI.insist(close)}>Annuler</button>
                        </div>
                    ` : ''}
                `,

                submit: (elements) => updateEntry(entry)
            });
        } finally {
            closeAddress();
        }
    }
}

function closeOrSubmit(e) {
    if (edit_key != null)
        toggleEdit(e, null);
}

function handlePopupClick(e) {
    if (e.target.tagName == 'DIV')
        toggleEdit(e, null);

    closeAddress();
}

function makeField(entry, key, type, view = null) {
    let value = entry[key];

    if (type == 'address' && value != null) {
        value = value.address;
    } else if (type == 'schedule' && value != null) {
        value = value.text;
    }

    if (view == null) {
        view = value;

        if (Array.isArray(view) && typeof view[0] == 'string') {
            view = view.join(', ');
        } else if (type == 'boolean') {
            view = view ? 'Oui' : 'Non';
        } else if (view == null || typeof view == 'string') {
            view = view || html`<span class="sub">(inconnu)</span>`;
        }
    }

    // View mode
    if (!isConnected()) {
        return view;
    } else if (key != edit_key) {
        if (type == 'markdown') {
            let rendered = markdown.render(entry[key] || '');

            return html`
                <span class="sub">Modifier :</span> <a @click=${e => toggleEdit(e, key)}>✎</a><br/>
                ${unsafeHTML(rendered)}
            `;
        } else {
            return html`${view} <a @click=${e => toggleEdit(e, key)}>✎</a>`;
        }
    }

    // Edition mode
    if (type == 'boolean') {
        return html`
            <span style="white-space: nowrap;">
                <label><input name="yesno" type="radio" value="1" .checked=${entry[key]} @change=${e => editEnum(entry, key, true)}> Oui</label>
                <label><input name="yesno" type="radio" value="0" .checked=${!entry[key]} @change=${e => editEnum(entry, key, false)}> Non</label>
            </span>
        `;
    } else if (Array.isArray(type) && !Array.isArray(entry[key])) {
        return html`
            <select @change=${e => editEnum(entry, key, e.target.value)}>
                ${type.map(opt => html`<option value=${opt} .selected=${opt == entry[key]}>${opt}</option>`)}
            </select>
        `;
    } else if (Array.isArray(type) && Array.isArray(entry[key])) {
        return html`
            <span style="white-space: nowrap;">
                ${type.map((opt, idx) =>
                    html`<label><input type="checkbox" .checked=${entry[key].includes(opt)}
                                       @change=${e => editMulti(entry, key, idx, opt, e.target.checked)}> ${opt}</label>`)}
            </span>
        `;
    } else if (type == 'markdown' || type == 'schedule') {
        return html`
            <textarea rows="3" style="width: 100%;"
                      @change=${e => editText(entry, key, type, e.target.value)}
                      @keypress=${e => handleTextShortcuts(e, entry, key, type)}>${value || ''}</textarea>
        `;
    } else if (type == 'address') {
        return html`
            <textarea rows="3" style="width: 100%;"
                      @input=${e => completeAddress(e, (_, result) => editText(entry, key, type, result.address))}
                      @keypress=${e => handleTextShortcuts(e, entry, key, type)}>${value || ''}</textarea>
        `;
    } else {
        return html`
            <input type="text" .value=${value || ''}
                   @change=${UI.wrap(e => editText(entry, key, type, e.target.value))}
                   @keypress=${e => handleTextShortcuts(e, entry, key, type)} />
        `;
    }
}

function makeEdit(entry, key) {
    if (!isConnected())
        return '';

     return html` <a @click=${e => toggleEdit(e, key)}>✎</a>`;
}

function handleTextShortcuts(e, entry, key, type) {
    if (e.keyCode == 27) {
        toggleEdit(e, null);
    } else if (e.target.tagName == 'TEXTAREA' && e.keyCode == 13 && e.ctrlKey) {
        editText(entry, key, type, e.target.value);
    } else if (e.target.tagName == 'INPUT' && e.keyCode == 13) {
        editText(entry, key, type, e.target.value);
        e.preventDefault();
    }
}

function toggleEdit(e, key) {
    edit_key = key;
    UI.runDialog();

    e.stopPropagation();
    e.preventDefault();
}

async function editText(entry, key, type, value) {
    if (type == 'address') {
        let results = await Net.post('api/admin/geocode', { address: value });

        if (results != null) {
            let info = results.find(result => result.address == value) || results[0];

            if (!info.address)
                info.address = value;
            entry[key] = info;

            if (key == 'address')
                map.move(entry.address.latitude, entry.address.longitude);
        }
    } else if (type == 'schedule') {
        let schedule = parse.parseSchedule(value);

        entry[key] = {
            text: value,
            schedule: schedule
        };
    } else {
        entry[key] = value;
    }

    edit_key = null;
    edit_changes.add(key);

    UI.runDialog();
}

function editMulti(entry, key, idx, value, insert) {
    if (!Array.isArray(entry[key]))
        entry[key] = [];

    let values = entry[key].filter(it => it != value);
    if (insert)
        values.splice(idx, 0, value);

    entry[key] = values;
    edit_changes.add(key);
}

async function editEnum(entry, key, value) {
    entry[key] = value;
    edit_changes.add(key);

    edit_key = null;
    UI.runDialog();
}

async function updateEntry(entry) {
    let payload = { id: entry.id };

    for (let key of edit_changes.values())
        payload[key] = entry[key];

    await Net.post('api/admin/edit', payload);

    await provider.loadMap();
    refreshMap();
}

async function deleteEntry(id) {
    await Net.post('api/admin/delete', {
        id: id
    });

    await provider.loadMap();
    refreshMap();
}

function renderMarkdown(text) {
    let html = markdown.render(text);
    return html;
}

function isConnected() {
    return profile.userid != null;
}

async function loadTexture(url) {
    let response = await Net.fetch(url);

    let blob = await response.blob();
    let texture = await createImageBitmap(blob);

    return texture;
}

export {
    start,

    zoom,
    login,
    logout,
    updateEntry,
    deleteEntry,
    refreshMap,
    makeField,
    makeEdit,
    renderMarkdown,
    isConnected,
    loadTexture
}
