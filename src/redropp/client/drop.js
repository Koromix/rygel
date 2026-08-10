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
    prepareFile,
    prepareZip,
    getDownloadStatus,
    isDownloading
} from './relay.js';
import { ASSETS } from '../assets/assets.js';

const EXPIRATION_DAYS = [1, 7, 30, 90];
const DEFAULT_EXPIRATION = 7;
const FRAGMENT_SIZE = 2097152;

let FileApi = null;

let send_codename = null;
let send_files = [];

let upload_map = new LruMap(64);
let secret_map = new LruMap(4);
let password_map = new LruMap(2);
let download_tasks = new WeakMap;

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
        <div class="heading">${T.drops}</div>

        <div class="block">
            <div style="text-align: center;">
                <p class="sub">${T.format(T.quota_x_of_x, formatSize(cache.drops.usage), formatSize(cache.drops.quota), formatFixed(cache.drops.usage / cache.drops.quota * 100, 1))}</p>
                <progress value=${Math.min(cache.drops.usage, cache.drops.quota)} max=${cache.drops.quota}></progress>
            </div>

            <table class="responsive" style="width: 100%; table-layout: fixed;">
                <colgroup>
                    <col/>
                    <col style="width: 180px;" />
                    <col style="width: 120px;" />
                    <col style="width: 160px;" />
                    <col style="width: 180px;" />
                    <col class="check" />
                </colgroup>
                <thead>
                    <tr>
                        ${UI.tableHeader('drops', 'name', T.name)}
                        ${UI.tableHeader('drops', 'ctime', T.published)}
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
                                    <div style="display: flex; align-items: center;" title=${drop.name}>
                                        <span class="nowrap" style="flex: 1;">${drop.name}</span>
                                        ${show_recover && can_recover ? html`
                                            <button type="button" class="small" title=""
                                                    @click=${UI.wrap(e => recoverLink(drop.kid))}>${T.recover_link}</button>
                                        ` : ''}
                                        ${show_recover && !can_recover ? html`
                                            <button type="button" class="small" disabled
                                                    title=${T.links_can_be_recovered_on_upload_machine}>${T.not_recoverable}</button>
                                        ` : ''}
                                    </div>
                                </td>
                                <td class="center">${dayjs(drop.ctime).format('lll')}</td>
                                <td class="center">
                                    ${formatSize(drop.total)}
                                    ${!drop.complete ? html`<span class="sub" style="color: red;">${T.incomplete}</span>` : ''}
                                </td>
                                <td class="center">${drop.protect ? T.protected : T.no_password}</td>
                                <td class="center" title=${drop.expire != null ? dayjs(drop.expire).format('lll') : ''}>
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
                    ${!drops.length ? html`<tr><td colspan="6" style="text-align: center;">${T.no_file}</td></tr>` : ''}
                </tbody>
            </table>
        </div>
        <div class="actions">
            <button type="button" @click=${UI.wrap(e => App.go('/send'))}>${T.send_files}</button>
        </div>
    `);
}

async function recoverLink(kid) {
    let secret = null;

    if (FileApi == null)
        FileApi = await import('./file.js');

    // Find secret in local database
    {
        let db = await openLocalDB(session.userid);

        let encrypted = await db.load('secrets', kid);
        let key = Base64.toBytes(session.ckey);

        try {
            secret = FileApi.decryptString(key, encrypted);

            // In versions < 0.9.4, the entire passphrase (secret + password) could end up in the local database.
            // It probably does not affect anyone, but strip the password just in case. Not ideal but at least things work.
            if (secret.includes('/'))
                secret = secret.substr(0, secret.indexOf('/'));
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
    if (route.drop != null) {
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
    }
    if (route.drop == null) {
        App.go('/send');
        return;
    }

    if (!cache.drop.complete) {
        await runUpload();
    } else {
        let secret = secret_map.get(cache.drop.kid);
        let is_sharing = (secret != null);

        if (!is_sharing) {
            secret = route.hash;
            if (!secret)
                throw new Error(T.message(`Missing decryption secret`));
        }

        if (is_sharing) {
            await runShare(secret);
        } else {
            await runDownload(secret);
        }
    }
}

async function runShare(secret) {
    let hash = `#${secret}`;
    let url = App.makeURL({ mode: 'drop', drop: cache.drop.kid }, hash);
    let logo = await Net.loadImage(ASSETS['main/redropp']);

    UI.main(html`
        <div class="heading">${cache.drop.name}</div>

        <div class="block" style="align-items: center;">
            <div>${formatSize(cache.drop.total)}</div>
            <div class="sub" style="text-align: center;"
                 title=${cache.drop.expire != null ? T.format(T.expires_on_x, dayjs(cache.drop.expire).format('lll')) : null}>
                ${T.format(T.published_on_x, dayjs(cache.drop.ctime).format('lll'))}<br/>
                ${cache.drop.expire != null ? T.format(T.expires_in_x, dayjs(cache.drop.expire).fromNow(true)) : ''}
                ${cache.drop.expire == null ? T.never_expires : ''}<br/>
                ${cache.drop.protect ? T.password_protected : ''}
            </div>
            <div class="command">
                <pre style="text-align: center;"
                     @click=${e => window.getSelection().selectAllChildren(e.target)}>${ENV.url + url}</pre>
            </div>
            ${drawQrCode(ENV.url + url, { logo: logo })}
        </div>

        <div class="actions">
            <button @click=${UI.wrap(e => copyClipboard(e, ENV.url + url))}>${T.copy_download_link}</button>
            <a @click=${UI.wrap(e => unshareLink(cache.drop.kid, secret))}>${T.show_download_page}</button>
        </div>
    `);
}

