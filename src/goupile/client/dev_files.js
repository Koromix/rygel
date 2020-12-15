// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let dev_files = new function() {
    let self = this;

    let editor_tabs;
    let editor_path;
    let editor_path_forced = false;

    let editor_el;
    let editor;
    let editor_buffers = new LruMap(32);
    let editor_timer_id;
    let editor_ignore_change = false;

    let files;
    let user_actions = {};

    this.runEditor = async function(path = null) {
        // XXX: Make sure we don't run loadScript more than once
        if (typeof ace === 'undefined')
            await net.loadScript(`${env.base_url}static/ace.js`);
        if (!editor_el) {
            editor_el = document.createElement('div');
            editor_el.setAttribute('style', 'flex: 1;');
        }

        editor_tabs = [
            {category: 'Application', name: 'Structure', path: '/files/main.js'},
            {category: 'Application', name: 'Modèle', path: '/files/main.html'},
            {category: 'Application', name: 'Style', path: '/files/main.css'}
        ];

        if (path) {
            editor_tabs.push({name: 'Page', path: path});

            if (editor_path_forced || !editor_tabs.find(tab => tab.path === editor_path)) {
                await syncEditor(path, false);
            } else {
                await syncEditor(editor_path, editor_path_forced);
            }
        } else {
            if (!editor_tabs.find(tab => tab.path === editor_path)) {
                await syncEditor('/files/main.js', true);
            } else {
                await syncEditor(editor_path, editor_path_forced);
            }
        }

        // Do this after syncEditor() to avoid flashing wrong file when editor panel is
        // enabled and there was another file rendered in editor_el before.
        renderEditorPanel();
    }

    async function renderEditorPanel() {
        let current_idx = editor_tabs.findIndex(tab => tab.path === editor_path);
        let current_name = (current_idx >= 0) ? editor_tabs[current_idx].name : editor_path;

        render(html`
            <div class="gp_toolbar dev">
                ${util.mapRLE(editor_tabs, tab => tab.category, (category, offset, length) => {
                    let tabs = editor_tabs.slice(offset, offset + length);

                    if (category != null) {
                        let active = current_idx >= offset && current_idx < (offset + length);
                        let name = active ? editor_tabs[current_idx].name : 'Application';

                        return html`
                            <div class="gp_dropdown">
                                <button type="button" class=${active ? 'active' : ''}>${name}</button>
                                <div>
                                    ${tabs.map(tab =>
                                        html`<button class=${tab.path === editor_path ? 'active' : ''}
                                                     @click=${e => toggleEditorFile(tab.path)}>${tab.name}</button>`)}
                                </div>
                            </div>
                        `;
                    } else {
                        return tabs.map(tab =>
                            html`<button class=${tab.path === editor_path ? 'active' : ''}
                                         @click=${e => toggleEditorFile(tab.path)}>${tab.name}</button>`);
                    }
                })}
            </div>
            ${editor_el}
        `, document.querySelector('#dev_editor'));
    }

    async function toggleEditorFile(path) {
        await syncEditor(path, false);
        renderEditorPanel();
    }

    async function syncEditor(path, forced) {
        if (!editor) {
            editor = ace.edit(editor_el);

            editor.setTheme('ace/theme/merbivore_soft');
            editor.setShowPrintMargin(false);
            editor.setFontSize(13);
        }

        let buffer = editor_buffers.get(path);
        let extension = path.substr(path.lastIndexOf('.'));

        if (!buffer || buffer.invalid) {
            try {
                let file = await vfs.load(path);
                let code = file ? await file.data.text() : '';

                if (buffer) {
                    buffer.sha256 = file ? file.sha256 : null;
                    buffer.invalid = false;

                    editor_ignore_change = true;
                    buffer.session.doc.setValue(code);
                    editor_ignore_change = false;
                } else {
                    let session;
                    switch (extension) {
                        case '.js': { session = new ace.EditSession(code, 'ace/mode/javascript'); } break;
                        case '.css': { session = new ace.EditSession(code, 'ace/mode/css'); } break;
                        case '.html': { session = new ace.EditSession(code, 'ace/mode/html'); } break;
                        default: { session = new ace.EditSession(code, 'ace/mode/text'); } break;
                    }

                    session.setOption('useWorker', false);
                    session.setUseWrapMode(true);
                    session.setUndoManager(buffer ? buffer.session.getUndoManager() : new ace.UndoManager());
                    session.on('change', e => handleEditorChange(path));

                    buffer = {
                        session: session,
                        sha256: file ? file.sha256 : null,
                        invalid: false
                    };

                    editor_buffers.set(path, buffer);
                }
            } catch (err) {
                log.error(err);
                editor_el.classList.add('broken');

                return;
            }
        }

        if (buffer.session !== editor.session) {
            editor.setSession(buffer.session);
            editor.setReadOnly(false);
        }

        // Auto-pairing of parentheses is problematic when doing
        // execute-as-you-type, because it easily leads to infinite for loops.
        editor.setBehavioursEnabled(extension !== '.js');

        editor_path = path;
        editor_path_forced = forced;

        editor.resize(false);
    }

    function handleEditorChange(path) {
        if (editor_ignore_change)
            return;

        editor_path_forced = false;

        clearTimeout(editor_timer_id);
        editor_timer_id = setTimeout(async () => {
            editor_timer_id = null;

            let code = editor.getValue();

            if (await goupile.validateCode(path, code)) {
                let file = await vfs.save(path, code);

                // Avoid unwanted buffer reloads in some cases (such as file syncing)
                let buffer = editor_buffers.get(path);
                if (buffer)
                    buffer.sha256 = file.sha256;
            }
        }, 180);
    }

    this.getBuffer = function(path) {
        let buffer = editor_buffers.get(path);
        return (buffer && !buffer.invalid) ? buffer.session.doc.getValue() : null;
    };

    this.runFiles = async function() {
        files = await vfs.status(net.isOnline() || !env.use_offline);
        for (let file of files)
            file.action = user_actions[file.path] || file.action;

        renderActions();
    };

    function renderActions() {
        let remote = net.isOnline() || !env.use_offline;
        let actions = files.reduce((acc, file) => acc + ((user_actions[file.path] || file.action) !== 'noop'), 0);

        render(html`
            <div class="gp_toolbar">
                <div style="flex: 1;"></div>
                ${remote ? html`<button @click=${runDeployDialog}>Déployer</button>` : ''}
            </div>

            <table class="sync_table">
                <thead><tr>
                    <th style="width: 2.4em;"></th>
                    <th>Fichier</th>
                    <th style="width: 4.6em;"></th>

                    ${remote ? html`
                        <th style="width: 8em;"></th>
                        <th>Distant</th>
                        <th style="width: 4.6em;"></th>
                    ` : ''}
                </tr></thead>

                <tbody>
                    ${files.map(file => {
                        if (file.sha256 || file.remote_sha256) {
                            let action = user_actions[file.path] || file.action;
                            let show_reset = file.deleted || (file.sha256 && (!env.use_offline ||
                                                                              file.sha256 !== file.remote_sha256));

                            return html`
                                <tr class=${file.action === 'conflict' ? 'conflict' : ''}>
                                    <td>
                                        <a @click=${e => runDeleteDialog(e, file.path)}>x</a>
                                        ${show_reset ? html`<a @click=${e => runResetDialog(e, file)}>&nbsp;👻\uFE0E</a>` : ''}
                                    </td>
                                    <td class=${makeLocalPathClass(file, action)}>${file.path}</td>
                                    <td class="sync_size">${file.sha256 ? util.formatDiskSize(file.size) : ''}</td>

                                    ${remote ? html`
                                        <td class="sync_actions">
                                            <a class=${action === 'pull' ? 'selected' : (file.sha256 === file.remote_sha256 ? 'disabled' : '')}
                                               @click=${e => toggleAction(file, 'pull')}>&lt;</a>
                                            <a class=${action === 'noop' ? 'selected' : ''}
                                               @click=${e => toggleAction(file, 'noop')}>=</a>
                                            <a class=${action === 'push' ? 'selected' : (file.sha256 === file.remote_sha256 || (!file.sha256 && !file.deleted) ? 'disabled' : '')}
                                               @click=${e => toggleAction(file, 'push')}>&gt;</a>
                                        </td>

                                        <td class=${makeRemotePathClass(file, action)}>${file.path}</td>
                                        <td class="sync_size">
                                            ${file.action === 'conflict' ? html`<span class="sync_conflict" title="Modifications en conflit">⚠\uFE0E</span>&nbsp;` : ''}
                                            ${file.remote_sha256 ? util.formatDiskSize(file.remote_size) : ''}
                                        </td>
                                    ` : ''}
                                </tr>
                            `;
                        } else {
                            // Getting here means that nobody has the file because it was deleted
                            // on both sides independently. We need to act on it anyway in order
                            // to clean up any remaining local state about this file.
                            return '';
                        }
                    })}

                    <tr class="action">
                        <th></th>
                        <th colspan="5"><a @click=${runCreateDialog}>Ajouter un fichier</a></th>
                    </tr>
                </tbody>
            </table>
        `, document.querySelector('#dev_files'));
    }

    function makeLocalPathClass(file, action) {
        let cls = 'sync_path';

        if (action === 'pull') {
            cls += file.remote_sha256 ? ' overwrite' : ' overwrite delete';
        } else if (!file.sha256) {
            cls += ' virtual' + (file.deleted ? ' delete' : '');
        }

        return cls;
    }

    function makeRemotePathClass(file, action) {
        let cls = 'sync_path';

        if (action === 'push') {
            cls += file.sha256 ? ' overwrite' : ' overwrite delete';
        } else if (!file.remote_sha256) {
            cls += ' none';
        }

        return cls;
    }

    function runCreateDialog(e, path, blob) {
        return dialog.run(e, (d, resolve, reject) => {
            let blob = d.file('file', 'Fichier :', {mandatory: true});

            let default_path = blob.value ? `/files/${blob.value.name}` : null;
            let path = d.text('path', 'Chemin :', {placeholder: default_path});
            if (!path.value)
                path.value = default_path;

            if (path.value) {
                if (!path.value.match(/^\/files\/./)) {
                    path.error('Le chemin doit commencer par \'/files/\'');
                } else if (!path.value.match(/^\/files(\/[A-Za-z0-9_\.]+)+$/)) {
                    path.error('Allowed path characters: a-z, _, 0-9 and / (not at the end)');
                } else if (path.value.includes('/../') || path.value.endsWith('/..')) {
                    path.error('Le chemin ne doit pas contenir de composants \'..\'');
                } else if (files.some(file => file.path === path.value && file.sha256)) {
                    path.error('Ce chemin est déjà utilisé');
                }
            }

            d.action('Créer', {disabled: !d.isValid()}, async () => {
                let entry = new log.Entry;

                entry.progress('Enregistrement du fichier');
                try {
                    let file = await vfs.save(path.value, blob.value || '');
                    markBufferInvalid(file.path, file.sha256);

                    goupile.initMain();

                    entry.success('Fichier enregistré');
                    resolve();
                } catch (err) {
                    entry.error(`Échec de l'enregistrement : ${err.message}`);
                    reject(err);
                }
            });
            d.action('Annuler', {}, () => reject(new Error('Action annulée')));
        });
    }

    function runDeployDialog(e) {
        let msg = 'Voulez-vous vraiment déployer les fichiers ?';
        return dialog.confirm(e, msg, 'Déployer', async () => {
            await deployFiles();
            goupile.initMain();
        });
    }

    async function runResetDialog(e, file) {
        if (file.sha256 === file.remote_sha256) {
            await resetFile(file.path);

            if (self.getBuffer(file.path) != null) {
                goupile.initMain();
            } else {
                self.runFiles();
            }
        } else {
            let msg = `Voulez-vous vraiment oublier les modifications locales pour '${file.path}' ?`;
            return dialog.confirm(e, msg, 'Oublier', async () => {
                await resetFile(file.path);
                goupile.initMain();
            });
        }
    }

    function runDeleteDialog(e, path) {
        let msg = `Voulez-vous vraiment supprimer '${path}' ?`;
        return dialog.confirm(e, msg, 'Supprimer', async () => {
            await deleteFile(path);
            goupile.initMain();
        });
    }

    async function resetFile(path) {
        await vfs.reset(path);
        markBufferInvalid(path, null);
        delete user_actions[path];
    }

    async function deleteFile(path) {
        await vfs.delete(path);
        markBufferInvalid(path, null);
        delete user_actions[path];
    }

    function toggleAction(file, action) {
        if (action !== file.action) {
            user_actions[file.path] = action;
        } else {
            delete user_actions[file.path];
        }

        renderActions();
    }

    function markBufferInvalid(path, sha256) {
        let buffer = editor_buffers.get(path);
        if (buffer && (sha256 == null || buffer.sha256 !== sha256))
            buffer.invalid = true;
    }

    async function deployFiles() {
        let entry = new log.Entry;
        entry.progress('Déploiement en cours');

        try {
            let actions = files.map(file => {
                file = Object.assign({}, file);
                file.action = user_actions[file.path] || file.action;
                return file;
            });
            await goupile.runConnected(() => vfs.sync(actions));

            entry.success('Déploiement terminé');
        } catch (err) {
            entry.error(err);
        }

        let files2 = await vfs.listLocal();
        let files2_map = files2.map(file => file.path);

        for (let [path, buffer] of editor_buffers) {
            let file = files2_map[path];
            buffer.invalid = (file && file.sha256 !== buffer.sha256) || (!file && buffer.sha256);
        }

        user_actions = {};
    }
};
