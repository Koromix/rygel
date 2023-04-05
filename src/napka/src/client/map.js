// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see https://www.gnu.org/licenses/.

import { render, html } from '../../node_modules/lit/html.js';
import { unsafeHTML } from '../../node_modules/lit/directives/unsafe-html.js';
import MarkdownIt from '../../node_modules/markdown-it/dist/markdown-it.js';
import { util, log, net } from '../../../web/libjs/util.js';
import { ui } from '../lib/ui.js';
import parse from '../lib/parse.js';
import { AppRunner } from '../../../web/libjs/runner.js';
import { TileMap } from '../lib/tilemap.js';

let provider = null;

let runner;
let map;
let map_markers = [];

let profile = null;

let markdown = null;

let complete_timer = null;
let complete_id = 0;

let edit_changes = new Set;
let edit_key = null;

export async function start(prov, options = {}) {
    log.pushHandler(ui.notifyHandler);

    provider = prov;
    await provider.loadMap();

    let canvas = document.querySelector('#map');

    runner = new AppRunner(canvas);
    map = new TileMap(runner);

    // Set up map
    map.init(ENV.map);
    map.setMarkers('Etablissements', map_markers);
    map.move(options.latitude, options.longitude, options.zoom);

    runner.onUpdate = map.update;
    runner.onDraw = map.draw;

    runner.idleTimeout = 1000;
    runner.start();

    profile = await net.get('api/admin/profile') || {};

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
    document.querySelector('#search input').addEventListener('input', completeAddress);

    refreshMap();

    document.body.classList.remove('loading');
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
            <div id="suggestions"></div>
        </div>

        <div style="flex: 1;"></div>

        ${provider.renderFilters()}

        <div id="admin">
            ${!isConnected() ? html`<button @click=${ui.wrap(login)}>Se connecter</button>` : ''}
            ${isConnected() ? html`<button @click=${ui.insist(logout)}>Se déconnecter</button>` : ''}
        </div>
    `, menu_el);
}

async function login() {
    await ui.dialog({
        submit_on_return: false,

        run: (render, close) => html`
            <div class="title">
                Se connecter
                <div style="flex: 1;"></div>
                <button type="button" class="secondary" @click=${ui.wrap(close)}>✖\uFE0E</button>
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
            profile = await net.post('api/admin/login', {
                username: elements.username.value,
                password: elements.password.value
            });

            renderMenu();
        }
    });
}

async function logout() {
    profile = await net.post('api/admin/logout') || {};
    renderMenu();
}

function geolocalize() {
    navigator.geolocation.getCurrentPosition(pos => {
        document.querySelector('#search input').value = '';
        map.move(pos.coords.latitude, pos.coords.longitude, 12);
    });
}

function completeAddress(e) {
    let addr = e.target.value;
    let id = ++complete_id;

    if (addr.length > 3) {
        if (complete_timer != null)
            clearTimeout(complete_timer);
        document.querySelector('#search').classList.add('busy');

        complete_timer = setTimeout(async () => {
            complete_timer = null;

            try {
                let results = await net.post('api/admin/geocode', { address: addr });

                if (complete_id == id) {
                    if (results.length) {
                        let list = document.querySelector('#suggestions');
                        render(results.map(result => html`<a @click=${e => selectAddress(result)}>${result.address}</a>`), list);
                    } else {
                        throw new Error('Aucun correspondance trouvée');
                    }
                }
            } catch (err) {
                log.error(err);
                closeSuggestions();
            } finally {
                if (complete_id == id)
                    document.querySelector('#search').classList.remove('busy');
            }
        }, 200);
    } else {
        closeSuggestions();
    }
}

function closeSuggestions() {
    let list = document.querySelector('#suggestions');
    render('', list);
}

function selectAddress(result) {
    if (result != null) {
        document.querySelector('#search input').value = result.address || '';

        map.setMarkers('User', [{
            latitude: result.latitude,
            longitude: result.longitude,

            circle: '#2d8261',
            size: 16
        }]);
        map.move(result.latitude, result.longitude, 12);
    }

    closeSuggestions();
}

export function refreshMap() {
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

    for (let marker of markers) {
        if (marker.etab == null)
            continue;

        marker.click = () => {
            let etab = isConnected() ? Object.assign({}, marker.etab) : marker.etab;

            edit_key = null;

            ui.dialog({
                run: (render, close) => html`
                    <div class="title">
                        ${makeField(etab, 'name', 'text')}
                        <div style="flex: 1;"></div>
                        <button type="button" class="secondary" @click=${ui.wrap(close)}>✖\uFE0E</button>
                    </div>

                    <div @click=${handlePopupClick}>
                        ${provider.renderPopup(etab, edit_key)}
                    </div>

                    ${isConnected() ? html`
                        <div class="footer">
                            <button type="button" class="danger"
                                    @click=${ui.confirm('Supprimer cet établissement', e => deleteEntry(etab.id).then(close))}>Supprimer</button>
                            <div style="flex: 1;"></div>
                            <button type="submit">Modifier</button>
                            <button type="button" class="secondary" @click=${ui.insist(close)}>Annuler</button>
                        </div>
                    ` : ''}
                `,
                submit: (elements) => updateEntry(etab)
            })
        };
    }

    map_markers.push(...markers);

    runner.busy();
}

