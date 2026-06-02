// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

import { render, html, live, svg } from 'vendor/lit-html/lit-html.bundle.js';
import dayjs from 'vendor/dayjs/dayjs.bundle.js';
import QRC from 'vendor/qrcodegen/js/qrcodegen.js';
import { Util, LruMap, Log, Net, HttpError } from 'lib/web/base/base.js';
import { Base64 } from 'lib/web/base/mixer.js';
import * as UI from 'lib/web/ui/ui.js';
import * as App from './main.js';
import { route, cache } from './main.js';
import { sendDrop, getProgress } from './relay.js';
import * as UserMod from './user.js';
import { formatSize, writeClipboard } from './common.js';
import { createKey, deriveKey, upload } from './file.js';
import { ASSETS } from '../assets/assets.js';

const EXPIRATION_DAYS = [1, 7, 30, 90];
const DEFAULT_EXPIRATION = 7;
const FRAGMENT_SIZE = 2097152;

let send_file = null;
let new_drops = new LruMap(4);

async function runDrops() {
    if (!App.isLogged())
        return UserMod.runLogin();

    cache.drops = await Net.cache('drops', '/api/drop/list');

    let drops = UI.tableValues('drops', cache.drops, 'name');
    let now = (new Date).valueOf();

    UI.main(html`
        <div class="header">${T.files}</div>

        <div class="block">
            <table style="table-layout: fixed; width: 100%;">
                <colgroup>
                    <col></col>
                    <col style="width: 200px;"></col>
                    <col style="width: 120px;"></col>
                    <col style="width: 60px;"/>
                </colgroup>
                <thead>
                    <tr>
                        ${UI.tableHeader('drops', 'name', T.name)}
                        ${UI.tableHeader('drops', 'expire', T.expiration)}
                        ${UI.tableHeader('drops', 'size', T.size)}
                        <th></th>
                    </tr>
                </thead>
                <tbody>
                    ${drops.map(drop => html`
                        <tr>
                            <td>${drop.name}</td>
                            <td>
                                ${drop.expire != null && drop.expire > now ? dayjs(drop.expire).format('lll') : ''}
                                ${drop.expire != null && drop.expire <= now ? T.expired : ''}
                                ${drop.expire == null ? T.never : ''}
                            </td>
                            <td style="text-align: right;">
                                ${formatSize(drop.size)}
                                ${!drop.complete ? html`<span class="sub" style="color: red;">${T.incomplete}</span>` : ''}
                            </td>
                            <td class="center">
                                <button type="button" class="small"
                                        @click=${UI.wrap(e => deleteDrop(drop.kid))}><img src=${ASSETS['ui/delete']} alt=${T.delete} /></button>
                            </td>
                        </tr>
                    `)}
                    ${!drops.length ? html`<tr><td colspan="4" style="text-align: center;">${T.no_file}</td></tr>` : ''}
                </tbody>
            </table>
        </div>
        <div class="actions">
            <button type="button" @click=${UI.wrap(e => App.go('/send'))}>${T.send_file}</button>
        </div>
    `);
}

async function deleteDrop(kid) {
    await UI.dialog((render, close) => html`
        <div class="title">${T.delete_file}</div>
        <div class="main">${T.confirm_not_reversible}</div>
        <div class="footer">
            <button type="button" class="secondary" @click=${UI.wrap(close)}>${T.cancel}</button>
            <button type="submit">${T.confirm}</button>
        </div>
    `);

    await Net.post('/api/drop/delete', { kid: kid });
    Net.invalidate('drops');

    if (route.drop == kid)
        route.drop = null;
}

