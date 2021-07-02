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

function InstancePublisher(instance, db) {
    let actions;
    let modifications;
    let dialog;

    this.runDialog = async function(e) {
        await computeActions();

        return ui.runDialog(e, 'Publication', (d, resolve, reject) => {
            dialog = d;

            d.output(html`
                <div class="ui_quick">
                    ${modifications || 'Aucune'} ${modifications > 1 ? 'modifications' : 'modification'} à effectuer
                    <div style="flex: 1;"></div>
                    <a @click=${ui.wrapAction(refresh)}>Rafraichir</a>
                </div>

                <table class="ui_table ins_deploy">
                    <colgroup>
                        <col style="width: 2.4em;"/>
                        <col/>
                        <col style="width: 5em;"/>
                        <col style="width: 8em;"/>
                        <col/>
                        <col style="width: 5em;"/>
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
                                remote_cls += ' virtual';
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
            });
        })
    }

    async function refresh() {
        await computeActions();
        dialog.refresh();
    }

    async function computeActions() {
        let range = IDBKeyRange.bound(profile.userid + ':', profile.userid + '`', false, true);
        let [local_files, remote_files] = await Promise.all([
            db.loadAll('fs_files', range),
            net.fetchJson(`${ENV.urls.base}api/files/list`).then(list => list.files)
        ]);

        let local_map = util.arrayToObject(local_files, file => file.filename);
        let remote_map = util.arrayToObject(remote_files, file => file.filename);

        actions = [];

        // Handle local files
        for (let local_file of local_files) {
            let remote_file = remote_map[local_file.filename];

            if (remote_file == null) {
                actions.push(makeAction(local_file.filename, local_file, null, 'push'));
            } else if (local_file.sha256 !== remote_file.sha256) {
                actions.push(makeAction(local_file.filename, local_file, remote_file, 'push'));
            } else {
                actions.push(makeAction(local_file.filename, local_file, remote_file, 'noop'));
            }
        }

        // Pull remote-only files
        for (let remote_file of remote_files) {
            if (local_map[remote_file.filename] == null)
                actions.push(makeAction(remote_file.filename, null, remote_file, 'noop'));
        }

        modifications = actions.reduce((acc, action) => acc + (action.type !== 'noop' || action.local_sha256 != null), 0);
    }

    function makeAction(filename, local, remote, type) {
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
        return ui.runDialog(e, 'Ajout de fichier', (d, resolve, reject) => {
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
                    let sha256 = await goupile.computeSha256(file.value);

                    await db.saveWithKey('fs_files', key, {
                        filename: filename.value,
                        size: file.value.size,
                        sha256: sha256,
                        blob: file.value
                    });

                    progress.success('Fichier enregistré');
                    resolve();

                    refresh();
                } catch (err) {
                    progress.close();

                    log.error(err);
                    d.refresh();
                }
            });
        });
    }

    function runDeleteFileDialog(e, filename) {
        return ui.runConfirm(e, `Voulez-vous vraiment supprimer le fichier '${filename}' ?`,
                                'Supprimer', async () => {
            let key = `${profile.userid}:${filename}`;

            await db.saveWithKey('fs_files', key, {
                filename: filename,
                size: null,
                sha256: null,
                blob: null
            });

            refresh();
        });
    }

    async function deploy(actions) {
        let progress = log.progress('Publication en cours');

        try {
            if (actions.some(action => action.type == 'conflict'))
                throw new Error('Conflits non résolus');

            let query = new URLSearchParams;
            for (let i = 0; i < actions.length; i += 10) {
                let p = actions.slice(i, i + 10).map(async action => {
                    switch (action.type) {
                        case 'push': {
                            if (action.local_sha256 != null) {
                                let url = util.pasteURL(`${ENV.urls.base}api/files/objects/${action.local_sha256}`, { filename: action.filename });
                                let key = `${profile.userid}:${action.filename}`;
                                let file = await db.load('fs_files', key);

                                let response = await net.fetch(url, {
                                    method: 'PUT',
                                    body: file.blob,
                                    timeout: null
                                });
                                if (!response.ok && response.status !== 409) {
                                    let err = (await response.text()).trim();
                                    throw new Error(err);
                                }

                                query.set(action.filename, action.local_sha256);
                            }
                        } break;

                        case 'noop':
                        case 'pull': {
                            if (action.remote_sha256 != null)
                                query.set(action.filename, action.remote_sha256);
                        } break;
                    }
                });
                await Promise.all(p);
            }

            // Publish!
            let response = await net.fetch(`${ENV.urls.base}api/files/publish`, {
                method: 'POST',
                body: query
            });
            if (response.ok) {
                let json = await response.json();

                ENV.urls.files = `${ENV.urls.base}files/${json.version}/`;
                ENV.version = json.version;
            } else {
                let err = (await response.text()).trim();
                throw new Error(err);
            }

            // Clear local copies
            {
                let range = IDBKeyRange.bound(profile.userid + ':', profile.userid + '`', false, true);
                await db.deleteAll('fs_files', range);
            }

            progress.success('Publication effectuée');
        } catch (err) {
            progress.close();
            throw err;
        }
    }
}
