// Copyright (C) 2024  Niels Martignène <niels.martignene@protonmail.com>
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

import { render, html, svg, until } from '../../../vendor/lit-html/lit-html.bundle.js';
import { Util, Log, Net, LruMap, Mutex, LocalDate, LocalTime } from '../../web/core/common.js';
import * as mixer from '../../web/core/mixer.js';
import * as IDB from '../../web/core/indexedDB.js';
import { ApplicationInfo, FormInfo, PageInfo, ApplicationBuilder } from './instance_app.js';
import * as goupile from '../client/goupile.js';
import { profile } from '../client/goupile.js';
import * as UI from '../client/ui.js';
import { InstancePublisher } from '../client/instance_publish.js';
import { FormState, FormModel, FormBuilder } from './form.js';
import { PeriodPicker } from '../../web/widgets/periodpicker.js';
import * as nacl from '../../../vendor/tweetnacl-js/nacl-fast.js';

let db = null;
let app = null;

// Explicit mutex to serialize (global) state-changing functions
let mutex = new Mutex;

let route = {
    form: null,
    page: null,
    ulid: null,
    version: null
};
let nav_sequence = null;

let main_works = true;
let head_length = Number.MAX_SAFE_INTEGER;
let page_div = document.createElement('div');

let form_record = null;
let form_state = null;
let form_builder = null;
let form_values = null;
let form_dictionaries = {};
let new_hid = null;

let editor_el = null;
let editor_ace = null;
let editor_filename = null;
let code_buffers = new LruMap(32);
let code_timer = null;

let error_entries = {};

let ignore_editor_change = false;
let ignore_editor_scroll = 0;
let ignore_page_scroll = 0;
let autosave_timer = null;

let data_form = null;
let data_rows = null;
let data_filter = null;
let data_date = null;

let prev_anchor = null;

async function init() {
    if (profile.develop) {
        ENV.urls.files = `${ENV.urls.base}files/0/`;
        ENV.version = 0;
    }

    await openInstanceDB();
    await initApp();
    initUI();

    if (profile.develop)
        code_timer = setTimeout(uploadFsChanges, 1000);
}

async function openInstanceDB() {
    let db_name = `goupile:${ENV.urls.instance}`;

    db = await IDB.open(db_name, 11, (db, old_version) => {
        switch (old_version) {
            case null: {
                db.createStore('usr_profiles');
            } // fallthrough
            case 1: {
                db.createStore('fs_files');
            } // fallthrough
            case 2: {
                db.createStore('rec_records');
            } // fallthrough
            case 3: {
                db.createIndex('rec_records', 'form', 'fkey', {unique: false});
            } // fallthrough
            case 4: {
                db.createIndex('rec_records', 'parent', 'pkey', {unique: false});
            } // fallthrough
            case 5: {
                db.deleteIndex('rec_records', 'parent');
                db.createIndex('rec_records', 'parent', 'pfkey', {unique: false});
            } // fallthrough
            case 6: {
                db.deleteIndex('rec_records', 'form');
                db.deleteIndex('rec_records', 'parent');
                db.createIndex('rec_records', 'form', 'keys.form', {unique: false});
                db.createIndex('rec_records', 'parent', 'keys.parent', {unique: false});
            } // fallthrough
            case 7: {
                db.createIndex('rec_records', 'anchor', 'keys.anchor', {unique: false});
                db.createIndex('rec_records', 'sync', 'keys.sync', {unique: false});
            } // fallthrough
            case 8: {
                db.deleteStore('usr_profiles');
            } // fallthrough
            case 9: {
                db.deleteStore('fs_files');
                db.createStore('fs_changes');
            } // fallthrough
            case 10: {
                db.createStore('rec_tags');
            } // fallthrough
        }
    });
}

async function initApp() {
    let code = await fetchCode('main.js');

    try {
        let new_app = new ApplicationInfo(profile);
        let builder = new ApplicationBuilder(new_app);

        await runCodeAsync('Application', code, {
            app: builder
        });
        if (!new_app.pages.size)
            throw new Error('Main script does not define any page');

        new_app.home = new_app.pages.values().next().value;
        app = Util.deepFreeze(new_app);
    } catch (err) {
        if (app == null) {
            let new_app = new ApplicationInfo(profile);
            let builder = new ApplicationBuilder(new_app);

            // For simplicity, a lot of code assumes at least one page exists
            builder.form("default", "Défaut", "Page par défaut");

            new_app.home = new_app.pages.values().next().value;
            app = Util.deepFreeze(new_app);
        }
    }

    if (app.head != null) {
        let template = document.createElement('template');
        render(app.head, template);

        // Clear previous changes
        for (let i = document.head.children.length - 1; i >= head_length; i--)
            document.head.removeChild(document.head.children[i]);
        head_length = document.head.children.length;

        for (let child of template.children)
            document.head.appendChild(child);
    }
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
};

function hasUnsavedData() {
    if (form_state == null)
        return false;
    if (!route.page.getOption('safety', form_record, true))
        return false;

    return form_state.hasChanged();
}

async function runTasks(sync) {
    if (sync && ENV.sync_mode !== 'offline')
        await mutex.chain(syncRecords);
}

function renderMenu() {
    let history = (!goupile.isLocked() && form_record.chain[0].saved && form_record.chain[0].hid != null);
    let menu = (profile.lock == null && (route.form.menu.length > 1 || route.form.chain.length > 1));
    let title = !menu;
    let wide = (route.form.chain[0].menu.length > 3);

    if (!form_record.saved && !form_state.hasChanged() && !UI.isPanelActive('view'))
        menu = false;

    let tabs = getEditorTabs();

    return html`
        <nav class=${goupile.isLocked() ? 'ui_toolbar locked' : 'ui_toolbar'} id="ui_top" style="z-index: 999999;">
            ${goupile.hasPermission('build_code') ? html`
                <div class="drop">
                    <button class=${'icon code' + (profile.develop ? ' active' : '')}
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
                <div style="width: 8px;"></div>
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
                <button class=${'icon view' + (!UI.hasTwoPanels() && UI.isPanelActive(1) ? ' active' : '')}
                        @click=${UI.wrap(e => togglePanels(false, true))}></button>
            ` : ''}
            <div style="flex: 1; min-width: 4px;"></div>

            ${history ? html`
                <button class="ins_hid"
                        @click=${goupile.hasPermission('data_audit') ? UI.wrap(e => runTrailDialog(e, route.ulid)) : null}>
                    <span>
                        ${form_record.chain[0].form.title} <span style="font-weight: bold;">#${form_record.chain[0].hid}</span>
                        ${form_record.historical ? html`<br/><span class="ins_note">${form_record.ctime.toLocaleString()}</span>` : ''}
                    </span>
                </button>
                <div style="width: 15px;"></div>
            ` : ''}
            ${menu && wide ? route.form.chain.map(form => {
                let meta = form_record.map[form.key];

                if (form.menu.length > 1) {
                    return html`
                        <div id="ins_drop" class="drop">
                            <button title=${form.title} @click=${UI.deployMenu}>${form.title}</button>
                            <div>${Util.map(form.menu, item => renderDropItem(meta, item))}</div>
                        </div>
                    `;
                } else {
                    return renderDropItem(meta, form);
                }
            }) : ''}
            ${menu && !wide ? html`
                ${Util.map(route.form.chain[0].menu, item => {
                    let active = UI.isPanelActive('view') &&
                                 (route.form.chain.some(form => form === item.form) || item.page === route.page);
                    let drop = (item.type === 'form' && item.form.menu.length > 1);
                    let enabled = (item.type === 'form') ? isFormEnabled(item.form, form_record) : isPageEnabled(item.page, form_record);

                    if (drop) {
                        let meta = form_record.map[route.form.chain[Math.min(route.form.chain.length - 1, 1)].key];

                        return html`
                            <div id="ins_drop" class="drop">
                                <button title=${item.title} class=${active ? 'active' : ''} ?disabled=${!enabled}
                                        @click=${UI.deployMenu}>${item.title}</button>
                                <div>${Util.map(item.form.menu, item => renderDropItem(meta, item))}</div>
                            </div>
                        `;
                    } else {
                        let meta = form_record.map[route.form.chain[0].key];
                        return renderDropItem(meta, item);
                    }
                })}
            ` : ''}
            ${title ? html`<button title=${route.page.title} class="active">${route.page.title}</button>` : ''}
            ${app.panels.data && (!UI.isPanelActive('view') || form_record.chain[0].saved) ? html`
                <div style="width: 15px;"></div>
                <button class="icon new" @click=${UI.wrap(e => go(e, route.form.chain[0].url + '/new'))}>Ajouter</button>
            ` : ''}
            <div style="flex: 1; min-width: 15px;"></div>

            ${!goupile.isLocked() && profile.instances == null ?
                html`<button class="icon lines" @click=${e => go(e, ENV.urls.instance)}>${ENV.title}</button>` : ''}
            ${!goupile.isLocked() && profile.instances != null ? html`
                <div class="drop right" @click=${UI.deployMenu}>
                    <button class="icon lines" @click=${UI.deployMenu}>${ENV.title}</button>
                    <div>
                        ${profile.instances.slice().sort(Util.makeComparator(instance => instance.name))
                                           .map(instance =>
                            html`<button class=${instance.url === ENV.urls.instance ? 'active' : ''}
                                         @click=${e => go(e, instance.url)}>${instance.name}</button>`)}
                    </div>
                </div>
            ` : ''}
            ${profile.lock == null ? html`
                <div class="drop right">
                    <button class=${goupile.isLoggedOnline() ? 'icon online' : 'icon offline'}
                            @click=${UI.deployMenu}>${profile.type !== 'auto' ? profile.username : ''}</button>
                    <div>
                        ${profile.type === 'auto' && profile.userid ? html`
                            <button style="text-align: center;">
                                ${profile.username}<br/>
                                <span style="font-size: 0.8em; font-style: italic; color: #555;">Identifiant temporaire</span>
                            </button>
                            <hr/>
                        ` : ''}
                        ${profile.type === 'login' ? html`
                            <button @click=${UI.wrap(goupile.runChangePasswordDialog)}>Modifier mon mot de passe</button>
                            <button @click=${UI.wrap(goupile.runResetTOTP)}>Configurer la double authentification</button>
                            <hr/>
                            ${goupile.hasPermission('data_export') ? html`
                                <button @click=${UI.wrap(generateExportKey)}>Générer une clé d'export</button>
                                <hr/>
                            ` : ''}
                        ` : ''}
                        ${profile.root || goupile.hasPermission('build_admin') ? html`
                            <button @click=${e => window.open('/admin/')}>Administration</button>
                            <hr/>
                        ` : ''}
                        ${profile.userid < 0 ? html`<button @click=${UI.wrap(goupile.logout)}>Changer de compte</button>` : ''}
                        <button @click=${UI.wrap(goupile.logout)}>${profile.userid ? 'Se déconnecter' : 'Se connecter'}</button>
                    </div>
                </div>
            ` : ''}
            ${profile.lock != null ?
                html`<button class="icon online" @click=${UI.wrap(goupile.goToLogin)}>Se connecter</button>` : ''}
        </nav>
    `;
}

