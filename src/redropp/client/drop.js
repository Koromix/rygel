// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

import { render, html, live, svg, unsafeHTML } from 'vendor/lit-html/lit-html.bundle.js';
import dayjs from 'vendor/dayjs/dayjs.bundle.js';
import QRC from 'vendor/qrcodegen/js/qrcodegen.js';
import { Util, LruMap, Log, Net, HttpError } from 'lib/web/base/base.js';
import * as IDB from 'lib/web/base/indexeddb.js';
import { Base64 } from 'lib/web/base/mixer.js';
import * as UI from 'lib/web/ui/ui.js';
import * as App from './app.js';
import { route, cache, session } from './app.js';
import * as UserMod from './user.js';
import {
    formatFixed,
    formatSize,
    formatDuration,
    ProgressMeter
} from './format.js';
import {
    prepareDownload,
    getDownloadStatus
} from './relay.js';
import { ASSETS } from '../assets/assets.js';

const EXPIRATION_DAYS = [1, 7, 30, 90];
const DEFAULT_EXPIRATION = 7;
const FRAGMENT_SIZE = 2097152;

let send_file = null;
let new_drops = new LruMap(4);

let refresh_timer = null;

async function runDrops() {
    if (!App.isLogged())
        return UserMod.runLogin();

    cache.drops = await Net.cache('drops', '/api/drop/list');

    let drops = UI.tableValues('drops', cache.drops.drops, 'name');
    let now = (new Date).valueOf();

    let db = await openLocalDB(session.userid);
    let passphrases = new Set(await db.list('passphrases'));

    UI.main(html`
        <div class="heading">${T.files}</div>

        <div class="block">
            <div style="text-align: center;">
                <p class="sub">${T.format(T.quota_x_of_x, formatSize(cache.drops.total), formatSize(cache.drops.quota), formatFixed(cache.drops.total / cache.drops.quota * 100, 1))}</p>
                <progress value=${Math.min(cache.drops.total, cache.drops.quota)} max=${cache.drops.quota}></progress>
            </div>

            <table class="responsive" style="table-layout: fixed;">
                <colgroup>
                    <col/>
                    <col style="width: 120px;" />
                    <col style="width: 180px;" />
                    <col style="width: 180px;" />
                    <col class="check" />
                </colgroup>
                <thead>
                    <tr>
                        ${UI.tableHeader('drops', 'name', T.name)}
                        ${UI.tableHeader('drops', 'size', T.size)}
                        ${UI.tableHeader('drops', 'protect', T.password)}
                        ${UI.tableHeader('drops', 'expire', T.expiration)}
                        <th></th>
                    </tr>
                </thead>
                <tbody>
                    ${drops.map(drop => {
                        let show_recover = drop.complete;
                        let can_recover = show_recover && passphrases.has(drop.kid);

                        return html`
                            <tr>
                                <td>
                                    <div style="display: flex; align-items: center;">
                                        <span style="flex: 1; overflow: hidden; text-overflow: ellipsis;">${drop.name}</span>
                                        ${show_recover ? html`
                                            <button type="button" class="small"
                                                    ?disabled=${!can_recover} title=${!can_recover ? T.links_can_be_recovered_on_upload_machine : ''}
                                                    @click=${UI.wrap(e => recoverLink(drop))}>${T.recover_link}</button>
                                        ` : ''}
                                    </div>
                                </td>
                                <td class="right">
                                    ${formatSize(drop.size)}
                                    ${!drop.complete ? html`<span class="sub" style="color: red;">${T.incomplete}</span>` : ''}
                                </td>
                                <td class="center">${drop.protect ? T.protected : T.no_password}</td>
                                <td class="right">
                                    ${drop.expire != null && drop.expire > now ? dayjs(drop.expire).format('lll') : ''}
                                    ${drop.expire != null && drop.expire <= now ? T.expired : ''}
                                    ${drop.expire == null ? T.never : ''}
                                </td>
                                <td class="check">
                                    <button type="button" class="small"
                                            @click=${UI.wrap(e => deleteDrop(drop.kid))}><img src=${ASSETS['ui/delete']} alt=${T.delete} /></button>
                                </td>
                            </tr>
                        `;
                    })}
                    ${!drops.length ? html`<tr><td colspan="5" style="text-align: center;">${T.no_file}</td></tr>` : ''}
                </tbody>
            </table>
        </div>
        <div class="actions">
            <button type="button" @click=${UI.wrap(e => App.go('/send'))}>${T.send_file}</button>
        </div>
    `);
}

