// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import { render, html } from 'vendor/lit-html/lit-html.bundle.js';
import { Util, Log, Net } from 'lib/web/base/base.js';
import { Sha256 } from 'lib/web/base/mixer.js';
import * as UI from './ui.js';

function InstancePublisher(bundler = null) {
    let actions;
    let refresh;

    this.runDialog = async function(e) {
        actions = await computeActions();

        return UI.dialog(e, T.publishing, {}, (d, resolve, reject) => {
            refresh = d.refresh;

            let modifications = actions.reduce((acc, action) => acc + (action.type !== 'noop'), 0);

            d.output(html`
                <div class="ui_quick">
                    ${!modifications ? T.no_modification : ''}
                    ${modifications == 1 ? '1 ' + T.modification.toLowerCase() : ''}
                    ${modifications > 1 ? modifications + ' ' + T.modifications.toLowerCase() : ''}
                    <div style="flex: 1;"></div>
                    <a @click=${UI.wrap(refresh)}>${T.refresh}</a>
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
                            let remote_cls = 'path';

                            if (action.type === 'pull') {
                                local_cls += ' overwrite' + (action.from == null ? ' delete' : '');
                            } else if (action.to == null) {
                                local_cls += ' virtual';
                            }
                            if (action.type === 'push') {
                                remote_cls += ' overwrite' + (action.to == null ? ' delete' : '');
                            } else if (action.from == null) {
                                remote_cls += ' virtual';
                            }

                            return html`
                                <tr class=${action.type === 'conflict' ? 'conflict' : ''}>
                                    <td>
                                        <a @click=${UI.wrap(e => deleteFile(e, action))}>x</a>
                                    </td>
                                    <td class=${local_cls}>${action.filename}</td>
                                    <td class="size">${action.to != null ? Util.formatDiskSize(action.to.size) : ''}</td>

                                    <td class="actions">
                                        <a class=${action.type === 'pull' ? 'active' : (action.to == null || action.to.sha256 === action.from?.sha256 ? 'disabled' : '')}
                                           @click=${e => { action.type = 'pull'; d.restart(); }}>&lt;</a>
                                        <a class=${action.type === 'push' ? 'active' : (action.to == null || action.to.sha256 === action.from?.sha256 ? 'disabled' : '')}
                                           @click=${e => { action.type = 'push'; d.restart(); }}>&gt;</a>
                                    </td>

                                    <td class=${remote_cls}>${action.filename}</td>
                                    <td class="size">
                                        ${action.type === 'conflict' ? html`<span class="conflict" title=${T.conflict_between_actions}>⚠\uFE0E</span>&nbsp;` : ''}
                                        ${action.from != null ? Util.formatDiskSize(action.from.size) : ''}
                                    </td>
                                </tr>
                            `;
                        })}
                    </tbody>
                </table>

                <div class="ui_quick">
                    <div style="flex: 1;"></div>
                    <a @click=${UI.wrap(addFile)}>${T.add_file}</a>
                </div>
            `);

            d.action(T.publish, { disabled: !modifications || !d.isValid() }, async () => {
                await deploy(actions);
                resolve();
            });
        });
    }

    function addFile(e) {
        return UI.dialog(e, T.add_file, {}, (d, resolve, reject) => {
            d.file('*file', T.file);
            d.text('*filename', T.path, { value: d.values.file ? d.values.file.name : null });

            if (d.values.filename) {
                if (!d.values.filename.match(/^[A-Za-z0-9_\-\.]+(\/[A-Za-z0-9_\-\.]+)*$/))
                    d.error('filename', T.path_characters);
                if (d.values.filename.includes('/../') || d.values.filename.endsWith('/..'))
                    d.error('filename', T.path_no_dotdot);
            }

            let exists = d.values.filename && actions.some(action => action.filename == d.values.filename);
            let label = exists ? T.replace : T.add;

            d.action(label, { disabled: !d.isValid() }, async () => {
                let progress = new Log.Entry;

                progress.progress(T.saving_file);
                try {
                    let sha256 = await Sha256.async(d.values.file);
                    let url = Util.pasteURL(`${ENV.urls.base}files/${d.values.filename}`, { sha256: sha256 });

                    let response = await Net.fetch(url, {
                        method: 'PUT',
                        body: d.values.file,
                        timeout: null
                    });
                    if (!response.ok) {
                        let err = await Net.readError(response);
                        throw new Error(err)
                    }

                    progress.success(T.saving_done);
                    resolve();

                    actions = await computeActions();
                    refresh();
                } catch (err) {
                    progress.close();

                    Log.error(err);
                    refresh();
                }
            });
        });
    }

    function deleteFile(e, action) {
        return UI.confirm(e, T.format(T.confirm_file_deletion, action.filename),
                             T.delete, async () => {
            let url = Util.pasteURL(`${ENV.urls.base}files/${action.filename}`, { sha256: action.to?.sha256 });
            let response = await Net.fetch(url, { method: 'DELETE' });

            if (!response.ok && response.status !== 404) {
                let err = await Net.readError(response);
                throw new Error(err)
            }

            actions = await computeActions();
            refresh();
        });
    }

     // XXX: What about conflicts?
    async function computeActions() {
        let actions = await Net.get(`${ENV.urls.base}api/files/delta`);

        for (let action of actions) {
            if (action.to?.sha256 === action.from?.sha256) {
                action.type = 'noop';
            } else {
                action.type = 'push';
            }
        }

        return actions;
    }

    async function deploy(actions) {
        let progress = Log.progress(T.publishing_in_progress);

        try {
            if (actions.some(action => action.type == 'conflict'))
                throw new Error(T.message(`Unsolved conflicts`));

            let files = {};

            for (let i = 0; i < actions.length; i += 10) {
                let p = actions.slice(i, i + 10).map(async action => {
                    switch (action.type) {
                        case 'push': {
                            if (action.to != null) {
                                if (shouldBundle(action.filename) && action.to.bundle == null) {
                                    let code = await fetchCode(action.filename);

                                    try {
                                        let build = await bundler.build(code, fetchCode);
                                        code = build.code;
                                    } catch (err) {
                                        let line = err.errors?.[0]?.location?.line;
                                        let msg = `Erreur dans ${action.filename}\n${line != null ? `Ligne ${line} : ` : ''}${err.errors[0].text}`;

                                        throw new Error(msg);
                                    }

                                    let blob = new Blob([code]);
                                    let sha256 = await Sha256.async(blob);
                                    let url = Util.pasteURL(`${ENV.urls.base}files/${action.filename}`, { sha256: sha256, bundle: 1 });

                                    let response = await Net.fetch(url, {
                                        method: 'PUT',
                                        body: blob,
                                        timeout: null
                                    });
                                    if (!response.ok && response.status !== 409) {
                                        let err = await Net.readError(response);
                                        throw new Error(err)
                                    }

                                    action.to.bundle = sha256;
                                }

                                files[action.filename] = {
                                    sha256: action.to.sha256,
                                    bundle: action.to.bundle
                                };
                            }
                        } break;

                        case 'noop':
                        case 'pull': {
                            if (action.from != null) {
                                files[action.filename] = {
                                    sha256: action.from.sha256,
                                    bundle: action.from.bundle
                                };
                            }
                        } break;
                    }
                });
                await Promise.all(p);
            }

            // Publish!
            await Net.post(`${ENV.urls.base}api/files/publish`, files);

            progress.success(T.publishing_done);
        } catch (err) {
            progress.close();
            throw err;
        }
    }

    function shouldBundle(filename) {
        if (bundler == null)
            return false;

        if (filename == 'main.js')
            return true;
        if (filename.startsWith('pages/') && filename.endsWith('.js'))
            return true;

        return false;
    }

    async function fetchCode(filename) {
        let url = `${ENV.urls.base}files/0/${filename}`;
        let response = await Net.fetch(url);

        if (response.ok) {
            let code = await response.text();
            return code;
        } else if (response.status == 404) {
            throw new Error(T.message(`Unknown file '{1}'`, filename));
        } else {
            let err = await Net.readError(response);
            throw new Error(err);
        }
    }
}

export { InstancePublisher }
