// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import { render, html, svg, unsafeHTML } from '../../../vendor/lit-html/lit-html.bundle.js';
import { Util, Log, Net, HttpError, LruMap, Mutex,
         LocalDate, LocalTime, FileReference } from '../../web/core/base.js';
import * as Data from '../../web/core/data.js';
import * as mixer from '../../web/core/mixer.js';
import * as goupile from './goupile.js';
import { profile } from './goupile.js';
import * as UI from './ui.js';
import { createExport, exportRecords } from './data_export.js';
import { DataRemote } from './data_remote.js';
import { ApplicationInfo, ApplicationBuilder } from './instance_app.js';
import { InstancePublisher } from './instance_publish.js';
import { FormState, FormModel, FormBuilder } from './form.js';
import { MetaModel, MetaInterface } from './form_meta.js';

import './instance.css';

let app = null;

// Explicit mutexes to serialize (global) state-changing functions
let data_mutex = new Mutex;

let route = {
    tid: null,
    anchor: null,
    page: null
};

let main_works = true;
let head_length = Number.MAX_SAFE_INTEGER;
let page_div = document.createElement('div');

let records = new DataRemote;

let form_thread = null;
let form_entry = null;
let form_raw = null;
let form_state = null;
let form_valid = false;
let form_model = null;
let form_cache = null;

let autosave_timer = null;

let data_tags = null;
let data_threads = null;
let data_columns = null;
let data_rows = null;

let code_buffers = new LruMap(32);
let code_builds = new LruMap(4);
let fs_timer = null;
let bundler = null;

let error_entry = new Log.Entry;
let error_map = new LruMap(8);

let editor_el;
let editor_ace;
let editor_tabs;
let editor_tab = 'form';
let editor_filename;

let ignore_editor_change = false;
let ignore_editor_scroll = 0;
let ignore_page_scroll = 0;

async function init() {
    if (profile.develop) {
        ENV.urls.files = `${ENV.urls.base}files/0/`;
        ENV.version = 0;

        bundler = await import(`${ENV.urls.static}bundler.js`);
        await bundler.init();
    }

    await initApp();
    initUI();

    if (ENV.demo)
        Log.warning(T.demo_mode_warning, 6000);
}

async function initApp() {
    try {
        let new_app = await runMainScript();

        new_app.homepage = new_app.pages.find(page => page.key == new_app.homepage) ?? null;
        app = Util.deepFreeze(new_app);
    } catch (err) {
        let new_app = new ApplicationInfo(profile);
        let builder = new ApplicationBuilder(new_app);

        // For simplicity, a lot of code assumes at least one page exists
        builder.form('default', 'Défaut', T.default_page);

        app = Util.deepFreeze(new_app);

        triggerError('main.js', err);
    }

    if (app.head != null) {
        let container = document.createElement('div');
        render(app.head, container);

        // Clear previous changes
        for (let i = document.head.children.length - 1; i >= head_length; i--)
            document.head.removeChild(document.head.children[i]);
        head_length = document.head.children.length;

        for (let child of container.children)
            document.head.appendChild(child);
    }
}

async function runMainScript() {
    let buffer = await fetchCode('main.js');

    let new_app = new ApplicationInfo(profile);
    let builder = new ApplicationBuilder(new_app);

    try {
        let build = await buildScript(buffer.code, ['app']);
        let func = build.func;

        await func({
            app: builder
        });
        if (!new_app.pages.length)
            throw new Error(T.message(`Main script does not define any page`));

        triggerError('main.js', null);
        main_works = true;
    } catch (err) {
        main_works = false;
        throw err;
    }

    return new_app;
}

function initUI() {
    UI.setMenu(renderMenu);

    if (app.panels.editor)
        UI.createPanel('editor', 0, renderEditor);
    if (app.panels.data)
        UI.createPanel('data', app.panels.editor ? 1 : 0, renderData);
    UI.createPanel('view', 1, renderPage);

    if (app.panels.editor) {
        UI.setPanels(['editor', 'view']);
    } else if (app.panels.data) {
        UI.setPanels(['data']);
    } else {
        UI.setPanels(['view']);
    }
}

function hasUnsavedData(fs = true) {
    if (fs && fs_timer != null)
        return true;

    if (form_state == null)
        return false;
    if (route.page.store == null)
        return false;

    return form_state.hasChanged();
}

async function runTasks(sync) {
    if (profile.develop && sync)
        await uploadFsChanges();
}

function renderMenu() {
    let show_menu = true;
    let wide_menu = isMenuWide(route.page) || !UI.allowTwoPanels();

    if (!UI.isPanelActive('editor') && !UI.isPanelActive('view'))
        show_menu = false;

    return html`
        <nav class=${goupile.isLocked() ? 'ui_toolbar locked' : 'ui_toolbar'} id="ui_top" style="z-index: 999999;">
            ${goupile.hasPermission('build_code') ? html`
                <div class="drop">
                    <button class=${'icon design' + (profile.develop ? ' active' : '')}
                            @click=${UI.deployMenu}>${T.conception}</button>
                    <div>
                        <button class=${profile.develop ? 'active' : ''}
                                @click=${UI.wrap(e => goupile.changeDevelopMode(!profile.develop))}>
                            <div style="flex: 1;">${T.conception_mode}</div>
                            ${profile.develop ? html`<div>&nbsp;✓\uFE0E</div>` : ''}
                        </button>
                    </div>
                </div>
            ` : ''}
            ${goupile.canUnlock() ? html`
                <button class="icon lock"
                        @click=${UI.wrap(goupile.runUnlockDialog)}>${T.unlock}</button>
            ` : ''}

            ${app.panels.editor || app.panels.data ? html`
                ${goupile.hasPermission('build_code') || goupile.canUnlock() ? html`<div style="width: 8px;"></div>` : ''}
                ${app.panels.editor ? html`
                    <button class=${'icon code' + (!UI.hasTwoPanels() && UI.isPanelActive('editor') ? ' active' : '')}
                            @click=${UI.wrap(e => togglePanels(true, false))}></button>
                ` : ''}
                ${app.panels.data && !app.panels.editor ? html`
                    <button class=${'icon data' + (!UI.hasTwoPanels() && UI.isPanelActive('data') ? ' active' : '')}
                            @click=${UI.wrap(e => togglePanels(true, false))}></button>
                ` : ''}
                ${UI.allowTwoPanels() ? html`
                    <button class=${'icon dual' + (UI.hasTwoPanels() ? ' active' : '')}
                            @click=${UI.wrap(e => togglePanels(true, true))}></button>
                ` : ''}
                ${app.panels.editor && app.panels.data ? html`
                    <div class="drop">
                        <button class=${'icon ' + (UI.isPanelActive('view') || route.tid != null ? 'view' : 'data')
                                                + (!UI.hasTwoPanels() && UI.isPanelActive(1) ? ' active' : '')}
                                @click=${UI.deployMenu}></button>
                        <div>
                            <button class=${'icon data' + (UI.isPanelActive('data') ? ' active' : '')}
                                    @click=${UI.wrap(e => togglePanels(UI.hasTwoPanels() && !UI.isPanelActive('data'), 'data'))}>${T.data}</button>
                            <button class=${'icon view' + (UI.isPanelActive('view') ? ' active' : '')}
                                    @click=${UI.wrap(e => togglePanels(UI.hasTwoPanels() && !UI.isPanelActive('view'), 'view'))}>${T.form}</button>
                        </div>
                    </div>
                ` : ''}
                ${!app.panels.editor || !app.panels.data ? html`
                    <button class=${'icon view' + (!UI.hasTwoPanels() && UI.isPanelActive('view') ? ' active' : '')}
                            @click=${UI.wrap(e => togglePanels(false, 'view'))}></button>
                ` : ''}
                <div style="flex: 1; min-width: 4px;"></div>
            ` : ''}

            ${show_menu && !wide_menu ? html`
                ${renderDropItem(route.page.chain[0], true)}
                ${Util.map(route.page.chain[0].children, page => renderDropItem(page, false))}
            ` : ''}
            ${show_menu && wide_menu ? route.page.chain.map((page, idx) => renderDropItem(page, !idx)) : ''}
            ${app.panels.data && (!UI.isPanelActive('view') || form_thread.saved) ? html`
                <div style="width: 15px;"></div>
                <button class="icon new" @click=${UI.wrap(e => go(e, route.page.chain[0].url))}>${T.add}</button>
            ` : ''}
            <div style="flex: 1; min-width: 4px;"></div>

            ${!goupile.isLocked() && profile.instances == null ?
                html`<button class="icon lines" @click=${UI.wrap(e => togglePanels(true, profile.develop ? 'view' : false))}>${ENV.title}</button>` : ''}
            ${!goupile.isLocked() && profile.instances != null ? html`
                <div class="drop right" @click=${UI.deployMenu}>
                    <button class="icon lines" @click=${UI.deployMenu}>${ENV.title}</button>
                    <div>
                        ${profile.instances.slice().sort(Util.makeComparator(instance => instance.name)).map(instance => {
                            if (instance.url === ENV.urls.instance) {
                                return html`<button class="active" @click=${UI.wrap(e => togglePanels(true, profile.develop ? 'view' : false))}>${instance.name}</button>`;
                            } else {
                                return html`<button @click=${UI.wrap(e => go(e, instance.url))}>${instance.name}</button>`;
                            }
                        })}
                    </div>
                </div>
            ` : ''}

            <div class="drop right">
                <button class=${goupile.isLoggedOnline() ? 'icon online' : 'icon offline'}
                        @click=${UI.deployMenu}>${profile.type !== 'auto' ? profile.username : ''}</button>
                <div>
                    ${profile.type === 'auto' && profile.userid ? html`
                        <button style="text-align: center;">${profile.username}</button>
                        <hr/>
                    ` : ''}
                    ${profile.type === 'login' ? html`
                        <button @click=${UI.wrap(goupile.runChangePasswordDialog)}>${T.change_my_password}</button>
                        <button @click=${UI.wrap(goupile.runResetTOTP)}>${T.configure_my_totp}</button>
                        <hr/>
                        ${goupile.hasPermission('export_create') || goupile.hasPermission('export_download') ? html`
                            <button @click=${UI.wrap(generateExportKey)}>${T.generate_export_key}</button>
                            <hr/>
                        ` : ''}
                    ` : ''}
                    ${profile.root || goupile.hasPermission('build_admin') ? html`
                        <button @click=${e => window.open('/admin/')}>${T.administration}</button>
                        <hr/>
                    ` : ''}
                    <button @click=${UI.wrap(goupile.logout)}>${profile.userid ? T.logout : T.login}</button>
                </div>
            </div>
        </nav>
    `;
}