async function generateExportKey(e) {
    let response = await Net.fetch(`${ENV.urls.instance}api/change/export_key`, { method: 'POST' });

    if (!response.ok) {
        let err = await Net.readError(response);
        throw new Error(err);
    }

    let export_key = (await response.text()).trim();

    await UI.dialog(e, 'Clé d\'export', {}, (d, resolve, reject) => {
        d.text('export_key', 'Clé d\'export', {
            value: export_key,
            readonly: true
        });
    });
}

function renderDropItem(meta, item) {
    let active;
    let url;
    let title;
    let status;

    if (item instanceof PageInfo || item.type === 'page') {
        let page = item.page || item;

        if (!isPageEnabled(page, form_record))
            return '';

        active = (page === route.page);
        url = page.url;
        title = page.title;
        status = (meta.status[page.key] != null);
    } else if (item instanceof FormInfo || item.type === 'form') {
        let form = item.form || item;

        if (!isFormEnabled(form, form_record))
            return '';

        active = route.form.chain.some(parent => form === parent);
        url = form.url;
        title = form.multi || form.title;
        status = (meta.status[form.key] != null);
    } else {
        throw new Error(`Unknown item type '${item.type}'`);
    }

    return html`
        <button class=${active ? 'active' : ''}
                @click=${UI.wrap(e => active ? togglePanels(null, true) : go(e, url))}>
            <div style="flex: 1;">${title}</div>
            ${status ? html`<div>&nbsp;✓\uFE0E</div>` : ''}
       </button>
    `;
}

