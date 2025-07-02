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

import { render, html, svg } from '../../../vendor/lit-html/lit-html.bundle.js';
import { Util, Log, Net, LruMap, Mutex, LocalDate, LocalTime } from '../../web/core/base.js';
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
let form_model = null;
let form_builder = null;

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

        await initBundler();
    }

    await initApp();
    initUI();

    if (ENV.demo)
        Log.warning('Mode de démonstration... Attention, les formulaires et les données peuvent disparaître à tout moment !', 6000);
}

async function initBundler() {
    bundler = await import(`${ENV.urls.static}bundler.js`);
    await bundler.init();
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
        builder.form('default', 'Défaut', 'Page par défaut');

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
            throw new Error('Main script does not define any page');

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
    if (!route.page.warn_unsaved)
        return false;

    return form_state.hasChanged();
}

async function runTasks(sync) {
    if (profile.develop && sync)
        await uploadFsChanges();
}

function renderMenu() {
    let show_menu = true;
    let menu_is_wide = isMenuWide(route.page);

    if (!UI.isPanelActive('editor') && !UI.isPanelActive('view'))
        show_menu = false;

    return html`
        <nav class=${goupile.isLocked() ? 'ui_toolbar locked' : 'ui_toolbar'} id="ui_top" style="z-index: 999999;">
            ${goupile.hasPermission('build_code') ? html`
                <div class="drop">
                    <button class=${'icon design' + (profile.develop ? ' active' : '')}
                            @click=${UI.deployMenu}>Conception</button>
                    <div>
                        <button class=${profile.develop ? 'active' : ''}
                                @click=${UI.wrap(e => goupile.changeDevelopMode(!profile.develop))}>
                            <div style="flex: 1;">Mode conception</div>
                            ${profile.develop ? html`<div>&nbsp;✓\uFE0E</div>` : ''}
                        </button>
                    </div>
                </div>
            ` : ''}
            ${goupile.canUnlock() ? html`
                <button class="icon lock"
                        @click=${UI.wrap(goupile.runUnlockDialog)}>Déverrouiller</button>
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
                                    @click=${UI.wrap(e => togglePanels(UI.hasTwoPanels() && !UI.isPanelActive('data'), 'data'))}>Données</button>
                            <button class=${'icon view' + (UI.isPanelActive('view') ? ' active' : '')}
                                    @click=${UI.wrap(e => togglePanels(UI.hasTwoPanels() && !UI.isPanelActive('view'), 'view'))}>Formulaire</button>
                        </div>
                    </div>
                ` : ''}
                ${!app.panels.editor || !app.panels.data ? html`
                    <button class=${'icon view' + (!UI.hasTwoPanels() && UI.isPanelActive('view') ? ' active' : '')}
                            @click=${UI.wrap(e => togglePanels(false, 'view'))}></button>
                ` : ''}
                <div style="flex: 1; min-width: 4px;"></div>
            ` : ''}

            ${show_menu && !menu_is_wide ? html`
                ${renderDropItem(route.page.chain[0], true)}
                ${Util.map(route.page.chain[0].children, page => renderDropItem(page, false))}
            ` : ''}
            ${show_menu && menu_is_wide ? route.page.chain.map((page, idx) => renderDropItem(page, !idx)) : ''}
            ${app.panels.data && (!UI.isPanelActive('view') || form_thread.saved) ? html`
                <div style="width: 15px;"></div>
                <button class="icon new" @click=${UI.wrap(e => go(e, route.page.chain[0].url))}>Ajouter</button>
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
                                return html`<button class="active" @click=${UI.wrap(e => togglePanels(true, profile.develop ? 'view' : false))}>${ENV.title}</button>`;
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
                        <button @click=${UI.wrap(goupile.runChangePasswordDialog)}>Modifier mon mot de passe</button>
                        <button @click=${UI.wrap(goupile.runResetTOTP)}>Configurer la double authentification</button>
                        <hr/>
                        ${goupile.hasPermission('data_export') && goupile.hasPermission('data_fetch') ? html`
                            <button @click=${UI.wrap(generateExportKey)}>Générer une clé d'export</button>
                            <hr/>
                        ` : ''}
                    ` : ''}
                    ${profile.root || goupile.hasPermission('build_admin') ? html`
                        <button @click=${e => window.open('/admin/')}>Administration</button>
                        <hr/>
                    ` : ''}
                    <button @click=${UI.wrap(goupile.logout)}>${profile.userid ? 'Se déconnecter' : 'Se connecter'}</button>
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
        let suffix = !goupile.isLocked() ? makeStatusText(status, page.progress) : '';

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

    await UI.dialog(e, 'Clé d\'export', {}, (d, resolve, reject) => {
        d.text('export_key', 'Clé d\'export', {
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
                <button @click=${UI.wrap(e => runHistoryDialog(e, editor_filename))}>Historique</button>
                <div style="flex: 1;"></div>
                ${editor_filename === 'main.js' ? html`
                    <button ?disabled=${!main_works || !fileHasChanged('main.js')}
                            @click=${e => { window.location.href = window.location.href; }}>Appliquer</button>
                ` : ''}
                <button ?disabled=${!main_works}
                        @click=${UI.wrap(runPublishDialog)}>Publier</button>
            </div>

            ${editor_el}
        </div>
    `;
}

function updateEditorTabs() {
    editor_tabs = [];

    editor_tabs.push({
        key: 'project',
        title: 'Projet',
        filename: 'main.js',
        active: false
    });
    if (route.page.filename != null) {
        editor_tabs.push({
            key: 'form',
            title: 'Formulaire',
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

    let p = UI.dialog(e, 'Historique du fichier', {}, (d, resolve, reject) => {
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
                        <td><a @click=${UI.wrap(e => loadFile(filename, 0))}>Charger</a></td>
                    </tr>

                    ${Util.mapRange(0, versions.length - 1, idx => {
                        let version = versions[versions.length - idx - 1];

                        return html`
                            <tr class=${buffer.version == version.version ? 'active' : ''}>
                                <td class="ui_sub">${version.version}</td>
                                <td>${(new Date(version.mtime)).toLocaleString()}</td>
                                <td><a @click=${UI.wrap(e => loadFile(filename, version.version))}>Charger</a></td>
                            </tr>
                        `;
                    })}
                </tbody>
            </table>
        `);

        d.action('Restaurer', { disabled: !d.isValid() || buffer.version == 0 }, async () => {
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
    let url = `${ENV.urls.base}files/${version}/${filename}`;
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
                        <label for="ins_tags">Filtrer :</label>
                    </div>
                    ${app.tags.map(tag => {
                        if (!tag.filter)
                            return '';

                        let id = 'ins_tag_' + tag.key;

                        return html`
                            <div  class=${data_tags == null ? 'fm_check disabled' : 'fm_check'} style="padding-top: 0;">
                                <input id=${id} type="checkbox"
                                       .checked=${data_tags == null || data_tags.has(tag.key)}
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
                    <col style="width: 60px;" />
                    <col style="width: 8em;"/>
                    ${Util.mapRange(0, data_columns.length, () => html`<col/>`)}
                    <col style="width: 2em;"/>
                </colgroup>

                <thead>
                    <tr>
                        <th>ID</th>
                        <th>Création</th>
                        ${data_columns.map(col => {
                            let stats = `${col.count} / ${data_rows.length}`;
                            let title = `${col.page.title}\nDisponible : ${stats} ${data_rows.length > 1 ? 'lignes' : 'ligne'}`;

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
                                    let status = computeStatus(col.page, row);
                                    let summary = row.entries[col.page.store.key]?.summary;
                                    let url = col.page.url + `/${row.tid}`;
                                    let highlight = active && route.page.chain.includes(col.page);

                                    if (status.complete) {
                                        return html`<td class=${highlight ? 'complete active' : 'complete'}
                                                        title=${col.page.title}><a href=${url}>${summary ?? '✓\uFE0E Complet'}</a></td>`;
                                    } else if (status.filled) {
                                        let progress = Math.floor(100 * status.filled / status.total);

                                        return html`<td class=${highlight ? 'partial active' : 'partial'}
                                                        title=${col.page.title}><a href=${url}>${summary ?? progress + '%'}</a></td>`;
                                    } else {
                                        return html`<td class=${highlight ? 'missing active' : 'missing'}
                                                        title=${col.page.title}><a href=${url}>Afficher</a></td>`;
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
                            <td class="missing" colspan=${data_columns.length}><a @click=${e => togglePanels(null, 'view')}>Nouvel enregistrement</a></td>
                        </tr>
                    ` : ''}
                    ${!data_rows.length && !recording_new ? html`<tr><td colspan=${2 + data_columns.length}>Aucune ligne à afficher</td></tr>` : ''}
                </tbody>
            </table>

            <div class="ui_actions">
                ${goupile.hasPermission('data_export') ? html`<button @click=${UI.wrap(runExportCreateDialog)}>Créer un export</button>` : ''}
                ${goupile.hasPermission('data_fetch') ? html`<button @click=${UI.wrap(runExportListDialog)}>Liste des exports récents</button>` : ''}
            </div>
        </div>
    `;
}

function runDeleteRecordDialog(e, row) {
    return UI.confirm(e, `Voulez-vous vraiment supprimer l'enregistrement '${row.sequence}' ?`,
                         'Supprimer', async () => {
        await data_mutex.run(async () => {
            await records.delete(row.tid);

            if (route.tid == row.tid)
                await openRecord(null, null, route.page);

            data_threads = null;
        });

        go();
    });
}

async function runExportCreateDialog(e) {
    let downloads = await Net.get(`${ENV.urls.instance}api/export/list`);
    let stores = app.stores.map(store => store.key);

    if (!downloads.length) {
        await create(null, null);
        return;
    }

    downloads.reverse();

    await UI.dialog(e, 'Créer un nouvel export', {}, (d, resolve, reject) => {
        if (downloads.length > 0) {
            d.enumRadio('mode', "Mode d'export", [
                ['sequence', 'Nouveaux enregistrements'],
                ['anchor', 'Nouveaux enregistrements et enregistrements modifiés'],
                ['all', 'Tous les enregistements']
            ], { value: 'sequence', untoggle: false });

            if (d.values.mode != 'all') {
                let props = downloads.map(download => [download.export, (new Date(download.ctime)).toLocaleString()]);
                d.enumDrop('since', 'Depuis cet export :', props, { value: downloads[0]?.export, untoggle: false });
            }
        }

        d.action('Créer l\'export', {}, async () => {
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
    });

    async function create(sequence, anchor) {
        let progress = Log.progress('Sauvegarde des modifications');

        try {
            let export_id = await createExport(sequence, anchor);

            if (goupile.hasPermission('data_fetch'))
                await exportRecords(export_id, stores);

            progress.success('Export complété');
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

    await UI.dialog(e, 'Exports de données', {}, (d, resolve, reject) => {
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
                        <th>Date</th>
                        <th>Utilisateur</th>
                        <th>Enregistrements</th>
                    </tr>
                </thead>

                <tbody>
                    ${downloads.map(download => html`
                        <tr>
                            <td>${(new Date(download.ctime)).toLocaleString()}</td>
                            <td>${download.username}</td>
                            <td>${download.threads}</td>
                            <td><a @click=${UI.wrap(e => exportRecords(download.export, stores))}>Télécharger</a></td>
                        </tr>
                    `)}
                    ${!downloads.length ? html`<tr><td colspan="4">Aucun export réalisé</td></tr>` : ''}
                </tbody>
            </table>
        `);
    });
}

function toggleTagFilter(tag) {
    if (tag == null) {
        if (data_tags == null) {
            data_tags = new Set(app.tags.filter(tag => tag.filter).map(tag => tag.key));
        } else {
            data_tags = null;
        }
    } else if (data_tags != null) {
        if (!data_tags.delete(tag))
            data_tags.add(tag);
    }

     return go();
}

async function renderPage() {
    let options = {
        annotate: app.annotate
    };

    let model = new FormModel;
    let builder = new FormBuilder(form_state, model, options);
    let meta = new MetaModel;

    try {
        let build = null;
        let func = null;

        if (route.page.filename != null) {
            let buffer = code_buffers.get(route.page.filename);

            build = code_builds.get(buffer.sha256);
            func = build?.func;
        } else {
            func = defaultFormPage;
        }

        if (func == null)
            throw null;

        await func({
            app: app,
            form: builder,
            page: route.page,
            meta: new MetaInterface(app, route.page, form_thread, meta),
            thread: form_thread,
            values: form_state.values
        });

        addAutomaticActions(builder, model);
        addAutomaticTags(model.variables);

        render(model.renderWidgets(), page_div);
        page_div.classList.remove('disabled');

        // Map widget code lines for developers
        if (build?.map != null) {
            for (let intf of model.widgets) {
                if (intf.id == null)
                    continue;

                let el = page_div.querySelector('#' + intf.id);
                let wrap = (el != null) ? Util.findParent(el, el => el.classList.contains('fm_wrap')) : null;

                if (wrap != null) {
                    let line = bundler.mapLine(build.map, intf.location.line, intf.location.column);
                    wrap.dataset.line = line;
                }
            }
        }

        if (form_state.justTriggered()) {
            let panel_el = document.querySelector('#ins_page')?.parentNode;
            panel_el?.scrollTo?.(0, panel_el.scrollHeight);
        }

        form_model = model;
        form_builder = builder;
        form_meta = meta;

        triggerError(route.page.filename, null);
    } catch (err) {
        if (!page_div.children.length)
            render('Impossible de générer la page à cause d\'une erreur', page_div);
        page_div.classList.add('disabled');

        if (err != null)
            triggerError(route.page.filename, err);
    }

    let show_menu = (route.page.chain.length > 2 ||
                     route.page.chain[0].children.length > 1);
    let menu_is_wide = isMenuWide(route.page);

    // Quick access to page sections
    let page_sections = model.widgets.filter(intf => intf.options.anchor).map(intf => ({
        title: intf.label,
        anchor: intf.options.anchor
    }));

    return html`
        <div class="print" @scroll=${syncEditorScroll}}>
            <div id="ins_page">
                <div id="ins_menu">
                    ${show_menu ? Util.mapRange(1 - menu_is_wide, route.page.chain.length,
                                                idx => renderPageMenu(route.page.chain[idx])) : ''}
                </div>

                <form id="ins_content" autocomplete="off" @submit=${e => e.preventDefault()}>
                    ${page_div}
                </form>

                <div id="ins_actions">
                    ${model.renderActions()}

                    ${page_sections.length > 1 ? html`
                        <h1>${route.page.title}</h1>
                        <ul>${page_sections.map(section => html`<li><a href=${'#' + section.anchor}>${section.title}</a></li>`)}</ul>
                    ` : ''}

                    <div style="flex: 1;"></div>
                    ${renderShortcuts()}
                </div>
            </div>
            <div style="flex: 1;"></div>

            ${profile.develop && !isPageEnabled(route.page, form_thread) ? html`
                <div id="ins_develop">
                    Cette page n'est théoriquement pas activée<br/>
                    Le mode de conception vous permet d'y accéder.
                </div>
            ` : ''}

            ${model.actions.length || app.shortcuts.length ? html`
                <nav class="ui_toolbar" id="ins_tasks" style="z-index: 999999;">
                    ${renderShortcuts()}

                    <div style="flex: 1;"></div>
                    ${model.actions.some(action => !action.options.always) ? html`
                        <div class="drop up right">
                            <button @click=${UI.deployMenu}>Autres actions</button>
                            <div>
                                ${model.actions.map(action => action.render())}
                            </div>
                        </div>
                        <hr/>
                    ` : ''}
                    ${Util.mapRange(0, model.actions.length, idx => {
                        let action = model.actions[model.actions.length - idx - 1];

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
                    text = 'Non disponible';
                } else if (status.complete) {
                    cls += ' complete';
                    text = 'Complet';
                } else if (status.filled && child.progress) {
                    let progress = Math.floor(100 * status.filled / status.total);

                    text = html`
                        <div class="ins_progress" style=${'--progress: ' + progress}>
                            <div></div>
                            <span>${status.filled} ${status.filled > 1 ? 'items' : 'item'} sur ${status.total}</span>
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
                    text = 'Non disponible';
                } else if (status.complete) {
                    cls += ' complete';
                    text = 'Complet';
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

function renderShortcuts() {
    return app.shortcuts.map(shortcut => {
        let cls = '';
        let style = '';

        if (shortcut.icon) {
            cls += ' icon';
            style += 'background-image: url(' + shortcut.icon + ');';
        }
        if (shortcut.color) {
            cls += ' color';
            style += `--color: ${shortcut.color};`;
        }

        return html`<button type="button" class=${cls} style=${style}
                            @click=${shortcut.click}/>${shortcut.label}</button>`;
    });
}

function addAutomaticActions(builder, model) {
    if (builder.hasErrors())
        builder.errorList();

    if (route.page.store != null) {
        let is_new = (form_entry.anchor < 0);

        let can_continue = route.page.sequence && model.variables.length;
        let can_save = !form_thread.locked &&
                       (form_state.hasChanged() || can_continue);
        let can_lock = form_thread.saved && route.page.has_lock &&
                       (!form_thread.locked || goupile.hasPermission('data_audit'));

        if (can_save) {
            let label = route.page.sequence ? '+Continuer' : '+Enregistrer';

            builder.action(label, { color: '#2d8261' }, async e => {
                form_builder.triggerErrors();

                await data_mutex.run(async () => {
                    let keep_open = goupile.hasPermission('data_read') || route.page.claim;
                    let confirm = route.page.confirm ?? !keep_open;

                    if (confirm) {
                        let text = keep_open ? html`Confirmez-vous l'enregistrement ?`
                                             : html`Confirmez-vous la <b>finalisation de cet enregistrement</b> ?`;
                        await UI.confirm(e, text, 'Continuer', () => {});
                    }

                    await saveRecord(form_thread.tid, form_entry, form_raw, form_meta, false);

                    if (keep_open) {
                        await openRecord(form_thread.tid, null, route.page);
                    } else {
                        let page = route.page.chain[0];
                        await openRecord(null, null, page);
                    }

                    data_threads = null;
                });

                // Redirect?
                {
                    let redirect = route.page.sequence;
                    let url = null;

                    if (redirect === true) {
                        for (let i = route.page.chain.length - 2; i >= 0; i--) {
                            let page = route.page.chain[i];
                            let status = computeStatus(page, form_thread);

                            if (page.url != route.page.url) {
                                url = contextualizeURL(page.url, form_thread);

                                if (!status.complete)
                                    break;
                            }
                        }

                        if (is_new && route.page.chain.length > 1) {
                            let parent = route.page.chain[route.page.chain.length - 2];
                            let idx = parent.children.indexOf(route.page);

                            if (idx >= 0 && idx < parent.children.length - 1) {
                                let next = parent.children[idx + 1];

                                while (next.children.length)
                                    next = next.children[0];

                                url = contextualizeURL(next.url, form_thread);
                            }
                        }
                    } else if (typeof redirect == 'string') {
                        let page = app.pages.find(page => page.key == redirect);

                        if (page != null)
                            url = contextualizeURL(page.url, form_thread);
                    }

                    go(null, url);
                }
            });
        }

        if (can_lock) {
            let label = !form_thread.locked ? '+Verrouiller' : 'Déverrouiller';
            let color = !form_thread.locked ? null : '#ff6600';

            builder.action(label, { color: color }, async e => {
                if (form_state.hasChanged())
                    await UI.confirm(e, html`Cette action <b>annulera les modifications en cours</b>, souhaitez-vous continuer ?`, label, () => {});

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

        if (!is_new && form_state.hasChanged()) {
            builder.action('-');
            builder.action('Oublier les modifications', {}, async e => {
                await UI.confirm(e, html`Souhaitez-vous réellement <b>annuler les modifications en cours</b> ?`,
                                       'Oublier', () => {});

                await data_mutex.run(async () => {
                    await openRecord(route.tid, route.anchor, route.page);
                });

                go();
            });
        }
    }
}

function addAutomaticTags(variables) {
    for (let intf of variables) {
        let tags = [];

        let notes = Data.annotate(intf.key.ptr, intf.key.name);
        let status = notes.status ?? {};

        if (form_thread.locked)
            intf.options.readonly = true;

        if (status.locked) {
            tags.push('locked');
            intf.options.readonly = true;
        } else if (status.filling == 'check') {
            tags.push('check');
            intf.errors.length = 0;
        } else if (status.filling == 'wait') {
            tags.push('wait');
            intf.errors.length = 0;
        } else if (status.filling != null) {
            if (intf.missing) {
                tags.push('na');
                intf.errors.length = 0;
            } else {
                status.filling = null;
            }
        } else if (intf.missing && intf.options.mandatory) {
            if (form_entry.anchor >= 0 || intf.errors.some(err => !err.delay))
                tags.push('incomplete');
        } else if (intf.errors.some(err => !err.delay)) {
            tags.push('error');
        }

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
        let build = await buildScript(buffer.code, ['app', 'form', 'meta', 'page', 'thread', 'values']);
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

    let publisher = new InstancePublisher;
    await publisher.runDialog(e);

    run();
}

async function go(e, url = null, options = {}) {
    options = Object.assign({ push_history: true }, options);

    await data_mutex.run(async () => {
        let new_route = Object.assign({}, route);
        let explicit_panels = false;

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
                Log.error(`La page '${key}' n'existe pas`);
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
                    Log.error('L\'indicateur de version n\'est pas un nombre');
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

                if (panels.length) {
                    UI.setPanels(panels);
                    explicit_panels = true;
                }
            }
        }

        let context_change = (new_route.tid != route.tid ||
                              new_route.anchor != route.anchor ||
                              new_route.page != route.page);

        // Warn about data loss before loading new data
        if (context_change) {
            if (hasUnsavedData(false)) {
                try {
                    if (route.page.autosave) {
                        clearTimeout(autosave_timer);
                        autosave_timer = null;

                        await saveRecord(form_thread.tid, form_entry, form_raw, form_meta, true);
                    } else {
                        await UI.dialog(e, 'Enregistrer (confirmation)', {}, (d, resolve, reject) => {
                            d.output(html`Si vous continuez, vos <b>modifications seront enregistrées</b>.`);

                            d.enumRadio('save', 'Que souhaitez-vous faire avant de continuer ?', [
                                [true, "Enregistrer mes modifications"],
                                [false, "Oublier mes modifications"]
                            ], { value: true, untoggle: false });

                            if (d.values.save) {
                                d.action('Enregistrer', {}, async e => {
                                    try {
                                        form_builder.triggerErrors();
                                        await saveRecord(form_thread.tid, form_entry, form_raw, form_meta, false);
                                    } catch (err) {
                                        reject(err);
                                    }

                                    resolve();
                                });
                            } else {
                                d.action('Oublier', {}, resolve);
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
        if (!UI.isPanelActive('view') && url != null && !explicit_panels) {
            UI.togglePanel('data', false);
            UI.togglePanel('view', true);
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

async function run(push_history = true) {
    await data_mutex.run(async () => {
        // Fetch and build page code for page panel
        if (route.page.filename != null) {
            let buffer = await fetchCode(route.page.filename);
            let build = code_builds.get(buffer.sha256);

            if (build == null) {
                try {
                    build = await buildScript(buffer.code, ['app', 'form', 'meta', 'page', 'thread', 'values']);
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

                    for (let store in thread.entries) {
                        let entry = thread.entries[store];

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
                        entries: thread.entries,
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
    });

    // Update URL and title
    {
        let url = contextualizeURL(route.page.url, form_thread);
        let panels = UI.getPanels().join('|');

        {
            let defaults = null;

            if (profile.develop) {
                defaults = 'editor|view';
            } else if (form_thread.saved) {
                defaults = 'view';
            } else {
                defaults = 'data';
            }

            if (panels == defaults)
                panels = null;
        }

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
        statuses.push(['wait', 'En attente']);
        statuses.push(['check', 'À vérifier']);
    }
    if (intf.missing) {
        statuses.push(
            ['nsp', goupile.isLocked() ? 'Je ne souhaite pas répondre' : 'Ne souhaite pas répondre'],
            ['na', goupile.isLocked() ? 'Ce n\'est pas applicable' : 'Non applicable'],
            ['nd', goupile.isLocked() ? 'Je n\'ai pas cette information' : 'Non disponible']
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

function throwParseError(err) {
    let line = err.errors?.[0]?.location?.line;
    let msg = `Erreur de script\n${line != null ? `Ligne ${line} : ` : ''}${err.errors[0].text}`;

    throw new Error(msg);
}

function throwRunError(err, map) {
    let location = Util.locateEvalError(err);

    let line = location?.line;
    if (map != null && line != null)
        line = bundler.mapLine(map, location.line, location.column);
    let msg = `Erreur de script\n${line != null ? `Ligne ${line} : ` : ''}${err.message}`;

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
            entries: {}
        };
    }

    // Initialize entry data
    if (page.store != null) {
        new_entry = new_thread.entries[page.store.key];

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

            new_thread.entries[page.store.key] = new_entry;
        }

        for (let store in new_thread.entries) {
            let entry = new_thread.entries[store];
            let [raw, obj] = Data.wrap(entry.data);

            if (entry == new_entry) {
                new_raw = raw;
                new_state = new FormState(raw, obj);
            }

            entry.data = obj;
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

        /* XXX if (new_record.saved && new_record.ulid == form_record.ulid)
            new_state.take_delayed = form_state.take_delayed; */
    }

    // Scroll to top if needed
    if (page != route.page) {
        let panel_el = document.querySelector('#ins_page')?.parentNode;
        panel_el?.scrollTo?.(0, 0);
    }

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
                    await saveRecord(form_thread.tid, form_entry, form_raw, form_meta, true);
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

    form_model = null;
    form_builder = null;
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

        let locked = d.values.hasOwnProperty('locked') ? d.values.locked : status.locked;
        let statuses = getFillingStatuses(intf);

        if (statuses.length >= 2)
            d.enumRadio('filling', null, statuses, { value: status.filling, disabled: locked });
        d.textArea('comment', 'Commentaire libre', { rows: 4, value: status.comment, disabled: locked });

        if (goupile.hasPermission('data_audit'))
            d.binary('locked', 'Validation finale', { value: status.locked });

        d.action('Confirmer', { disabled: !d.isValid() }, async () => {
            status.filling = d.values.filling;
            status.comment = d.values.comment;
            status.locked = d.values.locked;

            form_state.markChange();

            resolve();
        });
    });
}

// Call with data_mutex locked
async function saveRecord(tid, entry, raw, meta, draft) {
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

    await records.save(tid, entry, frag, ENV.version, meta.signup, route.page.claim);

    if (!profile.userid) {
        let url = route.page.url + `/${form_thread.tid}`;

        window.onbeforeunload = null;
        window.location.href = url;
    }
}

export {
    init,
    hasUnsavedData,
    computeStatus,
    contextualizeURL,
    runTasks,
    go
}