function isMenuWide(page) {
    let root = page.chain[0];

    if (root.children.length > 3)
        return true;
    for (let child of root.children) {
        if (child.children.length)
            return true;
    }

    return false;
}

function renderDropItem(page, first) {
    if (!profile.develop && !route.page.menu)
        return '';
    if (!page.menu)
        return '';

    let active = (route.page == page);
    let url = contextualizeURL(page.url, form_thread);
    let status = computeStatus(page, form_thread);
    let enabled = status.enabled || profile.develop;

    if (first) {
        return html`<button class=${'icon home' + (active ? ' active' : '')} ?disabled=${!enabled}
                            @click=${UI.wrap(e => (page != route.page) ? go(e, url) : togglePanels(null, true))}></button>`;
    } else {
        let suffix = (UI.allowTwoPanels() && !goupile.isLocked()) ? makeStatusText(status, page.progress) : '';

        return html`
            <span style="align-self: center; margin: 0 6px;">›</span>
            <button class=${active ? 'active' : ''} ?disabled=${!enabled}
                    @click=${UI.wrap(e => (page != route.page) ? go(e, url) : togglePanels(null, true))}>
                <div style="flex: 1;">${page.menu}</div>
                ${suffix ? html`&nbsp;&nbsp;<span class="ins_status">${suffix}</span>` : ''}
           </button>
        `;
    }
}

function makeStatusText(status, details) {
    if (!status.enabled) {
        return profile.develop ? '❌\uFE0E' : '';
    } else if (status.complete) {
        return '✓\uFE0E';
    } else if (status.filled && details) {
        let progress = Math.floor(100 * status.filled / status.total);
        return html`${progress}%`;
    } else {
        return '';
    }
}

function computeStatus(page, thread) {
    if (page.children.length) {
        let status = {
            filled: 0,
            total: 0,
            complete: false,
            enabled: isPageEnabled(page, thread)
        };

        for (let child of page.children) {
            let ret = computeStatus(child, thread);

            status.filled += ret.filled;
            status.total += ret.total;
        }
        status.complete = (status.filled == status.total);

        return status;
    } else {
        let complete = (thread.entries[page.store?.key]?.anchor >= 0);

        let status = {
            filled: 0 + complete,
            total: 1,
            complete: complete,
            enabled: isPageEnabled(page, thread)
        };

        return status;
    }
}

function isPageEnabled(page, thread) {
    let enabled = page.enabled;

    if (typeof enabled == 'function') {
        try {
            enabled = enabled(thread);
        } catch (err) {
            triggerError(err);
        }
    }
    enabled = !!enabled;

    if (page.children.length)
        enabled &&= !page.children.every(child => !isPageEnabled(child, thread));

    return enabled;
}

async function generateExportKey(e) {
    let export_key = await Net.post(`${ENV.urls.instance}api/change/export_key`);

    await UI.dialog(e, T.export_key, {}, (d, resolve, reject) => {
        d.text('export_key', T.export_key, {
            value: export_key,
            readonly: true
        });
    });
}

async function togglePanels(left, right) {
    if (left != null)
        UI.togglePanel(0, left);

    if (typeof right == 'string') {
        UI.togglePanel(right, true);
    } else if (right && profile.develop && !UI.isPanelActive('data') && route.tid != null) {
        UI.togglePanel('view', true);
    } else if (right != null) {
        UI.togglePanel(1, right);
    }

    await run();

    // Special behavior for some panels
    if (left) {
        syncFormScroll();
        syncFormHighlight(true);
    }
    if (right) {
        syncEditorScroll();
        syncFormHighlight(false);
    }
}

function renderEditor() {
    // Ask ACE to adjust if needed, it needs to happen after the render
    setTimeout(() => editor_ace.resize(false), 0);

    let active_tab = editor_tabs.find(tab => tab.filename == editor_filename) ?? editor_tabs[0];

    return html`
        <div style="--menu_color: #1d1d1d; --menu_color_n1: #2c2c2c;">
            <div class="ui_toolbar">
                <div class="drop">
                    <button @click=${UI.deployMenu}>${active_tab.title}</button>
                    <div>
                        ${editor_tabs.map(tab => html`<button class=${UI.isPanelActive('editor') && tab.active ? 'active' : ''}
                                                              @click=${UI.wrap(e => toggleEditorTab(e, tab))}>${tab.title}</button>`)}
                    </div>
                </div>
                <div style="flex: 1;"></div>
                <button @click=${UI.wrap(e => runHistoryDialog(e, editor_filename))}>${T.historical}</button>
                <div style="flex: 1;"></div>
                ${editor_filename === 'main.js' ? html`
                    <button ?disabled=${!main_works || !fileHasChanged('main.js')}
                            @click=${e => { window.location.href = window.location.href; }}>${T.apply}</button>
                ` : ''}
                <button ?disabled=${!main_works}
                        @click=${UI.wrap(runPublishDialog)}>${T.publish}</button>
            </div>

            ${editor_el}
        </div>
    `;
}

function updateEditorTabs() {
    editor_tabs = [];

    editor_tabs.push({
        key: 'project',
        title: T.project,
        filename: 'main.js',
        active: false
    });
    if (route.page.filename != null) {
        editor_tabs.push({
            key: 'form',
            title: T.form,
            filename: route.page.filename,
            active: false
        });
    }

    let active_tab = editor_tabs.find(tab => tab.key == editor_tab);

    if (active_tab != null) {
        editor_tab = active_tab.key;
        editor_filename = active_tab.filename;
    } else {
        editor_filename = editor_tabs[0].filename;
    }
}

async function runHistoryDialog(e, filename) {
    await uploadFsChanges();
    await fetchCode(filename);

    let url = Util.pasteURL(`${ENV.urls.base}api/files/history`, { filename: filename });
    let versions = await Net.get(url);

    let buffer = code_buffers.get(filename);
    let copy = Object.assign({}, code_buffers.get(filename));

    // Don't trash the undo/redo buffer
    buffer.session = null;

    let p = UI.dialog(e, T.file_history, {}, (d, resolve, reject) => {
        d.output(html`
            <table class="ui_table">
                <colgroup>
                    <col/>
                    <col/>
                    <col/>
                </colgroup>

                <tbody>
                    <tr class=${buffer.version == 0 ? 'active' : ''}>
                        <td class="ui_sub">(dev)</td>
                        <td>En développement</td>
                        <td><a @click=${UI.wrap(e => loadFile(filename, 0))}>${T.load}</a></td>
                    </tr>

                    ${Util.mapRange(0, versions.length - 1, idx => {
                        let version = versions[versions.length - idx - 1];

                        return html`
                            <tr class=${buffer.version == version.version ? 'active' : ''}>
                                <td class="ui_sub">${version.version}</td>
                                <td>${(new Date(version.mtime)).toLocaleString()}</td>
                                <td><a @click=${UI.wrap(e => loadFile(filename, version.version))}>${T.load}</a></td>
                            </tr>
                        `;
                    })}
                </tbody>
            </table>
        `);

        d.action(T.restore, { disabled: !d.isValid() || buffer.version == 0 }, async () => {
            await restoreFile(filename, buffer.sha256);
            resolve();
        });
    });
    p.catch(async err => {
        code_buffers.set(filename, copy);

        if (filename == 'main.js') {
            try {
                await runMainScript();
            } catch (err) {
                triggerError('main.js', err);
            }
        }
        run();

        throw err;
    });

    return p;
}