async function recoverLink(info) {
    let passphrase = null;

    // Find passphrase in local database
    {
        let { decryptString } = await import('./file.js');
        let db = await openLocalDB(session.userid);

        let encrypted = await db.load('passphrases', info.kid);
        let key = Base64.toBytes(session.ckey);

        try {
            passphrase = decryptString(key, encrypted);
        } catch (err) {
            if (encrypted != null)
                console.error(err);
            throw new Error(T.message(`Failed to recover link for this drop`));
        }
    }

    let drop = {
        kid: info.kid,
        name: info.name,
        size: info.size,
        expire: info.expire,
        passphrase: passphrase,
        uploaded: info.size,
        error: null
    };

    new_drops.set(info.kid, drop);

    let url = App.makeURL({ mode: 'drop', drop: info.kid }, passphrase);
    App.go(url);
}

async function deleteDrop(kid) {
    await UI.dialog((render, close) => html`
        <div class="title">
            ${T.delete_file}
            <div style="flex: 1;"></div>
            <button type="button" class="secondary" @click=${UI.wrap(close)}>✖\uFE0E</button>
        </div>
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

    if (route.drop == null) {
        cache.drop = null;

        App.go('/send');
        return;
    }

    cache.drop = new_drops.get(route.drop);

    if (cache.drop != null) {
        is_new = true;
        passphrase = cache.drop.passphrase;
    } else {
        try {
            let url = Util.pasteURL('/api/drop/info', { kid: route.drop });
            cache.drop = await Net.cache('drop', url);
        } catch (err) {
            cache.drop = null;

            if (err.status == 404)
                err = new Error(T.unknown_or_expired_drop);
            throw err;
        }

        passphrase = window.location.hash.substr(1);

        if (!passphrase)
            throw new Error(T.message(`Missing decryption passphrase`));
    }

    route.drop = cache.drop?.kid;

    if (cache.drop.uploaded < cache.drop.size) {
        let stat = cache.drop.progress.measure();

        if (stat.rate != null)
            refreshSoon();

        UI.main(html`
            <div class="heading">${T.send_file}</div>

            <div class="block" style="align-items: center;">
                <p>${cache.drop.name}</p>
                <progress value=${stat.value ?? 0} max=${cache.drop.size}></progress>
                ${cache.drop.error == null ? html`
                    <div class="sub" style="text-align: center;">
                        ${T.speed}${T._colon}${stat.rate != null ? formatSize(stat.rate * 1000) + '/s' : '-'}<br>
                        ${T.remaining_time}${T._colon}${stat.remaining != null ? formatDuration(stat.remaining) : '-'}
                    </div>
                ` : ''}
                ${cache.drop.error != null ? html`
                    <p style="text-align: center; color: red;">
                        ${T.error_has_occured}<br>
                        ${cache.drop.error.message}
                    </p>
                ` : ''}
            </div>
        `);
    } else if (is_new) {
        let hash = `#${passphrase}`;
        let url = App.makeURL({ mode: 'drop', drop: cache.drop.kid }, hash);

        UI.main(html`
            <div class="heading">${cache.drop.name}</div>

            <div class="block" style="align-items: center;">
                <div>${formatSize(cache.drop.size)}</div>
                <div class="command">
                    <pre style="text-align: center;"
                         @click=${e => window.getSelection().selectAllChildren(e.target)}>${ENV.url + url}</pre>
                </div>
                ${makeQrCodeSvg(ENV.url + url, 200)}
                <div class="sub">
                    ${cache.drop.expire != null ? T.format(T.expires_at_x, dayjs(cache.drop.expire).format('lll')) : ''}
                    ${cache.drop.expire == null ? T.never_expires : ''}
                </div>
            </div>

            <div class="actions">
                <button @click=${UI.wrap(e => copyClipboard(e, ENV.url + url))}>${T.copy_download_link}</button>
            </div>
        `);
    } else {
        let status = getDownloadStatus(cache.drop.kid);

        let stat = status?.meter?.measure?.();
        let complete = (stat != null && stat.value == stat.max);
        let enabled = (status == null || !status.busy);

        if (status?.busy && stat?.rate != null)
            refreshSoon();

        UI.main(html`
            <div class="heading">${cache.drop.name}</div>

            <form @submit=${UI.wrap(submit)}>
                <div class="block" style="align-items: center;">
                    <div>${formatSize(cache.drop.size)}</div>
                    <div class="sub">
                        ${cache.drop.expire != null ? T.format(T.expires_at_x, dayjs(cache.drop.expire).format('lll')) : ''}
                        ${cache.drop.expire == null ? T.never_expires : ''}
                    </div>
                    ${cache.drop.protect && stat == null ? html`
                        <label>
                            <span>${T.password}</span>
                            <input type="password" name="password" />
                        </label>
                    ` : ''}
                    ${stat != null ? html`
                        <progress value=${stat.value ?? 0} max=${stat.max}></progress>
                        ${!complete && status.error == null ? html`
                            <div class="sub" style="text-align: center;">
                                ${T.speed}${T._colon}${stat.rate != null ? formatSize(stat.rate * 1000) + '/s' : '-'}<br>
                                ${T.remaining_time}${T._colon}${stat.remaining != null ? formatDuration(stat.remaining) : '-'}
                            </div>
                            <div style="text-align: center;">${T.keep_tab_open_during_download}</div>
                        ` : ''}
                        ${complete ? html`<div class="sub" style="text-align: center;">${T.download_complete}</div>` : ''}
                    ` : ''}
                    ${status?.error != null ? html`
                        <p style="text-align: center; color: red;">
                            ${T.error_has_occured}<br>
                            ${status.error.message}
                        </p>
                    ` : ''}
                </div>

                <div class="actions">
                    <button type="submit" ?disabled=${!enabled}>${T.download}</button>
                    <a @click=${UI.wrap(e => otherDownloadOptions(cache.drop, passphrase))}>${T.show_other_download_options}</a>
                </div>
            </form>
        `);

        async function submit(e) {
            if (!enabled)
                return;

            let form = e.currentTarget;
            let password = form.elements.password?.value?.trim?.();

            if (cache.drop.protect && !password)
                throw new Error(T.message(`Missing password`));

            await download(cache.drop, passphrase, password);
        }
    }
}

