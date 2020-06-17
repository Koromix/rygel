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
            {name: 'Style', path: '/files/main.css'},
        ];

        if (asset.edit) {
            editor_tabs.push({name: asset.edit, path: asset.path});

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
                ${editor_tabs.map(tab =>
                    html`<button class=${tab.path === editor_path ? 'active' : ''}
                                 @click=${e => toggleEditorFile(tab.path)}>${tab.name}</button>`)}
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
        return buffer ? buffer.session.doc.getValue() : null;
    };

    this.runFiles = async function() {
        files = await virt_fs.status(net.isOnline() || !env.use_offline);

        // Overwrite with user actions (if any)
        for (let file of files)
            file.action = user_actions[file.path] || file.action;

        // Show locally deleted files last
        files.sort((file1, file2) => (!!file2.sha256 - !!file1.sha256) ||
                                      util.compareValues(file1.path, file2.path));

        renderActions();
    };

    function renderActions() {
        let remote = net.isOnline() || !env.use_offline;
        let enable_sync = files.some(file => file.action !== 'noop') &&
                          !files.some(file => file.action === 'conflict');

        render(html`
            <div class="gp_toolbar">
                <button @click=${showCreateDialog}>Ajouter</button>
                <div style="flex: 1;"></div>
                ${remote ? html`<button ?disabled=${!enable_sync} @click=${showSyncDialog}>Synchroniser</button>` : ''}
            </div>

            <table class="sync_table">
                <thead><tr>
                    <th style="width: 0.7em;"></th>
                    <th>Fichier</th>
                    <th style="width: 5em;"></th>

                    ${remote ? html`
                        <th style="width: 2.8em;"></th>
                        <th>Distant</th>
                        <th style="width: 5em;"></th>
                    ` : ''}
                </tr></thead>

                <tbody>${files.map(file => {
                    if (file.sha256 || file.remote_sha256) {
                        return html`<tr>
                            <td>${file.sha256 ?
                                html`<a href="#" @click=${e => { showDeleteDialog(e, file.path); e.preventDefault(); }}>x</a>` : ''}</td>
                            <td class=${file.action == 'pull' ? 'sync_path overwrite' : 'sync_path'}>${file.sha256 ? file.path : ''}</td>
                            <td class="sync_size">${file.sha256 ? util.formatDiskSize(file.size) : ''}</td>

                            ${remote ? html`
                                <td class="sync_actions">
                                    <a href="#" class=${file.action !== 'pull' ? 'gray' : ''}
                                       @click=${e => { toggleAction(file, 'pull'); e.preventDefault(); }}>&lt;</a>
                                    <a href="#" class=${file.action !== 'noop' ? 'gray' : ''}
                                       @click=${e => { toggleAction(file, 'noop'); e.preventDefault(); }}>${file.action === 'conflict' ? '?' : '='}</a>
                                    <a href="#" class=${file.action !== 'push' ? 'gray' : ''}
                                       @click=${e => { toggleAction(file, 'push'); e.preventDefault(); }}>&gt;</a>
                                </td>
                                <td class=${file.action == 'push' ? 'sync_path overwrite' : 'sync_path'}>${file.remote_sha256 ? file.path : ''}</td>
                                <td class="sync_size">${file.remote_sha256 ? util.formatDiskSize(file.remote_size) : ''}</td>
                            ` : ''}
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

    function showCreateDialog(e, path, blob) {
        ui.popup(e, page => {
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
                page.close();

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
            page.buttons(page.buttons.std.ok_cancel('Créer'));
        });
    }

    function showSyncDialog(e) {
        ui.popup(e, page => {
            page.output('Voulez-vous vraiment synchroniser les fichiers ?');

            page.submitHandler = async () => {
                page.close();

                await syncFiles();
                await goupile.initApplication();
            };
            page.buttons(page.buttons.std.ok_cancel('Synchroniser'));
        });
    }

    function showDeleteDialog(e, path) {
        ui.popup(e, page => {
            page.output(`Voulez-vous vraiment supprimer '${path}' ?`);

            page.submitHandler = async () => {
                page.close();

                await virt_fs.delete(path);
                syncBuffer(path, null);

                await goupile.initApplication();
            };
            page.buttons(page.buttons.std.ok_cancel('Supprimer'));
        });
    }

    function toggleAction(file, type) {
        file.action = type;
        user_actions[file.path] = file.action;

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
            await virt_fs.sync(files);
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