async function loadFile(filename, version) {
    let url = Util.pasteURL(`${ENV.urls.base}files/${version}/${filename}`, { bundle: 0 });
    let response = await Net.fetch(url);

    if (response.ok) {
        let code = await response.text();
        updateBuffer(filename, code, version);

        return run();
    } else if (response.status == 404) {
        updateBuffer(filename, '', version);
        return run();
    } else {
        let err = await Net.readError(response);
        throw new Error(err);
    }
}

async function restoreFile(filename, sha256) {
    let db = await goupile.openLocalDB();

    await Net.post(`${ENV.urls.base}api/files/restore`, {
        filename: filename,
        sha256: sha256
    });

    let key = `${profile.userid}/${filename}`;
    await db.delete('changes', key);

    code_buffers.delete(filename);

    return run();
}

function renderData() {
    let recording_new = !form_thread.saved && form_state.hasChanged();

    return html`
        <div class="padded">
            <div class="ui_quick" style="margin-right: 2.2em;">
                <div style="display: flex; gap: 8px; padding-bottom: 4px;">
                    <div class="fm_check">
                        <input id="ins_tags" type="checkbox" .checked=${data_tags != null}
                               @change=${UI.wrap(e => toggleTagFilter(null))} />
                        <label for="ins_tags">${T.filter}${T._colon}</label>
                    </div>
                    ${app.tags.map(tag => {
                        if (!tag.filter)
                            return '';

                        let id = 'ins_tag_' + tag.key;

                        return html`
                            <div class=${data_tags == null ? 'fm_check disabled' : 'fm_check'} style="padding-top: 0;">
                                <input id=${id} type="checkbox"
                                       ?disabled=${data_tags == null}
                                       .checked=${data_tags?.has?.(tag.key)}
                                       @change=${UI.wrap(e => toggleTagFilter(tag.key))} />
                                <label for=${id}><span class="ui_tag" style=${'background: ' + tag.color + ';'}>${tag.label}</label>
                            </div>
                        `;
                    })}
                </div>
            </div>

            <table class="ui_table fixed" id="ins_data"
                   style=${'min-width: ' + (5 + 5 * data_columns.length) + 'em;'}>
                <colgroup>
                    <col style="width: 6.5em;" />
                    <col style="width: 8em;"/>
                    ${Util.mapRange(0, data_columns.length, () => html`<col/>`)}
                    <col style="width: 2em;"/>
                </colgroup>

                <thead>
                    <tr>
                        <th>${T.sequence}</th>
                        <th>${T.creation}</th>
                        ${data_columns.map(col => {
                            let stats = `${col.count} / ${data_rows.length}`;
                            let title = `${col.page.title}\n${T.available}${T._colon}${stats} ${data_rows.length > 1 ? T.rows.toLowerCase() : T.row.toLowerCase()}`;

                            return html`
                                <th title=${title}>
                                    ${col.page.title}<br/>
                                    <span style="font-size: 0.7em; font-weight: normal;">${stats}</span>
                                </th>
                            `;
                        })}
                    </tr>
                </thead>

                <tbody>
                    ${data_rows.map(row => {
                        let active = (row.tid == route.tid);
                        let cls = (row.sequence == null ? 'missing' : '') + (active ? ' active' : '');

                        return html`
                            <tr>
                                <td class=${cls} title=${row.sequence}>${row.sequence != null ? row.sequence : 'NA'}</td>
                                <td class=${active ? ' active' : ''} title=${row.ctime.toLocaleString()}>${row.ctime.toLocaleDateString()}</td>
                                ${data_columns.map(col => {
                                    let entry = row.entries[col.page.store.key];

                                    let status = computeStatus(col.page, row);
                                    let summary = entry?.summary;
                                    let url = col.page.url + `/${row.tid}`;
                                    let highlight = active && route.page.chain.includes(col.page);

                                    let tooltip = col.page.title;
                                    let dots = '';

                                    if (entry != null) {
                                        let tags = app.tags.filter(tag => entry.tags.includes(tag.key));

                                        if (tags.length) {
                                            tooltip += '\nTags : ' + tags.map(tag => tag.label).join(', ');
                                            dots = tags.map(tag => html` <span style=${'color: ' + tag.color + ';'}>⏺\uFE0E</span>`);
                                        }
                                    }

                                    if (status.complete) {
                                        return html`<td class=${highlight ? 'complete active' : 'complete'}
                                                        title=${tooltip}><a href=${url}>${summary ?? '✓\uFE0E ' + T.filled}</a>${dots}</td>`;
                                    } else if (status.filled) {
                                        let progress = Math.floor(100 * status.filled / status.total);

                                        return html`<td class=${highlight ? 'partial active' : 'partial'}
                                                        title=${tooltip}><a href=${url}>${summary ?? progress + '%'}</a>${dots}</td>`;
                                    } else {
                                        return html`<td class=${highlight ? 'missing active' : 'missing'}
                                                        title=${tooltip}><a href=${url}>${T.show}</a>${dots}</td>`;
                                    }
                                })}
                                ${row.locked ? html`<th>🔒</th>` : ''}
                                ${goupile.hasPermission('data_delete') && !row.locked ?
                                    html`<th><a @click=${UI.wrap(e => runDeleteRecordDialog(e, row))}>✕</a></th>` : ''}
                            </tr>
                        `;
                    })}
                    ${recording_new ? html`
                        <tr>
                            <td class="active missing">NA</td>
                            <td class="active missing">NA</td>
                            <td class="missing" colspan=${data_columns.length}><a @click=${e => togglePanels(null, 'view')}>${T.new_record}</a></td>
                        </tr>
                    ` : ''}
                    ${!data_rows.length && !recording_new ? html`<tr><td colspan=${2 + data_columns.length}>${T.no_row}</td></tr>` : ''}
                </tbody>
            </table>

            <div class="ui_actions">
                ${goupile.hasPermission('export_create') || goupile.hasPermission('export_download') ? html`<button @click=${UI.wrap(runExportDialog)}>${T.exports}</button>` : ''}
            </div>
        </div>
    `;
}

function runDeleteRecordDialog(e, row) {
    return UI.confirm(e, T.format(T.confirm_record_deletion, row.sequence),
                         T.delete, async () => {
        await data_mutex.run(async () => {
            await records.delete(row.tid);

            if (route.tid == row.tid)
                await openRecord(null, null, route.page);

            data_threads = null;
        });

        go();
    });
}