async function togglePanels(left, right) {
    if (left != null)
        UI.togglePanel(0, left);

    if (typeof right == 'string') {
        UI.togglePanel(right, true);
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

    let tabs = getEditorTabs();
    let active_tab = tabs.find(tab => tab.active);

    return html`
        <div style="--menu_color: #1d1d1d; --menu_color_n1: #2c2c2c;">
            <div class="ui_toolbar">
                <div class="drop">
                    <button @click=${UI.deployMenu}>${active_tab.title}</button>
                    <div>
                        ${tabs.map(tab => html`<button class=${UI.isPanelActive('editor') && tab.active ? 'active' : ''}
                                                       @click=${UI.wrap(e => toggleEditorFile(e, tab.filename))}>${tab.title}</button>`)}
                    </div>
                </div>
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

function getEditorTabs() {
    let tabs = [];

    tabs.push({
        title: 'Projet',
        filename: 'main.js',
        active: false
    });
    tabs.push({
        title: 'Formulaire',
        filename: route.page.getOption('filename', form_record),
        active: false
    });

    for (let tab of tabs)
        tab.active = (editor_filename == tab.filename);

    return tabs;
}

async function applyMainScript() {
    let code = await fetchCode('main.js');

    let new_app = new ApplicationInfo(profile);
    let builder = new ApplicationBuilder(new_app);

    try {
        await runCodeAsync('Application', code, {
            app: builder
        });
        main_works = true;
    } catch (err) {
        // Don't log, because runCodeAsync does it already
        main_works = false;
        return;
    }

    if (!new_app.pages.size) {
        throw new Error('Main script does not define any page');
        main_works = false;
    }

    // It works! Refresh the page
    document.location.reload();
}

function renderData() {
    let visible_rows = data_rows;

    if (data_filter != null) {
        let func = makeFilterFunction(data_filter);
        visible_rows = visible_rows.filter(meta => func(meta.hid));
    }
    if (data_date != null) {
        visible_rows = visible_rows.filter(meta => {
            if (meta.ctime.getFullYear() == data_date.year &&
                    meta.ctime.getMonth() + 1 == data_date.month &&
                    meta.ctime.getDate() == data_date.day)
                return true;

            /*for (let key in meta.status) {
                let status = meta.status[key];
                if (status.ctime.toDateString() == data_date.toDateString())
                    return true;
            }*/

            return false;
        });
    }

    let column_stats = {};
    let hid_width = 80;
    for (let row of visible_rows) {
        for (let key of data_form.pages.keys())
            column_stats[key] = (column_stats[key] || 0) + (row.status[key] != null);
        for (let key of data_form.forms.keys())
            column_stats[key] = (column_stats[key] || 0) + (row.status[key] != null);

        hid_width = Math.max(hid_width, computeHidWidth(row.hid));
    }

    let recording = (form_record.chain[0].saved || form_state.hasChanged());
    let recording_new = !form_record.chain[0].saved && form_state.hasChanged();

    return html`
        <div class="padded">
            <div class="ui_quick" style="margin-right: 2.2em;">
                <input type="text" placeholder="Filtrer..." .value=${data_filter || ''}
                       @input=${e => { data_filter = e.target.value || null; run(); }} />
                <div style="flex: 1;"></div>
                <label>
                    Date de création :
                    <input type="date" .value=${data_date != null ? data_date.toString() : ''}
                           @input=${e => { data_date = e.target.value ? dates.parse(e.target.value) : null; run(); }} />
                </label>
                <div style="flex: 1;"></div>
                ${app.dashboard == null ? html`
                    <p>
                        ${visible_rows.length} ${visible_rows.length < data_rows.length ? `/ ${data_rows.length} ` : ''} ${data_rows.length > 1 ? 'lignes' : 'ligne'}
                        ${ENV.sync_mode !== 'offline' ? html`(<a @click=${UI.wrap(e => syncRecords(true, true))}>synchroniser</a>)` : ''}
                    </p>
                ` : ''}
                ${app.dashboard != null ? html`<a href=${app.dashboard} target="_blank">Tableau de bord</a>` : ''}
            </div>

            <table class="ui_table fixed" id="ins_data"
                   style=${'min-width: ' + (5 + 5 * data_form.menu.length) + 'em;'}>
                <colgroup>
                    <col style=${'width: ' + hid_width + 'px;'} />
                    <col style="width: 8em;"/>
                    ${Util.mapRange(0, data_form.menu.length, () => html`<col/>`)}
                    <col style="width: 2em;"/>
                </colgroup>

                <thead>
                    <tr>
                        <th>ID</th>
                        <th>Création</th>
                        ${data_form.menu.map(item => {
                            let prec = `${column_stats[item.key] || 0} / ${visible_rows.length}`;
                            let title = `${item.title}\nDisponible : ${prec} ${visible_rows.length > 1 ? 'lignes' : 'ligne'}`;

                            return html`
                                <th title=${title}>
                                    ${item.title}<br/>
                                    <span style="font-size: 0.7em; font-weight: normal;">${prec}</span>
                                </th>
                            `;
                        })}
                    </tr>
                </thead>

                <tbody>
                    ${visible_rows.map(row => {
                        let active = form_record.chain.some(record => record.ulid === row.ulid);

                        return html`
                            <tr>
                                <td class=${(row.hid == null ? 'missing' : '') +
                                            (active ? ' active' : '')}
                                    title=${row.hid || ''}>${row.hid != null ? row.hid : 'NA'}</td>
                                <td class=${active ? ' active' : ''} title=${row.ctime.toLocaleString()}>${row.ctime.toLocaleDateString()}</td>
                                ${row.form.menu.map(item => {
                                    if (item.type === 'page') {
                                        let page = item.page;
                                        let url = page.url + `/${row.ulid}`;
                                        let status = row.status[page.key];

                                        if (row.status[page.key] != null) {
                                            if (app.mtime) {
                                                let tooltip = 'Créé : ' + status.ctime.toLocaleString() +
                                                              (status.mtime.getTime() != status.ctime.getTime() ? '\nModifié : ' + status.mtime.toLocaleString() : '');

                                                return html`
                                                    <td class=${active && page === route.page ? 'complete active' : 'complete'} title=${tooltip}>
                                                        <a href=${url}>${status.mtime.toLocaleDateString()}</a>
                                                        ${renderTags(status.tags)}
                                                    </td>
                                                `;
                                            } else {
                                                return html`
                                                    <td class=${active && page === route.page ? 'complete active' : 'complete'}>
                                                        <a href=${url}>Rempli</a>
                                                        ${renderTags(status.tags)}
                                                    </td>
                                                `;
                                            }
                                        } else {
                                            return html`<td class=${active && page === route.page ? 'missing active' : 'missing'}
                                                            title=${item.title}><a href=${url}>Afficher</a></td>`;
                                        }
                                    } else if (item.type === 'form') {
                                        let form = item.form;

                                        if (row.status[form.key] != null) {
                                            let child = row.children[form.key][0];
                                            let url = form.url + `/${child.ulid}`;
                                            let status = row.status[form.key];

                                            if (app.mtime) {
                                                let tooltip = 'Créé : ' + status.ctime.toLocaleString() +
                                                              (status.mtime.getTime() != status.ctime.getTime() ? '\nModifié : ' + status.mtime.toLocaleString() : '');

                                                return html`
                                                    <td class=${active && route.form.chain.includes(form) ? 'complete active' : 'complete'} title=${tooltip}>
                                                        <a href=${url}>${status.mtime.toLocaleDateString()}</a>
                                                        ${renderTags(status.tags)}
                                                    </td>
                                                `;
                                            } else {
                                                return html`
                                                    <td class=${active && route.form.chain.includes(form) ? 'complete active' : 'complete'}>
                                                        <a href=${url}>Rempli</a>
                                                        ${renderTags(status.tags)}
                                                    </td>
                                                `;
                                            }
                                        } else {
                                            let url = form.url + `/${row.ulid}`;

                                            return html`<td class=${active && route.form.chain.includes(form) ? 'missing active' : 'missing'}
                                                            title=${item.title}><a href=${url}>Afficher</a></td>`;
                                        }
                                    }
                                })}
                                ${goupile.hasPermission('data_save') ?
                                    html`<th><a @click=${UI.wrap(e => runDeleteRecordDialog(e, row.ulid))}>✕</a></th>` : ''}
                            </tr>
                        `;
                    })}
                    ${!visible_rows.length && !recording ? html`<tr><td colspan=${2 + data_form.menu.length}>Aucune ligne à afficher</td></tr>` : ''}
                    ${recording_new ? html`
                        <tr>
                            <td class="missing">NA</td>
                            <td class="missing">NA</td>
                            <td class="missing" colspan=${data_form.menu.length}><a @click=${e => togglePanels(null, 'view')}>Nouvel enregistrement</a></td>
                        </tr>
                    ` : ''}
                </tbody>
            </table>

            <div class="ui_actions">
                ${goupile.hasPermission('data_export') ?
                    html`<button @click=${e => { window.location.href = ENV.urls.instance + 'api/records/export'; }}>Exporter les données</button>` : ''}
                ${ENV.backup_key != null ?
                    html`<button @click=${UI.wrap(backupRecords)}>Faire une sauvegarde chiffrée</button>` : ''}
            </div>
        </div>
    `;
}

function renderTags(tags) {
    tags = tags || [];
    return tags.map(tag => html` <span class="ui_tag" style=${'background: ' + tag.color + ';'}>${tag.type}</span>`);
}

function computeHidWidth(hid) {
    let ctx = computeHidWidth.ctx;

    if (ctx == null) {
        let canvas = document.createElement('canvas');
        let style = window.getComputedStyle(document.body);

        ctx = canvas.getContext('2d');
        ctx.font = style.getPropertyValue('font-size') + ' ' +
                   style.getPropertyValue('font-family');

        computeHidWidth.ctx = ctx;
    }

    let metrics = ctx.measureText(hid != null ? hid : 'NA');
    let width = Math.ceil(metrics.width * 1.04) + 20;

    width = Math.min(width, 700);

    return width;
}

function makeFilterFunction(filter) {
    let re = '';
    for (let i = 0; i < filter.length; i++) {
        let c = filter[i].toLowerCase();

        switch (c) {
            case 'e':
            case 'ê':
            case 'é':
            case 'è': { re += '[eèéê]'; } break;
            case 'a':
            case 'à':
            case 'â': { re += '[aàâ]'; } break;
            case 'i':
            case 'ï': { re += '[iï]'; } break;
            case 'u':
            case 'ù': { re += '[uù]'; } break;
            case 'ô': { re += '[ôo]'; } break;
            case 'o': {
                if (filter[i + 1] === 'e') {
                    re += '(oe|œ)';
                    i++;
                } else {
                    re += '[ôo]';
                }
            } break;
            case 'œ': { re += '(oe|œ)'; } break;
            case '—':
            case '–':
            case '-': { re += '[—–\\-]'; } break;

            // Escape special regex characters
            case '/':
            case '+':
            case '*':
            case '?':
            case '<':
            case '>':
            case '&':
            case '|':
            case '\\':
            case '^':
            case '$':
            case '(':
            case ')':
            case '{':
            case '}':
            case '[':
            case ']': { re += `\\${c}`; } break;

            // Special case '.' for CIM-10 codes
            case '.': { re += `\\.?`; } break;

            default: { re += c; } break;
        }
    }
    re = new RegExp(re, 'i');

    let func = value => {
        if (value != null) {
            if (typeof value !== 'string')
                value = value.toLocaleString();
            return value.match(re);
        } else {
            return false;
        }
    };

    return func;
}

function runDeleteRecordDialog(e, ulid) {
    return UI.confirm(e, 'Voulez-vous vraiment supprimer cet enregistrement ?',
                         'Supprimer', () => deleteRecord(ulid));
}

function renderPage() {
    let filename = route.page.getOption('filename', form_record);
    let code = code_buffers.get(filename).code;

    let model = new FormModel;
    let readonly = isReadOnly(form_record);

    form_builder = new FormBuilder(form_state, model, readonly);

    try {
        form_builder.pushOptions({});

        // Previous and next page?
        {
            let sequence = (profile.lock != null) ? profile.lock.pages : route.page.getOption('sequence', form_record);

            if (sequence === false)
                sequence = null;
            if (sequence != null && !Array.isArray(sequence)) {
                let root = route.form.chain[0];
                let pages = getAllPages(root.menu);

                sequence = pages.filter(page => {
                    if (page == route.page)
                        return true;

                    if (page.getOption('sequence', form_record) !== sequence)
                        return false;
                    if (!isPageEnabled(page, form_record, true))
                        return false;

                    return true;
                }).map(page => page.key);
            }

            nav_sequence = {
                prev: null,
                next: null,
                stay: true
            };

            if (sequence != null) {
                let idx = sequence.indexOf(route.page.key);

                if (idx - 1 >= 0) {
                    let page = app.pages.get(sequence[idx - 1]);

                    if (page != null) {
                        nav_sequence.prev = page.url;
                        nav_sequence.stay = form_record.saved;
                    }
                }

                if (idx >= 0 && idx + 1 < sequence.length) {
                    let page = app.pages.get(sequence[idx + 1]);

                    if (page != null) {
                        nav_sequence.next = page.url;
                        nav_sequence.stay = form_record.saved;
                    }
                }
            }
        }

        form_tags = [];

        let meta = Object.assign({}, form_record, {
            tag: (type, color) => {
                let tag = {
                    type: type,
                    color: color
                };

                let prev_idx = form_tags.findIndex(it => it.type == tag.type);

                if (prev_idx < 0) {
                    form_tags.push(tag);
                } else {
                    form_tags[prev_idx] = tag;
                }
            }
        });

        runCodeSync('Formulaire', code, {
            app: app,
            form: form_builder,
            meta: meta,
            forms: meta.forms,
            values: form_state.values,

            nav: {
                form: route.form,
                page: route.page,
                go: (url) => go(null, url),

                url: (key) => {
                    let page = app.pages.get(key);
                    return page ? page.url : null;
                },

                save: async () => {
                    await saveRecord(form_record, new_hid, form_values, route.page);
                    await run();
                },
                delete: (e, ulid, confirm = true) => {
                    if (e != null && e.target == null)
                        [e, ulid, confirm] = [null, e, ulid];

                    if (confirm) {
                        runDeleteRecordDialog(e, ulid);
                    } else {
                        deleteRecord(ulid);
                    }
                },

                lock: (e, keys) => {
                    if (e != null && e.target == null)
                        [e, keys] = [null, e];

                    let lock = {
                        ulid: form_record.chain[0].ulid,
                        pages: keys
                    };

                    return goupile.runLockDialog(e, lock);
                },
                isLocked: goupile.isLocked
            },

            data: {
                dict: form_dictionaries,

                store: (idx, values) => {
                    let form = route.form.chain[route.form.chain.length - idx - 1];
                    if (form == null)
                        throw new Error(`Parent ${idx} does not exist`);

                    let ptr = form_values[form.key];
                    if (ptr == null) {
                        ptr = {};
                        form_values[form.key] = ptr;
                    }
                    Object.assign(ptr, values);
                }
            },

            crypto: mixer
        });

        new_hid = meta.hid;

        form_builder.popOptions({});
        if (model.hasErrors())
            form_builder.errorList();

        let default_actions = !readonly && route.page.getOption('default_actions', form_record, true);

        if (default_actions && model.variables.length) {
            let prev_actions = model.actions;
            model.actions = [];

            if (nav_sequence.prev != null || nav_sequence.next != null) {
                form_builder.action('Précédent', {disabled: nav_sequence.prev == null}, async e => {
                    let url = nav_sequence.prev;
                    go(e, url);
                });

                if (nav_sequence.next != null) {
                    form_builder.action('Continuer', {color: '#2d8261', always: true}, async e => {
                        let url = nav_sequence.next;

                        if (!form_record.saved || form_state.hasChanged()) {
                            form_builder.triggerErrors();
                            await saveRecord(form_record, new_hid, form_values, route.page);
                        }

                        go(e, url);
                    });
                }

                if (nav_sequence.stay)
                    form_builder.action('-');
            }
            if (nav_sequence.stay || nav_sequence.next == null) {
                let name = (!form_record.saved || form_state.hasChanged()) ? 'Enregistrer' : '✓ Enregistré';

                form_builder.action(name, {disabled: !form_state.hasChanged(),
                                           color: nav_sequence.next == null ? '#2d8261' : null,
                                           always: nav_sequence.next == null}, async () => {
                    form_builder.triggerErrors();

                    await saveRecord(form_record, new_hid, form_values, route.page);
                    run();
                });
            }

            if (!goupile.isLocked()) {
                if (form_state.just_triggered) {
                    form_builder.action('Forcer l\'enregistrement', {}, async e => {
                        await UI.confirm(e, html`Confirmez-vous l'enregistrement <b>malgré la présence d'erreurs</b> ?`,
                                            'Enregistrer', () => {});

                        await saveRecord(form_record, new_hid, form_values, route.page);

                        run();
                    });
                }

                if (form_state.hasChanged()) {
                    form_builder.action('-');

                    form_builder.action('Oublier', {color: '#db0a0a', always: form_record.saved}, async e => {
                        await UI.confirm(e, html`Souhaitez-vous réellement <b>annuler les modifications en cours</b> ?`,
                                            'Oublier', () => {});

                        if (form_record.saved) {
                            let record = await loadRecord(form_record.ulid, null);

                            let load = route.page.getOption('load', record, []);
                            await expandRecord(record, load);

                            updateContext(route, record);
                        } else {
                            let record = Object.assign({}, form_record);
                            record.values = {};

                            updateContext(route, record);
                        }

                        run();
                    });
                }

                if (route.form.multi && form_record.saved) {
                    form_builder.action('-');
                    form_builder.action('Supprimer', {}, e => runDeleteRecordDialog(e, form_record.ulid));
                    form_builder.action('Nouveau', {}, e => {
                        let url = contextualizeURL(route.page.url, form_record.parent);
                        return go(e, url);
                    });
                }
            }

            if (prev_actions.length) {
                form_builder.action('-');
                model.actions.push(...prev_actions);
            }
        }

        render(model.renderWidgets(), page_div);
        page_div.classList.remove('disabled');
    } catch (err) {
        if (!page_div.children.length)
            render('Impossible de générer la page à cause d\'une erreur', page_div);
        page_div.classList.add('disabled');

        console.error(err);
    }

    let menu = (profile.lock == null) && (route.form.menu.length > 1 || route.form.chain.length > 1);
    let wide = (route.form.chain[0].menu.length > 3);
    let hid = (!goupile.isLocked() && form_record.chain[0].saved && form_record.chain[0].hid != null);

    let sections = model.widgets.filter(intf => intf.options.anchor).map(intf => ({
        title: intf.label,
        anchor: intf.options.anchor
    }));

    return html`
        <div class="print" @scroll=${syncEditorScroll}}>
            <div id="ins_page">
                <div id="ins_menu">${menu ? html`
                    ${Util.mapRange(1 - wide, route.form.chain.length, idx => renderFormMenu(route.form.chain[idx]))}
                    ${sections.length > 1 ? html`
                        <h1>${route.page.title}</h1>
                        <ul>${sections.map(section => html`<li><a href=${'#' + section.anchor}>${section.title}</a></li>`)}</ul>
                    ` : ''}
                ` : ''}</div>

                <form id="ins_form" autocomplete="off" @submit=${e => e.preventDefault()}>
                    ${page_div}
                </form>

                <div id="ins_actions">
                    ${model.renderActions()}
                    ${form_record.historical ? html`<p class="historical">${form_record.ctime.toLocaleString()}</p>` : ''}

                    ${route.form.chain.map(form => {
                        let meta = form_record.map[form.key];

                        if (profile.lock != null)
                            return '';
                        if (meta == null || meta.siblings == null)
                            return '';

                        return html`
                            <h1>${form.multi}</h1>
                            <ul>
                                ${meta.siblings.map(sibling => {
                                    let url = route.page.url + `/${sibling.ulid}`;
                                    return html`<li><a href=${url} class=${sibling.ulid === meta.ulid ? 'active' : ''}>${sibling.ctime.toLocaleString()}</a></li>`;
                                })}
                                <li><a href=${contextualizeURL(route.page.url, meta.parent)} class=${!meta.saved ? 'active' : ''}>Nouvelle fiche</a></li>
                            </ul>
                        `;
                    })}
                </div>
            </div>
            <div style="flex: 1;"></div>

            ${!isPageEnabled(route.page, form_record, true) ? html`
                <div id="ins_develop">
                    Cette page n'est théoriquement pas activée<br/>
                    Le mode de conception vous permet d'y accéder.
                </div>
            ` : ''}

            <nav class="ui_toolbar" id="ins_tasks" style="z-index: 999999;">
                ${hid ? html`
                    <button class="ins_hid" style=${form_record.historical ? 'color: #00ffff;' : ''}
                            @click=${goupile.hasPermission('data_audit') ? UI.wrap(e => runTrailDialog(e, route.ulid)) : null}>
                        ${form_record.chain[0].form.title} <span style="font-weight: bold;">#${form_record.chain[0].hid}</span>
                        ${form_record.historical ? html`<br/><span style="font-size: 0.5em;">(historique)</span>` : ''}
                    </button>
                ` : ''}
                <div style="flex: 1;"></div>
                ${model.actions.some(action => !action.options.always) ? html`
                    <div class="drop up right">
                        <button @click=${UI.deployMenu}>Actions</button>
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
                ${!hid ? html`<div style="flex: 1;"></div>` : ''}
            </nav>
        </div>
    `;
}

function getAllPages(items) {
    let pages = items.flatMap(item => {
        if (item.type === 'form') {
            let form = item.form;
            return getAllPages(form.menu);
        } else if (item.type == 'page') {
            return item.page;
        }
    })

    return pages;
}

function renderFormMenu(form) {
    let meta = form_record.map[form.key];

    let show = (form.menu.length > 1);
    let title = form.multi ? (meta.saved ? meta.ctime.toLocaleString() : 'Nouvelle fiche') : form.title;

    return html`
        ${show ? html`
            <h1>${title}</h1>
            <ul>
                ${Util.map(form.menu, item => {
                    if (item.type === 'page') {
                        let page = item.page;

                        let cls = '';
                        if (page === route.page)
                            cls += ' active';
                        if (!isPageEnabled(page, form_record, true)) {
                            if (profile.develop) {
                                cls += ' disabled';
                            } else {
                                return '';
                            }
                        }

                        return html`
                            <li><a class=${cls} href=${contextualizeURL(page.url, form_record)}>
                                <div style="flex: 1;">${page.title}</div>
                                ${meta && meta.status[page.key] != null ? html`<div>&nbsp;✓\uFE0E</div>` : ''}
                            </a></li>
                        `;
                    } else if (item.type === 'form') {
                        let form = item.form;

                        let cls = '';
                        if (route.form.chain.some(parent => form === parent))
                            cls += ' active';
                        if (!isFormEnabled(form, form_record, true)) {
                            if (profile.develop) {
                                cls += ' disabled';
                            } else {
                                return '';
                            }
                        }

                        return html`
                            <li><a class=${cls} href=${contextualizeURL(form.url, form_record)} style="display: flex;">
                                <div style="flex: 1;">${form.multi || form.title}</div>
                                ${meta && meta.status[form.key] != null ? html`<div>&nbsp;✓\uFE0E</div>` : ''}
                            </a></li>
                        `;
                    }
                })}
            </ul>
        ` : ''}
    `;
}

async function runCodeAsync(title, code, args) {
    args = Object.assign({
        goupile: goupile,
        profile: profile,

        util: Util,
        log: Log,
        net: Net,
        ui: UI,

        render: render,
        html: html,
        svg: svg,
        until: until,
        LocalDate: LocalDate,
        LocalTime: LocalTime,
        PeriodPicker: PeriodPicker,
        crypto: mixer,

        dates: LocalDate, // Deprecated
        times: LocalTime // Deprecated
    }, args);

    let entry = error_entries[title];
    if (entry == null) {
        entry = new Log.Entry;
        error_entries[title] = entry;
    }

    try {
        let AsyncFunction = Object.getPrototypeOf(async function(){}).constructor;

        let func = new AsyncFunction(...Object.keys(args), code);
        await func(...Object.values(args));

        entry.close();
    } catch (err) {
        let location = Util.locateEvalError(err);
        let msg = `Erreur sur ${title}\n${location?.line != null ? `Ligne ${location.line} : ` : ''}${err.message}`;

        entry.error(msg, -1);
        throw new Error(msg);
    }
}

function runCodeSync(title, code, args) {
     args = Object.assign({
        goupile: goupile,
        profile: profile,

        util: Util,
        log: Log,
        net: Net,
        ui: UI,

        render: render,
        html: html,
        svg: svg,
        until: until,
        LocalDate: LocalDate,
        LocalTime: LocalTime,
        PeriodPicker: PeriodPicker,

        dates: LocalDate, // Deprecated
        times: LocalTime // Deprecated
    }, args);

    let entry = error_entries[title];
    if (entry == null) {
        entry = new Log.Entry;
        error_entries[title] = entry;
    }

    try {
        let func = new Function(...Object.keys(args), code);
        func(...Object.values(args));

        entry.close();
    } catch (err) {
        let location = Util.locateEvalError(err);
        let msg = `Erreur sur ${title}\n${location?.line != null ? `Ligne ${location.line} : ` : ''}${err.message}`;

        entry.error(msg, -1);
        throw new Error(msg);
    }
}

function runTrailDialog(e, ulid) {
    return UI.dialog(e, 'Historique', {}, (d, resolve, reject) => {
        d.output(html`
            <table class="ui_table">
                <colgroup>
                    <col/>
                    <col/>
                    <col style="min-width: 12em;"/>
                </colgroup>

                <tbody>
                    <tr class=${route.version == null ? 'active' : ''}>
                        <td><a href=${route.page.url + `/${route.ulid}@`}>🔍\uFE0E</a></td>
                        <td colspan="2">Version actuelle</td>
                    </tr>

                    ${Util.mapRange(0, form_record.fragments.length, idx => {
                        let version = form_record.fragments.length - idx;
                        let fragment = form_record.fragments[version - 1];

                        let page = app.pages.get(fragment.page) || route.page;
                        let url = page.url + `/${route.ulid}@${version}`;

                        return html`
                            <tr class=${version === route.version ? 'active' : ''}>
                                <td><a href=${url}>🔍\uFE0E</a></td>
                                <td>${fragment.user}</td>
                                <td>${fragment.mtime.toLocaleString()}</td>
                            </tr>
                        `;
                    })}
                </tbody>
            </table>
        `);
    });
}

async function saveRecord(record, hid, values, page, silent = false) {
    if (isReadOnly(record))
        throw new Error('You are not allowed to save data');

    await mutex.run(async () => {
        let progress = Log.progress('Enregistrement en cours');

        try {
            let ulid = record.ulid;
            let page_key = page.key;
            let ptr = values[record.form.key];
            let tags = form_tags;

            await db.transaction('rw', ['rec_records'], async t => {
                do {
                    let fragment;
                    if (ptr != null) {
                        fragment = {
                            type: 'save',
                            user: profile.username,
                            mtime: new Date,
                            fs: ENV.version,
                            page: page_key,
                            values: ptr,
                            tags: tags
                        };
                    } else {
                        fragment = null;
                    }

                    let key = profile.namespaces.records + `:${record.ulid}`;
                    let obj = await t.load('rec_records', key);

                    let entry;
                    if (obj != null) {
                        entry = await decryptSymmetric(obj.enc, ['records', 'lock']);
                        if (hid != null)
                            entry.hid = hid;
                    } else {
                        obj = {
                            keys: {
                                form: null,
                                parent: null,
                                anchor: null,
                                sync: null
                            },
                            enc: null
                        };
                        entry = {
                            ulid: record.ulid,
                            hid: hid,
                            parent: null,
                            form: record.form.key,
                            fragments: []
                        };

                        if (record.parent != null) {
                            entry.parent = {
                                ulid: record.parent.ulid,
                                version: record.parent.version
                            };
                        }
                    }

                    if (record.version !== entry.fragments.length)
                        throw new Error('Cannot overwrite old record fragment');
                    if (fragment != null)
                        entry.fragments.push(fragment);

                    // Always rewrite keys to fix potential namespace changes
                    obj.keys.form = `${profile.namespaces.records}/${record.form.key}`;
                    if (record.parent != null) {
                        let mtime = entry.fragments.length ? entry.fragments[entry.fragments.length - 1].mtime.getTime() : null;
                        obj.keys.parent = `${profile.namespaces.records}:${record.parent.ulid}/${record.form.key}@${mtime}`;
                    }
                    obj.keys.sync = profile.namespaces.records;

                    obj.enc = await encryptSymmetric(entry, 'records');
                    await t.saveWithKey('rec_records', key, obj);

                    do {
                        page_key = record.form.key;
                        record = record.parent;
                        if (record == null)
                            break;
                        ptr = form_values[record.form.key];
                        tags = record.tags;
                    } while (record.saved && ptr == null);
                } while (record != null);
            });

            data_rows = null;

            if (ENV.sync_mode !== 'offline') {
                try {
                    if (!goupile.isLoggedOnline())
                        throw new Error('Cannot save online');

                    await mutex.chain(() => syncRecords(false));
                    if (!silent) {
                        progress.success('Enregistrement effectué');
                    } else {
                        progress.close();
                    }
                } catch (err) {
                    progress.info('Enregistrement local effectué');
                    enablePersistence();
                }
            } else {
                if (!silent) {
                    progress.success('Enregistrement local effectué');
                } else {
                    progress.close();
                }
                enablePersistence();
            }

            let new_route = Object.assign({}, route);
            let claim = silent || page.getOption('claim', record, true);

            if (claim) {
                record = await loadRecord(ulid, null);
            } else {
                new_route.ulid = null;
                new_route.version = null;
                new_route.page = Array.from(app.pages.values()).find(page => page.url == new_route.form.chain[0].url);
                new_route.form = new_route.page.form;

                record = await createRecord(new_route.form);
            }

            let load = new_route.page.getOption('load', record, []);
            await expandRecord(record, load);

            updateContext(new_route, record);
        } catch (err) {
            progress.close();
            throw err;
        }
    });
}

async function deleteRecord(ulid) {
    if (!goupile.hasPermission('data_save'))
        throw new Error('You are not allowed to delete data');

    await mutex.run(async () => {
        let progress = Log.progress('Suppression en cours');

        try {
            // XXX: Delete children (cascade)?
            await db.transaction('rw', ['rec_records'], async t => {
                let key = profile.namespaces.records + `:${ulid}`;
                let obj = await t.load('rec_records', key);

                if (obj == null)
                    throw new Error('Cet enregistrement est introuvable');

                let entry = await decryptSymmetric(obj.enc, ['records', 'lock']);

                // Mark as deleted with special fragment
                let fragment = {
                    type: 'delete',
                    user: profile.username,
                    mtime: new Date,
                    fs: ENV.version
                };
                entry.fragments.push(fragment);

                obj.keys.parent = null;
                obj.keys.form = null;
                obj.keys.sync = profile.namespaces.records;

                obj.enc = await encryptSymmetric(entry, 'records');
                await t.saveWithKey('rec_records', key, obj);
            });

            if (ENV.sync_mode !== 'offline') {
                try {
                    if (!goupile.isLoggedOnline())
                        throw new Error('Cannot delete online');

                    await mutex.chain(() => syncRecords(false));
                    progress.success('Suppression effectuée');
                } catch (err) {
                    progress.info('Suppression effectuée (non synchronisée)');
                    console.log(err);
                }
            } else {
                progress.success('Suppression locale effectuée');
            }

            data_rows = null;

            // Redirect if needed
            {
                let idx = form_record.chain.findIndex(record => record.ulid === ulid);

                if (idx >= 0) {
                    if (idx > 0) {
                        let record = form_record.chain[idx];
                        let url = contextualizeURL(record.form.multi ? route.page.url : record.parent.form.url, record.parent);

                        go(null, url, { force: true });
                    } else {
                        go(null, route.form.chain[0].url + '/new', { force: true });
                    }
                } else {
                    go();
                }
            }
        } catch (err) {
            progress.close();
            throw err;
        }
    });
}

function enablePersistence() {
    let storage_warning = 'Impossible d\'activer le stockage local persistant';

    if (navigator.storage && navigator.storage.persist) {
        navigator.storage.persist().then(granted => {
            if (granted) {
                console.log('Stockage persistant activé');
            } else {
                console.log(storage_warning);
            }
        });
    } else {
        console.log(storage_warning);
    }
}

async function syncEditor() {
    if (editor_el == null) {
        if (typeof ace === 'undefined')
            await Net.loadScript(`${ENV.urls.static}ace/ace.js`);

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
        session.selection.on('changeSelection', () => {
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

function toggleEditorFile(e, filename) {
    editor_filename = filename;
    return togglePanels(true, null);
}

async function handleFileChange(filename) {
    if (ignore_editor_change)
        return;

    let buffer = code_buffers.get(filename);
    let code = buffer.session.doc.getValue();
    let blob = new Blob([code]);
    let sha256 = await mixer.Sha256.async(blob);

    let key = `${profile.userid}:${filename}`;

    await db.saveWithKey('fs_changes', key, {
        filename: filename,
        size: blob.size,
        sha256: sha256,
        blob: blob
    });

    buffer.code = code;
    buffer.sha256 = sha256;

    if (code_timer != null)
        clearTimeout(code_timer);
    code_timer = setTimeout(uploadFsChanges, 3000);

    run();
}

const uploadFsChanges = Util.serialize(async function () {
    let progress = Log.progress('Envoi des modifications');

    try {
        let range = IDBKeyRange.bound(profile.userid + ':', profile.userid + '`', false, true);
        let changes = await db.loadAll('fs_changes', range);

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

            let key = `${profile.userid}:${file.filename}`;
            await db.delete('fs_changes', key);
        }

        progress.close();
    } catch (err) {
        progress.close();
        Log.error(err);
    }

    if (code_timer != null)
        clearTimeout(code_timer);
    code_timer = null;
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
            let line = parseInt(widget_els[i].dataset.line, 10);

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
                let line = parseInt(widget_els[i].dataset.line, 10);

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
                    let line = parseInt(widget_els[highlight_last].dataset.line, 10);
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
            let line = parseInt(el.dataset.line, 10);

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

async function runPublishDialog(e) {
    await uploadFsChanges();

    let publisher = new InstancePublisher;
    await publisher.runDialog(e);

    run();
}

async function go(e, url = null, options = {}) {
    options = Object.assign({ push_history: true }, options);

    await mutex.run(async () => {
        let new_route = Object.assign({}, route);
        let new_record;
        let new_dictionaries;
        let new_code;
        let explicit_panels = false;

        // Parse new URL
        if (url != null) {
            if (!(url instanceof URL))
                url = new URL(url, window.location.href);
            if (url.pathname === ENV.urls.instance)
                url = new URL(app.home.url, window.location.href);
            goupile.setCurrentHash(url.hash);

            if (!url.pathname.endsWith('/'))
                url.pathname += '/';

            // Goodbye!
            if (!url.pathname.startsWith(`${ENV.urls.instance}main/`)) {
                console.log(url.pathname, ENV.urls.instance);

                if (hasUnsavedData())
                    await goupile.confirmDangerousAction(e);

                window.onbeforeunload = null;
                window.location.href = url.href;

                return;
            }

            let path = url.pathname.substr(ENV.urls.instance.length + 5);
            let [key, what] = path.split('/').map(str => str.trim());

            // Support shorthand URLs: /main/<ULID>(@<VERSION>)
            if (key && key.match(/^[A-Z0-9]{26}(@[0-9]+)?$/)) {
                what = key;
                key = app.home.key;
            }

            // Follow lock sequence
            if (profile.lock != null) {
                if (!profile.lock.pages.includes(key))
                    key = profile.lock.pages[0];
            }

            // Find page information
            new_route.page = app.pages.get(key);
            if (new_route.page == null) {
                Log.error(`La page '${key}' n'existe pas`);
                new_route.page = app.home;
            }
            new_route.form = new_route.page.form;

            let [ulid, version] = what ? what.split('@') : [null, null];

            // Popping history
            if (!ulid && !options.push_history)
                ulid = 'new';

            // Go to default record?
            if (!ulid && profile.lock != null) {
                ulid = profile.lock.ulid;
            } else if (!ulid && profile.userid < 0) {
                let range = IDBKeyRange.only(profile.namespaces.records + `/${new_route.form.key}`);
                let keys = await db.list('rec_records/form', range);

                if (keys.length) {
                    let key = keys[0];
                    ulid = key.primary.substr(key.primary.indexOf(':') + 1);
                }
            }

            // Deal with ULID
            if (ulid && ulid !== new_route.ulid) {
                if (ulid === 'new') {
                    new_route.ulid = null;
                    new_route.version = null;
                } else if (form_record == null || !form_record.chain.some(record => record.ulid === ulid) ||
                                                  new_route.form.chain.some(form => form.multi)) {
                    new_route.ulid = ulid;
                    new_route.version = null;
                }
            }

            // And with version!
            if (version != null) {
                version = version.trim();

                if (version.match(/^[0-9]+$/)) {
                    new_route.version = parseInt(version, 10);
                } else if (!version.length) {
                    new_route.version = null;
                } else {
                    Log.error('L\'indicateur de version n\'est pas un nombre');
                    new_route.version = null;
                }
            }

            let panels = url.searchParams.get('p');
            if (panels) {
                panels = panels.split('|');
                panels = panels.filter(key => app.panels.hasOwnProperty(key));

                UI.setPanels(panels);
                explicit_panels = true;
            }
        }

        // Match requested page, record type, available page, etc.
        // We may need to restart in some cases, hence the fake loop to do it with continue.
        for (;;) {
            new_record = form_record;
            new_values = form_values;

            // Get to the record damn it
            if (new_record != null && options.reload) {
                new_route.version = null;
                new_record = null;
            }
            if (new_record != null) {
                let version = new_record.historical ? new_record.version : null;
                let mismatch = (new_route.ulid !== new_record.ulid ||
                                new_route.version !== version);

                if (mismatch)
                    new_record = null;
            }
            if (new_record == null && new_route.ulid != null)
                new_record = await loadRecord(new_route.ulid, new_route.version, !goupile.isLocked());
            if (new_record != null && new_record.form !== new_route.form)
                new_record = await moveToAppropriateRecord(new_record, new_route.form, true);
            if (new_route.ulid == null || new_record == null)
                new_record = await createRecord(new_route.form, new_route.ulid);

            // Load close records (parents, siblings, children)
            let load = new_route.page.getOption('load', new_record, []);
            await expandRecord(new_record, load);

            // Safety checks
            if (profile.lock != null && !new_record.chain.some(record => record.ulid === profile.lock.ulid))
                throw new Error('Enregistrement non autorisé');
            if (!isPageEnabled(new_route.page, new_record)) {
                new_route.page = findEnabledPage(new_route.form, new_record);
                if (new_route.page == null)
                    throw new Error('Cette page n\'est pas activée pour cet enregistrement');

                if (new_route.page.form !== new_route.form) {
                    new_route.form = new_route.page.form;
                    continue;
                }
            }

            // Confirm dangerous actions (at risk of data loss)
            if (!options.reload && !options.force) {
                if (hasUnsavedData() && (new_record !== form_record || 
                                         new_route.page !== route.page)) {
                    let autosave = route.page.getOption('autosave', form_record, false);

                    if (autosave || profile.userid < 0) {
                        if (!autosave)
                            form_builder.triggerErrors();
                        await mutex.chain(() => saveRecord(form_record, new_hid, form_values, route.page));

                        new_route.version = null;
                        options.reload = true;

                        continue;
                    } else {
                        try {
                            await UI.dialog(e, 'Enregistrer (confirmation)', {}, (d, resolve, reject) => {
                                d.output(html`Si vous continuez, vos <b>modifications seront enregistrées</b>.`);

                                d.enumRadio('action', 'Que souhaitez-vous faire avant de continuer ?', [
                                    [true, "Enregistrer mes modifications"],
                                    [false, "Oublier mes modifications"]
                                ], { value: true, untoggle: false });

                                if (d.values.action) {
                                    d.action('Enregistrer', {}, async e => {
                                        try {
                                            form_builder.triggerErrors();
                                            await mutex.chain(() => saveRecord(form_record, new_hid, form_values, route.page));
                                        } catch (err) {
                                            reject(err);
                                        }

                                        new_route.version = null;
                                        options.reload = true;

                                        resolve();
                                    });
                                } else {
                                    d.action('Oublier', {}, resolve);
                                }
                            });

                            if (options.reload)
                                continue;
                        } catch (err) {
                            if (err != null)
                                Log.error(err);

                            // If we're popping state, this will fuck up navigation history but we can't
                            // refuse popstate events. History mess is better than data loss.
                            await mutex.chain(run);
                            return;
                        }
                    }
                }
            }

            break;
        }

        // Dictionaries
        new_dictionaries = {};
        {
            let dictionaries = new_route.page.getOption('dictionaries', new_record, []);

            for (let dict of dictionaries) {
                let records = form_dictionaries[dict];
                if (records == null)
                    records = await loadRecords(null, dict);
                new_dictionaries[dict] = records;
            }
        }

        // Fetch and cache page code for page panel
        {
            let filename = new_route.page.getOption('filename', new_record);
            await fetchCode(filename);
        }

        // Help the user fill new or selected forms and pages
        if (form_record != null && (new_record.ulid !== form_record.ulid ||
                                    new_route.page !== route.page)) {
            setTimeout(() => {
                let el = document.querySelector('#ins_page');
                if (el != null)
                    el.parentNode.scrollTop = 0;
            }, 0);

            if (!explicit_panels && !UI.isPanelActive('view') && app.panels.view) {
                UI.togglePanel('data', false);
                UI.togglePanel('view', true);
            }
        } else if (new_record.chain[0].saved) {
            if (!explicit_panels && !UI.isPanelActive('view') && app.panels.view) {
                UI.togglePanel('data', false);
                UI.togglePanel('view', true);
            }
        }

        // Commit!
        updateContext(new_route, new_record);
        form_dictionaries = new_dictionaries;

        await mutex.chain(() => run(options.push_history));
    });
}

function findEnabledPage(form, record, parents = true) {
    for (let item of form.menu) {
        if (item.type === 'page') {
            if (isPageEnabled(item.page, record))
                return item.page;
        } else if (item.type === 'form') {
            let page = findEnabledPage(item.form, record, false);
            if (page != null)
                return page;
        }
    }

    if (parents && form.chain.length >= 2) {
        let parent = form.chain[form.chain.length - 2];
        return findEnabledPage(parent, record);
    }

    return null;
}

function isFormEnabled(form, record, reality = false) {
    for (let page of form.pages.values()) {
        if (isPageEnabled(page, record, reality))
            return true;
    }
    for (let child of form.forms.values()) {
        if (isFormEnabled(child, record, reality))
            return true;
    }

    return false;
}

function isPageEnabled(page, record, reality = false) {
    if (!reality && profile.develop)
        return true;

    let restrict = page.getOption('restrict', record, false);
    let enabled = page.getOption('enabled', record, true);

    if (goupile.isLocked() && restrict)
        return false;

    return enabled;
}

async function run(push_history = true) {
    await mutex.run(async () => {
        // Fetch and cache page code for page panel
        // Again to make sure we are up to date (e.g. publication)
        let filename = route.page.getOption('filename', form_record);
        await fetchCode(filename);

        // Sync editor (if needed)
        if (UI.isPanelActive('editor')) {
            if (editor_filename !== 'main.js')
                editor_filename = filename;

            await syncEditor();
        }

        // Load rows for data panel
        if (UI.isPanelActive('data')) {
            if (data_form !== form_record.chain[0].form) {
                data_form = form_record.chain[0].form;
                data_rows = null;
            }

            if (data_rows == null) {
                data_rows = await loadRecords(null, data_form.key);
                data_rows.sort(Util.makeComparator(meta => meta.hid, navigator.language, {
                    numeric: true,
                    ignorePunctuation: true,
                    sensitivity: 'base'
                }));
            }
        }

        // Update URL and title
        {
            let url = contextualizeURL(route.page.url, form_record);

            let panels;
            if (app.panels.data + app.panels.view < 2) {
                panels = null;
            } else if (url.match(/\/[A-Z0-9]{26}(@[0-9]+)?$/)) {
                panels = UI.getPanels().join('|');

                if (panels === 'view')
                    panels = null;
            } else {
                panels = UI.getPanels().join('|');

                if (panels === 'data')
                    panels = null;
            }

            url = Util.pasteURL(url, { p: panels });
            goupile.syncHistory(url, push_history);

            document.title = `${route.page.title} — ${ENV.title}`;
        }

        // Don't mess with the editor when render accidently triggers a scroll event!
        ignore_page_scroll = performance.now();
        UI.draw();
    });
};

function updateContext(new_route, new_record) {
    let copy_ui = (new_route.page === route.page);

    route = new_route;
    route.ulid = new_record.ulid;
    if (route.version != null)
        route.version = new_record.version;

    if (new_record !== form_record) {
        form_record = new_record;

        let new_state = new FormState(new_record.values);

        new_state.changeHandler = handleStateChange;
        if (form_state != null && copy_ui) {
            new_state.state_tabs = form_state.state_tabs;
            new_state.state_sections = form_state.state_sections;
            if (new_record.saved && new_record.ulid == form_record.ulid)
                new_state.take_delayed = form_state.take_delayed;
        }
        form_state = new_state;

        form_values = {};
        form_values[new_record.form.key] = form_state.values;
    }

    new_hid = new_record.hid;

    if (new_record !== form_record || !copy_ui) {
        if (autosave_timer != null)
            clearTimeout(autosave_timer);
        autosave_timer = null;
    }
}

async function handleStateChange() {
    await run();

    let autosave = route.page.getOption('autosave', form_record, false);

    if (autosave) {
        if (autosave_timer != null)
            clearTimeout(autosave_timer);

        if (typeof autosave !== 'number')
            autosave = 5000;

        autosave_timer = setTimeout(Util.serialize(async () => {
            autosave_timer = null;

            if (hasUnsavedData()) {
                try {
                    let prev_state = form_state;
                    let prev_values = form_values;

                    await mutex.chain(() => saveRecord(form_record, new_hid, form_values, route.page, true));

                    // Avoid unwanted value changes while user is doing stuff
                    form_state = prev_state;
                    form_values = prev_values;
                } catch (err) {
                    Log.error(err);
                }

                run();
            }
        }, mutex), autosave);
    }

    // Highlight might need to change (conditions, etc.)
    if (UI.isPanelActive('editor'))
        syncFormHighlight(false);
}

function contextualizeURL(url, record, default_ctx = '') {
    while (record != null && !record.saved)
        record = record.parent;

    if (record != null) {
        url += `/${record.ulid}`;
        if (record.historical)
            url += `@${record.version}`;
    } else {
        url += default_ctx;
    }

    return url;
}

async function fetchCode(filename) {
    let code = null;

    if (profile.develop) {
        // Anything in cache or in the editor?
        {
            let buffer = code_buffers.get(filename);

            if (buffer != null)
                return buffer.code;
        }

        // Try locally saved files
        if (code == null) {
            let key = `${profile.userid}:${filename}`;
            let file = await db.load('fs_changes', key);

            if (file != null) {
                if (file.blob != null) {
                    code = await file.blob.text();
                } else {
                    code = '';
                }
            }
        }
    }

    // The server is our last hope
    if (code == null) {
        let response = await Net.fetch(`${ENV.urls.files}${filename}`);

        if (response.ok) {
            code = await response.text();
        } else {
            code = '';
        }
    }

    // Create or update buffer
    {
        let buffer = code_buffers.get(filename);

        if (buffer == null) {
            let sha256 = mixer.Sha256(code);

            buffer = {
                code: code,
                sha256: sha256,
                orig_sha256: sha256,
                session: null
            };
            code_buffers.set(filename, buffer);
        } else {
            let sha256 = mixer.Sha256(code);

            if (buffer.session != null && sha256 !== buffer.sha256) {
                ignore_editor_change = true;
                buffer.session.doc.setValue(code);
                ignore_editor_change = false;
            }

            buffer.code = code;
            buffer.sha256 = sha256;
        }
    }

    return code;
}

function fileHasChanged(filename) {
    let buffer = code_buffers.get(filename);

    if (buffer != null) {
        let changed = (buffer.sha256 !== buffer.orig_sha256);
        return changed;
    } else {
        return false;
    }
}

async function createRecord(form, ulid = null, parent_record = null) {
    let record = {
        form: form,
        ulid: ulid || Util.makeULID(),
        hid: null,
        version: 0,
        historical: false,
        ctime: null,
        mtime: null,
        fragments: [],
        status: {},
        saved: false,
        values: {},
        tags: [],

        parent: null,
        children: {},
        chain: null, // Will be set later
        map: null, // Will be set later
        forms: null, // Will be set later
        siblings: null // Same, for multi-children only
    };
    record.ctime = new Date(Util.decodeULIDTime(record.ulid));

    if (form.chain.length > 1) {
        if (parent_record == null)
            parent_record = await createRecord(form.chain[form.chain.length - 2]);
        record.parent = parent_record;

        if (form.multi) {
            record.siblings = await listChildren(record.parent.ulid, record.form.key);
            record.siblings.sort(Util.makeComparator(record => record.ulid));
        }
    }

    return record;
}

async function loadRecord(ulid, version, error_missing = false) {
    let key = profile.namespaces.records + `:${ulid}`;
    let obj = await db.load('rec_records', key);

    if (obj != null) {
        let record = await decryptRecord(obj, version, false);

        if (record.form.multi && record.parent != null) {
            record.siblings = await listChildren(record.parent.ulid, record.form.key);
            record.siblings.sort(Util.makeComparator(record => record.ulid));
        }

        return record;
    } else if (error_missing) {
        throw new Error('L\'enregistrement demandé n\'existe pas');
    } else {
        return null;
    }
}

async function expandRecord(record, load_children = []) {
    // Load record parents
    if (record.chain == null) {
        let chain = [];
        let map = {};

        record.chain = chain;
        record.map = map;
        record.forms = map;
        chain.push(record);
        map[record.form.key] = record;

        let it = record;
        while (it.parent != null) {
            let parent = it.parent;

            if (parent.values == null) {
                parent = await loadRecord(it.parent.ulid, null);
                parent.historical = record.historical; // XXX
            }

            parent.chain = chain;
            parent.map = map;
            parent.forms = map;
            chain.push(parent);
            map[parent.form.key] = parent;
            it.parent = parent;

            it = parent;
        }

        chain.reverse();
    }

    // Load children (if requested)
    for (let key of load_children) {
        let form = app.forms.get(key);

        try {
            let child = await moveToAppropriateRecord(record, form, false);

            if (child != null) {
                if (form.multi) {
                    let children = await loadRecords(child.parent.ulid, form.key);
                    for (let child of children)
                        child.historical = record.historical; // XXX
                    record.map[key] = children;
                } else {
                    child.historical = record.historical; // XXX
                    record.map[key] = child;
                }
            } else {
                record.map[key] = null;
            }
        } catch (err) {
            record.map[key] = null;
            console.log(err);
        }
    }
}

async function loadRecords(parent_ulid = null, form_key = null)
{
    let objects;
    if (parent_ulid != null && form_key != null) {
        let range = IDBKeyRange.bound(profile.namespaces.records + `:${parent_ulid}/${form_key}`,
                                      profile.namespaces.records + `:${parent_ulid}/${form_key}\``, false, true);
        objects = await db.loadAll('rec_records/parent', range);
    } else if (parent_ulid != null) {
        let range = IDBKeyRange.bound(profile.namespaces.records + `:${parent_ulid}/`,
                                      profile.namespaces.records + `:${parent_ulid}\``, false, true);
        objects = await db.loadAll('rec_records/parent', range);
    } else if (form_key != null) {
        let range = IDBKeyRange.only(profile.namespaces.records + `/${form_key}`);
        objects = await db.loadAll('rec_records/form', range);
    }

    let records = [];
    for (let obj of objects) {
        try {
            let record = await decryptRecord(obj, null, false);
            records.push(record);
        } catch (err) {
            console.log(err);
        }
    }

    return records;
}

async function listChildren(parent_ulid, form_key = null)
{
    let keys;
    if (form_key != null) {
        let range = IDBKeyRange.bound(profile.namespaces.records + `:${parent_ulid}/${form_key}`,
                                      profile.namespaces.records + `:${parent_ulid}/${form_key}\``, false, true);
        keys = await db.list('rec_records/parent', range);
    } else {
        let range = IDBKeyRange.bound(profile.namespaces.records + `:${parent_ulid}/`,
                                      profile.namespaces.records + `:${parent_ulid}\``, false, true);
        keys = await db.list('rec_records/parent', range);
    }

    let children = [];
    for (let key of keys) {
        let child = {
            form: key.index.substr(key.index.indexOf('/') + 1),
            ulid: key.primary.substr(key.primary.indexOf(':') + 1),
            ctime: null,
            mtime: null
        };

        child.ctime = new Date(Util.decodeULIDTime(child.ulid));
        if (child.form.includes('@')) {
            let pos = child.form.indexOf('@');
            child.mtime = new Date(parseInt(child.form.substr(pos + 1), 10));
            child.form = child.form.substr(0, pos);
        }
        if (child.mtime == null || isNaN(child.mtime))
            child.mtime = new Date(child.ctime);

        children.push(child);
    }

    return children;
}

async function decryptRecord(obj, version, allow_deleted) {
    let historical = (version != null);

    let entry = await decryptSymmetric(obj.enc, ['records', 'lock']);
    let fragments = entry.fragments;

    let form = app.forms.get(entry.form);
    if (form == null)
        throw new Error(`Le formulaire '${entry.form}' n'existe pas ou plus`);

    if (version == null) {
        version = fragments.length;
    } else if (version > fragments.length) {
        throw new Error(`Cet enregistrement n'a pas de version ${version}`);
    }

    let values = {};
    let tags = [];
    let status = {};
    for (let i = 0; i < version; i++) {
        let fragment = fragments[i];

        if (fragment.type === 'save') {
            Object.assign(values, fragment.values);

            if (form.pages.get(fragment.page)) {
                if (status[fragment.page] == null) {
                    status[fragment.page] = {
                        ctime: new Date(fragment.mtime),
                        mtime: null,
                        tags: tags
                    };
                }

                status[fragment.page].mtime = new Date(fragment.mtime);
            }
        }

        if (fragment.tags != null) {
            tags.length = 0;
            tags.push(...fragment.tags);
        }
    }
    for (let fragment of fragments)
        fragment.mtime = new Date(fragment.mtime);

    if (!allow_deleted && fragments.length && fragments[fragments.length - 1].type === 'delete')
        throw new Error('L\'enregistrement demandé est supprimé');

    let children_map = {};
    {
        let children = await listChildren(entry.ulid);

        for (let child of children) {
            let array = children_map[child.form];
            if (array == null) {
                array = [];
                children_map[child.form] = array;
            }

            array.push(child);

            status[child.form] = {
                ctime: child.ctime,
                mtime: child.mtime,
                tags: await getTags(child.ulid)
            };
        }
    }

    let record = {
        form: form,
        ulid: entry.ulid,
        hid: entry.hid,
        version: version,
        historical: historical,
        ctime: new Date(Util.decodeULIDTime(entry.ulid)),
        mtime: fragments.length ? fragments[version - 1].mtime : null,
        fragments: fragments,
        status: status,
        saved: true,
        values: values,
        tags: tags,

        parent: entry.parent,
        children: children_map,
        chain: null, // Will be set later
        map: null, // Will be set later
        siblings: null // Same, for multi-children only
    };

    return record;
}

async function moveToAppropriateRecord(record, target_form, create_new) {
    let path = computePath(record.form, target_form);

    if (path != null) {
        for (let form of path.up) {
            record = record.parent;
            if (record.values == null)
                record = await loadRecord(record.ulid, null);

            if (record.form !== form)
                throw new Error('Saut impossible en raison d\'un changement de schéma');
        }

        for (let i = 0; i < path.down.length; i++) {
            let form = path.down[i];
            let follow = !form.multi || !create_new;

            if (follow && record.children[form.key] != null) {
                let children = record.children[form.key];
                let child = children[children.length - 1];

                record = await loadRecord(child.ulid, null);
                if (record.form !== form)
                    throw new Error('Saut impossible en raison d\'un changement de schéma');
            } else if (create_new) {
                record = await createRecord(form, null, record);
            } else {
                return null;
            }
        }

        return record;
    } else {
        return null;
    }
}

async function getTags(ulid) {
    let tags = await db.load('rec_tags', ulid);
    if (tags == null)
        tags = [];
    return tags;
}

// Assumes from != to
function computePath(from, to) {
    let prefix_len = 0;
    while (prefix_len < from.chain.length && prefix_len < to.chain.length) {
        let parent1 = from.chain[prefix_len];
        let parent2 = to.chain[prefix_len];

        if (parent1 !== parent2)
            break;

        prefix_len++;
    }
    prefix_len--;

    if (from.chain[prefix_len] === to) {
        let path = {
            up: from.chain.slice(prefix_len, from.chain.length - 1).reverse(),
            down: []
        };
        return path;
    } else if (to.chain[prefix_len] === from) {
        let path = {
            up: [],
            down: to.chain.slice(prefix_len + 1)
        };
        return path;
    } else if (prefix_len >= 0) {
        let path = {
            up: from.chain.slice(prefix_len, from.chain.length - 1).reverse(),
            down: to.chain.slice(prefix_len + 1)
        };
        return path;
    } else {
        return null;
    }
}

function isReadOnly(record) {
    if (record.historical)
        return true;
    if (!goupile.hasPermission('data_save'))
        return true;

    return false;
}

async function backupRecords() {
    let progress = Log.progress('Archivage sécurisé des données');

    try {
        let entries = [];
        {
            let range = IDBKeyRange.bound(profile.namespaces.records + ':', profile.namespaces.records + '`', false, true);
            let objects = await db.loadAll('rec_records', range);

            for (let obj of objects) {
                try {
                    let entry = await decryptSymmetric(obj.enc, ['records', 'lock']);
                    entries.push(entry);
                } catch (err) {
                    console.log(err);
                }
            }
        }

        let enc = await encryptBackup(entries);
        let json = JSON.stringify(enc);
        let blob = new Blob([json], {type: 'application/octet-stream'});

        let filename = `${ENV.urls.instance.replace(/\//g, '')}_${profile.username}_${dates.today()}.backup`;
        Util.saveBlob(blob, filename);

        console.log('Archive sécurisée créée');
        progress.close();
    } catch (err) {
        progress.close();
        throw err;
    }
}

async function syncRecords(standalone = true, full = false) {
    await mutex.run(async () => {
        let progress = standalone ? Log.progress('Synchronisation en cours') : null;

        try {
            let saved = false;
            let loaded = false;

            // Upload new fragments
            {
                let range = IDBKeyRange.only(profile.namespaces.records);
                let objects = await db.loadAll('rec_records/sync', range);

                let uploads = [];
                let buckets = {};
                let ulids = new Set;
                for (let obj of objects) {
                    try {
                        let record = await decryptRecord(obj, null, true);

                        let upload = {
                            form: record.form.key,
                            ulid: record.ulid,
                            hid: record.hid,
                            parent: record.parent,
                            fragments: record.fragments.map((fragment, idx) => ({
                                type: fragment.type,
                                mtime: fragment.mtime.toISOString(),
                                fs: (fragment.fs != null) ? fragment.fs : ENV.version, // Use ENV.version for transition,
                                                                                       // will be removed eventually
                                page: fragment.page,
                                json: JSON.stringify(fragment.values),
                                tags: JSON.stringify(fragment.tags || [])
                            }))
                        };

                        if (upload.parent == null) {
                            uploads.push(upload);
                        } else {
                            let bucket = buckets[upload.parent.ulid];
                            if (bucket == null) {
                                bucket = [];
                                buckets[upload.parent.ulid] = bucket;
                            }
                            bucket.push(upload);
                        }
                        ulids.add(record.ulid);
                    } catch (err) {
                        console.log(err);
                    }
                }

                // For the topological sort to be correct we first need to find records
                // that are not real roots but are parents of other records that we
                // want to upload.
                // We append to the array as we go, and I couldn't make sure that
                // a for of loop is okay with that (even though it probably is).
                // So instead I use a dumb increment to be sure it works correctly.
                for (let ulid in buckets) {
                    if (!ulids.has(ulid)) {
                        uploads.push(...buckets[ulid]);
                        delete buckets[ulid];
                    }
                }
                for (let i = 0; i < uploads.length; i++) {
                    let upload = uploads[i];
                    let bucket = buckets[upload.ulid];

                    if (bucket != null) {
                        uploads.push(...bucket);
                        delete buckets[upload.ulid];
                    }
                }
                for (let ulid in buckets) {
                    let bucket = buckets[ulid];
                    uploads.push(...bucket);
                }

                if (uploads.length) {
                    let url = `${ENV.urls.instance}api/records/save`;
                    let response = await Net.fetch(url, {
                        method: 'POST',
                        body: JSON.stringify(uploads),
                        timeout: 30000
                    });

                    if (!response.ok) {
                        let err = await Net.readError(response);
                        throw new Error(err);
                    }

                    saved = true;
                }
            }

            // Download new fragments
            {
                let range = IDBKeyRange.bound(profile.namespaces.records + '@',
                                              profile.namespaces.records + '`', false, true);
                let [, anchor] = await db.limits('rec_records/anchor', range);

                if (!full && anchor != null) {
                    anchor = anchor.toString();
                    anchor = anchor.substr(anchor.indexOf('@') + 1);
                    anchor = parseInt(anchor, 10);
                } else {
                    anchor = 0;
                }

                let url = Util.pasteURL(`${ENV.urls.instance}api/records/load`, {
                    anchor: anchor
                });
                let downloads = await Net.get(url, {
                    timeout: 120000
                });

                for (let download of downloads) {
                    let key = profile.namespaces.records + `:${download.ulid}`;
                    let mtime = download.fragments.length ? (new Date(download.fragments[download.fragments.length - 1].mtime)).getTime() : null;

                    let obj = {
                        keys: {
                            form: profile.namespaces.records + `/${download.form}`,
                            parent: (download.parent != null) ? (profile.namespaces.records + `:${download.parent.ulid}/${download.form}@${mtime}`) : null,
                            anchor: profile.namespaces.records + `@${(download.anchor + 1).toString().padStart(16, '0')}`,
                            sync: null
                        },
                        enc: null
                    };
                    if (download.fragments.length && download.fragments[download.fragments.length - 1].type == 'delete') {
                        obj.keys.form = null;
                        obj.keys.parent = null;
                    }

                    let entry = {
                        ulid: download.ulid,
                        hid: download.hid,
                        form: download.form,
                        parent: download.parent,
                        fragments: download.fragments.map(fragment => {
                            anchor = Math.max(anchor, fragment.anchor + 1);

                            let frag = {
                                anchor: fragment.anchor,
                                type: fragment.type,
                                user: fragment.username,
                                mtime: new Date(fragment.mtime),
                                fs: fragment.fs,
                                page: fragment.page,
                                values: fragment.values,
                                tags: fragment.tags
                            };
                            return frag;
                        })
                    };

                    obj.enc = await encryptSymmetric(entry, 'records');
                    await db.saveWithKey('rec_records', key, obj);

                    if (entry.fragments.length && entry.fragments[entry.fragments.length - 1].tags != null) {
                        let tags = entry.fragments[entry.fragments.length - 1].tags;
                        await db.saveWithKey('rec_tags', entry.ulid, tags);
                    } else {
                        await db.delete('rec_tags', entry.ulid);
                    }

                    loaded = true;
                }

                // Detect changes from other tabs
                if (prev_anchor != null && anchor !== prev_anchor)
                    loaded = true;
                prev_anchor = anchor;
            }

            if (loaded && standalone) {
                if (form_record != null || saved) {
                    progress.success('Synchronisation effectuée');
                } else {
                    progress.close();
                }

                if (form_record != null) {
                    data_rows = null;

                    let reload = form_record.saved && !form_state.hasChanged();
                    go(null, window.location.href, { reload: reload });
                }
            } else {
                if (standalone)
                    progress.close();
            }
        } catch (err) {
            if (standalone)
                progress.close();
            throw err;
        }
    });
}

function encryptSymmetric(obj, ns) {
    let key = goupile.profileKeys[ns];
    if (key == null)
        throw new Error(`Cannot encrypt without '${ns}' key`);

    return encryptSecretBox(obj, key);
}

function decryptSymmetric(enc, namespaces) {
    let last_err;

    for (let ns of namespaces) {
        let key = goupile.profileKeys[ns];

        if (key == null)
            continue;

        try {
            let obj = decryptSecretBox(enc, key);
            return obj;
        } catch (err) {
            last_err = err;
        }
    }

    throw last_err || new Error(`Cannot encrypt without valid namespace key`);
}

function encryptBackup(obj) {
    if (ENV.backup_key == null)
        throw new Error('This instance is not configured for offline backups');

    let key = goupile.profileKeys.records;
    if (key == null)
        throw new Error('Cannot encrypt backup without local key');

    let backup_key = mixer.Base64.toBytes(ENV.backup_key);
    return encryptBox(obj, backup_key, key);
};

function encryptSecretBox(obj, key) {
    let nonce = new Uint8Array(24);
    crypto.getRandomValues(nonce);

    let json = JSON.stringify(obj, (k, v) => v != null ? v : null);
    let message = (new TextEncoder()).encode(json);
    let box = nacl.secretbox(message, nonce, key);

    let enc = {
        format: 2,
        nonce: mixer.Base64.toBase64(nonce),
        box: mixer.Base64.toBase64(box)
    };
    return enc;
}

function decryptSecretBox(enc, key) {
    let nonce = mixer.Base64.toBytes(enc.nonce);
    let box = mixer.Base64.toBytes(enc.box);

    let message = nacl.secretbox.open(box, nonce, key);
    if (message == null)
        throw new Error('Failed to decrypt message: wrong key?');

    let json;
    if (enc.format >= 2) {
        json = (new TextDecoder()).decode(message);
    } else {
        json = window.atob(mixer.Base64.toBase64(message));
    }
    let obj = JSON.parse(json);

    return obj;
}

function encryptBox(obj, public_key, secret_key) {
    let nonce = new Uint8Array(24);
    crypto.getRandomValues(nonce);

    let json = JSON.stringify(obj, (k, v) => v != null ? v : null);
    let message = (new TextEncoder()).encode(json);
    let box = nacl.box(message, nonce, public_key, secret_key);

    let enc = {
        format: 2,
        nonce: mixer.Base64.toBase64(nonce),
        box: mixer.Base64.toBase64(box)
    };
    return enc;
}

export {
    init,
    hasUnsavedData,
    runTasks,
    go
}
