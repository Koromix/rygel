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

    this.runEditor = async function(asset) {
        // XXX: Make sure we don't run loadScript more than once
        if (typeof ace === 'undefined')
            await net.loadScript(`${env.base_url}static/ace.js`);
        if (!editor_el) {
            editor_el = document.createElement('div');
            editor_el.setAttribute('style', 'flex: 1;');
        }

        editor_tabs = [
            {name: 'Application', path: '/files/main.js'},
            {name: 'Modèle', path: '/files/main.html'},
            {name: 'Style', path: '/files/main.css'}
        ];

        if (asset && asset.edit) {
            editor_tabs.unshift({name: asset.edit, path: asset.path});

            if (editor_path_forced || !editor_tabs.find(tab => tab.path === editor_path)) {
                editor_path = asset.path;
                editor_path_forced = false;
            }
        } else {
            if (!editor_tabs.find(tab => tab.path === editor_path)) {
                editor_path = '/files/main.js';
                editor_path_forced = true;
            }
        }

        await syncEditor(editor_path);

        // Do this after syncEditor() to avoid flashing wrong file when editor panel is
        // enabled and there was another file rendered in editor_el before.
        renderEditorPanel();
    }

    async function renderEditorPanel() {
        render(html`
            <div class="gp_toolbar dev">
                ${editor_tabs.map((tab, idx) => html`
                    ${idx === editor_tabs.length - 3 ? html`<div style="flex: 1;"></div>` : ''}
                    <button class=${tab.path === editor_path ? 'active' : ''}
                            @click=${e => toggleEditorFile(tab.path)}>${tab.name}</button>
                `)}
            </div>
            ${editor_el}
        `, document.querySelector('#dev_editor'));
    }

    async function toggleEditorFile(path) {
        editor_path = path;
        editor_path_forced = false;

        renderEditorPanel();
        await syncEditor(path);
    }

    async function syncEditor(path) {
        if (!editor) {
            editor = ace.edit(editor_el);

            editor.setTheme('ace/theme/merbivore_soft');
            editor.setShowPrintMargin(false);
            editor.setFontSize(13);
        }

        let buffer = editor_buffers.get(path);
        let extension = path.substr(path.lastIndexOf('.'));

        if (!buffer || buffer.reload) {
            let file = await virt_fs.load(path);
            let code = file ? await file.data.text() : '';

            if (buffer) {
                buffer.sha256 = file ? file.sha256 : null;
                buffer.reload = false;

                editor_ignore_change = true;
                editor.session.doc.setValue(code);
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
                    reload: false
                };

                editor_buffers.set(path, buffer);
            }
        }

        if (buffer.session !== editor.session) {
            editor.setSession(buffer.session);
            editor.setReadOnly(false);
        }

        // Auto-pairing of parentheses is problematic when doing
        // execute-as-you-type, because it easily leads to infinite for loops.
        editor.setBehavioursEnabled(extension !== '.js');

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
                let file = await virt_fs.save(path, code);

                // Avoid unwanted buffer reloads in some cases (such as file syncing)
                let buffer = editor_buffers.get(path);
                if (buffer)
                    buffer.sha256 = file.sha256;
            }
        }, 180);
    }

    this.getBuffer = function(path) {
        let buffer = editor_buffers.get(path);
        return (buffer && !buffer.reload) ? buffer.session.doc.getValue() : null;
    };

    this.runFiles = async function() {
        files = await virt_fs.status(net.isOnline() || !env.use_offline);
        for (let file of files)
            file.action = user_actions[file.path] || file.action;

        renderActions();
    };

    function renderActions() {
        let remote = net.isOnline() || !env.use_offline;
        let actions = files.reduce((acc, file) => acc + ((user_actions[file.path] || file.action) !== 'noop'), 0);

        render(html`
            <div class="gp_toolbar">
                <button @click=${showCreateDialog}>Ajouter</button>
                ${remote ? html`
                    <div style="flex: 1;"></div>
                    <button ?disabled=${!actions} @click=${showSyncDialog}>Déployer</button>
                ` : ''}
            </div>

            <table class="sync_table">
                <thead><tr>
                    <th style="width: 1em;"></th>
                    <th>Fichier</th>
                    <th style="width: 4.6em;"></th>

                    ${remote ? html`
                        <th style="width: 8em;"></th>
                        <th>Distant</th>
                        <th style="width: 4.6em;"></th>
                    ` : ''}
                </tr></thead>

                <tbody>${files.map(file => {
                    if (file.sha256 || file.remote_sha256) {
                        let action = user_actions[file.path] || file.action;

                        return html`
                            <tr class=${file.action === 'conflict' ? 'conflict' : ''}>
                                <td><a @click=${e => showDeleteDialog(e, file.path)}>x</a></td>
                                <td>
                                    <span class=${makeLocalPathClass(file, action)}>${file.path}</span>
                                    ${(file.sha256 && file.sha256 !== file.remote_sha256) || file.deleted ?
                                        html`<a @click=${e => showResetDialog(e, file)}>&nbsp;👻\uFE0E</a>` : ''}
                                </td>
                                <td class="sync_size">${file.sha256 ? util.formatDiskSize(file.size) : ''}</td>

                                ${remote ? html`
                                    <td class="sync_actions">
                                        <a class=${action === 'pull' ? 'selected' : (file.action === 'pull' ? 'default' : '')}
                                           @click=${e => toggleAction(file, 'pull')}>&lt;</a>
                                        <a class=${action === 'noop' ? 'selected' : (file.action === 'noop' ? 'default' : '')}
                                           @click=${e => toggleAction(file, 'noop')}>=</a>
                                        <a class=${action === 'push' ? 'selected' : (file.action === 'push' ? 'default' : '')}
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
                })}</tbody>
            </table>
        `, document.querySelector('#dev_files'));
    }

    function makeLocalPathClass(file, action) {
        let cls = 'sync_path';

        if (action === 'pull') {
            cls += file.remote_sha256 ? ' overwrite' : ' delete';
        } else if (!file.sha256) {
            cls += ' virtual' + (file.deleted ? ' delete' : '');
        }

        return cls;
    }

    function makeRemotePathClass(file, action) {
        let cls = 'sync_path';

        if (action === 'push') {
            cls += file.sha256 ? ' overwrite' : ' delete';
        } else if (!file.remote_sha256) {
            cls += ' none';
        }

        return cls;
    }

    function showCreateDialog(e, path, blob) {
        goupile.popup(e, 'Créer', (page, close) => {
            let blob = page.file('file', 'Fichier :', {mandatory: true});

            let default_path = blob.value ? `/files/${blob.value.name}` : null;
            let path = page.text('path', 'Chemin :', {placeholder: default_path});
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

            page.submitHandler = async () => {
                close();

                let entry = new log.Entry;

                entry.progress('Enregistrement du fichier');
                try {
                    let file = await virt_fs.save(path.value, blob.value || '');
                    syncBuffer(file.path, file.sha256);

                    await goupile.initApplication();

                    entry.success('Fichier enregistré !');
                } catch (err) {
                    entry.error(`Echec de l'enregistrement : ${err.message}`);
                }
            };
        });
    }

    function showSyncDialog(e) {
        goupile.popup(e, 'Synchroniser', (page, close) => {
            page.output('Voulez-vous vraiment synchroniser les fichiers ?');

            page.submitHandler = async () => {
                close();

                await syncFiles();
                await goupile.initApplication();
            };
        });
    }

    async function showResetDialog(e, file) {
        if (file.sha256 === file.remote_sha256) {
            await resetFile(file.path);
            self.runFiles();
        } else {
            goupile.popup(e, 'Oublier', (page, close) => {
                page.output(`Voulez-vous vraiment oublier les modifications locales pour '${file.path}' ?`);

                page.submitHandler = async () => {
                    close();

                    await resetFile(file.path);
                    goupile.initApplication();
                };
            });
        }
    }

    function showDeleteDialog(e, path) {
        goupile.popup(e, 'Supprimer', (page, close) => {
            page.output(`Voulez-vous vraiment supprimer '${path}' ?`);

            page.submitHandler = async () => {
                close();

                await deleteFile(path);
                goupile.initApplication();
            };
        });
    }

    async function resetFile(path) {
        await virt_fs.reset(path);
        syncBuffer(path, null);
        delete user_actions[path];
    }

    async function deleteFile(path) {
        await virt_fs.delete(path);
        syncBuffer(path, null);
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

    function syncBuffer(path, sha256) {
        let buffer = editor_buffers.get(path);
        if (buffer && buffer.sha256 !== sha256)
            buffer.reload = true;
    }

    async function syncFiles() {
        let entry = new log.Entry;
        entry.progress('Synchronisation en cours');

        try {
            let actions = files.map(file => {
                file = Object.assign({}, file);
                file.action = user_actions[file.path] || file.action;
                return file;
            });
            await virt_fs.sync(actions);

            entry.success('Synchronisation terminée !');
        } catch (err) {
            entry.error(err.message);
        }

        let files2 = await virt_fs.listAll(false);
        let files2_map = files2.map(file => file.path);

        for (let [path, buffer] of editor_buffers) {
            let file = files2_map[path];
            buffer.reload = (file && file.sha256 !== buffer.sha256) || (!file && buffer.sha256);
        }

        user_actions = {};
    }
};
