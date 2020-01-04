// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let dev_files = new function() {
    let self = this;

    let remote = true;

    let editor;
    let editor_buffers = new LruMap(32);
    let editor_timer_id;
    let editor_ignore_change = false;

    let files;
    let user_actions = {};

    this.syncEditor = async function(path) {
        if (!editor) {
            // XXX: Make sure we don't run loadScript more than once
            if (typeof ace === 'undefined')
                await util.loadScript(`${env.base_url}static/ace.js`);

            editor = ace.edit(document.querySelector('#dev_editor > div'));

            editor.setTheme('ace/theme/monokai');
            editor.setShowPrintMargin(false);
            editor.setFontSize(13);

            // Auto-pairing of parentheses is problematic when doing execute-as-you-type,
            // because it easily leads to infinite for loops.
            editor.setBehavioursEnabled(false);
        }

        let buffer = editor_buffers.get(path);

        if (!buffer || buffer.reload) {
            let file = await virt_fs.load(path);
            let extension = path.substr(path.lastIndexOf('.'));
            let script = file ? await file.data.text() : '';

            if (buffer) {
                buffer.sha256 = file ? file.sha256 : null;
                buffer.reload = false;

                editor_ignore_change = true;
                editor.session.doc.setValue(script);
                editor_ignore_change = false;
            } else {
                let session;
                switch (extension) {
                    case '.js': { session = new ace.EditSession(script, 'ace/mode/javascript'); } break;
                    case '.css': { session = new ace.EditSession(script, 'ace/mode/css'); } break;
                    default: { session = new ace.EditSession(script, 'ace/mode/text'); } break;
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

        editor.resize(false);
    }

    function handleEditorChange(path) {
        if (editor_ignore_change)
            return;

        clearTimeout(editor_timer_id);
        editor_timer_id = setTimeout(async () => {
            editor_timer_id = null;

            let code = editor.getValue();
            let extension = path.substr(path.lastIndexOf('.'));

            switch (extension) {
                case '.js': {
                    if (await goupile.validateCode(path, code))
                        await saveEditedFile(path, code);

                    window.history.replaceState(null, null, app.makeURL());
                } break;

                case '.css': {
                    updateApplicationCSS(code);
                    await saveEditedFile(path, code);
                } break;
            }
        }, 180);
    }

    async function saveEditedFile(path, data) {
        let file = await virt_fs.save(path, data);

        let buffer = editor_buffers.get(path);
        if (buffer)
            buffer.sha256 = file.sha256;
    }

    this.runFiles = async function() {
        if (remote) {
            files = await virt_fs.status();

            // Overwrite with user actions (if any)
            for (let file of files)
                file.action = user_actions[file.path] || file.action;

            // Show locally deleted files last
            files.sort((file1, file2) => (!!file2.sha256 - !!file1.sha256) ||
                                             util.compareValues(file1.path, file2.path));
        } else {
            files = await virt_fs.listLocal();
        }

        renderActions();
    };

    function renderActions() {
        let enable_sync = files.some(file => file.action !== 'noop') &&
                          !files.some(file => file.action === 'conflict');

        render(html`
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

            <div id="gp_toolbar">
                <button @click=${showCreateDialog}>Ajouter</button>
                <div style="flex: 1;"></div>
                ${remote ?
                    html`<button ?disabled=${!enable_sync} @click=${showSyncDialog}>Synchroniser</button>` : ''}
                <button class=${remote ? 'active' : ''} @click=${toggleRemote}>Distant</button>
            </div>
        `, document.querySelector('#dev_files'));
    }

    function showCreateDialog(e, path, blob) {
        goupile.popup(e, page => {
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
                    entry.error(`Echec de l'enregistrement : ${err}`);
                }
            };
            page.buttons(page.buttons.std.ok_cancel('Créer'));
        });
    }

    function toggleRemote() {
        remote = !remote;
        self.runFiles();
    }

    function showSyncDialog(e) {
        goupile.popup(e, page => {
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
        goupile.popup(e, page => {
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