async function runDownload(secret) {
    let zip_status = getDownloadStatus(cache.drop.kid);
    let file_statuses = cache.drop.files.map(file => getDownloadStatus(file.kid));

    let busy = zip_status?.busy || file_statuses.some(status => status?.busy);
    let agg = null;

    // Aggregate stats (if any) for this drop
    {
        let statuses = [zip_status, ...file_statuses];

        agg = {
            value: null,
            max: null,
            rate: null,
            remaining: null
        };

        for (let status of statuses) {
            if (status == null)
                continue;

            let stat = status.meter.measure();
            let task = download_tasks.get(status);

            agg.value += stat.value;
            agg.max += stat.max;

            if (stat.rate != null) {
                agg.rate += stat.rate;
                agg.remaining = Math.max(agg.remaining, stat.remaining);

                refreshSoon();
            }

            if (!status.busy && task != null) {
                task.close();
                download_tasks.delete(status);
            }
        }

        if (!busy) {
            agg = null;

            // Clear stats to avoid weirdness for new downloads
            for (let status of statuses) {
                if (status != null)
                    status.meter.reset();
            }
        }
    }

    UI.main(html`
        <div class="heading">${cache.drop.name}</div>

        <form @submit=${UI.wrap(submit)}>
            <div class="block" style="align-items: center;">
                <div>${formatSize(cache.drop.total)}</div>
                <div class="sub" style="text-align: center;"
                     title=${cache.drop.expire != null ? T.format(T.expires_on_x, dayjs(cache.drop.expire).format('lll')) : null}>
                    ${T.format(T.published_on_x, dayjs(cache.drop.ctime).format('lll'))}<br/>
                    ${cache.drop.expire != null ? T.format(T.expires_in_x, dayjs(cache.drop.expire).fromNow(true)) : ''}
                    ${cache.drop.expire == null ? T.never_expires : ''}<br/>
                    ${cache.drop.protect ? T.password_protected : ''}
                </div>
                <table class="responsive" style="table-layout: fixed;">
                    <colgroup>
                        <col/>
                        <col style="width: 100px;" />
                        <col style="width: 100px;" />
                    </colgroup>
                    <tbody>
                        ${cache.drop.files.map((file, idx) => {
                            let status = file_statuses[idx];

                            return html`
                                <tr>
                                    <td class="nowrap" title=${file.name}>${file.name}</td>
                                    <td class="center"><span class="sub">${formatSize(file.size)}</span></td>
                                    <td class="center">
                                        <button type="button" class="small" ?disabled=${status?.busy ?? false}
                                                @click=${UI.wrap(e => start(file))}>${T.download}</button>
                                    </td>
                                </tr>
                                ${status?.error != null ? html`
                                    <tr class="cling">
                                        <td colspan="3" class="center">
                                            <p style="font-size: 0.9em; color: var(--color, red);">${status.error.message}</p>
                                        </td>
                                    </tr>
                                ` : ''}
                            `;
                        })}
                    </tbody>
                </table>
                ${agg != null ? html`
                    <progress value=${agg.value} max=${agg.max}></progress>
                    ${busy ? html`
                        <div class="sub" style="text-align: center;">
                            ${T.speed}${T._colon}${agg.rate != null ? formatSize(agg.rate * 1000) + '/s' : '-'}<br>
                            ${T.remaining_time}${T._colon}${agg.remaining != null ? formatDuration(agg.remaining) : '-'}
                        </div>
                        <div style="text-align: center;">${T.keep_tab_open_during_download}</div>
                    ` : ''}
                ` : ''}
                ${zip_status?.error != null ? html`<p style="text-align: center; color: red;">${zip_status.error.message}</p>` : ''}
            </div>

            <div class="actions">
                <button type="submit" ?disabled=${zip_status?.busy ?? false}>${cache.drop.files.length > 1 ? T.download_all : T.download}</button>
                <a @click=${UI.wrap(e => otherDownloadOptions(cache.drop, secret))}>${T.show_other_download_options}</a>
                <a @click=${UI.wrap(e => shareLink(cache.drop.kid, secret))}>${T.share_drop_link}</a>
            </div>
        </form>
    `);

    async function submit(e) {
        if (cache.drop.files.length > 1) {
            await start(null);
        } else {
            await start(cache.drop.files[0]);
        }
    }

    async function start(file) {
        if (file != null) {
            let key = cache.drop.protect ? await openWithPassword(cache.drop, file, secret)
                                         : await openKey(file, secret, null);

            await downloadFile(cache.drop, file, key);
        } else {
            let key0 = cache.drop.protect ? await openWithPassword(cache.drop, cache.drop.files[0], secret)
                                          : await openKey(cache.drop.files[0], secret, null);
            let keys = [key0];

            for (let i = 1; i < cache.drop.files.length; i++) {
                let password = password_map.get(cache.drop.kid);
                let key = await openKey(cache.drop.files[i], secret, password);

                keys.push(key);
            }

            await downloadZip(cache.drop, keys);
        }
    }
}