async function download(info, passphrase, password) {
    let key = null;

    // The scrypt code in decodeHeader blocks for some time.
    // But this dynamic import also gives the browser time for a repaint.
    const { decodeHeader } = await import('./file.js');

    try {
        key = await decodeHeader(info.header, info.nonce, passphrase, password);
    } catch (err) {
        console.error(err);

        let msg = info.protect ? T.message(`Invalid decryption key or password`)
                               : T.message(`Invalid decryption key`);
        throw new Error(msg);
    }

    await prepareDownload(info, key, refreshSoon);

    let url = '/drop/decrypt/' + info.kid;
    let response = await Net.fetch(url, { method: 'HEAD' });

    if (!response.ok)
        throw new Error(T.message(`Failed to communicate with service worker for download. Refresh the page and try again.`));

    window.location.href = '/drop/decrypt/' + info.kid;
}

async function otherDownloadOptions(info, passphrase) {
    let curl = `curl -o ${escapeShellArgument(info.name + '.age')} ${ENV.url}/drop/download/${info.kid}`;
    let age = `age -d -o ${escapeShellArgument(info.name)} ${escapeShellArgument(info.name + '.age')}`;
    let suffix = info.protect ? html`<span style="color: red;">${T.password_suffix}</span>` : '';

    await UI.dialog((render, close) => html`
        <div class="title">
            ${T.format(T.download_x, info.name)}
            <div style="flex: 1;"></div>
            <button type="button" class="secondary" @click=${UI.wrap(close)}>✖\uFE0E</button>
        </div>

        <div class="main">
            <div class="section">${T.command_line}</div>
            <div style="max-width: 40em;">
                <p>${unsafeHTML(T.use_curl_to_download_encrypted_file)}</p>
                <div class="command">
                    <pre @click=${e => window.getSelection().selectAllChildren(e.target)}>${curl}</pre>
                    <button type="button" class="small" @click=${UI.wrap(e => copyClipboard(e, curl))}>${T.copy}</button>
                </div>
                <p>${unsafeHTML(T.use_age_to_decrypt_the_file)}</p>
                <div class="command">
                    <pre @click=${e => window.getSelection().selectAllChildren(e.target)}>${age}</pre>
                    <button type="button" class="small" @click=${UI.wrap(e => copyClipboard(e, age))}>${T.copy}</button>
                </div>
                <p>${T.use_passphrase_to_decrypt_with_age}</p>
                <div class="command">
                    <pre @click=${e => window.getSelection().selectAllChildren(e.target)}>${passphrase}${suffix ? '/' : ''}${suffix}</pre>
                    <button type="button" class="small" @click=${UI.wrap(e => copyClipboard(e, passphrase + (suffix ? '/' : '')))}>${T.copy}</button>
                </div>
                ${info.protect ? html`<span class="sub" style="color: red;">${T.add_password_after_passphrase}</span>` : ''}
            </div>
        </div>

        <div class="footer">
            <button type="button" class="secondary" @click=${UI.wrap(close)}>${T.close}</button>
        </div>
    `);
}