async function runExportDialog(e) {
    let downloads = await Net.get(`${ENV.urls.instance}api/export/list`);
    let stores = app.stores.map(store => store.key);

    downloads.reverse();

    await UI.dialog(e, T.exports, {}, (d, resolve, reject) => {
        let intf = d.tabs('tabs', () => {
            d.tab(T.new_export, () => {
                if (downloads.length > 0) {
                    d.enumRadio('mode', T.export_mode, [
                        ['all', T.export_all],
                        ['sequence', T.export_sequence],
                        ['anchor', T.export_anchor]
                    ], { value: 'all', untoggle: false });

                    if (d.values.mode != 'all') {
                        let props = downloads.map(download => [download.export, (new Date(download.ctime)).toLocaleString()]);
                        d.enumDrop('since', T.export_since, props, { value: downloads[0]?.export, untoggle: false });
                    }
                } else {
                    d.output(T.create_first_export);
                }

                d.action(T.create_export, {}, async () => {
                    let sequence = null;
                    let anchor = null;

                    switch (d.values.mode) {
                        case 'sequence': {
                            let download = downloads.find(download => download.export == d.values.since);
                            sequence = download.sequence + 1;
                        } break;

                        case 'anchor': {
                            let download = downloads.find(download => download.export == d.values.since);
                            anchor = download.anchor + 1;
                        } break;
                    }

                    await create(sequence, anchor);

                    resolve();
                });
            }, { disabled: !goupile.hasPermission('export_create') });

            d.tab(T.past_exports, () => {
                d.output(html`
                    <table class="ui_table">
                        <colgroup>
                            <col/>
                            <col/>
                            <col/>
                            <col/>
                        </colgroup>

                        <thead>
                            <tr>
                                <th>${T.date}</th>
                                <th>${T.threads}</th>
                                <th>${T.automatic}</th>
                                <th>${T.download}</th>
                            </tr>
                        </thead>

                        <tbody>
                            ${downloads.map(download => html`
                                <tr>
                                    <td>${(new Date(download.ctime)).toLocaleString()}</td>
                                    <td>${download.threads}</td>
                                    <td>${download.scheduled ? T.yes : T.no}</td>
                                    <td>
                                        ${download.threads ? html`<a @click=${UI.wrap(e => exportRecords(download.export, null, stores))}>${T.download}</a>` : ''}
                                        ${!download.threads ? '(' + T.not_available.toLowerCase() + ')' : ''}
                                    </td>
                                </tr>
                            `)}
                            ${!downloads.length ? html`<tr><td colspan="4">${T.no_export}</td></tr>` : ''}
                        </tbody>
                    </table>
                `);
            }, { disabled: !goupile.hasPermission('export_download') });
        });
    });

    async function create(sequence, anchor) {
        let progress = Log.progress(T.export_in_progress);

        try {
            let info = await createExport(sequence, anchor);
            let stores = app.stores.map(store => store.key);

            await exportRecords(info.export, info.secret, stores);

            progress.success(T.export_done);
        } catch (err) {
            progress.close();
            Log.error(err);
        }
    }
}

async function runExportListDialog(e) {
    let downloads = await Net.get(`${ENV.urls.instance}api/export/list`);
    let stores = app.stores.map(store => store.key);

    downloads.reverse();

    await UI.dialog(e, T.data_exports, {}, (d, resolve, reject) => {
        d.output(html`
            <table class="ui_table">
                <colgroup>
                    <col/>
                    <col/>
                    <col/>
                    <col/>
                </colgroup>

                <thead>
                    <tr>
                        <th>${T.date}</th>
                        <th>${T.threads}</th>
                        <th>${T.automatic}</th>
                        <th>${T.download}</th>
                    </tr>
                </thead>

                <tbody>
                    ${downloads.map(download => html`
                        <tr>
                            <td>${(new Date(download.ctime)).toLocaleString()}</td>
                            <td>${download.threads}</td>
                            <td>${download.scheduled ? T.yes : T.no}</td>
                            <td>
                                ${download.threads ? html`<a @click=${UI.wrap(e => exportRecords(download.export, null, stores))}>${T.download}</a>` : ''}
                                ${!download.threads ? '(' + T.not_available.toLowerCase() + ')' : ''}
                            </td>
                        </tr>
                    `)}
                    ${!downloads.length ? html`<tr><td colspan="4">${T.no_export}</td></tr>` : ''}
                </tbody>
            </table>
        `);
    });
}

function toggleTagFilter(tag) {
    if (tag == null) {
        data_tags = (data_tags == null) ? new Set : null;
    } else if (data_tags != null) {
        if (!data_tags.delete(tag))
            data_tags.add(tag);
    }

    return go();
}

async function renderPage() {
    let show_menu = (route.page.chain.length > 2 ||
                     route.page.chain[0].children.length > 1);
    let wide_menu = isMenuWide(route.page);

    if (form_valid) {
        render(form_model.renderWidgets(), page_div);
        page_div.classList.remove('disabled');
    } else {
        if (!page_div.children.length)
            render(T.failed_to_build_page, page_div);
        page_div.classList.add('disabled');
    }

    // Try to map widget code lines for developers
    if (profile.develop) {
        let buffer = code_buffers.get(route.page.filename);
        let build = code_builds.get(buffer?.sha256);

        if (build?.map != null) {
            for (let intf of form_model.widgets) {
                if (intf.id == null)
                    continue;
                if (intf.location == null)
                    continue;

                let el = page_div.querySelector('#' + intf.id);
                let wrap = (el != null) ? Util.findParent(el, el => el.classList.contains('fm_wrap')) : null;

                if (wrap != null) {
                    let line = bundler.mapLine(build.map, intf.location.line, intf.location.column);
                    wrap.dataset.line = line;
                }
            }
        }
    }

    // Quick access to page sections
    let page_sections = form_model.widgets.filter(intf => intf.options.anchor).map(intf => ({
        title: intf.label,
        anchor: intf.options.anchor
    }));

    return html`
        <div class="print" @scroll=${syncEditorScroll}}>
            <div id="ins_page">
                <div id="ins_menu">
                    ${show_menu ? Util.mapRange(1 - wide_menu, route.page.chain.length,
                                                idx => renderPageMenu(route.page.chain[idx])) : ''}
                </div>

                <form id="ins_content" autocomplete="off" @submit=${e => e.preventDefault()}>
                    ${page_div}
                </form>

                <div id="ins_actions">
                    ${form_model.renderActions()}

                    ${page_sections.length > 1 ? html`
                        <h1>${route.page.title}</h1>
                        <ul>${page_sections.map(section => html`<li><a href=${'#' + section.anchor}>${section.title}</a></li>`)}</ul>
                    ` : ''}
                </div>
            </div>
            <div style="flex: 1;"></div>

            ${profile.develop && !isPageEnabled(route.page, form_thread) ?
                html`<div id="ins_develop">${T.page_disabled_warning}</div>` : ''}

            ${form_model.actions.length ? html`
                <nav class="ui_toolbar" id="ins_tasks" style="z-index: 999999;">
                    <div style="flex: 1;"></div>
                    ${form_model.actions.some(action => !action.options.always) ? html`
                        <div class="drop up right">
                            <button @click=${UI.deployMenu}>${T.other_actions}</button>
                            <div>
                                ${form_model.actions.map(action => action.render())}
                            </div>
                        </div>
                        <hr/>
                    ` : ''}
                    ${Util.mapRange(0, form_model.actions.length, idx => {
                        let action = form_model.actions[form_model.actions.length - idx - 1];

                        if (action.label.match(/^\-+$/))
                            return '';
                        if (!action.options.always)
                            return '';

                        return action.render();
                    })}
                    <div style="flex: 1;"></div>
                </nav>
            ` : ''}
        </div>
    `;
}

function defaultFormPage(ctx) {
    let app = ctx.app;
    let form = ctx.form;
    let meta = ctx.meta;
    let page = ctx.page;
    let thread = ctx.thread;
    let values = ctx.values;

    form.output(html`
        ${page.help ? html`<div class="ins_help">${page.help}</div>` : ''}

        <div id="ins_levels">
            ${page.chain.map(child => {
                if (!child.progress && !child.help)
                    return '';

                let url = meta.contextualize(child.url, thread);
                let status = meta.status(child, thread);

                let cls = 'ins_level';

                if (!status.enabled) {
                    cls += ' disabled';
                    text = T.not_available;
                } else if (status.complete) {
                    cls += ' complete';
                    text = T.filled;
                } else if (status.filled && child.progress) {
                    let progress = Math.floor(100 * status.filled / status.total);

                    text = html`
                        <div class="ins_progress" style=${'--progress: ' + progress}>
                            <div></div>
                            <span>${status.filled} ${status.filled > 1 ? T.items.toLowerCase() : T.item.toLowerCase()} / ${status.total}</span>
                        </div>
                    `;
                } else {
                    text = '';
                }

                return html`
                    <div class=${cls} @click=${UI.wrap(e => go(e, url))}>
                        <div class="title">${child.title}</div>
                        <div class="status">${text}</div>
                    </div>
                `;
            })}
        </div>

        <div id="ins_tiles">
            ${page.children.map((child, idx) => {
                let url = meta.contextualize(child.url, thread);
                let status = meta.status(child, thread);

                let cls = 'ins_tile';
                let text = null;

                if (!status.enabled) {
                    cls += ' disabled';
                    text = T.not_available;
                } else if (status.complete) {
                    cls += ' complete';
                    text = T.filled;
                } else if (status.filled && child.progress) {
                    let progress = Math.floor(100 * status.filled / status.total);

                    text = html`
                        <div class="ins_progress" style=${'--progress: ' + progress}>
                            <div></div>
                            <span>${status.filled}/${status.total}</span>
                        </div>
                    `;
                } else {
                    text = '';
                }

                return html`
                    <div class=${cls} @click=${UI.wrap(e => go(e, url))}>
                        ${child.icon != null ? html`<img src=${child.icon} alt="" />` : ''}
                        ${child.icon == null ? html`<div class="index">${idx + 1}</div>` : ''}
                        <div class="title">${child.title}</div>
                        <div class="status">${text}</div>
                    </div>
                `;
            })}
        </div>
    `);
}

