// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

import { render, html, live, unsafeHTML } from 'vendor/lit-html/lit-html.bundle.js';
import { Util, LruMap, Log, Net, HttpError } from 'lib/web/base/base.js';
import * as UI from 'lib/web/ui/ui.js';
import * as App from './main.js';
import { route, cache, sw } from './main.js';
import * as UserMod from './user.js';
import {
    formatSize,
    writeClipboard
} from './common.js';
import { ASSETS } from '../assets/assets.js';

const EXPIRATION_DAYS = [1, 7, 30, 90];
const DEFAULT_EXPIRATION = 7;
const FRAGMENT_SIZE = 2097152;

let send_file = null;
let send_drops = new LruMap(4);

async function runSend() {
    if (!App.isLogged())
        return UserMod.runLogin();

    UI.main(html`
        <div class="header">${T.send_file}</div>

        <form style="text-align: center;" @submit=${UI.wrap(submit)}>
            <div class="block" style="align-items: center;">
                <label>
                    <span>${T.expiration}</span>
                    <select name="expiration">
                        ${EXPIRATION_DAYS.map(days =>
                            html`<option value=${days} ?selected=${days == DEFAULT_EXPIRATION}>${T.count(T.expiration_delay, days)}</option>`)}
                    </select>
                </label>
                <div class="sub">${T.drag_or_browse_file}</div>
                <label>
                    <span>${T.file}</span>
                    <input type="file" name="file" style="display: none;" @change=${change} />
                    <button type="button" @click=${e => { e.target.parentNode.click(); e.preventDefault(); }}>${T.browse_for_file}</button>
                </label>
                ${send_file != null ? html`<div class="sub">${send_file.name} (${formatSize(send_file.size)})` : ''}
            </div>

            <div class="actions">
                <button type="submit" ?disabled=${send_file == null}>${T.send}</button>
            </div>
        </form>
    `);

    // UI.main() resets these each time
    window.ondragenter = (e) => e.preventDefault();
    window.ondragover = drop;
    window.ondrop = drop;

    async function change(e) {
        send_file = e.target.files[0];
        App.go();
    }

    async function submit(e) {
        let form = e.currentTarget;
        let elements = form.elements;

        let file = send_file;
        let expiration = parseInt(elements.expiration.value, 10) * 86400000;

        let kid = await prepare(file, expiration);

        let drop = {
            kid: kid,
            name: file.name,
            size: file.size,
            uploaded: 0,
            error: null
        };

        send_file = null;
        send_drops.set(kid, drop);

        let url = App.makeURL({ mode: 'drop', drop: kid });
        App.go(url);

        try {
            await send(kid, file, uploaded => progress(drop, uploaded));
        } catch (err) {
            drop.error = err;
            throw err;
        }
    }

    function progress(drop, uploaded) {
        drop.uploaded = uploaded;
        App.go();
    }

    function drop(e) {
        let dt = e.dataTransfer || e.clipboardData;

        let src = null;
        let found = false;

        for (let i = 0; i < dt.items.length; i++) {
            let item = dt.items[i];

            if (item.kind == 'file') {
                let file = item.getAsFile();

                if (file != null)
                    src = file;

                found = true;
                break;
            }
        }

        if (e.type == 'dragover') {
            dt.dropEffect = found ? 'move' : 'none';
        } else if (e.type == 'drop' && src != null) {
            send_file = src;
            App.go();
        }

        e.preventDefault();
    }
}

async function prepare(file, expiration) {
    let info = await Net.post('/api/drop/create', {
        name: file.name,
        size: file.size,
        expiration: expiration
    });

    return info.kid;
}

async function send(kid, file, progress = () => {}) {
    let stream = file.stream();
    let reader = stream.getReader();

    // Upload fragments
    {
        let chunks = [];
        let available = 0;
        let complete = false;

        let fragment = 0;

        do {
            while (!complete && available < FRAGMENT_SIZE) {
                let { value, done } = await reader.read();

                if (done) {
                    complete = true;
                    break;
                }

                chunks.push(value);
                available += value.length;
            }

            while (available >= FRAGMENT_SIZE || complete) {
                let buf = new Uint8Array(FRAGMENT_SIZE);
                let offset = 0;

                if (!available)
                    break;

                while (available && offset < buf.length) {
                    let chunk = chunks[0];
                    let remain = buf.length - offset;

                    if (chunk.length <= remain) {
                        buf.set(chunk, offset);

                        offset += chunk.length;
                        available -= chunk.length;

                        chunks.shift();
                    } else {
                        buf.set(chunk.slice(0, remain), offset);

                        offset += remain;
                        available -= remain;

                        chunks[0] = chunk.slice(remain);
                        break;
                    }
                }

                // Upload fragment
                {
                    let url = Util.pasteURL('/api/drop/upload', { kid: kid, fragment: fragment });

                    let response = await Net.fetch(url, {
                        method: 'PUT',
                        body: buf.slice(0, offset),
                        timeout: 30000
                    });
                    if (!response.ok) {
                        let msg = await Net.readError(response);
                        throw new HttpError(response.status, msg);
                    }
                }

                let uploaded = Math.min((fragment + 1) * FRAGMENT_SIZE, file.size);

                // Mark progress
                {
                    let url = Util.pasteURL('/api/drop/mark', { kid: kid });
                    await Net.post(url, { uploaded: uploaded });
                }

                progress(uploaded);

                fragment++;
            }
        } while (!complete);
    }
}

async function runDrop() {
    let cached = false;

    if (route.drop != null) {
        cache.drop = send_drops.get(route.drop);
        cached = (cache.drop != null);

        if (!cached) {
            try {
                let url = Util.pasteURL('/api/drop/info', { kid: route.drop });
                cache.drop = await Net.cache('drop', url);
            } catch (err) {
                if (err.status != 404 && err.status != 422)
                    throw err;

                cache.drop = null;
            }
        }
    } else {
        cache.drop = null;
    }

    route.drop = cache.drop?.kid;

    if (cache.drop == null) {
        App.go('/send');
        return;
    }

    if (cache.drop.uploaded < cache.drop.size) {
        let progress = Math.floor(cache.drop.uploaded / cache.drop.size * 100);

        UI.main(html`
            <div class="header">${T.send_file}</div>

            <div class="block" style="align-items: center;">
                <p>${cache.drop.name}</p>
                <progress id="uploading" value=${cache.drop.uploaded} max=${cache.drop.size}>${progress}%</progress>
                ${cache.drop.error != null ? html`
                    <p style="color: red;">
                        ${T.error_has_occured}<br>
                        ${cache.drop.error.message}
                    </p>
                ` : ''}
            </div>
        `);
    } else {
        let url = App.makeURL({ mode: 'drop', drop: cache.drop.kid });

        UI.main(html`
            <div class="header">${cache.drop.name}</div>

            <div class="block" style="align-items: center;">
                <div>${formatSize(cache.drop.size)}</div>
                ${cached ? html`<input id="link" type="text" style="width: 60em; text-align: center;" readonly value=${ENV.url + url} />` : ''}
            </div>

            <div class="actions">
                ${cached ? html`<button @click=${UI.wrap(e => writeClipboard(T.download_link, ENV.url + url))}>${T.copy_download_link}</button>` : ''}
                ${!cached ? html`<button @click=${e => window.location.href = '/api/drop/download/' + cache.drop.kid}>${T.download}</button>` : ''}
            </div>
        `);
    }
}

export {
    runSend,
    runDrop
}