async function runDrop() {
    let is_new = false;
    let passphrase = null;

    if (route.drop != null) {
        cache.drop = new_drops.get(route.drop);
        is_new = (cache.drop != null);

        if (is_new) {
            passphrase = cache.drop.passphrase;
        } else {
            try {
                let url = Util.pasteURL('/api/drop/info', { kid: route.drop });
                cache.drop = await Net.cache('drop', url);
            } catch (err) {
                if (err.status != 404 && err.status != 422)
                    throw err;

                cache.drop = null;
            }

            if (!window.location.hash)
                throw new Error('Missing decryption passphrase');

            passphrase = window.location.hash.substr(1);
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
        UI.main(html`
            <div class="header">${T.send_file}</div>

            <div class="block" style="align-items: center;">
                <p>${cache.drop.name}</p>
                <progress value=${cache.drop.uploaded} max=${cache.drop.size}></progress>
                ${cache.drop.error != null ? html`
                    <p style="color: red;">
                        ${T.error_has_occured}<br>
                        ${cache.drop.error.message}
                    </p>
                ` : ''}
            </div>
        `);
    } else if (is_new) {
        let hash = `#${passphrase}`;
        let url = App.makeURL({ mode: 'drop', drop: cache.drop.kid }, hash);

        window.location.hash = hash;

        UI.main(html`
            <div class="header">${cache.drop.name}</div>

            <div class="block" style="align-items: center;">
                <div>${formatSize(cache.drop.size)}</div>
                <input class="link" type="text" readonly value=${ENV.url + url}
                       @click=${e => e.target.select()} />
                ${makeQrCodeSvg(ENV.url + url, 200)}
            </div>

            <div class="actions">
                <button @click=${UI.wrap(e => writeClipboard(T.download_link, ENV.url + url))}>${T.copy_download_link}</button>
            </div>
        `);
    } else {
        let progress = getProgress(cache.drop.kid);

        if (progress && progress.value == progress.max)
            progress = null;

        UI.main(html`
            <div class="header">${cache.drop.name}</div>

            <form @submit=${UI.wrap(submit)}>
                <div class="block" style="align-items: center;">
                    <div>${formatSize(cache.drop.size)}</div>
                    ${cache.drop.protect && progress == null ? html`
                        <label>
                            <span>${T.password}</span>
                            <input type="password" name="password" />
                        </label>
                    ` : ''}
                    ${progress != null ? html`
                        <progress value=${progress.value} max=${progress.max}></progress>
                        <div class="sub">${T.keep_tab_open_during_download}</div>
                    ` : ''}
                </div>

                <div class="actions">
                    <button type="submit" ?disabled=${progress != null}>${T.download}</button>
                </div>
            </form>
        `);

        async function submit(e) {
            if (progress != null)
                return;

            let form = e.currentTarget;
            let password = form.elements.password?.value?.trim?.();

            await download(cache.drop, passphrase, password);
        }
    }
}

async function download(info, passphrase, password) {
    let key = deriveKey(info.salt, passphrase, password);

    await sendDrop(info, key);
    window.location.href = '/api/drop/download/' + info.kid;
}

async function runSend() {
    if (!App.isLogged())
        return UserMod.runLogin();

    UI.main(html`
        <div class="header">${T.send_file}</div>

        <form @submit=${UI.wrap(submit)}>
            <div class="block" style="align-items: center;">
                <label>
                    <span>${T.expiration}</span>
                    <select name="expiration">
                        ${EXPIRATION_DAYS.map(days =>
                            html`<option value=${days} ?selected=${days == DEFAULT_EXPIRATION}>${T.count(T.expiration_delay, days)}</option>`)}
                    </select>
                </label>
                <label>
                    <span>${T.password}</span>
                    <input type="password" name="password" />
                    <span class="sub">${T.optional}</span>
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
        let password = elements.password.value.trim();

        let { salt, passphrase, key } = createKey(password);
        let info = await createDrop(file, expiration, salt, !!password);

        let drop = {
            kid: info.kid,
            name: file.name,
            size: file.size,
            salt: salt,
            passphrase: passphrase,
            uploaded: 0,
            error: null
        };

        send_file = null;
        new_drops.set(info.kid, drop);

        let url = App.makeURL({ mode: 'drop', drop: info.kid });
        App.go(url);

        try {
            await uploadFile(info, key, file, uploaded => progress(drop, uploaded));
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

async function createDrop(file, expiration, salt, protect) {
    let info = await Net.post('/api/drop/create', {
        name: file.name,
        size: file.size,
        expiration: expiration,
        salt: salt,
        protect: protect
    });

    Net.invalidate('drops');

    return info;
}

async function uploadFile(info, key, file, progress = () => {}) {
    let stream = file.stream();
    let chunks = readChunks(stream);

    let fragments = upload(info, key, chunks);
    let uploaded = 0;

    for await (let frag of fragments) {
        uploaded += frag.length;
        progress(uploaded);
    }

    Net.invalidate('drops');
}

async function* readChunks(stream) {
    let reader = stream.getReader();

    for (;;) {
        let { value, done } = await reader.read();

        if (done)
            break;

        yield value;
    }
}

function makeQrCodeSvg(text, size, border = 2) {
    let qr = QRC.QrCode.encodeText(text, QRC.QrCode.Ecc.MEDIUM);

    let parts = [];

	for (let y = 0; y < qr.size; y++) {
		for (let x = 0; x < qr.size; x++) {
			if (qr.getModule(x, y))
				parts.push(`M${x + border},${y + border}h1v1h-1z`);
		}
	}

	return svg`
        <svg width=${size + border * 2} height=${size + border * 2} style="margin: 0.5em;"
             viewBox="0 0 ${qr.size + border * 2} ${qr.size + border * 2}" stroke="none">
       	    <rect width="100%" height="100%" fill="#FFFFFF"/>
           	<path d="${parts.join(" ")}" fill="#000000"/>
        </svg>
    `;
}

export {
    runDrops,
    runDrop,
    runSend
}