function addAutomaticActions(builder, model) {
    if (builder.hasErrors())
        builder.errorList();

    builder.resetOptions();

    if (route.page.store != null) {
        let next = selectNextPage(route.page, form_thread);

        let can_save = !form_thread.locked && model.variables.length &&
                       (form_state.hasChanged() || next != null);
        let can_lock = form_thread.saved && (route.page.lock != null) &&
                       (!form_thread.locked || goupile.hasPermission('data_audit'));

        if (can_save) {
            let label = '+' + (next != null ? T.continue : T.save);

            builder.action(label, { color: '#2d8261' }, async e => {
                form_state.triggerErrors(form_model);

                await data_mutex.run(async () => {
                    let keep = goupile.hasPermission('data_read') || route.page.claim;
                    let finalize = (route.page.lock === true) || !keep;
                    let confirm = route.page.confirm ?? finalize;

                    if (confirm) {
                        let text = finalize ? unsafeHTML(T.confirm_final) : unsafeHTML(T.confirm_save);
                        await UI.confirm(e, text, T.continue, () => {});
                    }

                    await saveRecord(form_thread, route.page, form_state, form_entry, form_raw, form_meta, false);

                    if (keep) {
                        await openRecord(form_thread.tid, null, route.page);
                    } else {
                        let page = route.page.chain[0];
                        await openRecord(null, null, page);
                    }

                    data_threads = null;
                });

                if (next == null)
                    next = route.page;

                let url = contextualizeURL(next.url, form_thread);
                go(null, url);
            });
        }

        if (can_lock) {
            let label = !form_thread.locked ? T.lock : T.unlock;
            let color = !form_thread.locked ? '#ff6600' : null;

            builder.action(label, { color: color }, async e => {
                if (form_state.hasChanged())
                    await UI.confirm(e, unsafeHTML(T.confirm_forget_changes), label, () => {});

                if (!form_thread.locked) {
                    await records.lock(form_thread.tid);
                } else {
                    await records.unlock(form_thread.tid);
                }

                await data_mutex.run(async () => {
                    await openRecord(form_thread.tid, null, route.page);
                    data_threads = null;
                });

                go();
            });
        }

        if (form_entry.anchor >= 0 && form_state.hasChanged()) {
            builder.action('-');
            builder.action(T.forget_changes, {}, async e => {
                await UI.confirm(e, unsafeHTML(T.confirm_forget_changes),
                                    T.forget, () => {});

                await data_mutex.run(async () => {
                    await openRecord(route.tid, route.anchor, route.page);
                });

                go();
            });
        }
    }
}

function selectNextPage(page, thread) {
    let sequence = page.sequence;
    let entry = thread.entries[page.store.key];

    if (sequence == null)
        return null;
    if (UI.isPanelActive('data'))
        return null;

    if (entry.anchor < 0) {
        if (typeof sequence == 'string') {
            let next = app.pages.find(page => page.key == sequence);
            return next;
        }

        if (sequence === true) {
            let parent = page.chain[page.chain.length - 2];
            sequence = (parent != null) ? parent.children.map(page => page.key) : [];
        }

        if (Array.isArray(sequence)) {
            let idx = sequence.indexOf(page.key);

            if (idx < 0) {
                console.error(T.message(`Page '{1}' is not in sequence`, page.key));
                return null;
            }

            if (idx + 1 < sequence.length) {
                let next = app.pages.find(page => page.key == sequence[idx + 1]);
                return next;
            }
        }
    }

    // Try to go back to parent
    {
        let next = null;

        for (let i = page.chain.length - 2; i >= 0; i--) {
            next = page.chain[i];

            let status = computeStatus(next, thread);

            if (next.url != page.url && !status.complete)
                break;
        }

        if (next != null)
            return next;
    }

    return null;
}

function addAutomaticTags(variables) {
    for (let intf of variables) {
        let tags = [];

        let notes = Data.annotate(intf.key.ptr, intf.key.name);
        let status = notes.status ?? {};
        let error = intf.errors.some(err => !err.delay);

        if (form_thread.locked)
            intf.options.readonly = true;

        if (status.locked) {
            tags.push('locked');
            intf.options.readonly = true;
        } else if (status.filling == 'check') {
            tags.push('check');

            for (let err of intf.errors)
                err.block = false;
        } else if (status.filling == 'wait') {
            tags.push('wait');

            for (let err of intf.errors)
                err.block = false;
        } else if (status.filling != null) {
            if (intf.missing) {
                tags.push('na');

                for (let err of intf.errors)
                    err.block = false;
            } else {
                status.filling = null;
            }
        } else if (intf.missing && intf.options.mandatory) {
            if (form_entry.anchor >= 0 || intf.errors.some(err => !err.delay)) {
                tags.push('incomplete');
                error = false;
            }
        }

        if (error)
            tags.push('error');

        if (Array.isArray(intf.options.tags))
            tags.push(...intf.options.tags);

        intf.options.tags = app.tags.filter(tag => tags.includes(tag.key));
    }
}

function renderPageMenu(page) {
    if (!profile.develop && !route.page.menu)
        return '';
    if (!page.children.some(child => child.menu))
        return '';
    if (!profile.develop && !page.children.some(child => isPageEnabled(child)))
        return '';

    return html`
        <a href=${contextualizeURL(page.url, form_thread)}>${page.menu || page.title}</a>
        <ul>
            ${Util.map(page.children, (child, idx) => {
                if (!child.menu)
                    return '';

                let active = route.page.chain.includes(child);
                let url = contextualizeURL(child.url, form_thread);
                let status = computeStatus(child, form_thread);

                if (!profile.develop && !status.enabled)
                    return '';

                return html`
                    <li><a class=${active ? 'active' : ''} href=${url}>
                        <div style="flex: 1;">${child.menu}</div>
                        <span style="font-size: 0.9em;">${makeStatusText(status, child.progress)}</span>
                    </a></li>
                `;
            })}
        </ul>
    `;
}

async function syncEditor() {
    if (editor_el == null) {
        if (typeof ace === 'undefined') {
            await import(`${ENV.urls.static}ace/ace.js`);
            ace.config.set('basePath', `${ENV.urls.static}ace`);
        }

        editor_el = document.createElement('div');
        editor_el.setAttribute('style', 'flex: 1;');
        editor_ace = ace.edit(editor_el);

        editor_ace.setShowPrintMargin(false);
        editor_ace.setFontSize(13);
        editor_ace.setBehavioursEnabled(false);
        editor_ace.setOptions({
            scrollPastEnd: 1
        });
    }

    let buffer = code_buffers.get(editor_filename);

    if (buffer == null) {
        await fetchCode(editor_filename);
        buffer = code_buffers.get(editor_filename);
    }

    if (buffer.session == null) {
        let session = new ace.EditSession('', 'ace/mode/javascript');

        session.setOption('useWorker', false);
        session.setUseWrapMode(true);
        session.doc.setValue(buffer.code);
        session.setUndoManager(new ace.UndoManager());
        session.on('change', e => handleFileChange(editor_filename));

        session.on('changeScrollTop', () => {
            if (UI.isPanelActive('view'))
                setTimeout(syncFormScroll, 0);
        });
        session.on('changeSelection', () => {
            syncFormHighlight(true);
            ignore_editor_scroll = performance.now();
        });

        buffer.session = session;
    }

    if (editor_filename === 'main.js') {
        editor_ace.setTheme('ace/theme/monokai');
    } else {
        editor_ace.setTheme('ace/theme/merbivore_soft');
    }

    editor_ace.setSession(buffer.session);
}

function toggleEditorTab(e, tab) {
    editor_tab = tab.key;
    editor_filename = tab.filename;

    return togglePanels(true, null);
}

async function handleFileChange(filename) {
    if (ignore_editor_change)
        return;

    let db = await goupile.openLocalDB();

    let buffer = code_buffers.get(filename);
    let code = buffer.session.doc.getValue();
    let blob = new Blob([code]);
    let sha256 = await mixer.Sha256.async(blob);

    let key = `${profile.userid}/${filename}`;

    await db.saveWithKey('changes', key, {
        filename: filename,
        size: blob.size,
        sha256: sha256,
        blob: blob
    });

    buffer.code = code;
    buffer.sha256 = sha256;

    try {
        let build = await buildScript(buffer.code, ['app', 'cache', 'form',
                                                    'meta', 'page', 'thread', 'values']);
        code_builds.set(buffer.sha256, build);

        triggerError(filename, null);
    } catch (err) {
        triggerError(filename, err);
    }

    if (fs_timer != null)
        clearTimeout(fs_timer);
    fs_timer = setTimeout(uploadFsChanges, 2000);

    if (filename == 'main.js') {
        try {
            await runMainScript();
        } catch (err) {
            triggerError('main.js', err);
        }
    }
    run();
}