async function openWithPassword(drop, file, secret) {
    // Quick open with cached password (if any)
    {
        let password = password_map.get(drop.kid);

        if (password != null) {
            try {
                let key = await openKey(file, secret, password);
                return key;
            } catch (err) {
                console.error(err);
                password_map.delete(cache.drop.kid);

                // Weird... but go on, show the dialog
            }
        }
    }

    let key = await UI.dialog({
        run: (render, close) => html`
            <div class="title">
                ${T.password_protection}
                <div style="flex: 1;"></div>
                <button type="button" class="secondary" @click=${UI.wrap(close)}>✖\uFE0E</button>
            </div>
            <div class="main">
                <label>
                    <span>${T.password}</span>
                    <input type="password" name="password" autocomplete="new-password" />
                </label>
            </div>
            <div class="footer">
                <button type="button" class="secondary" @click=${UI.wrap(close)}>${T.cancel}</button>
                <button type="submit">${T.confirm}</button>
            </div>
        `,

        submit: async (elements) => {
            let password = elements.password.value.trim();

            if (!password)
                throw new Error(T.message(`Missing password`));

            let key = await openKey(file, secret, password);

            // Success! Cache it for quick access to other drop files.
            password_map.set(cache.drop.kid, password);

            return key;
        }
    });

    return key;
}

async function openKey(file, secret, password) {
    if (FileApi == null)
        FileApi = await import('./file.js');
    await Util.waitFor(0); // DoEvents

    try {
        let passphrase = makePassphrase(secret, password);
        let key = await FileApi.decodeHeader(file.header, file.nonce, passphrase);

        return key;
    } catch (err) {
        console.error(err);

        let msg = password ? T.message(`Invalid decryption key or password`)
                           : T.message(`Invalid decryption key`);
        throw new Error(msg);
    }
}

