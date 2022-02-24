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
    let dialog;

    this.runDialog = async function(e) {
        await computeActions();

        let modifications = actions.reduce((acc, action) => acc + (action.type !== 'noop'), 0);

        return ui.runDialog(e, 'Publication', {}, (d, resolve, reject) => {
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
                                local_cls += action.from_sha256 ? ' overwrite' : ' overwrite delete';
                            } else if (action.to_sha256 == null) {
                                local_cls += ' virtual';
                            }

                            let remote_cls = 'path';
                            if (action.type === 'push') {
                                remote_cls += action.to_sha256 ? ' overwrite' : ' overwrite delete';
                            } else if (action.from_sha256 == null) {
                                remote_cls += ' virtual';
                            }

                            return html`
                                <tr class=${action.type === 'conflict' ? 'conflict' : ''}>
                                    <td>
                                        <a @click=${ui.wrapAction(e => runDeleteFileDialog(e, action))}>x</a>
                                    </td>
                                    <td class=${local_cls}>${action.filename}</td>
                                    <td class="size">${action.to_sha256 ? util.formatDiskSize(action.to_size) : ''}</td>

                                    <td class="actions">
                                        <a class=${action.type === 'pull' ? 'active' : (action.to_sha256 == null || action.to_sha256 === action.from_sha256 ? 'disabled' : '')}
                                           @click=${e => { action.type = 'pull'; d.restart(); }}>&lt;</a>
                                        <a class=${action.type === 'push' ? 'active' : (action.to_sha256 == null || action.to_sha256 === action.from_sha256 ? 'disabled' : '')}
                                           @click=${e => { action.type = 'push'; d.restart(); }}>&gt;</a>
                                    </td>

                                    <td class=${remote_cls}>${action.filename}</td>
                                    <td class="size">
                                        ${action.type === 'conflict' ? html`<span class="conflict" title="Modifications en conflit">⚠\uFE0E</span>&nbsp;` : ''}
                                        ${action.from_sha256 ? util.formatDiskSize(action.from_size) : ''}
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

     // XXX: What about conflicts?
    async function computeActions() {
        actions = await net.fetchJson(`${ENV.urls.base}api/files/delta`);

        for (let action of actions) {
            if (action.to_sha256 === action.from_sha256) {
                action.type = 'noop';
            } else {
                action.type = 'push';
            }
        }
    }

    function runAddFileDialog(e) {
        return ui.runDialog(e, 'Ajout de fichier', {}, (d, resolve, reject) => {
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
                    let sha256 = await goupile.computeSha256(file.value);
                    let url = util.pasteURL(`${ENV.urls.base}files/${filename.value}`, { sha256: sha256 });

                    let response = await net.fetch(url, {
                        method: 'PUT',
                        body: file.value,
                        timeout: null
                    });
                    if (!response.ok) {
                        let err = await net.readError(response);
                        throw new Error(err)
                    }

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

    function runDeleteFileDialog(e, action) {
        return ui.runConfirm(e, `Voulez-vous vraiment supprimer le fichier '${action.filename}' ?`,
                                'Supprimer', async () => {
            let url = util.pasteURL(`${ENV.urls.base}files/${action.filename}`, { sha256: action.to_sha256 });

            let response = await net.fetch(url, { method: 'DELETE' });
            if (!response.ok && response.status !== 404) {
                let err = await net.readError(response);
                throw new Error(err)
            }

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
                            if (action.to_sha256 != null)
                                query.set(action.filename, action.to_sha256);
                        } break;

                        case 'noop':
                        case 'pull': {
                            if (action.from_sha256 != null)
                                query.set(action.filename, action.from_sha256);
                        } break;
                    }
                });
                await Promise.all(p);
            }

            // Publish!
            await net.fetchJson(`${ENV.urls.base}api/files/publish`, {
                method: 'POST',
                body: query
            });

            progress.success('Publication effectuée');
        } catch (err) {
            progress.close();
            throw err;
        }
    }
}
