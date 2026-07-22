// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

import { render, html, live, unsafeHTML } from 'vendor/lit-html/lit-html.bundle.js';
import dayjs from 'vendor/dayjs/dayjs.bundle.js';
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
    ProgressMeter,
    drawQrCode
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
let upload_map = new LruMap(64);
let secret_map = new LruMap(4);

let refresh_timer = null;

async function runDrops() {
    if (!App.isLogged())
        return UserMod.runLogin();

    cache.drops = await Net.cache('drops', '/api/drop/list');

    let drops = UI.tableValues('drops', cache.drops.drops, 'name');
    let now = (new Date).valueOf();

    let db = await openLocalDB(session.userid);
    let secrets = new Set(await db.list('secrets'));

    UI.main(html`
        <div class="heading">${T.files}</div>

        <div class="block">
            <div style="text-align: center;">
                <p class="sub">${T.format(T.quota_x_of_x, formatSize(cache.drops.usage), formatSize(cache.drops.quota), formatFixed(cache.drops.usage / cache.drops.quota * 100, 1))}</p>
                <progress value=${Math.min(cache.drops.usage, cache.drops.quota)} max=${cache.drops.quota}></progress>
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
                        ${UI.tableHeader('drops', 'total', T.size)}
                        ${UI.tableHeader('drops', 'protect', T.password)}
                        ${UI.tableHeader('drops', 'expire', T.expiration)}
                        <th></th>
                    </tr>
                </thead>
                <tbody>
                    ${drops.map(drop => {
                        let has_expired = (drop.expire <= now);
                        let show_recover = !has_expired && drop.complete;
                        let can_recover = show_recover && secrets.has(drop.kid);

                        return html`
                            <tr>
                                <td>
                                    <div style="display: flex; align-items: center;">
                                        <span style="flex: 1; overflow: hidden; text-overflow: ellipsis;">${drop.name}</span>
                                        ${show_recover && can_recover ? html`
                                            <button type="button" class="small"
                                                    @click=${UI.wrap(e => recoverLink(drop.kid))}>${T.recover_link}</button>
                                        ` : ''}
                                        ${show_recover && !can_recover ? html`
                                            <button type="button" class="small" disabled
                                                    title=${T.links_can_be_recovered_on_upload_machine}>${T.not_recoverable}</button>
                                        ` : ''}
                                    </div>
                                </td>
                                <td class="right">
                                    ${formatSize(drop.total)}
                                    ${!drop.complete ? html`<span class="sub" style="color: red;">${T.incomplete}</span>` : ''}
                                </td>
                                <td class="center">${drop.protect ? T.protected : T.no_password}</td>
                                <td class="right" title=${drop.expire != null ? dayjs(drop.expire).format('lll') : ''}>
                                    ${drop.expire != null && !has_expired ? Util.capitalize(dayjs(drop.expire).fromNow()) : ''}
                                    ${drop.expire != null && has_expired ? html`${T.expired} <span class="sub">(${dayjs(drop.expire).fromNow()})</span>`: ''}
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

async function recoverLink(kid) {
    let secret = null;

    // Find secret in local database
    {
        let { decryptString } = await import('./file.js');
        let db = await openLocalDB(session.userid);

        let encrypted = await db.load('secrets', kid);
        let key = Base64.toBytes(session.ckey);

        try {
            secret = decryptString(key, encrypted);
        } catch (err) {
            if (encrypted != null)
                console.error(err);
            throw new Error(T.message(`Failed to recover link for this drop`));
        }
    }

    shareLink(kid, secret);
}

function shareLink(kid, secret) {
    secret_map.set(kid, secret);

    let url = App.makeURL({ mode: 'drop', drop: kid }, secret);
    App.go(url);
}

function unshareLink(kid, secret) {
    secret_map.delete(kid);

    let url = App.makeURL({ mode: 'drop', drop: kid }, secret);
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
            <button type="submit" class="danger">${T.confirm}</button>
        </div>
    `);

    let url = Util.pasteURL('/api/drop/delete', { kid: kid });
    await Net.post(url);

    Net.invalidate('drops');

    if (route.drop == kid)
        route.drop = null;
}

async function runDrop() {
    let secret = null;
    let is_sharing = false;

    if (route.drop == null) {
        cache.drop = null;

        App.go('/send');
        return;
    }

    try {
        let url = Util.pasteURL('/api/drop/info', { kid: route.drop });
        cache.drop = await Net.cache('drop', url);
    } catch (err) {
        cache.drop = null;

        if (err.status == 404)
            err = new Error(T.unknown_or_expired_drop);
        throw err;
    }

    route.drop = cache.drop?.kid;

    secret = secret_map.get(cache.drop.kid);

    if (secret != null) {
        is_sharing = true;
    } else {
        secret = window.location.hash.substr(1);
        if (!secret)
            throw new Error(T.message(`Missing decryption secret`));
        is_sharing = false;
    }

    if (!cache.drop.complete) {
        let status = getUploadStatus(cache.drop.files[0]);
        if (status == null)
            throw new Error(T.message(`Incomplete drop '${cache.drop.kid}'`));
        let stat = status.progress.measure();

        if (stat.rate != null)
            refreshSoon();

        UI.main(html`
            <div class="heading">${T.send_file}</div>

            <div class="block" style="align-items: center;">
                <p>${cache.drop.name}</p>
                <progress value=${stat.value ?? 0} max=${cache.drop.total}></progress>
                ${status.error == null ? html`
                    <div class="sub" style="text-align: center;">
                        ${T.speed}${T._colon}${stat.rate != null ? formatSize(stat.rate * 1000) + '/s' : '-'}<br>
                        ${T.remaining_time}${T._colon}${stat.remaining != null ? formatDuration(stat.remaining) : '-'}
                    </div>
                ` : ''}
                ${status.error != null ? html`
                    <p style="text-align: center; color: red;">
                        ${T.error_has_occured}<br>
                        ${status.error.message}
                    </p>
                ` : ''}
            </div>
        `);
    } else if (is_sharing) {
        let hash = `#${secret}`;
        let url = App.makeURL({ mode: 'drop', drop: cache.drop.kid }, hash);
        let logo = await Net.loadImage(ASSETS['main/redropp']);

        UI.main(html`
            <div class="heading">${cache.drop.name}</div>

            <div class="block" style="align-items: center;">
                <div>${formatSize(cache.drop.total)}</div>
                <div class="command">
                    <pre style="text-align: center;"
                         @click=${e => window.getSelection().selectAllChildren(e.target)}>${ENV.url + url}</pre>
                </div>
                ${drawQrCode(ENV.url + url, { logo: logo })}
                <div class="sub" title=${cache.drop.expire != null ? dayjs(cache.drop.expire).format('lll') : null}>
                    ${cache.drop.expire != null ? T.format(T.expires_in_x, dayjs(cache.drop.expire).fromNow(true)) : ''}
                    ${cache.drop.expire == null ? T.never_expires : ''}
                </div>
            </div>

            <div class="actions">
                <button @click=${UI.wrap(e => copyClipboard(e, ENV.url + url))}>${T.copy_download_link}</button>
                <a @click=${UI.wrap(e => unshareLink(cache.drop.kid, secret))}>${T.show_download_page}</button>
            </div>
        `);
    } else {
        let status = getDownloadStatus(cache.drop.files[0].kid);

        let stat = status?.meter?.measure?.();
        let complete = (stat != null && stat.value == stat.max);
        let enabled = (status == null || !status.busy);

        if (status?.busy && stat?.rate != null)
            refreshSoon();

        UI.main(html`
            <div class="heading">${cache.drop.name}</div>

            <form @submit=${UI.wrap(submit)}>
                <div class="block" style="align-items: center;">
                    <div>${formatSize(cache.drop.total)}</div>
                    <div class="sub" title=${cache.drop.expire != null ? dayjs(cache.drop.expire).format('lll') : ''}>
                        ${cache.drop.expire != null ? T.format(T.expires_in_x, dayjs(cache.drop.expire).fromNow(true)) : ''}
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
                    <a @click=${UI.wrap(e => otherDownloadOptions(cache.drop, secret))}>${T.show_other_download_options}</a>
                    <a @click=${UI.wrap(e => shareLink(cache.drop.kid, secret))}>${T.share_drop_link}</a>
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

            await download(cache.drop.files[0], secret, password);
        }
    }
}

async function download(info, secret, password) {
    let key = null;

    // The scrypt code in decodeHeader blocks for some time.
    // But this dynamic import also gives the browser time for a repaint.
    const { decodeHeader } = await import('./file.js');

    try {
        let passphrase = makePassphrase(secret, password);

        key = await decodeHeader(info.header, info.nonce, passphrase, password);
    } catch (err) {
        console.error(err);

        let msg = info.protect ? T.message(`Invalid decryption key or password`)
                               : T.message(`Invalid decryption key`);
        throw new Error(msg);
    }

    await prepareDownload(info, key, refreshSoon);

    // Make sure service worker is ready
    {
        let url = `/auto/download/${info.kid}`;
        let response = await Net.fetch(url, { method: 'HEAD' });

        if (!response.ok)
            throw new Error(T.message(`Failed to communicate with service worker for download. Refresh the page and try again.`));
    }

    let url = `/auto/download/${info.kid}/${encodeURIComponent(info.name)}`;
    window.location.href = url;
}

async function otherDownloadOptions(info, secret) {
    let curl = `curl -o ${escapeShellArgument(info.name + '.age')} ${ENV.url}/download/${info.kid}`;
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
                    <pre @click=${e => window.getSelection().selectAllChildren(e.target)}>${secret}${suffix ? '/' : ''}${suffix}</pre>
                    <button type="button" class="small" @click=${UI.wrap(e => copyClipboard(e, secret + (suffix ? '/' : '')))}>${T.copy}</button>
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
        const { createHeader, randomBytes } = await import('./file.js');

        let secret = Base64.toBase64Url(randomBytes(32));
        let passphrase = makePassphrase(secret, password);

        let { header, nonce, key } = await createHeader(passphrase);
        let { drop, files } = await createDrop(file, expiration, !!password, header, nonce);

        // Encrypt and save passphrase locally
        if (session != null) {
            let { encryptString } = await import('./file.js');
            let db = await openLocalDB(session.userid);

            let key = Base64.toBytes(session.ckey);
            let encrypted = encryptString(key, passphrase);

            await db.saveWithKey('secrets', drop.kid, encrypted);
        }

        send_file = null;

        upload_map.set(files[0].kid, {
            progress: new ProgressMeter(files[0].size),
            error: null
        });
        shareLink(drop.kid, secret);

        let ui_lock = UI.blockClose();

        try {
            await uploadFile(files[0], key, file, uploaded => progress(files[0], uploaded));
            await completeDrop(drop.kid);
        } catch (err) {
            let status = getUploadStatus(files[0]);
            status.error = err;

            throw err;
        } finally {
            UI.unblockClose(ui_lock);
        }
    }

    function progress(file, uploaded) {
        let status = getUploadStatus(file);
        status.progress.add(uploaded);

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

function makePassphrase(secret, password) {
    let passphrase = secret;
    if (password)
        passphrase += '/' + password;
    return passphrase;
}

async function createDrop(file, expiration, protect, header, nonce) {
    let drop = await Net.post('/api/drop/create', {
        name: file.name,
        expiration: expiration,
        protect: protect
    });

    let files = null;

    // Populate drop files
    {
        let url = Util.pasteURL('/api/drop/populate', { kid: drop.kid });

        files = await Net.post(url, [{
            name: file.name,
            size: file.size,
            header: header,
            nonce: nonce
        }]);
    }

    return { drop, files };
}

async function completeDrop(kid) {
    let url = Util.pasteURL('/api/drop/complete', { kid: kid });
    await Net.post(url);

    Net.invalidate('drop');
    Net.invalidate('drops');
}

async function uploadFile(info, key, file, progress = () => {}) {
    let { upload } = await import('./file.js');

    let stream = file.stream();
    let chunks = readChunks(stream);

    await upload(info, key, chunks, progress);
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

async function openLocalDB(id) {
    let db_name = `redropp:${id}:drops`;
    let target_version = 2;

    let db = await IDB.open(db_name, target_version, (db, old_version) => {
        switch (old_version) {
            case null: {
                db.createStore('passphrases');
            } // fallthrough

            case 1: {
                db.deleteStore('passphrases');
                db.createStore('secrets');
            } // fallthrough
        }
    });

    return db;
}

function getUploadStatus(file) {
    let status = upload_map.get(file.kid);
    return status;
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