async function downloadFile(drop, file, key) {
    await prepareFile(file, key, status => {
        let task = download_tasks.get(status);

        if (task == null) {
            task = Log.progress(file.name, 0, status.total);

            task.click = () => {
                let url = App.makeURL({ mode: 'drop', drop: drop.kid }, secret);
                App.go(url);
            };
            task.visible = () => (route.mode != 'drop' || route.drop != drop.kid);

            download_tasks.set(status, task);
        }

        if (status.busy) {
            let stat = status.meter.measure();
            task.progress(status.name, stat.value);
        } else if (status.error != null) {
            task.error(status.error, null);
        } else {
            task.success(T.download_complete);
        }

        refreshSoon();
    });

    // Make sure service worker is ready
    {
        let url = `/auto/download/${file.kid}`;
        let response = await Net.fetch(url, { method: 'HEAD' });

        if (!response.ok)
            throw new Error(T.message(`Failed to communicate with service worker for download. Refresh the page and try again.`));
    }

    let url = `/auto/download/${file.kid}/${encodeURIComponent(file.name)}`;
    triggerDownload(url);
}

async function downloadZip(drop, keys) {
    await prepareZip(drop, keys, status => {
        let task = download_tasks.get(status);

        if (task == null) {
            task = Log.progress(drop.name, 0, status.total);

            task.click = () => {
                let url = App.makeURL({ mode: 'drop', drop: drop.kid }, secret);
                App.go(url);
            };
            task.visible = () => (route.mode != 'drop' || route.drop != drop.kid);

            download_tasks.set(status, task);
        }

        if (status.busy) {
            let stat = status.meter.measure();
            task.progress(status.name, stat.value);
        } else if (status.error != null) {
            task.error(status.error, null);
        } else {
            task.success(T.download_complete);
        }

        refreshSoon();
    });

    // Make sure service worker is ready
    {
        let url = `/auto/zip/${drop.kid}`;
        let response = await Net.fetch(url, { method: 'HEAD' });

        if (!response.ok)
            throw new Error(T.message(`Failed to communicate with service worker for download. Refresh the page and try again.`));
    }

    let url = `/auto/zip/${drop.kid}/${encodeURIComponent(drop.name)}.zip`;
    triggerDownload(url);
}

function triggerDownload(url) {
    // Other ways, such as clicking on <a download>, do not work because some niche browsers
    // (such as Chrome) bypass the service worker for these downloads.

    let prev_unload = window.onbeforeunload;

    window.onbeforeunload = '';
    window.location.href = url;
    window.onbeforeunload = prev_unload;
}

async function otherDownloadOptions(drop, secret) {
    let file = drop.files[0];

    await UI.dialog((render, close) => {
        let curl = `curl -o ${escapeShellArgument(file.name + '.age')} ${ENV.url}/download/${file.kid}`;
        let age = `age -d -o ${escapeShellArgument(file.name)} ${escapeShellArgument(file.name + '.age')}`;
        let suffix = drop.protect ? html`<span style="color: red;">${T.password_suffix}</span>` : '';

        return html`
            <div class="title">
                ${T.format(T.download_x, drop.name)}
                <div style="flex: 1;"></div>
                <button type="button" class="secondary" @click=${UI.wrap(close)}>✖\uFE0E</button>
            </div>

            <div class="main">
                ${drop.files.length > 1 ? html`
                    <div style="text-align: center;">
                        <select name="file" @change=${e => { file = drop.files[parseInt(e.target.value, 10)]; render(); }}>
                            ${drop.files.map((it, idx) => html`<option value=${idx} ?selected=${it == file}>${it.name}</option>`)}
                        </select>
                    </div>
                ` : ''}

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
                    ${drop.protect ? html`<span class="sub" style="color: red;">${T.add_password_after_passphrase}</span>` : ''}
                </div>
            </div>

            <div class="footer">
                <button type="button" class="secondary" @click=${UI.wrap(close)}>${T.close}</button>
            </div>
        `;
    });
}

