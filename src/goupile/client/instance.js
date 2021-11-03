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

function InstanceController() {
    let self = this;

    let db;
    let app;

    // Explicit mutex to serialize (global) state-changing functions
    let mutex = new Mutex;

    let route = {
        form: null,
        page: null,
        ulid: null,
        version: null
    };

    let head_length = Number.MAX_SAFE_INTEGER;
    let page_div = document.createElement('div');

    let form_record;
    let form_state;
    let form_builder;
    let form_values;
    let form_dictionaries = {};

    let editor_el;
    let editor_ace;
    let editor_filename;
    let code_buffers = new LruMap(32);

    let develop = false;
    let error_entries = {};

    let ignore_editor_change = false;
    let ignore_editor_scroll = false;
    let ignore_page_scroll = false;
    let autosave_timer;

    let data_form;
    let data_rows;
    let data_filter;

    let prev_anchor;

    this.init = async function() {
        await openInstanceDB();
        await initApp();
        initUI();
    };

    async function openInstanceDB() {
        let db_name = `goupile:${ENV.urls.instance}`;
        db = await indexeddb.open(db_name, 9, (db, old_version) => {
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
            }
        });
    }

    async function initApp() {
        let code = await fetchCode('main.js');

        try {
            let new_app = new ApplicationInfo;
            let builder = new ApplicationBuilder(new_app);

            await runCodeAsync('Application', code, {
                app: builder
            });
            if (!new_app.pages.size)
                throw new Error('Main script does not define any page');

            new_app.home = new_app.pages.values().next().value;
            app = util.deepFreeze(new_app);
        } catch (err) {
            if (app == null) {
                let new_app = new ApplicationInfo;
                let builder = new ApplicationBuilder(new_app);

                // For simplicity, a lot of code assumes at least one page exists
                builder.form("default", "Défaut", "Page par défaut");

                new_app.home = new_app.pages.values().next().value;
                app = util.deepFreeze(new_app);
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
        ui.setMenu(renderMenu);
        ui.createPanel('editor', 0, false, renderEditor);
        ui.createPanel('data', 0, app.panels.data, renderData);
        ui.createPanel('view', 1, app.panels.view && !app.panels.data, renderPage);
    };

    this.hasUnsavedData = function() {
        if (form_state == null)
            return false;

        return form_state.hasChanged();
    };

    this.runTasks = async function(online) {
        if (online && ENV.sync_mode !== 'offline')
            await mutex.chain(syncRecords);
    };
    this.runTasks = util.serialize(this.runTasks, mutex);

    function renderMenu() {
        let historical = (route.version < form_record.fragments.length);

        return html`
            ${app.panels.editor ? html`
                <button class=${'icon' + (ui.isPanelEnabled('editor') ? ' active' : '')}
                        style="background-position-y: calc(-230px + 1.2em);"
                        @click=${ui.wrapAction(e => togglePanel(e, 'editor'))}>Code</button>
            ` : ''}
            ${app.panels.data ? html`
                <button class=${'icon' + (ui.isPanelEnabled('data') ? ' active' : '')}
                        style="background-position-y: calc(-274px + 1.2em);"
                        @click=${ui.wrapAction(e => togglePanel(e, 'data'))}>Liste</button>
            ` : ''}
            ${app.panels.view ? html`
                <button class=${'icon' + (ui.isPanelEnabled('view') ? ' active' : '')}
                        style="background-position-y: calc(-318px + 1.2em);"
                        @click=${ui.wrapAction(e => togglePanel(e, 'view'))}>Saisie</button>
            ` : ''}
            ${profile.lock != null && profile.lock.unlockable ? html`
                <button class="icon" style="background-position-y: calc(-186px + 1.2em)"
                        @click=${ui.wrapAction(goupile.runUnlockDialog)}>Déverrouiller</button>
            ` : ''}
            <div style="flex: 1; min-width: 15px;"></div>

            ${app.panels.data && form_record.chain[0].saved && form_record.chain[0].hid != null ? html`
                <button class="ins_hid" @click=${ui.wrapAction(e => runTrailDialog(e, route.ulid))}>
                    ${form_record.chain[0].form.title} <span style="font-weight: bold;">#${form_record.chain[0].hid}</span>
                    ${historical ? html`<br/><span style="font-size: 0.7em;">${form_record.ctime.toLocaleString()}</span>` : ''}
                </button>
            ` : ''}
            <div class="drop">
                <button @click=${ui.deployMenu}>${route.page.title}</button>
                <div>
                    ${util.mapRange(0, route.form.chain.length - 1, idx => renderFormDrop(route.form.chain[idx]))}
                    ${profile.lock == null && route.form.multi ? html`
                        <div class="expand">
                            <button @click=${ui.expandMenu}>${route.form.multi}</button>
                            <div>
                                ${form_record.siblings.map(sibling => {
                                    let url = route.page.url + `/${sibling.ulid}`;
                                    return html`<button @click=${ui.wrapAction(e => self.go(e, url))}
                                                        class=${sibling.ulid === form_record.ulid ? 'active' : ''}>${sibling.ctime.toLocaleString()}</a>`;
                                })}
                                <button @click=${ui.wrapAction(e => self.go(e, contextualizeURL(route.page.url, form_record.parent)))}
                                        class=${!form_record.saved ? 'active' : ''}>Nouvelle fiche</button>
                            </div>
                        </div>
                    ` : ''}
                    ${route.form.chain.length === 1 || route.form.menu.length > 1 ? renderFormDrop(route.form) : ''}
                </div>
            </div>
            <div style="flex: 1; min-width: 15px;"></div>

            ${!goupile.isLocked() && profile.instances == null ?
                html`<button class="icon" style="background-position-y: calc(-538px + 1.2em);"
                             @click=${e => self.go(e, ENV.urls.instance)}>${ENV.title}</button>` : ''}
            ${!goupile.isLocked() && profile.instances != null ? html`
                <div class="drop right" @click=${ui.deployMenu}>
                    <button class="icon" style="background-position-y: calc(-538px + 1.2em);"
                            @click=${ui.deployMenu}>${ENV.title}</button>
                    <div>
                        ${profile.instances.map(instance =>
                            html`<button class=${instance.url === ENV.urls.instance ? 'active' : ''}
                                         @click=${e => self.go(e, instance.url)}>${instance.name}</button>`)}
                    </div>
                </div>
            ` : ''}
            ${profile.lock == null ? html`
                <div class="drop right">
                    <button class="icon" style=${'background-position-y: calc(-' + (goupile.isLoggedOnline() ? 450 : 494) + 'px + 1.2em);'}
                            @click=${ui.deployMenu}>${profile.type !== 'auto' ? profile.username : ''}</button>
                    <div>
                        ${profile.type === 'auto' && profile.userid ? html`
                            <button style="text-align: center;">
                                ${profile.username}<br/>
                                <span style="font-size: 0.8em; font-style: italic; color: #555;">Identifiant temporaire</span>
                            </button>
                            <hr/>
                        ` : ''}
                        ${profile.type === 'login' ? html`
                            <button @click=${ui.wrapAction(goupile.runChangePasswordDialog)}>Changer le mot de passe</button>
                            <button @click=${ui.wrapAction(goupile.runResetTOTP)}>Changer les codes TOTP</button>
                            <hr/>
                            ${app.lockable ? html`<button @click=${ui.wrapAction(e => goupile.runLockDialog(e, form_record.chain[0].ulid))}>Verrouiller</button>` : ''}
                        ` : ''}
                        ${profile.admin ? html`
                            <button @click=${e => window.open('/admin/')}>Administration</button>
                            <hr/>
                        ` : ''}
                        <button @click=${ui.wrapAction(goupile.logout)}>Se déconnecter</button>
                    </div>
                </div>
            ` : ''}
            ${profile.lock != null ?
                html`<button class="icon" @click=${ui.wrapAction(goupile.goToLogin)}
                             style="background-position-y: calc(-450px + 1.2em);">Se connecter</button>` : ''}
        `;
    }

    function renderFormDrop(form) {
        let meta = form_record.map[form.key];

        let items= [];
        let active_item;
        for (let item of form.menu) {
            if (item.type === 'page') {
                if (!isPageEnabled(item.page, form_record))
                    continue;

                if (item.page === route.page)
                    active_item = item;

                items.push(item);
            } else if (item.type === 'form') {
                if (!isFormEnabled(item.form, form_record))
                    continue;

                if (route.form.chain.some(parent => item.form === parent))
                    active_item = item;

                items.push(item);
            } else {
                throw new Error(`Unknown item type '${item.type}'`);
            }
        }

        if (items.length > 1) {
            let active_item = form.menu.find(item => {
                if (item.type === 'page') {
                    return item.page === route.page;
                } else if (item.type === 'form') {
                    return route.form.chain.some(parent => item.form === parent);
                }
            });

            return html`
                <div class="expand">
                    <button @click=${ui.expandMenu}>${form.title}</button>
                    <div>
                        ${util.map(form.menu, item => {
                            let active;
                            let url;
                            let title;
                            let status;

                            if (item.type === 'page') {
                                let page = item.page;

                                if (!isPageEnabled(page, form_record))
                                    return '';

                                active = (page === route.page);
                                url = page.url;
                                title = page.title;
                                status = (meta != null) && (meta.status[form.key] != null);
                            } else if (item.type === 'form') {
                                let form = item.form;

                                if (!isFormEnabled(form, form_record))
                                    return '';

                                active = route.form.chain.some(parent => form === parent);
                                url = form.url;
                                title = form.multi || form.title;
                                status = (meta != null) && (meta.status[form.key] != null);
                            } else {
                                throw new Error(`Unknown item type '${item.type}'`);
                            }

                            return html`
                                <button class=${active ? 'active' : ''} @click=${ui.wrapAction(e => self.go(e, url))}>
                                    <div style="flex: 1;">${title}</div>
                                    ${status ? html`<div>&nbsp;✓\uFE0E</div>` : ''}
                               </button>
                            `;
                        })}
                    </div>
                </div>
            `;
        } else {
            return '';
        }
    }

    async function togglePanel(e, key) {
        let enable = !ui.isPanelEnabled(key);
        ui.setPanelState(key, enable);

        await self.run();

        if (enable && key === 'view') {
            syncFormScroll();
            syncFormHighlight(true);
        } else if (key === 'editor') {
            if (enable)
                syncEditorScroll();
            syncFormHighlight(false);
        }
    }

    function renderEditor() {
        let tabs = [];
        tabs.push({
            title: 'Application',
            filename: 'main.js'
        });
        tabs.push({
            title: 'Formulaire',
            filename: route.page.filename
        });

        // Ask ACE to adjust if needed, it needs to happen after the render
        setTimeout(() => editor_ace.resize(false), 0);

        return html`
            <div style="--menu_color: #1d1d1d; --menu_color_n1: #2c2c2c;">
                <div class="ui_toolbar">
                    ${tabs.map(tab => html`<button class=${editor_filename === tab.filename ? 'active' : ''}
                                                   @click=${ui.wrapAction(e => toggleEditorFile(tab.filename))}>${tab.title}</button>`)}
                    ${editor_filename === 'main.js' ? html`
                        <div style="flex: 1;"></div>
                        <button ?disabled=${!fileHasChanged('main.js')}
                                @click=${ui.wrapAction(applyMainScript)}>Appliquer</button>
                    ` : ''}
                    <div style="flex: 1;"></div>
                    <button @click=${ui.wrapAction(runPublishDialog)}>Publier</button>
                </div>

                ${editor_el}
            </div>
        `;
    }

    function fileHasChanged(filename) {
        let buffer = code_buffers.get(filename);

        if (buffer != null) {
            return buffer.sha256 !== buffer.orig_sha256;
        } else {
            return false;
        }
    }

    async function applyMainScript() {
        let code = await fetchCode('main.js');

        let new_app = new ApplicationInfo;
        let builder = new ApplicationBuilder(new_app);

        try {
            await runCodeAsync('Application', code, {
                app: builder
            });
            if (!new_app.pages.size)
                throw new Error('Main script does not define any page');
        } catch (err) {
            // Don't log, because runCodeAsync does it already
            return;
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

        let column_stats = {};
        let hid_width = 40;
        for (let row of visible_rows) {
            for (let key of data_form.pages.keys())
                column_stats[key] = (column_stats[key] || 0) + (row.status[key] != null);
            for (let key of data_form.forms.keys())
                column_stats[key] = (column_stats[key] || 0) + (row.status[key] != null);

            hid_width = Math.max(hid_width, computeHidWidth(row.hid));
        }

        return html`
            <div class="padded">
                <div class="ui_actions" style="margin-bottom: 1.2em;">
                    <button @click=${ui.wrapAction(e => self.go(e, route.form.chain[0].url + '/new'))}>Ajouter un ${data_form.title.toLowerCase()}</button>
                </div>

                <div class="ui_quick" style=${visible_rows.length ? 'margin-right: 2.2em;' : ''}>
                    <input type="text" placeholder="Filtrer..." .value=${data_filter || ''}
                           @input=${e => { data_filter = e.target.value || null; self.run(); }} />
                    <div style="flex: 1;"></div>
                    <p>
                        ${visible_rows.length} ${visible_rows.length < data_rows.length ? `/ ${data_rows.length} ` : ''} ${data_rows.length > 1 ? 'lignes' : 'ligne'}
                        ${ENV.sync_mode !== 'offline' ? html`(<a @click=${ui.wrapAction(e => syncRecords())}>synchroniser</a>)` : ''}
                    </p>
                </div>

                <table class="ui_table fixed" id="ins_data"
                       style=${'min-width: ' + (5 + 5 * data_form.menu.length) + 'em;'}>
                    <colgroup>
                        <col style=${'width: ' + hid_width + 'px;'} />
                        ${util.mapRange(0, data_form.menu.length, () => html`<col/>`)}
                        <col style="width: 2em;"/>
                    </colgroup>

                    <thead>
                        <tr>
                            <th>ID</th>
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
                                    ${row.form.menu.map(item => {
                                        if (item.type === 'page') {
                                            let page = item.page;
                                            let url = page.url + `/${row.ulid}`;

                                            if (row.status[page.key] != null) {
                                                return html`<td class=${active && page === route.page ? 'saved active' : 'saved'}
                                                                title=${row.status[page.key].toLocaleString()}><a href=${url}>${row.status[page.key].toLocaleDateString()}</a></td>`;
                                            } else {
                                                return html`<td class=${active && page === route.page ? 'missing active' : 'missing'}
                                                                title=${item.title}><a href=${url}>Afficher</a></td>`;
                                            }
                                        } else if (item.type === 'form') {
                                            let form = item.form;

                                            if (row.status[form.key] != null) {
                                                let child = row.children[form.key][0];
                                                let url = form.url + `/${child.ulid}`;

                                                return html`<td class=${active && route.form.chain.includes(form) ? 'saved active' : 'saved'}
                                                                title=${item.title}><a href=${url}>${row.status[form.key].toLocaleDateString()}</a></td>`;
                                            } else {
                                                let url = form.url + `/${row.ulid}`;

                                                return html`<td class=${active && route.form.chain.includes(form) ? 'missing active' : 'missing'}
                                                                title=${item.title}><a href=${url}>Afficher</a></td>`;
                                            }
                                        }
                                    })}
                                    ${row.saved ? html`<th><a @click=${ui.wrapAction(e => runDeleteRecordDialog(e, row.ulid))}>✕</a></th>` : ''}
                                </tr>
                            `;
                        })}
                        ${!visible_rows.length ? html`<tr><td colspan=${1 + data_form.menu.length}>Aucune ligne à afficher</td></tr>` : ''}
                    </tbody>
                </table>

                <div class="ui_quick">
                    ${goupile.hasPermission('data_export') ? html`
                        <a href=${ENV.urls.instance + 'api/records/export'} download>Exporter les données</a>
                    ` : ''}
                    <div style="flex: 1;"></div>
                    ${ENV.backup_key != null ? html`
                        <a @click=${ui.wrapAction(backupRecords)}>Faire une sauvegarde chiffrée</a>
                    ` : ''}
                </div>
            </div>
        `;
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
        return ui.runConfirm(e, 'Voulez-vous vraiment supprimer cet enregistrement ?',
                                'Supprimer', () => deleteRecord(ulid));
    }

    function renderPage() {
        let code = code_buffers.get(route.page.filename).code;

        let readonly = (route.version < form_record.fragments.length);

        let model = new FormModel;
        form_builder = new FormBuilder(form_state, model, readonly);

        try {
            // Don't mess with the editor when render accidently triggers a scroll event!
            ignore_page_scroll = true;

            form_builder.pushOptions({});

            let meta = Object.assign({}, form_record);
            runCodeSync('Formulaire', code, {
                app: app,

                form: form_builder,
                meta: meta,
                values: form_state.values,

                nav: {
                    form: route.form,
                    page: route.page,
                    go: (url) => self.go(null, url),

                    save: async () => {
                        await saveRecord(form_record, form_values, route.page);
                        await self.run();
                    },
                    delete: (e, ulid, confirm = true) => {
                        if (confirm) {
                            runDeleteRecordDialog(e, ulid);
                        } else {
                            deleteRecord(ulid);
                        }
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
                }
            });
            form_record.hid = meta.hid;

            form_builder.popOptions({});

            if (model.hasErrors())
                form_builder.errorList();

            if (route.page.options.default_actions && model.variables.length) {
                let prev_actions = model.actions;
                model.actions = [];

                form_builder.action('Enregistrer', {disabled: !form_state.hasChanged()}, async () => {
                    form_builder.triggerErrors();

                    await saveRecord(form_record, form_values, route.page);

                    self.run();
                });

                if (!goupile.isLocked()) {
                    if (form_state.just_triggered) {
                        form_builder.action('-');

                        form_builder.action('Forcer l\'enregistrement', {}, async e => {
                            await ui.runConfirm(e, html`Confirmez-vous l'enregistrement <b>malgré la présence d'erreurs</b> ?`,
                                                   'Enregistrer', () => {});

                            await saveRecord(form_record, form_values, route.page);

                            self.run();
                        });
                        form_builder.action('Effacer les modifications', {}, async e => {
                            await ui.runConfirm(e, html`Souhaitez-vous réellement <b>annuler les modifications en cours</b> ?`,
                                                   'Effacer', () => {});

                            if (form_record.saved) {
                                let record = await loadRecord(form_record.ulid, form_record.version);
                                await expandRecord(record, route.page.options.load || []);

                                updateContext(route, record);
                            } else {
                                let record = Object.assign({}, form_record);
                                record.values = {};

                                updateContext(route, record);
                            }

                            self.run();
                        });
                    }

                    if (route.form.multi && form_record.saved) {
                        form_builder.action('-');
                        form_builder.action('Supprimer', {}, e => runDeleteRecordDialog(e, form_record.ulid));
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

            ignore_page_scroll = false;
        }

        let historical = (route.version < form_record.fragments.length);

        return html`
            <div class="print" @scroll=${syncEditorScroll}}>
                <div id="ins_page">
                    <div id="ins_menu">
                        ${util.mapRange(0, route.form.chain.length - 1, idx => renderFormMenu(route.form.chain[idx]))}
                        ${profile.lock == null && route.form.multi ? html`
                            <h1>${route.form.multi}</h1>
                            <ul>
                                ${form_record.siblings.map(sibling => {
                                    let url = route.page.url + `/${sibling.ulid}`;
                                    return html`<li><a href=${url} class=${sibling.ulid === form_record.ulid ? 'active' : ''}>${sibling.ctime.toLocaleString()}</a></li>`;
                                })}
                                <li><a href=${contextualizeURL(route.page.url, form_record.parent)} class=${!form_record.saved ? 'active' : ''}>Nouvelle fiche</a></li>
                            </ul>
                        ` : ''}
                        ${route.form.chain.length === 1 || route.form.menu.length > 1 ? renderFormMenu(route.form) : ''}
                    </div>

                    <form id="ins_form" autocomplete="off" @submit=${e => e.preventDefault()}>
                        ${page_div}
                    </form>

                    <div id="ins_actions">
                        ${model.renderActions()}
                    </div>
                </div>
                <div style="flex: 1;"></div>

                ${develop ? html`
                    <div id="ins_notice">
                        Formulaires en développement<br/>
                        Publiez les avant d'enregistrer des données
                    </div>
                ` : ''}

                <nav class="ui_toolbar" id="ins_tasks" style="z-index: 999999;">
                    ${!goupile.isLocked() && form_record.chain[0].saved && form_record.chain[0].hid != null ? html`
                        <button class="ins_hid" style=${historical ? 'color: #00ffff;' : ''}
                                @click=${ui.wrapAction(e => runTrailDialog(e, route.ulid))}>
                            ${form_record.chain[0].form.title} <span style="font-weight: bold;">#${form_record.chain[0].hid}</span>
                            ${historical ? ' (historique)' : ''}
                        </button>
                    ` : ''}
                    <div style="flex: 1;"></div>
                    ${model.actions.length > 1 ? html`
                        <div class="drop up right">
                            <button @click=${ui.deployMenu}>Autres</button>
                            <div>
                                ${util.mapRange(1, model.actions.length, idx => {
                                    let action = model.actions[idx];
                                    return action.render();
                                })}
                            </div>
                        </div>
                        <hr/>
                    ` : ''}
                    ${model.actions.length ? model.actions[0].render() : ''}
                </nav>
            </div>
        `;
    }

    function renderFormMenu(form) {
        let meta = form_record.map[form.key];

        return html`
            <h1>${form.title}</h1>
            <ul>
                ${util.map(form.menu, item => {
                    if (item.type === 'page') {
                        let page = item.page;

                        let cls = '';
                        if (!isPageEnabled(page, form_record)) {
                            if (goupile.isLocked())
                                return '';

                            cls = 'disabled';
                        } else if (page === route.page) {
                            cls = 'active';
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
                        if (!isFormEnabled(form, form_record)) {
                            if (goupile.isLocked())
                                return '';

                            cls = 'disabled';
                        } else if (route.form.chain.some(parent => form === parent)) {
                            cls = 'active';
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
        `;
    }

    async function runCodeAsync(title, code, arguments) {
        let entry = error_entries[title];
        if (entry == null) {
            entry = new log.Entry;
            error_entries[title] = entry;
        }

        try {
            let AsyncFunction = Object.getPrototypeOf(async function(){}).constructor;

            let func = new AsyncFunction(...Object.keys(arguments), code);
            await func(...Object.values(arguments));

            entry.close();
        } catch (err) {
            let line = util.parseEvalErrorLine(err);
            let msg = `Erreur sur ${title}\n${line != null ? `Ligne ${line} : ` : ''}${err.message}`;

            entry.error(msg, -1);
            throw new Error(msg);
        }
    }

    function runCodeSync(title, code, arguments) {
        let entry = error_entries[title];
        if (entry == null) {
            entry = new log.Entry;
            error_entries[title] = entry;
        }

        try {
            let func = new Function(...Object.keys(arguments), code);
            func(...Object.values(arguments));

            entry.close();
        } catch (err) {
            let line = util.parseEvalErrorLine(err);
            let msg = `Erreur sur ${title}\n${line != null ? `Ligne ${line} : ` : ''}${err.message}`;

            entry.error(msg, -1);
            throw new Error(msg);
        }
    }

    function runTrailDialog(e, ulid) {
        return ui.runDialog(e, 'Historique', {}, (d, resolve, reject) => {
            d.output(html`
                <table class="ui_table">
                    <colgroup>
                        <col/>
                        <col/>
                        <col style="min-width: 12em;"/>
                    </colgroup>

                    <tbody>
                        ${!form_record.fragments.length ? html`<tr><td colspan="3">Aucune modification enregistrée</td></tr>` : ''}
                        ${util.mapRange(0, form_record.fragments.length, idx => {
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

    async function saveRecord(record, values, page) {
        await mutex.run(async () => {
            if (develop)
                throw new Error('Enregistrement refusé : formulaire non publié');

            let progress = log.progress('Enregistrement en cours');

            try {
                let ulid = record.ulid;
                let page_key = page.key;
                let ptr = values[record.form.key];

                await db.transaction('rw', ['rec_records'], async t => {
                    do {
                        let fragment;
                        if (ptr != null) {
                            fragment = {
                                type: 'save',
                                user: profile.username,
                                mtime: new Date,
                                page: page_key,
                                values: ptr
                            };
                        } else {
                            fragment = null;
                        }

                        let key = profile.namespaces.records + `:${record.ulid}`;
                        let obj = await t.load('rec_records', key);

                        let entry;
                        if (obj != null) {
                            entry = await goupile.decryptSymmetric(obj.enc, 'records');
                            if (record.hid != null)
                                entry.hid = record.hid;
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
                                hid: record.hid,
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

                        // Always rewrite keys to fix potential namespace changes
                        obj.keys.form = `${profile.namespaces.records}/${record.form.key}`;
                        if (record.parent != null)
                            obj.keys.parent = `${profile.namespaces.records}:${record.parent.ulid}/${record.form.key}`;
                        obj.keys.sync = profile.namespaces.records;

                        if (record.version !== entry.fragments.length)
                            throw new Error('Cannot overwrite old record fragment');
                        if (fragment != null)
                            entry.fragments.push(fragment);

                        obj.enc = await goupile.encryptSymmetric(entry, 'records');
                        await t.saveWithKey('rec_records', key, obj);

                        do {
                            page_key = record.form.key;
                            record = record.parent;
                            if (record == null)
                                break;
                            ptr = form_values[record.form.key];
                        } while (record.saved && ptr == null);
                    } while (record != null);
                });

                data_rows = null;

                if (ENV.sync_mode !== 'offline') {
                    try {
                        if (!goupile.isLoggedOnline())
                            throw new Error('Cannot save online');

                        await mutex.chain(() => syncRecords(false));
                        progress.success('Enregistrement effectué');
                    } catch (err) {
                        progress.info('Enregistrement local effectué');
                        enablePersistence();
                    }
                } else {
                    progress.success('Enregistrement local effectué');
                    enablePersistence();
                }

                record = await loadRecord(ulid, null);
                await expandRecord(record, page.options.load || []);

                updateContext(route, record);
            } catch (err) {
                progress.close();
                throw err;
            }
        });
    }

    async function deleteRecord(ulid) {
        await mutex.run(async () => {
            let progress = log.progress('Suppression en cours');

            try {
                // XXX: Delete children (cascade)?
                await db.transaction('rw', ['rec_records'], async t => {
                    let key = profile.namespaces.records + `:${ulid}`;
                    let obj = await t.load('rec_records', key);

                    if (obj == null)
                        throw new Error('Cet enregistrement est introuvable');

                    let entry = await goupile.decryptSymmetric(obj.enc, 'records');

                    // Mark as deleted with special fragment
                    let fragment = {
                        type: 'delete',
                        user: profile.username,
                        mtime: new Date
                    };
                    entry.fragments.push(fragment);

                    obj.keys.parent = null;
                    obj.keys.form = null;
                    obj.keys.sync = profile.namespaces.records;

                    obj.enc = await goupile.encryptSymmetric(entry, 'records');
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

                            self.go(null, url, { force: true });
                        } else {
                            self.go(null, route.form.chain[0].url + '/new', { force: true });
                        }
                    } else {
                        self.go();
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
                await net.loadScript(`${ENV.urls.static}ace.js`);

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
                if (ui.isPanelEnabled('view'))
                    setTimeout(syncFormScroll, 0);
            });
            session.selection.on('changeSelection', () => {
                syncFormHighlight(true);
                ignore_editor_scroll = true;
            });

            buffer.session = session;
        }

        if (editor_filename.startsWith('pages/')) {
            editor_ace.setTheme('ace/theme/merbivore_soft');
        } else {
            editor_ace.setTheme('ace/theme/monokai');
        }

        editor_ace.setSession(buffer.session);
    }

    function toggleEditorFile(filename) {
        editor_filename = filename;
        return self.run();
    }

    async function handleFileChange(filename) {
        if (ignore_editor_change)
            return;

        let buffer = code_buffers.get(filename);
        let code = buffer.session.doc.getValue();

        // Should never fail, but who knows..
        if (buffer != null) {
            let key = `${profile.userid}:${filename}`;
            let blob = new Blob([code]);
            let sha256 = await goupile.computeSha256(blob);

            await db.saveWithKey('fs_files', key, {
                filename: filename,
                size: blob.size,
                sha256: sha256,
                blob: blob
            });

            buffer.code = code;
            buffer.sha256 = sha256;
        }

        self.run();
    }

    function syncFormScroll() {
        if (!ui.isPanelEnabled('editor') || !ui.isPanelEnabled('view'))
            return;
        if (ignore_editor_scroll) {
            ignore_editor_scroll = false;
            return;
        }

        try {
            let panel_el = document.querySelector('#ins_page').parentNode;
            let widget_els = panel_el.querySelectorAll('*[data-line]');

            let editor_line = editor_ace.getFirstVisibleRow() + 1;

            let prev_line;
            for (let i = 0; i < widget_els.length; i++) {
                let line = parseInt(widget_els[i].dataset.line, 10);

                if (line >= editor_line) {
                    if (!i) {
                        ignore_page_scroll = true;
                        panel_el.scrollTop = 0;
                    } else if (i === widget_els.length - 1) {
                        let top = computeRelativeTop(panel_el, widget_els[i]);

                        ignore_page_scroll = true;
                        panel_el.scrollTop = top;
                    } else {
                        let top1 = computeRelativeTop(panel_el, widget_els[i - 1]);
                        let top2 = computeRelativeTop(panel_el, widget_els[i]);
                        let frac = (editor_line - prev_line) / (line - prev_line);

                        ignore_page_scroll = true;
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
        if (!ui.isPanelEnabled('view'))
            return;

        try {
            let panel_el = document.querySelector('#ins_page').parentNode;
            let widget_els = panel_el.querySelectorAll('*[data-line]');

            if (ui.isPanelEnabled('editor') && widget_els.length) {
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
                        ignore_page_scroll = true;
                        panel_el.scrollTop += rect.top - 50;
                    } else if (rect.bottom >= window.innerHeight) {
                        ignore_page_scroll = true;
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
        if (!ui.isPanelEnabled('editor') || !ui.isPanelEnabled('view'))
            return;
        if (ignore_page_scroll) {
            ignore_page_scroll = false;
            return;
        }

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
                        ignore_editor_scroll = true;
                        editor_ace.renderer.scrollToLine(0);
                    } else {
                        let frac = -prev_top / (top - prev_top);
                        let line2 = Math.floor(prev_line + frac * (line - prev_line));

                        ignore_editor_scroll = true;
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
        let publisher = new InstancePublisher(self, db);
        await publisher.runDialog(e);

        self.run();
    }

    this.go = async function(e, url = null, options = {}) {
        options = Object.assign({ push_history: true }, options);

        let new_route = Object.assign({}, route);
        let new_record;
        let new_dictionaries;
        let new_code;
        let explicit_panels = false;

        // Parse new URL
        if (url != null) {
            url = new URL(url, window.location.href);
            if (url.pathname === ENV.urls.instance)
                url = new URL(app.home.url, window.location.href);

            if (!url.pathname.endsWith('/'))
                url.pathname += '/';

            // Goodbye!
            if (!url.pathname.startsWith(`${ENV.urls.instance}main/`)) {
                if (self.hasUnsavedData())
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

            // Find page information
            new_route.page = app.pages.get(key);
            if (new_route.page == null) {
                log.error(`La page '${key}' n'existe pas`);
                new_route.page = app.home;
            }
            new_route.form = new_route.page.form;

            let [ulid, version] = what ? what.split('@') : [null, null];

            // Popping history
            if (!ulid && !options.push_history)
                ulid = 'new';

            // Go to default record?
            if (!ulid && !app.panels.data) {
                if (profile.lock != null) {
                    ulid = profile.lock.ctx;
                } else {
                    let range = IDBKeyRange.only(profile.namespaces.records + `/${new_route.form.key}`);
                    let keys = await db.list('rec_records/form', range);

                    if (keys.length) {
                        let key = keys[0];
                        ulid = key.primary.substr(key.primary.indexOf(':') + 1);
                    }
                }
            }

            // Deal with ULID
            if (ulid && ulid !== new_route.ulid) {
                if (ulid === 'new')
                    ulid = null;

                new_route.ulid = ulid;
                new_route.version = null;
            }

            // And with version!
            if (version) {
                version = version.trim();

                if (version.match(/^[0-9]+$/)) {
                    new_route.version = parseInt(version, 10);
                } else {
                    log.error('L\'indicateur de version n\'est pas un nombre');
                    new_route.version = null;
                }
            }

            let panels = url.searchParams.get('p');
            if (panels) {
                panels = panels.split('|');
                panels = panels.filter(key => app.panels[key]);

                ui.setEnabledPanels(panels);
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
            if (new_record != null && (new_route.ulid !== new_record.ulid ||
                                       new_route.version !== new_record.version))
                new_record = null;
            if (new_record == null && new_route.ulid != null)
                new_record = await loadRecord(new_route.ulid, new_route.version, !goupile.isLocked());
            if (new_record != null && new_record.form !== new_route.form)
                new_record = await moveToAppropriateRecord(new_record, new_route.form, true);
            if (new_route.ulid == null || new_record == null)
                new_record = createRecord(new_route.form, new_route.ulid);

            // Load close records (parents, siblings, children)
            await expandRecord(new_record, new_route.page.options.load || []);

            // Safety checks
            if (profile.lock != null && !new_record.chain.some(record => record.ulid === profile.lock.ctx))
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
                if (self.hasUnsavedData() && (new_record !== form_record || 
                                              new_route.page !== route.page)) {
                    if (route.page.options.autosave) {
                        await mutex.chain(() => saveRecord(form_record, form_values, route.page));
                        new_route.version = null;

                        options.reload = true;
                        continue;
                    } else {
                        try {
                            await ui.runConfirm(e, html`Si vous continuez, vos <b>modifications seront enregistrées</b>. Voulez-vous enregistrer ?`,
                                                   "Enregistrer", async () => {
                                form_builder.triggerErrors();

                                await mutex.chain(() => saveRecord(form_record, form_values, route.page));
                                new_route.version = null;
                            });

                            options.reload = true;
                            continue;
                        } catch (err) {
                            if (err != null)
                                log.error(err);

                            // If we're popping state, this will fuck up navigation history but we can't
                            // refuse popstate events. History mess is better than data loss.
                            await mutex.chain(self.run);
                            return;
                        }
                    }
                }
            }

            break;
        }

        // Fetch and cache page code for page panel
        await fetchCode(new_route.page.filename);

        // Dictionaries
        new_dictionaries = {};
        if (new_route.page.options.dictionaries != null) {
            for (let dict of new_route.page.options.dictionaries) {
                let records = form_dictionaries[dict];
                if (records == null)
                    new_dictionaries[dict] = await loadRecords(null, dict);
                new_dictionaries[dict] = records;
            }
        }

        // Help the user fill new or selected forms and pages
        if (form_record != null && (new_record.ulid !== form_record.ulid ||
                                    new_route.page !== route.page)) {
            setTimeout(() => {
                let el = document.querySelector('#ins_page');
                if (el != null)
                    el.parentNode.scrollTop = 0;
            }, 0);

            if (!explicit_panels && !ui.isPanelEnabled('view') && app.panels.view) {
                ui.setPanelState('view', true);
                ui.setPanelState('data', false);
            }
        } else if (form_record == null && new_record.chain[0].saved) {
            if (!explicit_panels && !ui.isPanelEnabled('view') && app.panels.view) {
                ui.setPanelState('view', true);
                ui.setPanelState('data', false);
            }
        }

        // Commit!
        updateContext(new_route, new_record);
        form_dictionaries = new_dictionaries;

        await mutex.chain(() => self.run(options.push_history));
    };
    this.go = util.serialize(this.go, mutex);

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

    function isFormEnabled(form, record) {
        for (let page of form.pages.values()) {
            if (isPageEnabled(page, record))
                return true;
        }
        for (let child of form.forms.values()) {
            if (isFormEnabled(child, record))
                return true;
        }

        return false;
    }

    function isPageEnabled(page, record) {
        if (goupile.isLocked() && !page.options.lockable)
            return false;

        if (typeof page.options.enabled === 'function') {
            try {
                return page.options.enabled(record);
            } catch (err) {
                let line = util.parseEvalErrorLine(err);
                let msg = `Erreur\n${line != null ? `Ligne ${line} : ` : ''}${err.message}`;
                log.error(msg);

                return false;
            }
        } else if (page.options.enabled != null) {
            return page.options.enabled;
        } else {
            return true;
        }
    }

    this.run = async function(push_history = true) {
        // Is the user developing?
        {
            let range = IDBKeyRange.bound(profile.userid + ':', profile.userid + '`', false, true);
            let count = await db.count('fs_files', range);

            develop = !!count;
        }

        // Fetch and cache page code for page panel
        // Again to make sure we are up to date (e.g. publication)
        await fetchCode(route.page.filename);

        // Sync editor (if needed)
        if (ui.isPanelEnabled('editor')) {
            if (editor_filename == null || editor_filename.startsWith('pages/'))
                editor_filename = route.page.filename;

            await syncEditor();
        }

        // Load rows for data panel
        if (ui.isPanelEnabled('data')) {
            if (data_form !== form_record.chain[0].form) {
                data_form = form_record.chain[0].form;
                data_rows = null;
            }

            if (data_rows == null) {
                data_rows = await loadRecords(null, data_form.key);
                data_rows.sort(util.makeComparator(meta => meta.hid, navigator.language, {
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
            if (app.panels.editor + app.panels.data + app.panels.view < 2) {
                panels = null;
            } else if (url.match(/\/[A-Z0-9]{26}(@[0-9]+)?$/)) {
                panels = ui.getEnabledPanels().join('|');

                if (panels === 'view')
                    panels = null;
            } else {
                panels = ui.getEnabledPanels().join('|');

                if (panels === 'data')
                    panels = null;
            }

            url = util.pasteURL(url, { p: panels });
            goupile.syncHistory(url, push_history);

            document.title = `${route.page.title} — ${ENV.title}`;
        }

        ui.render();
    };
    this.run = util.serialize(this.run, mutex);

    function updateContext(new_route, new_record) {
        let copy_ui = (new_route.page === route.page);

        route = new_route;
        route.ulid = new_record.ulid;
        route.version = new_record.version;

        if (new_record !== form_record) {
            form_record = new_record;

            let new_state = new FormState(new_record.values);
            new_state.changeHandler = handleStateChange;
            if (form_state != null && copy_ui) {
                new_state.state_tabs = form_state.state_tabs;
                new_state.state_sections = form_state.state_sections;
            }
            form_state = new_state;

            form_values = {};
            form_values[new_record.form.key] = form_state.values;
        }

        if (new_record !== form_record || !copy_ui) {
            if (autosave_timer != null)
                clearTimeout(autosave_timer);
            autosave_timer = null;
        }
    }

    async function handleStateChange() {
        await self.run();

        if (route.page.options.autosave) {
            if (autosave_timer != null)
                clearTimeout(autosave_timer);

            let timeout;
            if (typeof route.page.options.autosave === 'number') {
                timeout = route.page.options.autosave;
            } else {
                timeout = 5000;
            }

            autosave_timer = setTimeout(util.serialize(async () => {
                if (self.hasUnsavedData()) {
                    try {
                        await mutex.chain(() => saveRecord(form_record, form_values, route.page));
                    } catch (err) {
                        log.error(err);
                    }

                    self.run();
                }

                autosave_timer = null;
            }, mutex), timeout);
        }

        // Highlight might need to change (conditions, etc.)
        if (ui.isPanelEnabled('editor'))
            syncFormHighlight(false);
    }

    function contextualizeURL(url, record, default_ctx = '') {
        while (record != null && !record.saved)
            record = record.parent;

        if (record != null) {
            url += `/${record.ulid}`;
            if (record.version < record.fragments.length)
                url += `@${record.version}`;
        } else {
            url += default_ctx;
        }

        return url;
    }

    async function fetchCode(filename) {
        let code = null;

        // Anything in cache or in the editor?
        {
            let buffer = code_buffers.get(filename);

            if (buffer != null && buffer.version === ENV.version)
                return buffer.code;
        }

        // Try locally saved files
        if (code == null) {
            let key = `${profile.userid}:${filename}`;
            let file = await db.load('fs_files', key);

            if (file != null) {
                if (file.blob != null) {
                    code = await file.blob.text();
                } else {
                    code = '';
                }
            }
        }

        // The server is our last hope
        if (code == null) {
            let response = await net.fetch(`${ENV.urls.files}${filename}`);

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
                let sha256 = Sha256(code);

                buffer = {
                    code: code,
                    version: ENV.version,
                    sha256: sha256,
                    orig_sha256: sha256,
                    session:null
                };
                code_buffers.set(filename, buffer);
            } else {
                let sha256 = Sha256(code);

                if (buffer.session != null && sha256 !== buffer.sha256) {
                    ignore_editor_change = true;
                    buffer.session.doc.setValue(code);
                    ignore_editor_change = false;
                }

                buffer.code = code;
                buffer.version = ENV.version;
                buffer.sha256 = sha256;
            }
        }

        return code;
    }

    function createRecord(form, ulid = null, parent_record = null) {
        let record = {
            form: form,
            ulid: ulid || util.makeULID(),
            hid: null,
            version: 0,
            ctime: null,
            mtime: null,
            fragments: [],
            status: new Set,
            saved: false,
            values: {},

            parent: null,
            children: {},
            chain: null, // Will be set later
            map: null, // Will be set later
            siblings: null // Same, for multi-children only
        };
        record.ctime = new Date(util.decodeULIDTime(record.ulid));

        if (form.chain.length > 1) {
            if (parent_record == null)
                parent_record = createRecord(form.chain[form.chain.length - 2]);
            record.parent = parent_record;
        }

        return record;
    }

    async function loadRecord(ulid, version, error_missing = false) {
        let key = profile.namespaces.records + `:${ulid}`;
        let obj = await db.load('rec_records', key);

        if (obj != null) {
            let record = await decryptRecord(obj, version, false);
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
            chain.push(record);
            map[record.form.key] = record;

            let it = record;
            while (it.parent != null) {
                let parent_record = it.parent;
                if (parent_record.values == null)
                    parent_record = await loadRecord(it.parent.ulid, null);

                parent_record.chain = chain;
                parent_record.map = map;
                chain.push(parent_record);
                map[parent_record.form.key] = parent_record;
                it.parent = parent_record;

                it = parent_record;
            }

            chain.reverse();
        }

        // Siblings (formMulti)
        if (record.form.multi && record.parent != null) {
            record.siblings = await listChildren(record.parent.ulid, record.form.key);
            record.siblings.sort(util.makeComparator(record => record.ulid));
        }

        // Load children (if requested)
        for (let key of load_children) {
            let form = app.forms.get(key);

            try {
                let child = await moveToAppropriateRecord(record, form, false);

                if (child != null) {
                    if (form.multi) {
                        record.map[key] = await loadRecords(child.parent.ulid, form.key);
                    } else {
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
            let range = IDBKeyRange.only(profile.namespaces.records + `:${parent_ulid}/${form_key}`);
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
            let range = IDBKeyRange.only(profile.namespaces.records + `:${parent_ulid}/${form_key}`);
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
                ctime: null
            };
            child.ctime = new Date(util.decodeULIDTime(child.ulid));

            children.push(child);
        }

        return children;
    }

    async function decryptRecord(obj, version, allow_deleted) {
        let entry = await goupile.decryptSymmetric(obj.enc, 'records');
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
        let status = {};
        for (let i = 0; i < version; i++) {
            let fragment = fragments[i];

            if (fragment.type === 'save') {
                Object.assign(values, fragment.values);

                if (form.pages.get(fragment.page) && status[fragment.page] == null)
                    status[fragment.page] = new Date(fragment.mtime);
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
                status[child.form] = child.ctime;
            }
        }

        let record = {
            form: form,
            ulid: entry.ulid,
            hid: entry.hid,
            version: version,
            ctime: new Date(util.decodeULIDTime(entry.ulid)),
            mtime: fragments.length ? fragments[version - 1].mtime : null,
            fragments: fragments,
            status: status,
            saved: true,
            values: values,

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
                    record = createRecord(form, null, record);
                } else {
                    return null;
                }
            }

            return record;
        } else {
            return null;
        }
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

    async function backupRecords() {
        let progress = log.progress('Archivage sécurisé des données');

        try {
            let entries = [];
            {
                let range = IDBKeyRange.bound(profile.namespaces.records + ':', profile.namespaces.records + '`', false, true);
                let objects = await db.loadAll('rec_records', range);

                for (let obj of objects) {
                    try {
                        let entry = await goupile.decryptSymmetric(obj.enc, 'records');
                        entries.push(entry);
                    } catch (err) {
                        console.log(err);
                    }
                }
            }

            let enc = await goupile.encryptBackup(entries);
            let json = JSON.stringify(enc);
            let blob = new Blob([json], {type: 'application/octet-stream'});

            let filename = `${ENV.urls.instance.replace(/\//g, '')}_${profile.username}_${dates.today()}.backup`;
            util.saveBlob(blob, filename);

            console.log('Archive sécurisée créée');
            progress.close();
        } catch (err) {
            progress.close();
            throw err;
        }
    }

    async function syncRecords(standalone = true) {
        await mutex.run(async () => {
            let progress = standalone ? log.progress('Synchronisation en cours') : null;

            try {
                let changed = false;

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
                                    page: fragment.page,
                                    json: JSON.stringify(fragment.values)
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
                        let response = await net.fetch(url, {
                            method: 'POST',
                            body: JSON.stringify(uploads)
                        });

                        if (!response.ok) {
                            let err = (await response.text()).trim();
                            throw new Error(err);
                        }

                        changed = true;
                    }
                }

                // Download new fragments
                {
                    let range = IDBKeyRange.bound(profile.namespaces.records + '@',
                                                  profile.namespaces.records + '`', false, true);
                    let [, anchor] = await db.limits('rec_records/anchor', range);

                    if (anchor != null) {
                        anchor = anchor.toString();
                        anchor = anchor.substr(anchor.indexOf('@') + 1);
                        anchor = parseInt(anchor, 10);
                    } else {
                        anchor = 0;
                    }

                    let url = util.pasteURL(`${ENV.urls.instance}api/records/load`, {
                        anchor: anchor
                    });
                    let downloads = await net.fetchJson(url, {
                        timeout: 30000
                    });

                    for (let download of downloads) {
                        let key = profile.namespaces.records + `:${download.ulid}`;

                        let obj = {
                            keys: {
                                form: profile.namespaces.records + `/${download.form}`,
                                parent: (download.parent != null) ? (profile.namespaces.records + `:${download.parent.ulid}/${download.form}`) : null,
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
                                    page: fragment.page,
                                    values: fragment.values
                                };
                                return frag;
                            })
                        };

                        obj.enc = await goupile.encryptSymmetric(entry, 'records');
                        await db.saveWithKey('rec_records', key, obj);

                        changed = true;
                    }

                    // Detect changes from other tabs
                    if (prev_anchor != null && anchor !== prev_anchor)
                        changed = true;
                    prev_anchor = anchor;
                }

                if (changed && standalone) {
                    progress.success('Synchronisation terminée');

                    // XXX: Keep current user value changes
                    if (form_record != null) {
                        data_rows = null;
                        self.go(null, window.location.href, { reload: true });
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
};