function escapeShellArgument(str) {
    let safe = str.match(/^[a-zA-Z0-9_\-\.]+$/);
    return safe ? str : `"${str.replace(/([$`"\!~])/g, '\\$1')}"`;
}

async function runSend() {
    if (!App.isLogged())
        return UserMod.runLogin();

    UI.main(html`
        <div class="heading">${T.send_file}</div>

        <form @submit=${UI.wrap(submit)}>
            <div class="block" style="align-items: center;">
                <label>
                    <span>${T.expiration}</span>
                    <select name="expiration">
                        ${EXPIRATION_DAYS.map(days => {
                            if (days * 86400000 > ENV.max_duration)
                                return '';

                            return html`<option value=${days} ?selected=${days == DEFAULT_EXPIRATION}>${T.count(T.expiration_delay, days)}</option>`;
                        })}
                        ${ENV.allow_infinite ? html`<option value="0">${T.no_expiration}</option>` : ''}
                    </select>
                </label>
                <div class="sub">${T.drag_or_browse_file}</div>
                <label>
                    <span>${T.file}</span>
                    <input type="file" name="file" style="display: none;" @change=${change} />
                    <button type="button" @click=${e => { e.target.parentNode.click(); e.preventDefault(); }}>${T.browse_for_file}</button>
                </label>
                ${send_file != null ? html`<div class="sub">${send_file.name} (${formatSize(send_file.size)})` : ''}
                <label>
                    <span>${T.password} <span class="sub">(${T.optional.toLowerCase()})</span></span>
                    <input type="password" name="password" />
                </label>
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
        let expiration = (parseInt(elements.expiration.value, 10) * 86400000) || null;
        let password = elements.password.value.trim();

        // The scrypt code in createHeader blocks for some time.
        // But this dynamic import also gives the browser time for a repaint.
        const { createHeader } = await import('./file.js');

        let { passphrase, header, nonce, key } = await createHeader(password);
        let info = await createDrop(file, expiration, !!password, header, nonce);

        // Encrypt and save passphrase locally
        if (session != null) {
            let { encryptString } = await import('./file.js');
            let db = await openLocalDB(session.userid);

            let key = Base64.toBytes(session.ckey);
            let encrypted = encryptString(key, passphrase);

            await db.saveWithKey('passphrases', info.kid, encrypted);
        }

        let drop = {
            kid: info.kid,
            name: file.name,
            size: file.size,
            expire: info.expire,
            passphrase: passphrase,
            uploaded: 0,
            progress: new ProgressMeter(file.size),
            error: null
        };

        send_file = null;
        new_drops.set(info.kid, drop);

        let url = App.makeURL({ mode: 'drop', drop: info.kid }, passphrase);
        App.go(url);

        let ui_lock = UI.blockClose();

        try {
            await uploadFile(info, key, file, uploaded => progress(drop, uploaded));
        } catch (err) {
            drop.error = err;
            throw err;
        } finally {
            UI.unblockClose(ui_lock);
        }
    }

    function progress(drop, uploaded) {
        drop.uploaded = uploaded;
        drop.progress.add(uploaded);

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

async function createDrop(file, expiration, protect, header, nonce) {
    let info = await Net.post('/api/drop/create', {
        name: file.name,
        size: file.size,
        expiration: expiration,
        protect: protect,
        header: header,
        nonce: nonce
    });

    Net.invalidate('drops');

    return info;
}

async function uploadFile(info, key, file, progress = () => {}) {
    let { upload } = await import('./file.js');

    let stream = file.stream();
    let chunks = readChunks(stream);

    await upload(info, key, chunks, progress);

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

async function openLocalDB(id) {
    let db_name = `rokkerd:${id}:drops` ;

    let db = await IDB.open(db_name, 1, (db, old_version) => {
        switch (old_version) {
            case null: {
                db.createStore('passphrases');
            } // fallthrough
        }
    });

    return db;
}

function refreshSoon() {
    if (refresh_timer != null)
        return;

    refresh_timer = setTimeout(() => {
        refresh_timer = null;
        App.go();
    }, 500);
}

async function copyClipboard(el, text) {
    if (el instanceof Event)
        el = el.currentTarget;

    await navigator.clipboard.writeText(text);
    UI.flash(el, T.copied_flash);
}

export {
    runDrops,
    runDrop,
    runSend
}