const uploadFsChanges = Util.serialize(async function () {
    let progress = Log.progress('Sauvegarde des modifications');

    try {
        let db = await goupile.openLocalDB();

        let range = IDBKeyRange.bound(profile.userid + '/',
                                      profile.userid + '`', false, true);
        let changes = await db.loadAll('changes', range);

        for (let file of changes) {
            let url = Util.pasteURL(`${ENV.urls.base}files/${file.filename}`, { sha256: file.sha256 });

            let response = await Net.fetch(url, {
                method: 'PUT',
                body: file.blob,
                timeout: null
            });
            if (!response.ok && response.status !== 409) {
                let err = await Net.readError(response);
                throw new Error(err)
            }

            let key = `${profile.userid}/${file.filename}`;
            await db.delete('changes', key);
        }

        progress.close();
    } catch (err) {
        progress.close();
        Log.error(err);
    }

    if (fs_timer != null)
        clearTimeout(fs_timer);
    fs_timer = null;
});

function syncFormScroll() {
    if (!UI.isPanelActive('editor') || !UI.isPanelActive('view'))
        return;
    if (performance.now() - ignore_editor_scroll < 500)
        return;
    if (!editor_ace.isFocused())
        return;

    try {
        let panel_el = document.querySelector('#ins_page').parentNode;
        let widget_els = panel_el.querySelectorAll('*[data-line]');

        let editor_line = editor_ace.getFirstVisibleRow() + 1;

        let prev_line;
        for (let i = 0; i < widget_els.length; i++) {
            let line = getElementLine(widget_els[i]);

            if (line >= editor_line) {
                if (!i) {
                    ignore_page_scroll = performance.now();
                    panel_el.scrollTop = 0;
                } else if (i === widget_els.length - 1) {
                    let top = computeRelativeTop(panel_el, widget_els[i]);

                    ignore_page_scroll = performance.now();
                    panel_el.scrollTop = top;
                } else {
                    let top1 = computeRelativeTop(panel_el, widget_els[i - 1]);
                    let top2 = computeRelativeTop(panel_el, widget_els[i]);
                    let frac = (editor_line - prev_line) / (line - prev_line);

                    ignore_page_scroll = performance.now();
                    panel_el.scrollTop = top1 + frac * (top2 - top1);
                }

                break;
            }

            prev_line = line;
        }
    } catch (err) {
        // Meh, don't wreck the editor if for some reason we can't sync the
        // two views, this is not serious so just log it.
        console.log(err);
    }
}

function computeRelativeTop(parent, el) {
    let top = 0;
    while (el !== parent) {
        top += el.offsetTop;
        el = el.offsetParent;
    }
    return top;
}

function syncFormHighlight(scroll) {
    if (!UI.isPanelActive('view'))
        return;

    try {
        let panel_el = document.querySelector('#ins_page').parentNode;
        let widget_els = panel_el.querySelectorAll('*[data-line]');

        if (UI.isPanelActive('editor') && widget_els.length) {
            let selection = editor_ace.session.selection;
            let editor_lines = [
                selection.getRange().start.row + 1,
                selection.getRange().end.row + 1
            ];

            let highlight_first;
            let highlight_last;
            for (let i = 0;; i++) {
                let line = getElementLine(widget_els[i]);

                if (line > editor_lines[0]) {
                    if (i > 0)
                        highlight_first = i - 1;
                    break;
                }

                if (i >= widget_els.length - 1) {
                    highlight_first = i;
                    break;
                }
            }
            if (highlight_first != null) {
                highlight_last = highlight_first;

                while (highlight_last < widget_els.length) {
                    let line = getElementLine(widget_els[highlight_last]);
                    if (line > editor_lines[1])
                        break;
                    highlight_last++;
                }
                highlight_last--;
            }

            for (let i = 0; i < widget_els.length; i++)
                widget_els[i].classList.toggle('ins_highlight', i >= highlight_first && i <= highlight_last);

            // Make sure widget is in viewport
            if (scroll && highlight_first != null &&
                          highlight_last === highlight_first) {
                let el = widget_els[highlight_first];
                let rect = el.getBoundingClientRect();

                if (rect.top < 0) {
                    ignore_page_scroll = performance.now();
                    panel_el.scrollTop += rect.top - 50;
                } else if (rect.bottom >= window.innerHeight) {
                    ignore_page_scroll = performance.now();
                    panel_el.scrollTop += rect.bottom - window.innerHeight + 30;
                }
            }
        } else {
            for (let el of widget_els)
                el.classList.remove('ins_highlight');
        }
    } catch (err) {
        // Meh, don't wreck the editor if for some reason we can't sync the
        // two views, this is not serious so just log it.
        console.log(err);
    }
}

function syncEditorScroll() {
    if (!UI.isPanelActive('editor') || !UI.isPanelActive('view'))
        return;
    if (performance.now() - ignore_page_scroll < 500)
        return;

    try {
        let panel_el = document.querySelector('#ins_page').parentNode;
        let widget_els = panel_el.querySelectorAll('*[data-line]');

        let prev_top;
        let prev_line;
        for (let i = 0; i < widget_els.length; i++) {
            let el = widget_els[i];

            let top = el.getBoundingClientRect().top;
            let line = getElementLine(el);

            if (top >= 0) {
                if (!i) {
                    ignore_editor_scroll = performance.now();
                    editor_ace.renderer.scrollToLine(0);
                } else {
                    let frac = -prev_top / (top - prev_top);
                    let line2 = Math.floor(prev_line + frac * (line - prev_line));

                    ignore_editor_scroll = performance.now();
                    editor_ace.renderer.scrollToLine(line2);
                }

                break;
            }

            prev_top = top;
            prev_line = line;
        }
    } catch (err) {
        // Meh, don't wreck anything if for some reason we can't sync the
        // two views, this is not serious so just log it.
        console.log(err);
    }
}

function getElementLine(el) {
    let line = parseInt(el.dataset.line, 10);
    return line;
}

async function runPublishDialog(e) {
    await uploadFsChanges();

    let publisher = new InstancePublisher(bundler);
    await publisher.runDialog(e);

    run();
}

async function go(e, url = null, options = {}) {
    options = Object.assign({ push_history: true }, options);

    await data_mutex.run(async () => {
        let new_route = Object.assign({}, route);
        let explicit_panels = null;

        if (url != null) {
            if (!(url instanceof URL))
                url = new URL(url, window.location.href);

            if (route.page == null && url.pathname == ENV.urls.instance) {
                let page = app.homepage ?? app.pages[0];
                url = new URL(page.url, window.location.href);

                if (app.homepage != null)
                    url.searchParams.set('p', 'view');
            }
            goupile.setCurrentHash(url.hash);

            if (!url.pathname.endsWith('/'))
                url.pathname += '/';

            // Goodbye!
            if (!url.pathname.startsWith(ENV.urls.instance)) {
                if (fs_timer != null)
                    await uploadFsChanges();
                if (hasUnsavedData(false))
                    await goupile.confirmDangerousAction(e);

                window.onbeforeunload = null;
                window.location.href = url.href;

                return;
            }

            let path = url.pathname.substr(ENV.urls.instance.length);
            let [key, what] = path.split('/').map(str => str.trim());

            // Find page information
            new_route.page = app.pages.find(page => page.key == key);
            if (new_route.page == null) {
                Log.error(T.message(`Page '{1}' does not exist`, key));
                new_route.page = app.homepage ?? app.pages[0];
            }

            let [tid, anchor] = what ? what.split('@') : [null, null];

            // Deal with TID and anchor
            if (tid != new_route.tid) {
                new_route.tid = tid || null;
                new_route.anchor = null;
            }
            if (anchor != null) {
                anchor = anchor.trim();

                if (anchor.match(/^[0-9]+$/)) {
                    new_route.anchor = parseInt(anchor, 10);
                } else if (!anchor.length) {
                    new_route.anchor = null;
                } else {
                    Log.error(T.message(`Version indicator is not a number`));
                    new_route.anchor = null;
                }
            }

            // Handle single-thread mode
            if (profile.single && new_route.tid == null) {
                let threads = await Net.get(`${ENV.urls.instance}api/records/list`);

                if (threads.length)
                    new_route.tid = threads[0].tid;
            }

            // Restore explicit panels (if any)
            let panels = url.searchParams.get('p');

            if (panels) {
                panels = panels.split('|');
                panels = panels.filter(key => app.panels.hasOwnProperty(key) || key == 'view');

                if (panels.length)
                    explicit_panels = panels;
            }
        }

        let context_change = (new_route.tid != route.tid ||
                              new_route.anchor != route.anchor ||
                              new_route.page != route.page);
        let first_run = (route.page == null);

        // Warn about data loss before loading new data
        if (context_change) {
            if (hasUnsavedData(false)) {
                try {
                    if (route.page.autosave) {
                        clearTimeout(autosave_timer);
                        autosave_timer = null;

                        await saveRecord(form_thread, route.page, form_state, form_entry, form_raw, form_meta, true);
                    } else {
                        await UI.dialog(e, T.context_confirm, {}, (d, resolve, reject) => {
                            d.output(unsafeHTML(T.context_continue));

                            d.enumRadio('save', T.context_action, [
                                [true, T.context_save],
                                [false, T.context_forget]
                            ], { value: true, untoggle: false });

                            if (d.values.save) {
                                d.action(T.save, {}, async e => {
                                    try {
                                        form_state.triggerErrors(form_model);
                                        await saveRecord(form_thread, route.page, form_state, form_entry, form_raw, form_meta, false);
                                    } catch (err) {
                                        reject(err);
                                    }

                                    resolve();
                                });
                            } else {
                                d.action(T.forget, {}, resolve);
                            }
                        });
                    }
                } catch (err) {
                    if (err != null)
                        Log.error(err);

                    // If we're popping state, this will fuck up navigation history but we can't
                    // refuse popstate events. History mess is better than data loss.
                    options.push_history = false;
                    return;
                }
            }

            try {
                await openRecord(new_route.tid, new_route.anchor, new_route.page);
            } catch (err) {
                if (route.page != null)
                    throw err;

                Log.error(err);

                let page = new_route.page.chain[0];
                await openRecord(null, null, page);
            }
        } else {
            route = new_route;
        }

        // Show form automatically?
        if (explicit_panels != null) {
            UI.setPanels(explicit_panels);
        } else if (!UI.isPanelActive('view')) {
            let force = (url != null) && (form_thread.saved || !first_run);

            if (force) {
                UI.togglePanel('data', false);
                UI.togglePanel('view', true);
            }
        }
    });

    await run(options.push_history);
}