function escapeShellArgument(str) {
    let safe = str.match(/^[a-zA-Z0-9_\-\.]+$/);
    return safe ? str : `"${str.replace(/([$`"\!~])/g, '\\$1')}"`;
}

async function runSend() {
    if (!App.isLogged())
        return UserMod.runLogin();

    if (send_codename == null)
        send_codename = await Net.get('/api/drop/codename').then(json => json.codename);

    let name_placeholder = (send_files.length == 1) ? send_files[0].name : send_codename;

    UI.main({
        open: () => {
            window.addEventListener('dragenter', drop);
            window.addEventListener('dragover', drop);
            window.addEventListener('drop', drop);
            window.addEventListener('paste', drop);
        },

        close: () => {
            window.removeEventListener('dragenter', drop);
            window.removeEventListener('dragover', drop);
            window.removeEventListener('drop', drop);
            window.removeEventListener('paste', drop);
        },

        content: html`
            <div class="heading">${T.send_files}</div>

            <form @submit=${UI.wrap(submit)}>
                <div class="block" style="align-items: center;">
                    <label>
                        <span>${T.title} <span class="sub">(${T.optional.toLowerCase()})</span></span>
                        <input type="text" name="name" placeholder=${send_files.length ? name_placeholder : ''} />
                    </label>
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
                    <table class="responsive" style="table-layout: fixed;">
                        <colgroup>
                            <col class=${send_files.length > 1 ? 'check' : ''} />
                            <col/>
                            <col style="width: 100px;" />
                            <col style="width: 100px;" />
                        </colgroup>
                        <tbody>
                            ${send_files.map(file => html`
                                <tr ${UI.reorderItems(send_files, file)}>
                                    ${send_files.length > 1 ? html`<th class="grab"><img src=${ASSETS['ui/move']} width="16" height="16" alt=${T.move} /></th>` : ''}
                                    <td colspan=${send_files.length > 1 ? 1 : 2} class="nowrap" title=${file.name}>${file.name}</td>
                                    <td class="center"><span class="sub">${formatSize(file.size)}</span></td>
                                    <td class="center">
                                        <button type="button" class="small" @click=${UI.insist(e => remove_file(file))}>${T.remove}</button>
                                    </td>
                                </tr>
                            `)}
                            <tr>
                                <td colspan="4" class="center">
                                    ${!send_files.length ? html`<div class="sub" style="margin-bottom: 1em;">${T.drag_or_browse_file}</div>` : ''}
                                    <input type="file" name="file" multiple style="display: none;" @change=${UI.wrap(add_files)} />
                                    <button type="button" @click=${e => { e.target.previousElementSibling.click(); e.preventDefault(); }}>${T.add}</button>
                                </td>
                            </tr>
                        </tbody>
                    </table>
                    <label>
                        <span>${T.password} <span class="sub">(${T.optional.toLowerCase()})</span></span>
                        <input type="password" name="password" autocomplete="new-password" />
                    </label>
                </div>

                <div class="actions">
                    <button type="submit" ?disabled=${!send_files.length}>${T.send}</button>
                </div>
            </form>
        `
    });

    async function add_files(e) {
        send_files.push(...e.target.files);
    }

    function remove_file(file) {
        send_files = send_files.filter(it => it != file);
    }

    async function submit(e) {
        let form = e.currentTarget;
        let elements = form.elements;

        let sources = send_files.slice();
        let name = elements.name.value || (sources.length > 1 ? send_codename : sources[0].name);
        let expiration = (parseInt(elements.expiration.value, 10) * 86400000) || null;
        let password = elements.password.value.trim();

        if (FileApi == null)
            FileApi = await import('./file.js');
        await Util.waitFor(0); // DoEvents

        let secret = Base64.toBase64Url(FileApi.randomBytes(32));
        let passphrase = makePassphrase(secret, password);

        let drop = await createDrop(name, expiration, !!password);
        let files = await populateDrop(drop.kid, sources, passphrase);
        let total = files.reduce((acc, file) => acc + file.size, 0);

        // Encrypt and save passphrase locally
        if (session != null) {
            let db = await openLocalDB(session.userid);

            let key = Base64.toBytes(session.ckey);
            let encrypted = FileApi.encryptString(key, secret);

            await db.saveWithKey('secrets', drop.kid, encrypted);
        }

        send_codename = null;
        send_files = [];

        let task = Log.progress(name, 0, total);

        task.click = () => {
            let url = App.makeURL({ mode: 'drop', drop: drop.kid }, secret);
            App.go(url);
        };
        task.visible = () => (route.mode != 'drop' || route.drop != drop.kid);

        let status = {
            name: name,
            uploads: new Map(files.map(file => [file.kid, 0])),
            total: total,
            meter: new ProgressMeter(total),
            task: task,
            busy: true,
            error: null
        };
        upload_map.set(drop.kid, status);

        shareLink(drop.kid, secret);

        try {
            await Promise.all(files.map(file => {
                let report = uploaded => progress(status, file, uploaded);
                return uploadFile(file, file.key, file.source, report);
            }));

            await completeDrop(drop.kid);

            task.success(T.upload_complete);
        } catch (err) {
            status.error = err;
            task.error(err, null);
        } finally {
            status.busy = false;
        }
    }

    function progress(status, file, uploaded) {
        status.uploads.set(file.kid, uploaded);

        let progress = 0;
        for (let uploaded of status.uploads.values())
            progress += uploaded;

        status.meter.add(progress);
        status.task.progress(status.name, progress);

        App.go();
    }

    function drop(e) {
        let dt = e.dataTransfer ?? e.clipboardData;

        if (dt != null) {
            let files = [];
            let found = false;

            for (let i = 0; i < dt.items.length; i++) {
                let item = dt.items[i];

                if (item.kind == 'file') {
                    let file = item.getAsFile();
                    if (file != null)
                        files.push(file);
                    found = true;
                }
            }

            if (e.type == 'dragover') {
                dt.dropEffect = found ? 'move' : 'none';
            } else if (e.type == 'drop'  || e.type == 'paste') {
                send_files.push(...files);
                App.go();
            }
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

async function createDrop(name, expiration, protect) {
    let drop = await Net.post('/api/drop/create', {
        name: name,
        expiration: expiration,
        protect: protect
    });

    return drop;
}

async function populateDrop(kid, sources, passphrase) {
    let url = Util.pasteURL('/api/drop/populate', { kid: kid });

    let pairs = await Promise.all(sources.map(async src => {
        let { header, nonce, key } = await FileApi.createHeader(passphrase);

        let info = {
            name: src.name,
            size: src.size,
            header: header,
            nonce: nonce
        };

        return [info, key];
    }));

    let files = await Net.post(url, pairs.map(pair => pair[0]));

    // Regroup information
    files = files.map((file, idx) => ({
        ...file,

        name: sources[idx].name,
        source: sources[idx],
        key: pairs[idx][1]
    }));

    return files;
}

async function completeDrop(kid) {
    let url = Util.pasteURL('/api/drop/complete', { kid: kid });
    await Net.post(url);

    Net.invalidate('drop');
    Net.invalidate('drops');
}

async function uploadFile(info, key, file, progress = () => {}) {
    let stream = file.stream();
    let chunks = readChunks(stream);

    await FileApi.upload(info, key, chunks, progress);
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

async function runUpload() {
    let status = getUploadStatus(cache.drop.kid);
    if (status == null)
        throw new Error(T.message(`Incomplete drop '${cache.drop.kid}'`));

    let stat = status.meter.measure();

    if (stat.rate != null)
        refreshSoon();

    // The error notification stays open until the user comes here
    if (status.task != null && status.error != null) {
        status.task.close();
        status.task = null;
    }

    UI.main(html`
        <div class="heading">${T.send_files}</div>

        <div class="block" style="align-items: center;">
            <p>${cache.drop.name}</p>
            ${status.error == null ? html`
                <progress value=${stat.value ?? 0} max=${stat.max}></progress>
                <div class="sub" style="text-align: center;">
                    ${T.speed}${T._colon}${stat.rate != null ? formatSize(stat.rate * 1000) + '/s' : '-'}<br>
                    ${T.remaining_time}${T._colon}${stat.remaining != null ? formatDuration(stat.remaining) : '-'}
                </div>
            ` : ''}
            ${status.error != null ? html`<p style="text-align: center; color: red;">${status.error.message}</p>` : ''}
        </div>
    `);
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

function getUploadStatus(kid) {
    let status = upload_map.get(kid);
    return status;
}

function isUploading() {
    if (send_files.length > 0)
        return true;

    for (let status of upload_map.values()) {
        if (status.busy)
            return true;
    }

    return false;
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
    isDownloading,

    runSend,
    isUploading
}
