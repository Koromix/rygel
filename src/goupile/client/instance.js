// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function InstanceController() {
    let self = this;

    let app;

    let route = {
        form: null,
        page: null,
        ulid: null,
        version: null
    };

    let code_cache = new LruMap(4);
    let page_code;
    let page_div = document.createElement('div');

    let form_record;
    let form_state;

    let editor_el;
    let editor_ace;
    let editor_filename;
    let editor_buffers = new LruMap(32);

    let ignore_editor_change = false;
    let ignore_editor_scroll = false;
    let ignore_page_scroll = false;

    let data_form;
    let data_rows;

    let head_length = Number.MAX_SAFE_INTEGER;
    let develop = false;
    let error_entries = {};

    this.start = async function() {
        initUI();
        await initApp();

        self.go(null, window.location.href).catch(err => {
            log.error(err);

            // Fall back to home page
            self.go(null, ENV.base_url);
        });
    };

    this.hasUnsavedData = function() {
        return form_state != null && form_state.hasChanged();
    };

    async function initApp() {
        let code = await fetchCode('main.js');

        try {
            let new_app = new ApplicationInfo;
            let builder = new ApplicationBuilder(new_app);

            runUserCode('Application', code, {
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
                builder.page("default", "Page par défaut");

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
        document.documentElement.className = 'instance';

        ui.setMenu(renderMenu);
        ui.createPanel('editor', 0, false, renderEditor);
        ui.createPanel('data', 0, !goupile.isLocked(), renderData);
        ui.createPanel('page', 1, goupile.isLocked(), renderPage);
    };

    function renderMenu() {
        return html`
            ${!goupile.isLocked() ? html`
                <button class="icon" style="background-position-y: calc(-538px + 1.2em);"
                        @click=${e => self.go(e, ENV.base_url)}>${ENV.title}</button>
                ${goupile.hasPermission('develop') ? html`
                    <button class=${'icon' + (ui.isPanelEnabled('editor') ? ' active' : '')}
                            style="background-position-y: calc(-230px + 1.2em);"
                            @click=${ui.wrapAction(e => togglePanel(e, 'editor'))}>Code</button>
                ` : ''}
                <button class=${'icon' + (ui.isPanelEnabled('data') ? ' active' : '')}
                        style="background-position-y: calc(-274px + 1.2em);"
                        @click=${ui.wrapAction(e => togglePanel(e, 'data'))}>Suivi</button>
                <button class=${'icon' + (ui.isPanelEnabled('page') ? ' active' : '')}
                        style="background-position-y: calc(-318px + 1.2em);"
                        @click=${ui.wrapAction(e => togglePanel(e, 'page'))}>Formulaire</button>
            ` : ''}

            <div style="flex: 1; min-width: 15px;"></div>
            ${util.mapRange(0, route.form.chain.length - 1, idx => {
                let form = route.form.chain[idx];
                return renderFormMenuDrop(form);
            })}
            ${renderFormMenuDrop(route.form)}
            ${route.form.menu.length > 1 ?
                html`<button class="active" @click=${ui.wrapAction(e => self.go(e, route.page.url))}>${route.page.title}</button>` : ''}
            <div style="flex: 1; min-width: 15px;"></div>

            ${!goupile.isLocked() ? html`
                <div class="drop right">
                    <button class="icon" style="background-position-y: calc(-494px + 1.2em)">${profile.username}</button>
                    <div>
                        <button @click=${ui.wrapAction(goupile.logout)}>Se déconnecter</button>
                    </div>
                </div>
            ` : ''}
        `;
    }

    function renderFormMenuDrop(form) {
        if (form.menu.length > 1) {
            return html`
                <div class="drop">
                    <button>${form.title}</button>
                    <div>
                        ${util.map(form.menu, item => {
                            if (item.type === 'page') {
                                let page = item.page;
                                return html`<button ?disabled=${!isPageEnabled(page, form_record)}
                                                    class=${page === route.page ? 'active' : ''}
                                                    @click=${ui.wrapAction(e => self.go(e, page.url))}>${page.title}</button>`;
                            } else if (item.type === 'form') {
                                let form = item.form;
                                return html`<button ?disabled=${!isFormEnabled(form, form_record)}
                                                    class=${route.form.chain.some(parent => form === parent) ? 'active' : ''}
                                                    @click=${ui.wrapAction(e => self.go(e, form.url))}>${form.title}</button>`;
                            }
                        })}
                    </div>
                </div>
            `;
        } else {
            return html`<button ?disabled=${!isFormEnabled(form, form_record)}
                                class=${route.form.chain.some(parent => form === parent) ? 'active' : ''}
                                @click=${ui.wrapAction(e => self.go(e, form.url))}>${form.title}</button>`;
        }
    }

    async function togglePanel(e, key) {
        let enable = !ui.isPanelEnabled(key);
        ui.setPanelState(key, enable);

        await self.run();

        if (enable && key === 'page') {
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
            <div style="--menu_color: #383936;">
                <div class="ui_toolbar">
                    ${tabs.map(tab => html`<button class=${editor_filename === tab.filename ? 'active' : ''}
                                                   @click=${ui.wrapAction(e => toggleEditorFile(tab.filename))}>${tab.title}</button>`)}
                    <div style="flex: 1;"></div>
                    <button @click=${ui.wrapAction(runDeployDialog)}>Publier</button>
                </div>

                ${editor_el}
            </div>
        `;
    }

    function renderData() {
        return html`
            <div class="padded">
                <div class="ui_quick">
                    <a @click=${ui.wrapAction(goNewRecord)}>Créer</a>
                    <div style="flex: 1;"></div>
                    ${ENV.backup_key != null ? html`
                        <a @click=${ui.wrapAction(e => backupClientData('file'))}>Faire une sauvegarde chiffrée</a>
                        <div style="flex: 1;"></div>
                    ` : ''}
                    ${data_rows.length || 'Aucune'} ${data_rows.length > 1 ? 'lignes' : 'ligne'}
                        (<a @click=${ui.wrapAction(e => { data_rows = null; return self.run(); })}>rafraichir</a>)
                </div>

                <table class="ui_table fixed" id="ins_data">
                    ${data_rows.length ? html`
                        <colgroup>
                            <col style="width: 160px;"/>
                            ${util.mapRange(0, data_form.menu.length, () => html`<col/>`)}
                            <col style="width: 2em;"/>
                        </colgroup>
                    ` : ''}

                    <tbody>
                        ${data_rows.map(row => renderDataRow(row, true))}
                        ${!data_rows.length ? html`<tr><td>Aucune ligne à afficher</td></tr>` : ''}
                    </tbody>
                </table>
            </div>
        `;
    }

    function renderDataRow(row, hid) {
        let active = form_record.chain.some(record => record.ulid === row.ulid);

        return html`
            <tr class=${active ? 'active' : ''}>
                ${hid ? html`<td class=${row.hid == null ? 'missing' : ''}>${row.hid != null ? row.hid : 'NA'}</td>` : ''}
                ${row.form.menu.map(item => {
                    if (item.type === 'page') {
                        let page = item.page;
                        let url = page.url + `/${row.ulid}`;

                        if (row.status.has(page.key)) {
                            return html`<td class=${active && page === route.page ? 'saved active' : 'saved'}><a href=${url}>${item.title}</a></td>`;
                        } else {
                            return html`<td class=${active && page === route.page ? 'missing active' : 'missing'}><a href=${url}>${item.title}</a></td>`;
                        }
                    } else if (item.type === 'form') {
                        let form = item.form;

                        if (row.status.has(form.key)) {
                            let child = row.children[form.key][0];
                            let url = form.url + `/${child.ulid}`;

                            return html`<td class=${active && form === route.form ? 'saved active' : 'saved'}><a href=${url}>${item.title}</a></td>`;
                        } else {
                            let url = form.url + `/${row.ulid}`;

                            return html`<td class=${active && form === route.form ? 'missing active' : 'missing'}><a href=${url}>${item.title}</a></td>`;
                        }
                    }
                })}
                <td>
                    <a @click=${ui.wrapAction(e => runDeleteRecordDialog(e, row.ulid))}>✕</a>
                </td>
            </tr>
            ${active && hid ? util.mapRange(1, route.form.chain.length, idx => {
                let form = route.form.chain[idx];
                let record = form_record.map[form.key];

                if (form !== route.form || form.menu.length > 1) {
                    return html`
                        <tr>
                            <th></th>
                            <td colspan=${1 + row.form.menu.length} style="padding: 0; border: 0;">
                                <table class="ui_table fixed">
                                    <colgroup>
                                        ${util.mapRange(0, form.menu.length, () => html`<col/>`)}
                                        <col style="width: 2em;"/>
                                    </colgroup>

                                    <tbody>
                                        ${renderDataRow(record, false)}
                                    </tbody>
                                </table>
                            </td>
                        </tr>
                    `;
                } else {
                    return '';
                }
            }) : ''}
        `;
    }

    function renderPage() {
        let readonly = (route.version < form_record.fragments.length);

        let model = new FormModel;
        let builder = new FormBuilder(form_state, model, readonly);

        try {
            builder.pushOptions({});

            let meta = Object.assign({}, form_record);
            runUserCode('Formulaire', page_code, {
                form: builder,
                values: form_state.values,
                meta: meta,
                nav: {
                    form: route.form,
                    page: route.page,
                    go: (url) => self.go(null, url),

                    isLocked: goupile.isLocked
                }
            });
            form_record.hid = meta.hid;

            builder.popOptions({});

            if (model.hasErrors())
                builder.errorList();

            if (model.variables.length) {
                let enable_save = !model.hasErrors() && form_state.hasChanged();

                builder.action('Enregistrer', {disabled: !enable_save}, () => {
                    if (builder.triggerErrors())
                        return saveRecord();
                });
                builder.action('-');
                builder.action('Nouveau', {}, goNewRecord);
            }

            render(model.render(), page_div);
            page_div.classList.remove('disabled');
        } catch (err) {
            if (!page_div.children.length)
                render('Impossible de générer la page à cause d\'une erreur', page_div);
            page_div.classList.add('disabled');
        }

        return html`
            <div class="print" @scroll=${syncEditorScroll}}>
                <div id="ins_page">
                    <div class="ui_quick">
                        ${model.variables.length ? html`
                            ${!form_record.chain[0].version ? 'Nouvel enregistrement' : ''}
                            ${form_record.chain[0].version > 0 && form_record.chain[0].hid != null ? `Enregistrement : ${form_record.chain[0].hid}` : ''}
                            ${form_record.chain[0].version > 0 && form_record.chain[0].hid == null ? 'Enregistrement local' : ''}
                        `  : ''}
                        (<a @click=${e => window.print()}>imprimer</a>)
                        <div style="flex: 1;"></div>
                        ${route.version > 0 ? html`
                            ${route.version < form_record.fragments.length ?
                                html`<span style="color: red;">Ancienne version du ${form_record.mtime.toLocaleString()}</span>` : ''}
                            ${route.version === form_record.fragments.length ? html`<span>Version actuelle</span>` : ''}
                            &nbsp;(<a @click=${ui.wrapAction(e => runTrailDialog(e, route.ulid))}>historique</a>)
                        ` : ''}
                    </div>

                    <form id="ins_form" @submit=${e => e.preventDefault()}>
                        ${page_div}
                    </form>
                </div>

                ${develop ? html`
                    <div style="flex: 1;"></div>
                    <div id="ins_notice">
                        Formulaires en développement<br/>
                        Publiez les avant d'enregistrer des données
                    </div>
                ` : ''}
            </div>
        `;
    }

    function runUserCode(title, code, arguments) {
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
        return ui.runDialog(e, (d, resolve, reject) => {
            if (ulid !== route.ulid)
                reject();

            d.output(html`
                <table class="ui_table">
                    <tbody>
                        ${util.mapRange(0, form_record.fragments.length, idx => {
                            let version = form_record.fragments.length - idx;
                            let fragment = form_record.fragments[version - 1];

                            let page = app.pages.get(fragment.page) || route.page;
                            let url = page.url + `/${ulid}@${version}`;

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

    function goNewRecord(e) {
        let url = route.form.chain[0].url + '/new';
        return self.go(e, url);
    }

    async function saveRecord() {
        if (develop)
            throw new Error('Enregistrement refusé : formulaire non publié');

        let progress = log.progress('Enregistrement en cours');

        try {
            let page = route.page;
            let record = form_record;
            let values = Object.assign({}, form_state.values);

            // Can't store undefined in IndexedDB...
            for (let key in values) {
                if (values[key] === undefined)
                    values[key] = null;
            }

            let key = `${profile.userid}:${record.ulid}`;
            let fragment = {
                type: 'save',
                user: profile.username,
                mtime: new Date,
                page: page.key,
                values: values
            };

            await db.transaction('rw', ['rec_records'], async t => {
                let obj = await t.load('rec_records', key);

                let entry;
                if (obj != null) {
                    entry = await goupile.decryptLocal(obj.enc);
                    if (record.hid != null)
                        entry.hid = record.hid;
                } else {
                    obj = {
                        fkey: `${profile.userid}/${record.form.key}`,
                        pfkey: null,
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
                        obj.pfkey = `${profile.userid}:${record.parent.ulid}/${record.form.key}`;
                        entry.parent = {
                            form: record.parent.form.key,
                            ulid: record.parent.ulid,
                            version: record.parent.version
                        };
                    }
                }

                if (record.version !== entry.fragments.length)
                    throw new Error('Cannot overwrite old record fragment');
                entry.fragments.push(fragment);

                obj.enc = await goupile.encryptLocal(entry);
                await t.saveWithKey('rec_records', key, obj);
            });

            progress.success('Enregistrement effectué');
            enablePersistence();

            // XXX: Trigger reload in a better way...
            route.version = null;
            form_record = null;
            form_state = null;
            data_rows = null;

            self.go(null, window.location.href);
        } catch (err) {
            progress.close();
            throw err;
        }
    }

    function runDeleteRecordDialog(e, ulid) {
        return ui.runConfirm(e, 'Voulez-vous vraiment supprimer cet enregistrement ?', 'Supprimer', async () => {
            let progress = log.progress('Suppression en cours');

            try {
                let key = `${profile.userid}:${ulid}`;
                let fragment = {
                    type: 'delete',
                    user: profile.username,
                    mtime: new Date
                };

                // XXX: Delete children (cascade)?
                await db.transaction('rw', ['rec_records'], async t => {
                    let obj = await t.load('rec_records', key);
                    if (obj == null)
                        throw new Error('Cet enregistrement est introuvable');

                    let entry = await goupile.decryptLocal(obj.enc);

                    // Mark as deleted with special fragment
                    entry.fragments.push(fragment);

                    obj.enc = await goupile.encryptLocal(entry);
                    await t.saveWithKey('rec_records', key, obj);
                });

                progress.success('Suppression effectuée');

                data_rows = null;
                if (ulid === form_record.chain[0].ulid) {
                    form_state = null;
                    goNewRecord(null);
                } else {
                    self.run();
                }
            } catch (err) {
                progress.close();
                throw err;
            }
        })
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
                await net.loadScript(`${ENV.base_url}static/ace.js`);

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

        let buffer = editor_buffers.get(editor_filename);

        if (buffer == null) {
            let code = await fetchCode(editor_filename);

            let session = new ace.EditSession('', 'ace/mode/javascript');
            session.setOption('useWorker', false);
            session.setUseWrapMode(true);
            session.doc.setValue(code);
            session.setUndoManager(new ace.UndoManager());
            session.on('change', e => handleFileChange(editor_filename));

            session.on('changeScrollTop', () => {
                if (ui.isPanelEnabled('page'))
                    setTimeout(syncFormScroll, 0);
            });
            session.selection.on('changeSelection', () => {
                syncFormHighlight(true);
                ignore_editor_scroll = true;
            });

            buffer = {
                session: session,
                reload: false
            };
            editor_buffers.set(editor_filename, buffer);
        } else if (buffer.reload) {
            let code = await fetchCode(editor_filename);

            ignore_editor_change = true;
            buffer.session.doc.setValue(code);
            ignore_editor_change = false;

            buffer.reload = false;
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

        let buffer = editor_buffers.get(filename);

        // Should never fail, but who knows..
        if (buffer != null) {
            let key = `${profile.userid}:${filename}`;
            let blob = new Blob([buffer.session.doc.getValue()]);
            let sha256 = await computeSha256(blob);

            await db.saveWithKey('fs_files', key, {
                filename: filename,
                size: blob.size,
                sha256: await computeSha256(blob),
                blob: blob
            });
        }

        self.run();
    }

    function syncFormScroll() {
        if (!ui.isPanelEnabled('editor') || !ui.isPanelEnabled('page'))
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
        if (!ui.isPanelEnabled('page'))
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
        if (!ui.isPanelEnabled('editor') || !ui.isPanelEnabled('page'))
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

    // Javascript Blob/File API sucks, plain and simple
    async function computeSha256(blob) {
        let sha256 = new Sha256;

        for (let offset = 0; offset < blob.size; offset += 65536) {
            let piece = blob.slice(offset, offset + 65536);
            let buf = await new Promise((resolve, reject) => {
                let reader = new FileReader;

                reader.onload = e => resolve(e.target.result);
                reader.onerror = e => {
                    reader.abort();
                    reject(new Error(e.target.error));
                };

                reader.readAsArrayBuffer(piece);
            });

            sha256.update(buf);
        }

        return sha256.finalize();
    }

    // XXX: Broken refresh, etc
    async function runDeployDialog(e) {
        let actions = await computeDeployActions();
        let modifications = actions.reduce((acc, action) => acc + (action.type !== 'noop'), 0);

        return ui.runDialog(e, (d, resolve, reject) => {
            d.output(html`
                <div class="ui_quick">
                    ${modifications || 'Aucune'} ${modifications.length > 1 ? 'modifications' : 'modification'} à effectuer
                    <div style="flex: 1;"></div>
                    <a @click=${ui.wrapAction(e => { actions = null; return self.run(); })}>Rafraichir</a>
                </div>

                <table class="ui_table ins_deploy">
                    <colgroup>
                        <col style="width: 2.4em;"/>
                        <col/>
                        <col style="width: 4.6em;"/>
                        <col style="width: 8em;"/>
                        <col/>
                        <col style="width: 4.6em;"/>
                    </colgroup>

                    <tbody>
                        ${actions.map(action => {
                            let local_cls = 'path';
                            if (action.type === 'pull') {
                                local_cls += action.remote_sha256 ? ' overwrite' : ' overwrite delete';
                            } else if (action.local_sha256 == null) {
                                local_cls += ' virtual';
                            }

                            let remote_cls = 'path';
                            if (action.type === 'push') {
                                remote_cls += action.local_sha256 ? ' overwrite' : ' overwrite delete';
                            } else if (action.remote_sha256 == null) {
                                remote_cls += ' none';
                            }

                            return html`
                                <tr class=${action.type === 'conflict' ? 'conflict' : ''}>
                                    <td>
                                        <a @click=${ui.wrapAction(e => runDeleteFileDialog(e, action.filename))}>x</a>
                                    </td>
                                    <td class=${local_cls}>${action.filename}</td>
                                    <td class="size">${action.local_sha256 ? util.formatDiskSize(action.local_size) : ''}</td>

                                    <td class="actions">
                                        <a class=${action.type === 'pull' ? 'active' : (action.local_sha256 == null || action.local_sha256 === action.remote_sha256 ? 'disabled' : '')}
                                           @click=${e => { action.type = 'pull'; d.restart(); }}>&lt;</a>
                                        <a class=${action.type === 'push' ? 'active' : (action.local_sha256 == null || action.local_sha256 === action.remote_sha256 ? 'disabled' : '')}
                                           @click=${e => { action.type = 'push'; d.restart(); }}>&gt;</a>
                                    </td>

                                    <td class=${remote_cls}>${action.filename}</td>
                                    <td class="size">
                                        ${action.type === 'conflict' ? html`<span class="conflict" title="Modifications en conflit">⚠\uFE0E</span>&nbsp;` : ''}
                                        ${action.remote_sha256 ? util.formatDiskSize(action.remote_size) : ''}
                                    </td>
                                </tr>
                            `;
                        })}
                    </tbody>
                </table>

                <div class="ui_quick">
                    <div style="flex: 1;"></div>
                    <a @click=${ui.wrapAction(runAddFileDialog)}>Ajouter un fichier</a>
                </div>
            `);

            d.action('Publier', {disabled: !d.isValid()}, async () => {
                await deploy(actions);

                resolve();
                self.run();
            });
        });
    }

    async function computeDeployActions() {
        let range = IDBKeyRange.bound(profile.userid + ':', profile.userid + '`', false, true);
        let [local_files, remote_files] = await Promise.all([
            db.loadAll('fs_files', range),
            net.fetchJson(`${ENV.base_url}api/files/list`)
        ]);

        let local_map = util.arrayToObject(local_files, file => file.filename);
        // XXX: let sync_map = util.arrayToObject(sync_files, file => file.filename);
        let remote_map = util.arrayToObject(remote_files, file => file.filename);

        let actions = [];

        // Handle local files
        for (let local_file of local_files) {
            let remote_file = remote_map[local_file.filename];
            let sync_file = remote_file; // XXX: sync_map[local_file.filename];

            if (remote_file != null) {
                if (local_file.sha256 != null) {
                    if (local_file.sha256 === remote_file.sha256) {
                        actions.push(makeDeployAction(local_file.filename, local_file, remote_file, 'noop'));
                    } else if (sync_file != null && sync_file.sha256 === remote_file.sha256) {
                        actions.push(makeDeployAction(local_file.filename, local_file, remote_file, 'push'));
                    } else {
                        actions.push(makeDeployAction(local_file.filename, local_file, remote_file, 'conflict'));
                    }
                } else {
                    actions.push(makeDeployAction(local_file.filename, local_file, remote_file, 'noop'));
                }
            } else {
                if (sync_file == null) {
                    actions.push(makeDeployAction(local_file.filename, local_file, null, 'push'));
                } else if (sync_file.sha256 === local_file.sha256) {
                    actions.push(makeDeployAction(local_file.filename, local_file, null, 'pull'));
                } else {
                    actions.push(makeDeployAction(local_file.filename, local_file, null, 'conflict'));
                }
            }
        }

        // Pull remote-only files
        {
            // let new_sync_files = [];

            for (let remote_file of remote_files) {
                if (local_map[remote_file.filename] == null) {
                    actions.push(makeDeployAction(remote_file.filename, null, remote_file, 'noop'));

                    //new_sync_files.push(remote_file);
                }
            }

            // await db.saveAll('fs_sync', new_sync_files);
        }

        return actions;
    };

    function makeDeployAction(filename, local, remote, type) {
        let action = {
            filename: filename,
            type: type
        };

        if (local != null) {
            action.local_size = local.size;
            action.local_sha256 = local.sha256;
        }
        if (remote != null) {
            action.remote_size = remote.size;
            action.remote_sha256 = remote.sha256;
        }

        return action;
    }

    function runAddFileDialog(e) {
        return ui.runDialog(e, (d, resolve, reject) => {
            let file = d.file('*file', 'Fichier :');
            let filename = d.text('*filename', 'Chemin :', {value: file.value ? file.value.name : null});

            if (filename.value) {
                if (!filename.value.match(/^[A-Za-z0-9_\.]+(\/[A-Za-z0-9_\.]+)*$/))
                    filename.error('Caractères autorisés: a-z, 0-9, \'_\', \'.\' et \'/\' (pas au début ou à la fin)');
                if (filename.value.includes('/../') || filename.value.endsWith('/..'))
                    filename.error('Le chemin ne doit pas contenir de composants \'..\'');
            }

            d.action('Créer', {disabled: !d.isValid()}, async () => {
                let progress = new log.Entry;

                progress.progress('Enregistrement du fichier');
                try {
                    let key = `${profile.userid}:${filename.value}`;
                    await db.saveWithKey('fs_files', key, {
                        filename: filename.value,
                        size: file.value.size,
                        sha256: await computeSha256(filename.value),
                        blob: file.value
                    })

                    let buffer = editor_buffers.get(file.filename);
                    if (buffer != null)
                        buffer.reload = true;

                    progress.success('Fichier enregistré');
                    resolve();

                    self.run();
                } catch (err) {
                    progress.close();
                    reject(err);
                }
            });
        });
    }

    function runDeleteFileDialog(e, filename) {
        log.error('NOT IMPLEMENTED');
    }

    async function deploy(actions) { // XXX: reentrancy
        let progress = log.progress('Publication en cours');

        try {
            if (actions.some(action => action.type == 'conflict'))
                throw new Error('Conflits non résolus');

            for (let i = 0; i < actions.length; i += 10) {
                let p = actions.slice(i, i + 10).map(async action => {
                    switch (action.type) {
                        case 'push': {
                            let url = util.pasteURL(`${ENV.base_url}files/${action.filename}`, {sha256: action.remote_sha256 || ''});

                            if (action.local_sha256 != null) {
                                let key = `${profile.userid}:${action.filename}`;
                                let file = await db.load('fs_files', key);

                                let response = await net.fetch(url, {method: 'PUT', body: file.blob});
                                if (!response.ok) {
                                    let err = (await response.text()).trim();
                                    throw new Error(err);
                                }

                                await db.delete('fs_files', key);
                            } else {
                                let response = await net.fetch(url, {method: 'DELETE'});
                                if (!response.ok && response.status !== 404) {
                                    let err = (await response.text()).trim();
                                    throw new Error(err);
                                }
                            }
                        } break;

                        case 'noop':
                        case 'pull': {
                            let key = `${profile.userid}:${action.filename}`;
                            await db.delete('fs_files', key);

                            let buffer = editor_buffers.get(action.filename);
                            if (buffer != null)
                                buffer.reload = true;
                        } break;
                    }
                });
                await Promise.all(p);
            }

            progress.success('Publication effectuée');
        } catch (err) {
            progress.close();
            throw err;
        } finally {
            code_cache.clear();
        }
    };

    this.go = async function(e = null, url = null, push_history = true) {
        await goupile.syncProfile();
        if (!goupile.isAuthorized()) {
            await goupile.runLogin();

            if (net.isOnline() && ENV.backup_key != null)
                await backupClientData('server');
        }

        // Fix up URL
        if (!url.endsWith('/'))
            url += '/';
        url = new URL(url, window.location.href);
        if (url.pathname === ENV.base_url)
            url = new URL(app.home.url, window.location.href);

        // Goodbye!
        if (!url.pathname.startsWith(`${ENV.base_url}main/`))
            window.location.href = url.href;

        let new_route = Object.assign({}, route);
        let new_record = form_record;
        let new_state = form_state;

        // Parse new URL
        {
            let path = url.pathname.substr(ENV.base_url.length + 5);
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
            if (!ulid && !push_history)
                ulid = 'new';

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
        }

        // Load record if needed
        if (new_record != null && (new_route.ulid !== new_record.ulid ||
                                   new_route.version !== new_record.version))
            new_record = null;
        if (new_record == null && new_route.ulid != null) {
            new_record = await loadRecord(new_route.ulid, new_route.version);

            new_state = new FormState(new_record.values);
            new_state.changeHandler = handleStateChange;
        }

        // Go to parent or child automatically
        if (new_record != null && new_record.form !== new_route.form) {
            new_record = await moveToAppropriateRecord(new_record, new_route.form);

            if (new_record != null) {
                new_route.ulid = new_record.ulid;
                new_route.version = new_record.version;
                new_state = new FormState(new_record.values);
                new_state.changeHandler = handleStateChange;
            } else {
                new_route.ulid = null;
                new_record = null;
            }
        }

        // Create new record if needed
        if (new_route.ulid == null || new_record == null) {
            new_record = createRecord(new_route.form, null);

            new_route.ulid = new_record.ulid;
            new_route.version = new_record.version;
            new_state = new FormState(new_record.values);
            new_state.changeHandler = handleStateChange;
        }

        // Load record parents
        if (new_record.chain == null) {
            let chain = [];
            let map = {};

            new_record.chain = chain;
            new_record.map = map;
            chain.push(new_record);
            map[new_record.form.key] = new_record;

            let record = new_record;
            while (record.parent != null) {
                let parent_record = await loadRecord(record.parent.ulid);

                parent_record.chain = chain;
                parent_record.map = map;
                chain.push(parent_record);
                map[parent_record.form.key] = parent_record;
                record.parent = parent_record;

                record = parent_record;
            }

            chain.reverse();
        }

        if (!isPageEnabled(new_route.page, new_record))
            throw new Error('Cette page n\'est pas accessible pour le moment');

        // Confirm dangerous actions (at risk of data loss)
        if (form_state != null && form_state.hasChanged() && new_record !== form_record) {
            try {
                // XXX: Improve message if going to child form
                await ui.runConfirm(e, "Si vous continuez, vous perdrez les modifications en cours. Voulez-vous continuer ?",
                                       "Continuer", () => {});
            } catch (err) {
                // If we're popping state, this will fuck up navigation history but we can't
                // refuse popstate events. History mess is better than data loss.
                return self.run();
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

            ui.setPanelState('page', true);
        } else if (form_record == null && (new_record.version > 0 ||
                                           new_record.parent != null)) {
            ui.setPanelState('page', true);
        }

        // Commit!
        route = new_route;
        route.version = new_record.version;
        form_record = new_record;
        form_state = new_state;

        document.title = `${route.page.title} — ${ENV.title}`;
        await self.run(push_history);
    };
    this.go = util.serializeAsync(this.go);

    async function handleStateChange() {
        await self.run();

        // Highlight might need to change (conditions, etc.)
        if (ui.isPanelEnabled('editor'))
            syncFormHighlight(false);
    }

    function createRecord(form, parent_record) {
        let record = {
            form: form,
            ulid: util.makeULID(),
            hid: null,
            version: 0,
            mtime: null,
            fragments: [],
            status: new Set,
            values: {},

            parent: null,
            children: {},
            chain: null, // Will be set later
            map: null // Will be set later
        };

        if (form.chain.length > 1) {
            if (parent_record == null)
                throw new Error('Impossible de créer cet enregistrement sans parent');

            record.parent = {
                form: parent_record.form.key,
                ulid: parent_record.ulid,
                version: parent_record.version
            };
        }

        return record;
    }

    async function loadRecord(ulid, version) {
        let key = `${profile.userid}:${ulid}`;
        let obj = await db.load('rec_records', key);

        if (obj == null)
            throw new Error('L\'enregistrement demandé n\'existe pas');

        let record = await decryptRecord(obj, version);
        return record;
    }

    async function decryptRecord(obj, version, allow_fake = true, load_values = true) {
        let entry = await goupile.decryptLocal(obj.enc);
        let fragments = entry.fragments;

        // Deleted record
        if (fragments[fragments.length - 1].type === 'delete')
            throw new Error('L\'enregistrement demandé est supprimé');

        if (version == null) {
            version = fragments.length;
        } else if (version > fragments.length) {
            throw new Error(`Cet enregistrement n'a pas de version ${version}`);
        }

        let values = {};
        let status = new Set;
        for (let i = 0; i < version; i++) {
            let fragment = fragments[i];

            if (fragment.type === 'save') {
                if (load_values)
                    Object.assign(values, fragment.values);
                status.add(fragment.page);
            }
        }
        for (let fragment of fragments) {
            fragment.mtime = new Date(fragment.mtime);

            delete fragment.child;
            delete fragment.values;
        }

        // Could be a fake record
        if (!allow_fake && !status.size)
            throw new Error('Skipping fake record');

        let children_map = {};
        {
            let range = IDBKeyRange.bound(profile.userid + ':' + entry.ulid + '/',
                                          profile.userid + ':' + entry.ulid + '`', false, true);
            let keys = await db.list('rec_records/parent', range);

            for (let key of keys) {
                let child = {
                    form: key.index.substr(key.index.indexOf('/') + 1),
                    ulid: key.primary.substr(key.primary.indexOf(':') + 1)
                };

                let array = children_map[child.form];
                if (array == null) {
                    array = [];
                    children_map[child.form] = array;
                }

                array.push(child);
                status.add(child.form);
            }
        }

        let record = {
            form: app.forms.get(entry.form),
            ulid: entry.ulid,
            hid: entry.hid,
            version: version,
            mtime: fragments[version - 1].mtime,
            fragments: fragments,
            status: status,
            values: values,

            parent: entry.parent,
            children: children_map,
            chain: null, // Will be set later
            map: null // Will be set later
        };
        if (record.form == null)
            throw new Error(`Le formulaire '${entry.form}' n'existe pas ou plus`);
        if (!load_values)
            delete record.values;

        return record;
    }

    async function moveToAppropriateRecord(record, target_form) {
        let path = computePath(record.form, target_form);

        if (path != null) {
            for (let form of path.up) {
                record = await loadRecord(record.parent.ulid, null);

                if (record.form !== form)
                    throw new Error('Saut impossible en raison d\'un changement de schéma');
            }

            for (let i = 0; i < path.down.length; i++) {
                let form = path.down[i];

                if (record.status.has(form.key)) {
                    let child = record.children[form.key][0];
                    record = await loadRecord(child.ulid);

                    if (record.form !== form)
                        throw new Error('Saut impossible en raison d\'un changement de schéma');
                } else {
                    // Save fake intermediate records if needed
                    if (!record.version) {
                        let key = `${profile.userid}:${record.ulid}`;

                        let obj = {
                            fkey: `${profile.userid}/${record.form.key}`,
                            pfkey: null,
                            enc: null
                        };
                        let entry = {
                            ulid: record.ulid,
                            hid: record.hid,
                            parent: null,
                            form: record.form.key,
                            fragments: [{
                                type: 'fake',
                                user: profile.username,
                                mtime: new Date
                            }]
                        };

                        if (record.parent != null) {
                            obj.pfkey = `${profile.userid}:${record.parent.ulid}/${record.form.key}`;
                            entry.parent = {
                                form: record.parent.form.key,
                                ulid: record.parent.ulid,
                                version: record.parent.version
                            };
                        }

                        obj.enc = await goupile.encryptLocal(entry);
                        await db.saveWithKey('rec_records', key, obj);
                    }

                    record = createRecord(form, record);
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

    function isFormEnabled(form, record) {
        for (let page of form.pages.values()) {
            if (isPageEnabled(page, record))
                return true;
        }

        return false;
    }

    function isPageEnabled(page, record) {
        if (typeof page.enabled === 'function') {
            return page.enabled(record);
        } else {
            return page.enabled;
        }
    }

    this.run = async function(push_history = false) {
        // Is the user developing?
        {
            let range = IDBKeyRange.bound(profile.userid + ':', profile.userid + '`', false, true);
            let count = await db.count('fs_files', range);

            develop = !!count;
        }

        // Update browser URL
        {
            let url = route.page.url;
            if (route.version > 0) {
                url += `/${route.ulid}`;
                if (route.version < form_record.fragments.length)
                    url += `@${route.version}`;
            } else if (form_record.parent != null) {
                url += `/${form_record.parent.ulid}`;
            }
            goupile.syncHistory(url, push_history);
        }

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
                data_rows = [];
                await loadDataRecords(data_form, null, data_rows);
            }
        }

        // Fetch page code (for page panel)
        page_code = await fetchCode(route.page.filename);

        ui.render();
    };
    this.run = util.serializeAsync(this.run);

    async function loadDataRecords(form, parent, records) {
        let objects;
        if (parent == null) {
            let range = IDBKeyRange.only(`${profile.userid}/${form.key}`);
            objects = await db.loadAll('rec_records/form', range);
        } else {
            let range = IDBKeyRange.only(`${profile.userid}:${parent.ulid}/${form.key}`);
            objects = await db.loadAll('rec_records/parent', range);
        }

        for (let obj of objects) {
            try {
                let record = await decryptRecord(obj, null, false, false);
                records.push(record);
            } catch (err) {
                console.log(err);
            }
        }
    }

    async function fetchCode(filename) {
        // Anything in the editor?
        {
            let buffer = editor_buffers.get(filename);
            if (buffer != null && !buffer.reload)
                return buffer.session.doc.getValue();
        }

        // Try the cache
        {
            let code = code_cache.get(filename);
            if (code != null)
                return code;
        }

        // Try locally saved files
        if (goupile.isAuthorized()) {
            let key = `${profile.userid}:${filename}`;
            let file = await db.load('fs_files', key);

            if (file != null) {
                let code = await file.blob.text();
                code_cache.set(filename, code);
                return code;
            }
        }

        // The server is our last hope
        {
            let response = await net.fetch(`${ENV.base_url}files/${filename}`);

            if (response.ok) {
                let code = await response.text();
                code_cache.set(filename, code);
                return code;
            } else {
                return '';
            }
        }
    }

    async function backupClientData(dest) {
        let progress = log.progress('Archivage sécurisé des données');

        try {
            let range = IDBKeyRange.bound(profile.userid + ':', profile.userid + '`', false, true);
            let objects = await db.loadAll('rec_records', range);

            let entries = [];
            for (let obj of objects) {
                try {
                    let entry = await goupile.decryptLocal(obj.enc);
                    entries.push(entry);
                } catch (err) {
                    console.log(err);
                }
            }

            let enc = await goupile.encryptBackup(entries);
            let json = JSON.stringify(enc);

            if (dest === 'server') {
                let response = await fetch(`${ENV.base_url}api/files/backup`, {
                    method: 'POST',
                    body: json
                });

                if (response.ok) {
                    console.log('Archive sécurisée envoyée');
                    progress.close();
                } else if (response.status === 409) {
                    console.log('Archivage ignoré (archive récente)');
                    progress.close();
                } else {
                    let err = (await response.text()).trim();
                    throw new Error(err);
                }
            } else if (dest === 'file') {
                let blob = new Blob([json], {
                    type: 'application/octet-stream'
                });

                console.log('Archive sécurisée créée');
                progress.close();

                let filename = `${ENV.base_url.replace(/\//g, '')}_${profile.username}_${dates.today()}.backup`;
                util.saveBlob(blob, filename);
            } else {
                throw new Error(`Invalid backup destination '${dest}'`);
            }
        } catch (err) {
            progress.close();
            throw err;
        }
    }
};