function contextualizeURL(url, thread) {
    if (thread != null && thread.saved) {
        url += `/${thread.tid}`;

        if (thread == form_thread && route.anchor != null)
            url += `@${route.anchor}`;
    }

    return url;
}

function mapEntries(entries) {
    let obj = {};

    for (let entry of entries) {
        let prev = obj[entry.store];

        entry.siblings = prev?.siblings ?? [];
        entry.siblings.push(entry);

        obj[entry.store] = entry;
    }

    return obj;
}

async function run(push_history = true) {
    await data_mutex.run(async () => {
        // Fetch and build page code for page panel
        if (route.page.filename != null) {
            let buffer = await fetchCode(route.page.filename);
            let build = code_builds.get(buffer.sha256);

            if (build == null) {
                try {
                    build = await buildScript(buffer.code, ['app', 'cache', 'form',
                                                            'meta', 'page', 'thread', 'values']);
                    code_builds.set(buffer.sha256, build);
                } catch (err) {
                    triggerError(route.page.filename, err);
                }
            }
        }

        // Sync editor (if needed)
        if (UI.isPanelActive('editor')) {
            updateEditorTabs();

            let err = error_map.get(editor_filename);

            if (err != null) {
                error_entry.error(err, -1);
            } else {
                error_entry.close();
            }

            await syncEditor();
        }

        // Load data rows (if needed)
        if (UI.isPanelActive('data')) {
            let root = route.page.chain[0];

            if (data_threads == null) {
                let threads = await Net.get(`${ENV.urls.instance}api/records/list`);

                data_threads = threads.map(thread => {
                    let min_ctime = Number.MAX_SAFE_INTEGER;
                    let max_mtime = 0;
                    let tags = new Set;

                    for (let entry of thread.entries) {
                        for (let tag of entry.tags)
                            tags.add(tag);

                        min_ctime = Math.min(min_ctime, entry.ctime);
                        max_mtime = Math.max(max_mtime, entry.mtime);

                        entry.ctime = new Date(entry.ctime);
                        entry.mtime = new Date(entry.mtime);
                    }

                    return {
                        tid: thread.tid,
                        sequence: thread.sequence,
                        locked: thread.locked,
                        ctime: new Date(min_ctime),
                        mtime: new Date(max_mtime),
                        entries: mapEntries(thread.entries),
                        tags: Array.from(tags)
                    };
                });
            }

            data_rows = data_threads;
            data_columns = [];

            if (data_tags != null)
                data_rows = data_rows.filter(thread => thread.tags.some(tag => data_tags.has(tag)));

            for (let page of root.children) {
                if (page.store == null)
                    continue;

                let col = {
                    page: page,
                    count: data_rows.reduce((acc, row) => acc + (row.entries[page.store.key] != null), 0)
                };

                data_columns.push(col);
            }
        }

        // Run page script
        try {
            let [model, meta] = await runPage(route.page, form_thread, form_state);

            form_model = model;
            form_meta = meta;
            form_valid = true;

            triggerError(route.page.filename, null);
        } catch (err) {
            if (form_model == null)
                form_model = new FormModel();
            if (form_meta == null)
                form_meta = new MetaModel();
            form_valid = false;

            if (err != null)
                triggerError(route.page.filename, err);
        }
    });

    // Update URL and title
    {
        let defaults = null;

        if (profile.develop) {
            defaults = 'editor|view';
        } else if (form_thread.saved || !app.panels.data) {
            defaults = 'view';
        } else {
            defaults = 'data';
        }

        let url = contextualizeURL(route.page.url, form_thread);
        let panels = UI.getPanels().join('|');

        if (panels == defaults)
            panels = null;

        url = Util.pasteURL(url, { p: panels });
        goupile.syncHistory(url, push_history);

        document.title = `${route.page.title} — ${ENV.title}`;
    }

    // Don't mess with the editor when render accidently triggers a scroll event!
    ignore_page_scroll = performance.now();

    await UI.draw();
}

async function fetchCode(filename) {
    // Anything in cache
    {
        let buffer = code_buffers.get(filename);

        if (buffer != null)
            return buffer;
    }

    // Try locally saved files
    if (profile.develop) {
        let db = await goupile.openLocalDB();

        let key = `${profile.userid}/${filename}`;
        let file = await db.load('changes', key);

        if (file != null) {
            if (file.blob != null) {
                let code = await file.blob.text();
                return updateBuffer(filename, code);
            } else {
                return updateBuffer(filename, '');
            }
        }
    }

    // The server is our last hope
    {
        let url = `${ENV.urls.files}${filename}`;
        let response = await Net.fetch(url);

        if (response.ok) {
            let code = await response.text();
            return updateBuffer(filename, code);
        } else if (response.status != 404) {
            let err = await Net.readError(response);
            throw new Error(err);
        }
    }

    // Got nothing
    return updateBuffer(filename, '');
}

function updateBuffer(filename, code, version = null) {
    let buffer = code_buffers.get(filename);

    let sha256 = mixer.Sha256(code);

    if (buffer == null) {
        buffer = {
            code: code,
            sha256: null,
            original_sha256: sha256,
            session: null,
            version: null
        };
        code_buffers.set(filename, buffer);
    } else if (buffer.session != null && sha256 !== buffer.sha256) {
        try {
            ignore_editor_change = true;
            buffer.session.doc.setValue(code);
        } finally {
            ignore_editor_change = false;
        }
    }

    buffer.code = code;
    buffer.sha256 = sha256;
    buffer.version = version || 0;

    return buffer;
}

function fileHasChanged(filename) {
    let buffer = code_buffers.get(filename);

    if (buffer != null) {
        let changed = (buffer.sha256 !== buffer.original_sha256);
        return changed;
    } else {
        return false;
    }
}

function triggerError(filename, err) {
    if (err != null) {
        if (!profile.develop)
            throw err;

        if (filename == editor_filename)
            error_entry.error(err, -1);
        error_map.set(filename, err);
    } else {
        if (filename == editor_filename)
            error_entry.close();
        error_map.delete(filename);
    }
}

function getFillingStatuses(intf) {
    let statuses = [];

    if (!goupile.isLocked()) {
        statuses.push(['wait', T.status_wait]);
        statuses.push(['check', T.status_check]);
    }
    if (intf.missing) {
        statuses.push(
            ['nsp', T.status_nsp],
            ['na', T.status_na],
            ['nd', T.status_nd]
        );
    }

    return statuses;
}