function handlePopupClick(e) {
    if (e.target.tagName == 'DIV')
        toggleEdit(e, null);
}

export function makeField(etab, key, type, view = null) {
    let value = etab[key];

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
            let rendered = markdown.render(etab[key] || '');

            return html`
                <span class="sub">Text complémentaire :</span> <a @click=${e => toggleEdit(e, key)}>✎</a><br/>
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
                <label><input name="yesno" type="radio" value="1" .checked=${etab[key]} @change=${e => editEnum(etab, key, true)}> Oui</label>
                <label><input name="yesno" type="radio" value="0" .checked=${!etab[key]} @change=${e => editEnum(etab, key, false)}> Non</label>
            </span>
        `;
    } else if (Array.isArray(type) && !Array.isArray(etab[key])) {
        return html`
            <select @change=${e => editEnum(etab, key, e.target.value)}>
                ${type.map(opt => html`<option value=${opt} .selected=${opt == etab[key]}>${opt}</option>`)}
            </select>
        `;
    } else if (Array.isArray(type) && Array.isArray(etab[key])) {
        return html`
            <span style="white-space: nowrap;">
                ${type.map((opt, idx) =>
                    html`<label><input type="checkbox" .checked=${etab[key].includes(opt)}
                                       @change=${e => editMulti(etab, key, idx, opt, e.target.checked)}> ${opt}</label>`)}
            </span>
        `;
    } else if (type == 'markdown' || type == 'schedule' || type == 'address') {
        return html`<textarea rows="3" style="width: 100%;"
                              @change=${e => editText(etab, key, type, e.target.value)}
                              @keypress=${e => handleTextShortcuts(e, etab, key, type)}>${value || ''}</textarea>`;
    } else {
        return html`<input type="text" .value=${value || ''}
                           @change=${ui.wrap(e => editText(etab, key, type, e.target.value))}
                           @keypress=${e => handleTextShortcuts(e, etab, key, type)} />`;
    }
}

export function makeEdit(etab, key) {
    if (!isConnected())
        return '';

     return html` <a @click=${e => toggleEdit(e, key)}>✎</a>`;
}

function handleTextShortcuts(e, etab, key, type) {
    if (e.keyCode == 27) {
        toggleEdit(e, null);
    } else if (e.target.tagName == 'TEXTAREA' && e.keyCode == 13 && e.ctrlKey) {
        editText(etab, key, type, e.target.value);
    } else if (e.target.tagName == 'INPUT' && e.keyCode == 13) {
        editText(etab, key, type, e.target.value);
        e.preventDefault();
    }
}

function toggleEdit(e, key) {
    edit_key = key;
    ui.runDialog();

    e.stopPropagation();
    e.preventDefault();
}

async function editText(etab, key, type, value) {
    if (type == 'address') {
        let results = await net.post('api/admin/geocode', { address: value });

        if (results != null) {
            let info = results[0];

            if (!info.address)
                info.address = value;
            etab[key] = info;

            if (key == 'address')
                map.move(etab.address.latitude, etab.address.longitude);
        }
    } else if (type == 'schedule') {
        let schedule = parse.parseSchedule(value);

        etab[key] = {
            text: value,
            schedule: schedule
        };
    } else {
        etab[key] = value;
    }

    edit_key = null;
    edit_changes.add(key);

    ui.runDialog();
}

function editMulti(etab, key, idx, value, insert) {
    if (!Array.isArray(etab[key]))
        etab[key] = [];

    let values = etab[key].filter(it => it != value);
    if (insert)
        values.splice(idx, 0, value);

    etab[key] = values;
    edit_changes.add(key);
}

async function editEnum(etab, key, value) {
    etab[key] = value;
    edit_changes.add(key);

    edit_key = null;
    ui.runDialog();
}

export async function updateEntry(etab) {
    let payload = { id: etab.id };

    for (let key of edit_changes.values())
        payload[key] = etab[key];

    await net.post('api/admin/edit', payload);

    await provider.loadMap();
    refreshMap();
}

export async function deleteEntry(id) {
    await net.post('api/admin/delete', {
        id: id
    });

    await provider.loadMap();
    refreshMap();
}

export function renderMarkdown(text) {
    let html = markdown.render(text);
    return html;
}

export function isConnected() {
    return profile.userid != null;
}
