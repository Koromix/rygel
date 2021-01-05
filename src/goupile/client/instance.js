// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function InstanceController() {
    let self = this;

    let app;

    let page_key;
    let page_code;
    let page_state;

    let editor_el;
    let editor_ace;
    let editor_filename;
    let editor_buffers = new LruMap(32);
    let editor_ignore_change = false;

    let code_cache = new LruMap(4);

    let develop = false;
    let error_entries = {};

    this.start = async function() {
        initUI();
        await initApp();

        self.go(window.location.href);
    };

    async function initApp() {
        let code = await fetchCode('main.js');

        try {
            let new_app = new ApplicationInfo;
            let builder = new ApplicationBuilder(new_app);

            runUserCode('Application', code, {
                app: builder
            });
            if (new_app.pages.size)
                new_app.home = new_app.pages.values().next().value.key;

            app = new_app;
        } catch (err) {
            if (app == null)
                app = new ApplicationInfo;
        }
    }

    function initUI() {
        document.documentElement.className = 'instance';

        ui.setMenu(() => html`
            <button class="icon" style="background-position-y: calc(-538px + 1.2em);"
                    @click=${e => self.go(ENV.base_url)}>${ENV.title}</button>
            <button class=${'icon' + (ui.isPanelEnabled('editor') ? ' active' : '')}
                    style="background-position-y: calc(-274px + 1.2em);"
                    @click=${e => togglePanel('editor')}>Éditeur</button>
            <button class=${'icon' + (ui.isPanelEnabled('form') ? ' active' : '')}
                    style="background-position-y: calc(-318px + 1.2em);"
                    @click=${e => togglePanel('form')}>Formulaire</button>
            <div style="flex: 1;"></div>
            <div class="drop right">
                <button>${profile.username}</button>
                <div>
                    <button @click=${ui.wrapAction(goupile.logout)}>Se déconnecter</button>
                </div>
            </div>
        `);

        ui.createPanel('editor', false, () => {
            let tabs = [];

            tabs.push({
                title: 'Application',
                filename: 'main.js'
            });
            if (page_key != null) {
                tabs.push({
                    title: 'Page',
                    filename: getPageFileName(page_key)
                });
            }

            return html`
                <div class="ins_panel" style="--menu_color: #383936;">
                    <div class="ui_toolbar">
                        ${tabs.map(tab => html`<button class=${editor_filename === tab.filename ? 'active' : ''}
                                                       @click=${ui.wrapAction(e => toggleEditorFile(tab.filename))}>${tab.title}</button>`)}
                        <div style="flex: 1;"></div>
                        <button @click=${ui.wrapAction(runDeployDialog)}
                                ?disabled=${!develop}>Déployer</button>
                    </div>

                    ${editor_el}
                </div>
            `;
        });

        ui.createPanel('form', true, () => {
            let model = new FormModel;
            let builder = new FormBuilder(page_state, model);

            let error;
            try {
                runUserCode('Page', page_code, {
                    form: builder
                });
            } catch (err) {
                error = err;
                console.log(err, error);
            }

            return html`
                <div class="ins_panel">
                    ${error == null ? html`
                        <form id="ins_form">
                            ${model.render()}
                        </form>
                    ` : ''}
                    ${error != null ? html`<span class="gp_wip">${error.message}</span>` : ''}

                    ${develop ? html`
                        <div style="flex: 1;"></div>
                        <div id="ins_notice">
                            Formulaires en développement<br/>
                            Déployez les avant d'enregistrer des données
                        </div>
                    ` : ''}
                </div>
            `;
        });
    };

    function togglePanel(key, enable = null) {
        ui.setPanelState(key, !ui.isPanelEnabled(key));
        self.go();
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

    async function syncEditor() {
        if (editor_el == null) {
            if (typeof ace === 'undefined')
                await net.loadScript(`${ENV.base_url}static/ace.js`);

            editor_el = document.createElement('div');
            editor_el.setAttribute('style', 'flex: 1;');
            editor_ace = ace.edit(editor_el);

            editor_ace.setTheme('ace/theme/merbivore_soft');
            editor_ace.setShowPrintMargin(false);
            editor_ace.setFontSize(13);
            editor_ace.setBehavioursEnabled(false);
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

            buffer = {
                session: session,
                reload: false
            };
            editor_buffers.set(editor_filename, buffer);
        } else if (buffer.reload) {
            let code = await fetchCode(editor_filename);

            editor_ignore_change = true;
            buffer.session.doc.setValue(code);
            editor_ignore_change = false;

            buffer.reload = false;
        }

        editor_ace.setSession(buffer.session);
    }

    function toggleEditorFile(filename) {
        editor_filename = filename;
        self.go();
    }

    async function handleFileChange(filename) {
        if (editor_ignore_change)
            return;

        let buffer = editor_buffers.get(filename);

        // Should never fail, but who knows..
        if (buffer != null) {
            let key = `${profile.username}:${filename}`;
            let blob = new Blob([buffer.session.doc.getValue()]);
            let sha256 = await computeSha256(blob);

            await db.saveWithKey('fs_files', key, {
                filename: filename,
                size: blob.size,
                sha256: await computeSha256(blob),
                blob: blob
            });
        }

        self.go();
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

    async function runDeployDialog(e) {
        let actions = await computeDeployActions();
        let modifications = actions.reduce((acc, action) => acc + (action.type !== 'noop'), 0);

        return ui.runDialog(null, (d, resolve, reject) => {
            d.output(html`
                <div class="ui_quick">
                    ${modifications || 'Aucune'} ${modifications.length > 1 ? 'modifications' : 'modification'} à effectuer
                    <a @click=${ui.wrapAction(e => { users = null; return self.go(); })}>🗘</a>
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
                    <a @click=${ui.wrapAction(runAddFileDialog)}>Ajouter un fichier</a>
                </div>
            `);

            d.action('Déployer', {disabled: !d.isValid()}, async () => {
                await deploy(actions);

                resolve();
                self.go();
            });
        });
    }

    async function computeDeployActions() {
        let [local_files, remote_files] = await Promise.all([
            db.loadAll('fs_files'),
            net.fetch(`${ENV.base_url}api/files/list`).then(response => response.json())
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
                    let key = `${profile.username}:${filename.value}`;
                    await db.saveWithKey('fs_files', key, {
                        filename: filename.value,
                        size: file.value.size,
                        sha256: await computeSha256(filename.value),
                        blob: filename.value
                    })

                    let buffer = editor_buffers.get(file.filename);
                    if (buffer != null)
                        buffer.reload = true;

                    progress.success('Fichier enregistré');
                    resolve();

                    self.go();
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
        let progress = log.progress('Déploiement en cours');

        try {
            if (actions.some(action => action.type == 'conflict'))
                throw new Error('Conflits non résolus');

            for (let i = 0; i < actions.length; i += 10) {
                let p = actions.slice(i, i + 10).map(async action => {
                    switch (action.type) {
                        case 'push': {
                            let url = util.pasteURL(`${ENV.base_url}files/${action.filename}`, {sha256: action.remote_sha256 || ''});

                            if (action.local_sha256 != null) {
                                let key = `${profile.username}:${action.filename}`;
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
                            let key = `${profile.username}:${action.filename}`;
                            await db.delete('fs_files', key);

                            let buffer = editor_buffers.get(action.filename);
                            if (buffer != null)
                                buffer.reload = true;
                        } break;
                    }
                });
                await Promise.all(p);
            }

            progress.success('Déploiement effectué');
        } catch (err) {
            progress.close();
            throw err;
        }
    };

    this.go = async function(url = null, push_history = true) {
        await goupile.syncProfile();
        if (!goupile.isAuthorized())
            await goupile.runLogin();

        // Find page
        if (url != null) {
            if (!url.endsWith('/'))
                url += '/';
            url = new URL(url, window.location.href);

            if (url.pathname === ENV.base_url) {
                page_key = app.home;
            } else {
                let prefix = `${ENV.base_url}main/`;

                if (url.pathname.startsWith(prefix)) {
                    let path = url.pathname.substr(prefix.length);
                    let [key, id] = path.split('/');

                    page_key = key;
                } else {
                    window.location.href = url.href;
                }
            }
        }

        // Is the user changing stuff?
        {
            let range = IDBKeyRange.bound(profile.username + ':', profile.username + '`', false, true);
            let count = await db.count('fs_files', range);

            develop = !!count;
        }

        // Fetch page code and sync state
        if (page_key != null && !app.pages.has(page_key)) {
            log.error(`La page ${page_key} n'existe pas`);
            page_key = app.home;
        }
        if (page_key != null) {
            let url = makePageURL(page_key);
            let filename = getPageFileName(page_key);

            goupile.syncHistory(url, push_history);

            page_code = await fetchCode(filename);
        } else {
            goupile.syncHistory(ENV.base_url, push_history);
            page_code = null;
        }
        if (page_state == null) {
            page_state = new FormState;
            page_state.changeHandler = () => self.go();
        }

        // Sync editor (if needed)
        if (ui.isPanelEnabled('editor')) {
            if (editor_filename == null || editor_filename.startsWith('pages/'))
                editor_filename = (page_key != null) ? getPageFileName(page_key) : 'main.js';

            await syncEditor();
        }

        ui.render();
    };
    this.go = util.serializeAsync(this.go);

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
            let key = `${profile.username}:${filename}`;
            let file = await db.load('fs_files', key);

            if (file != null) {
                let code = await file.blob.text();
                code_cache.set(filename, code);
                return code;
            }
        }

        // The server is our last hope
        {
            let response = await fetch(`${ENV.base_url}files/${filename}`);

            if (response.ok) {
                let code = await response.text();
                code_cache.set(filename, code);
                return code;
            } else {
                return '';
            }
        }
    }

    function getPageFileName(key) { return `pages/${key}.js`; };
    function makePageURL(key) { return `${ENV.base_url}main/${key}`; }
};