async function buildScript(code, variables) {
    // JS is always classy
    let AsyncFunction = Object.getPrototypeOf(async function(){}).constructor;

    let base = {
        goupile: goupile,
        profile: profile,

        render: render,
        html: html,
        svg: svg,

        Util: Util,
        Net: Net,
        Log: Log,
        UI: UI,
        LocalDate: LocalDate,
        LocalTime: LocalTime,
        crypto: mixer,

        dates: LocalDate, // Deprecated
        times: LocalTime // Deprecated
    };

    variables = [...Object.keys(base), ...variables];

    let build = null;

    if (bundler != null) {
        try {
            build = await bundler.build(code, async filename => {
                let file = await fetchCode(filename);
                return file.code;
            });
        } catch (err) {
            throwParseError(err);
        }
    } else {
        build = { code: code, map: null };
    }

    try {
        let func = new AsyncFunction(variables, build.code);

        build.func = async api => {
            api = Object.assign({}, base, api);

            try {
                let values = variables.map(key => api[key]);
                await func(...values);
            } catch (err) {
                throwRunError(err, build.map);
            }
        };
    } catch (err) {
        throwRunError(err, build.map);
    }

    return build;
}

async function runPage(page, thread, state) {
    let options = {
        annotate: app.annotate && (page.store != null)
    };

    let model = new FormModel;
    let builder = new FormBuilder(state, model, options);
    let meta = new MetaModel;

    let build = null;
    let func = null;

    if (page.filename != null) {
        let buffer = code_buffers.get(page.filename);

        build = code_builds.get(buffer.sha256);
        func = build?.func;
    } else {
        func = defaultFormPage;
    }

    if (func == null)
        throw null;

    await func({
        app: app,
        cache: form_cache,
        form: builder,
        page: page,
        meta: new MetaInterface(app, page, thread, meta),
        thread: thread,
        values: state.values
    });

    addAutomaticActions(builder, model);
    addAutomaticTags(model.variables);

    return [model, meta];
}

function throwParseError(err) {
    let line = err.errors?.[0]?.location?.line;
    let msg = `${T.script_error}\n${line != null ? `${T.row} ${line}${T._colon}` : ''}${err.errors[0].text}`;

    throw new Error(msg);
}

function throwRunError(err, map) {
    let location = Util.locateEvalError(err);

    let line = location?.line;
    if (map != null && line != null)
        line = bundler.mapLine(map, location.line, location.column);
    let msg = `${T.script_error}\n${line != null ? `${T.row} ${line}${T._colon}` : ''}${err.message}`;

    throw new Error(msg);
}

// Call with data_mutex locked
async function openRecord(tid, anchor, page) {
    let new_thread = null;
    let new_entry = null;
    let new_raw = null;
    let new_state = null;

    // Load or create thread
    if (tid != null) {
        new_thread = await records.load(tid, anchor);
    } else {
        new_thread = {
            tid: Util.makeULID(),
            counters: {},
            saved: false,
            locked: false,
            entries: [],
            data: null
        };
    }

    // Initialize entry data
    if (page.store != null) {
        new_entry = new_thread.entries.findLast(entry => entry.store == page.store.key);

        if (new_entry == null) {
            let now = (new Date).valueOf();

            new_entry = {
                store: page.store.key,
                eid: Util.makeULID(),
                deleted: false,
                anchor: -1,
                ctime: now,
                mtime: now,
                sequence: null,
                tags: [],
                data: {}
            };

            new_thread.entries.push(new_entry);
        }

        for (let entry of new_thread.entries) {
            let [raw, obj] = Data.wrap(entry.data);

            if (entry == new_entry) {
                new_raw = raw;
                new_state = new FormState(raw, obj);
            }

            entry.data = obj;
        }

        new_thread.entries = mapEntries(new_thread.entries);
        new_thread.data = {};

        for (let store of app.stores) {
            let data = new_thread.entries[store.key]?.data;

            if (data == null) {
                let [raw, obj] = Data.wrap({});
                data = obj;
            }

            new_thread.data[store.key] = data;
        }
    } else {
        new_raw = null;
        new_state = new FormState;
    }

    if (!profile.develop && !isPageEnabled(page, new_thread))
        page = page.chain[0];

    // Copy UI state if needed
    if (form_state != null && page == route.page) {
        new_state.state_tabs = form_state.state_tabs;
        new_state.state_sections = form_state.state_sections;
    }

    // Scroll to top if needed
    if (page != route.page) {
        let panel_el = document.querySelector('#ins_page')?.parentNode;
        panel_el?.scrollTo?.(0, 0);
    }

    // Restore delayed errors for existing entry
    if (new_entry.anchor >= 0)
        new_state.trigger_errors = true;

    new_state.changeHandler = async () => {
        await run();

        // Highlight might need to change (conditions, etc.)
        if (UI.isPanelActive('editor'))
            syncFormHighlight(false);

        if (page.autosave) {
            let delay = (typeof page.autosave == 'number') ? page.autosave : 5000;

            if (autosave_timer != null)
                clearTimeout(autosave_timer);

            autosave_timer = setTimeout(() => {
                autosave_timer = null;

                data_mutex.run(async () => {
                    await saveRecord(form_thread, route.page, form_state, form_entry, form_raw, form_meta, true);
                    await openRecord(form_thread.tid, null, page);

                    await data_mutex.chain(run);
                });
            }, delay);
        }
    };
    new_state.annotateHandler = runAnnotationDialog;

    if (autosave_timer != null)
        clearTimeout(autosave_timer);
    autosave_timer = null;

    form_thread = new_thread;
    form_entry = new_entry;
    form_raw = new_raw;
    form_state = new_state;
    form_cache = {};

    form_model = null;
    form_meta = null;

    route.tid = tid;
    route.anchor = anchor;
    route.page = page;
}

function runAnnotationDialog(e, intf) {
    return UI.dialog(e, intf.label, {}, (d, resolve, reject) => {
        let notes = Data.annotate(intf.key.ptr, intf.key.name);
        let status = notes.status;

        if (status == null) {
            status = {};
            notes.status = status;
        }

        let locked = d.values.locked ?? status.locked;
        let statuses = getFillingStatuses(intf);

        if (statuses.length >= 2)
            d.enumRadio('filling', null, statuses, { value: status.filling, disabled: locked });
        d.textArea('comment', T.annotate_comment, { rows: 4, value: status.comment, disabled: locked });

        if (goupile.hasPermission('data_audit')) {
            d.boolean('locked', T.annotate_lock, { value: status.locked ?? false, untoggle: false });
        } else if (locked) {
            d.calc('locked', T.annotate_lock, 0 + locked, { text: locked => locked ? T.yes : T.no });
        }

        d.action(T.confirm, { disabled: !d.isValid() }, async () => {
            status.filling = d.values.filling;
            status.comment = d.values.comment;

            if (goupile.hasPermission('data_audit'))
                status.locked = !!d.values.locked;

            form_state.markChange();

            resolve();
        });
    });
}

// Call with data_mutex locked
async function saveRecord(thread, page, state, entry, raw, meta, draft) {
    for (;;) {
        let reservation = await records.reserve(meta.counters);

        thread = Object.assign({}, thread);
        thread.counters = Object.assign({}, thread.counters);
        thread.sequence ??= reservation.sequence;
        for (let key in reservation.counters)
            thread.counters[key] ??= reservation.counters[key];
        [_, meta] = await runPage(page, thread, state);

        let frag = {
            summary: meta.summary,
            data: raw,
            tags: null,
            constraints: meta.constraints,
            counters: meta.counters,
            publics: []
        };

        // Gather global list of tags for this record entry
        {
            let tags = new Set;
            for (let intf of form_model.variables) {
                if (Array.isArray(intf.options.tags)) {
                    for (let tag of intf.options.tags)
                        tags.add(tag.key);
                }
            }
            if (draft)
                tags.add('draft');
            frag.tags = Array.from(tags);
        }

        // Gather list of public variables
        for (let key in raw) {
            if (raw[key].$n?.variable?.public)
                frag.publics.push(key);
        }

        let actions = {
            reservation: reservation.reservation,

            signup: null,
            claim: true,
            lock: false
        };

        if (!draft) {
            actions.signup = meta.signup;
            actions.claim = route.page.claim;
            actions.lock = route.page.lock;
        }

        try {
            await records.save(thread.tid, entry, frag, ENV.version, actions);
            break;
        } catch (err) {
            if (!(err instanceof HttpError))
                throw err;
            if (err.status != 412)
                throw err;
        }
    }

    if (!profile.userid)
        await goupile.syncProfile();
}

export {
    init,
    hasUnsavedData,
    computeStatus,
    contextualizeURL,
    runTasks,
    go
}
