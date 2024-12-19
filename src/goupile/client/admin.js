// Copyright (C) 2024  Niels Martignène <niels.martignene@protonmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

import { render, html } from '../../../vendor/lit-html/lit-html.bundle.js';
import { Util, Log, Net, Mutex } from '../../web/core/common.js';
import { Base64 } from '../../web/core/mixer.js';
import * as goupile from './goupile.js';
import { profile } from './goupile.js';
import * as UI from './ui.js';

import './admin.css';

let instances;
let users;
let archives;

let selected_instance;
let all_users;
let selected_permissions;

// Explicit mutex to serialize (global) state-changing functions
let mutex = new Mutex;

async function init(fallback) {
    UI.setMenu(renderMenu);

    UI.createPanel('instances', 0, renderInstances);
    UI.createPanel('users', 1, renderUsers);
    if (profile.root)
        UI.createPanel('archives', 1, renderArchives);

    UI.setPanels(['instances','users']);
}

function renderMenu() {
    return html`
        <nav class="ui_toolbar" id="ui_top" style="z-index: 999999;">
            <button class="icon lines" @click=${UI.wrap(e => go(e, '/admin/'))}>Admin</button>
            <button class=${'icon instances' + (UI.isPanelActive('instances') ? ' active' : '')}
                    @click=${UI.wrap(e => togglePanel('instances'))}>Projets</button>
            <button class=${'icon users' + (UI.isPanelActive('users') ? ' active' : '')}
                    @click=${UI.wrap(e => togglePanel('users'))}>Utilisateurs</button>
            ${profile.root ? html`
                <button class=${'icon archives' + (UI.isPanelActive('archives') ? ' active' : '')}
                        @click=${UI.wrap(e => togglePanel('archives'))}>Archives</button>
            ` : ''}
            <div style="flex: 1;"></div>
            <div class="drop right" @click=${UI.deployMenu}>
                <button class=${goupile.isLoggedOnline() ? 'icon online' : 'icon offline'}>${profile.username}</button>
                <div>
                    <button @click=${UI.wrap(goupile.runChangePasswordDialog)}>Modifier mon mot de passe</button>
                    <button @click=${UI.wrap(goupile.runResetTOTP)}>Configurer la double authentification</button>
                    <hr/>
                    <button @click=${UI.wrap(goupile.logout)}>Se déconnecter</button>
                </div>
            </div>
        </nav>
    `;
}

function togglePanel(key, enable = null) {
    UI.togglePanel(key, enable);
    return go();
}

function renderInstances() {
    return html`
        <div class="padded" style="background: #f8f8f8;">
            <div class="ui_quick">
                ${profile.root ? html`<a @click=${UI.wrap(runCreateInstanceDialog)}>Créer un projet</a>` : ''}
                <div style="flex: 1;"></div>
                Projets (<a @click=${UI.wrap(e => { instances = null; return go(); })}>rafraichir</a>)
            </div>

            <table class="ui_table fixed">
                <colgroup>
                    <col/>
                    <col style="width: 100px;"/>
                    <col style="width: 100px;"/>
                    <col style="width: 100px;"/>
                    <col style="width: 100px;"/>
                </colgroup>

                <tbody>
                    ${!instances.length ? html`<tr><td colspan="5">Aucun projet</td></tr>` : ''}
                    ${instances.map(instance => html`
                        <tr class=${instance === selected_instance ? 'active' : ''}>
                            <td style="text-align: left;" class=${instance.master != null ? 'child' : ''}>
                                ${instance.master == null && instance.legacy ? html`<span class="ui_tag" style="background: #bbb;">v2</span>`  : ''}
                                ${instance.master != null ? html`<span style="color: #ccc;">${instance.master} / </span>${instance.key.replace(/^.*\//, '')}` : ''}
                                ${instance.master == null ? instance.key : ''}
                                (<a href=${'/' + instance.key} target="_blank">accès</a>)
                            </td>
                            <td><a role="button" tabindex="0" @click=${UI.wrap(e => runSplitInstanceDialog(e, instance.key))}>Diviser</a></td>
                            <td><a role="button" tabindex="0" href=${Util.pasteURL('/admin/', { select: instance.key })}
                                   @click=${UI.wrap(instance != selected_instance ? (e => togglePanel('users', true))
                                                                                        : (e => { go(e, '/admin/'); e.preventDefault(); }))}>Droits</a></td>
                            <td><a role="button" tabindex="0" @click=${UI.wrap(e => runConfigureInstanceDialog(e, instance))}>Configurer</a></td>
                            <td>${profile.root || instance.master != null ? html`<a role="button" tabindex="0" @click=${UI.wrap(e => runDeleteInstanceDialog(e, instance))}>Supprimer</a>` : ''}</td>
                        </tr>
                    `)}
                </tbody>
            </table>
        </div>
    `;
}

function renderUsers() {
    let visible_users = users.filter(user => selected_instance == null || all_users || selected_permissions.permissions[user.userid]);

    return html`
        <div class="padded" style="flex-grow: 1.5;">
            <div class="ui_quick">
                <a @click=${UI.wrap(runCreateUserDialog)}>Créer un utilisateur</a>
                <div style="flex: 1;"></div>
                ${selected_instance != null ? html`
                    <div class="fm_check">
                        <input id="all_users" type="checkbox" .checked=${all_users}
                               @change=${UI.wrap(e => { all_users = e.target.checked; return go(); })} />
                        <label for="all_users">Afficher tout le monde</label>
                    </div>
                    <div style="flex: 1;"></div>
                ` : ''}
                Utilisateurs (<a @click=${UI.wrap(e => { users = null; selected_permissions = null; return go(); })}>rafraichir</a>)
            </div>

            <table class="ui_table fixed">
                <colgroup>
                    ${selected_instance == null ? html`
                        <col style="width: 160px;"/>
                        <col/>
                        <col style="width: 160px;"/>
                        <col style="width: 100px;"/>
                        <col style="width: 100px;"/>
                    ` : ''}
                    ${selected_instance != null ? html`
                        <col style="width: 160px;"/>
                        <col style="width: 100px;"/>
                        <col/>
                        <col style="width: 100px;"/>
                    ` : ''}
                </colgroup>

                <tbody>
                    ${!visible_users.length ? html`<tr><td colspan=${selected_instance != null ? 5 : 4}>Aucun utilisateur</td></tr>` : ''}
                    ${visible_users.map(user => {
                        let permissions;
                        if (selected_instance != null) {
                            permissions = selected_permissions.permissions[user.userid] || [];
                        } else {
                            permissions = [];
                        }

                        return html`
                            <tr>
                                <td style=${'text-align: left;' + (user.root ? ' color: #db0a0a;' : '')}>
                                    ${user.username}
                                    ${user.root ? html`<span title="Super-administrateur">♛\uFE0E</span>` : ''}
                                </td>
                                ${selected_instance == null ? html`
                                    <td style="text-align: left;">${user.email != null ? html`<a href=${'mailto:' + user.email}>${user.email}</a>` : ''}</td>
                                    <td style="text-align: left;">${user.phone != null ? html`<a href=${'tel:' + user.phone}>${user.phone}</a>` : ''}</td>
                                ` : ''}
                                <td><a role="button" tabindex="0"
                                       @click=${UI.wrap(e => runEditUserDialog(e, user))}>Modifier</a></td>
                                ${selected_instance != null ? html`
                                    <td class=${!permissions.length ? 'ui_sub' : ''}
                                        style="white-space: normal;">
                                        ${selected_instance.master == null ? makePermissionsTag(permissions, 'build_', '#b518bf') : ''}
                                        ${!selected_instance.slaves ? makePermissionsTag(permissions, 'data_', '#258264') : ''}
                                        ${!selected_instance.slaves ? makePermissionsTag(permissions, 'misc_', '#c97f1a') : ''}
                                        ${!permissions.length ? 'Non assigné' : ''}
                                    </td>
                                    <td><a role="button" tabindex="0"
                                           @click=${UI.wrap(e => runAssignUserDialog(e, selected_instance, user,
                                                                                              permissions))}>Assigner</a></td>
                                ` : ''}
                                ${selected_instance == null ?
                                    html`<td><a role="button" tabindex="0"
                                                @click=${UI.wrap(e => runDeleteUserDialog(e, user))}>Supprimer</a></td>` : ''}
                            </tr>
                        `;
                    })}
                </tbody>
            </table>
        </div>
    `;
}

function makePermissionsTag(permissions, prefix, background) {
    permissions = permissions.filter(perm => perm.startsWith(prefix));

    if (permissions.length) {
        let names = permissions.map(perm => Util.capitalize(perm.substr(prefix.length)));
        return html`<span class="ui_tag" style=${'background: ' + background + ';'}>${names.join('|')}</span>`;
    } else {
        return '';
    }
}

function renderArchives() {
    return html`
        <div class="padded">
            <div style="margin-bottom: 2em;">
                <p>Les archives créées manuellement ou automatiquement (chaque jour) sont gardées localement
                pour une <span style="color: red; font-weight: bold;">période de ${ENV.retention} jours</span>.
                Vous pouvez les télécharger et les enregistrer sur vos propres supports de stockage.</p>

                <p>N'oubliez pas que <b>sans la clé de déchiffrement</b> qui vous a été confiée lors de l'ouverture
                du domaine, le contenu de ces archives ne peut pas être restauré.</p>
            </div>

            <div class="ui_quick">
                <a @click=${UI.wrap(createBackup)}>Créer une archive</a>
                <div style="flex: 1;"></div>
                Archives (<a @click=${UI.wrap(e => { archives = null; return go(); })}>rafraichir</a>)
            </div>

            <table class="ui_table fixed">
                <colgroup>
                    <col/>
                    <col style="width: 100px;"/>
                    <col style="width: 100px;"/>
                    <col style="width: 100px;"/>
                </colgroup>

                <tbody>
                    ${!archives.length ? html`<tr><td colspan="4">Aucune archive</td></tr>` : ''}
                    ${archives.map(archive => html`
                        <tr>
                            <td style="text-align: left;"><a href=${'/admin/api/archives/files/' + archive.filename}
                                                             download>${archive.filename}</a></td>
                            <td>${Util.formatDiskSize(archive.size)}</td>
                            <td><a @click=${UI.wrap(e => runRestoreBackupDialog(e, archive.filename))}>Restaurer</a></td>
                            <td><a @click=${UI.wrap(e => runDeleteBackupDialog(e, archive.filename))}>Supprimer</a></td>
                        </tr>
                    `)}
                </tbody>
            </table>

            <div class="ui_quick">
                <div style="flex: 1;"></div>
                <a @click=${UI.wrap(runUploadBackupDialog)}>Uploader une archive</a>
            </div>
        </div>
    `;
}

async function createBackup() {
    let progress = Log.progress('Archivage en cours');

    try {
        await Net.post('/admin/api/archives/create', null, { timeout: 180000 });

        progress.success('Archivage complété');
        archives = null;

        go();
    } catch (err) {
        progress.close();
        throw err;
    }
}

async function runUploadBackupDialog(e) {
    return UI.dialog(e, 'Envoi d\'archive', {}, (d, resolve, reject) => {
        d.file('*archive', 'Archive');

        d.action('Envoyer', { disabled: !d.isValid() }, async () => {
            let progress = Log.progress('Envoi en cours');

            try {
                let url = '/admin/api/archives/files/' + d.values.archive.name;

                let response = await Net.fetch(url, {
                    method: 'PUT',
                    body: d.values.archive,
                    timeout: null
                });

                if (response.ok) {
                    resolve();
                    progress.success('Envoi complété');

                    archives = null;

                    go();
                } else {
                    let err = await Net.readError(response);
                    throw new Error(err);
                }
            } catch (err) {
                progress.close();

                Log.error(err);
                d.refresh();
            }
        });
    });
}

async function runRestoreBackupDialog(e, filename) {
    return UI.dialog(e, `Restauration de '${filename}'`, {}, (d, resolve, reject) => {
        d.password('*key', 'Clé de restauration');
        d.boolean('*restore_users', 'Restaurer les utilisateurs et leurs droits', { value: false, untoggle: false });

        d.action('Restaurer', { disabled: !d.isValid() }, async () => {
            let progress = Log.progress('Restauration en cours');

            try {
                await Net.post('/admin/api/archives/restore', {
                    filename: filename,
                    key: d.values.key,
                    users: d.values.restore_users
                });

                resolve();
                progress.success(`Archive '${filename}' restaurée`);

                instances = null;
                users = null;
                archives = null;

                go();
            } catch (err) {
                progress.close();

                Log.error(err);
                d.refresh();
            }
        });
    });
}

function runDeleteBackupDialog(e, filename) {
    return UI.confirm(e, `Voulez-vous vraiment supprimer l'archive '${filename}' ?`,
                            'Supprimer', async () => {
        await Net.post('/admin/api/archives/delete', { filename: filename });

        Log.success(`Archive '${filename}' supprimée`);
        archives = null;

        go();
    });
}

async function go(e, url = null, options = {}) {
    await mutex.run(async () => {
        options = Object.assign({ push_history: true }, options);

        let new_instances = instances;
        let new_users = users;
        let new_archives = archives;
        let new_selected = selected_instance;
        let new_all_users = all_users;
        let new_permissions = selected_permissions;
        let explicit_panels = false;

        if (new_instances == null)
            new_instances = await Net.get('/admin/api/instances/list');
        if (new_users == null)
            new_users = await Net.get('/admin/api/users/list');

        if (url != null) {
            if (!(url instanceof URL))
                url = new URL(url, window.location.href);
            goupile.setCurrentHash(url.hash);

            let panels = url.searchParams.get('p');

            if (panels) {
                panels = panels.split('|').filter(panel => panel);

                if (panels.length) {
                    UI.setPanels(panels);
                    explicit_panels = true;
                }
            }

            if (url.searchParams.has('select')) {
                let select = url.searchParams.get('select');
                new_selected = new_instances.find(instance => instance.key === select);

                if (new_selected == null)
                    throw new Error(`Cannot select instance '${select}' (does not exist)`);
            } else {
                new_selected = null;
            }

            if (url.searchParams.has('all')) {
                new_all_users = (url.searchParams.get('all') == 1);
            } else if (new_all_users == null) {
                new_all_users = true;
            }
        }

        if (UI.isPanelActive('archives') && new_archives == null) {
            new_archives = await Net.get('/admin/api/archives/list');
            new_archives.sort(Util.makeComparator(archive => archive.filename));
        }

        if (new_selected != null)
            new_selected = new_instances.find(instance => instance.key === new_selected.key);
        if (new_selected != null) {
            if (new_permissions == null || new_permissions.key != new_selected.key) {
                let url = Util.pasteURL('/admin/api/instances/permissions', { instance: new_selected.key });
                let permissions = await Net.get(url);

                new_permissions = {
                    key: new_selected.key,
                    permissions: permissions
                };
            }

            if (!explicit_panels && (selected_instance == null ||
                                     new_selected.key !== selected_instance.key))
                UI.togglePanel('users', true);
        } else {
            new_permissions = null;
        }

        // Commit
        instances = new_instances;
        users = new_users;
        archives = new_archives;
        selected_instance = new_selected;
        selected_permissions = new_permissions;
        all_users = new_all_users;

        // Update browser URL
        {
            let params = {};

            if (selected_instance != null)
                params.select = selected_instance.key;
            params.all = 0 + all_users;
            params.p = UI.getPanels().join('|') || null;

            let url = Util.pasteURL('/admin/', params);
            goupile.syncHistory(url, options.push_history);
        }

        UI.draw();
    });
}

function runCreateInstanceDialog(e) {
    return UI.dialog(e, 'Création d\'un projet', {}, (d, resolve, reject) => {
        d.text('*key', 'Clé du projet', {
            help: [
                'Longueur maximale : 24 caractères',
                'Caractères autorisés : a-z (minuscules), 0-9 et \'-\''
            ]
        });
        d.text('name', 'Nom', { value: d.values.key });
        d.boolean('demo', 'Paramétrer les pages de démonstration', { value: true, untoggle: false });

        d.action('Créer', { disabled: !d.isValid() }, async () => {
            try {
                await Net.post('/admin/api/instances/create', {
                    key: d.values.key,
                    name: d.values.name,
                    demo: d.values.demo
                });

                resolve();
                Log.success(`Projet '${d.values.key}' créé`);

                instances = null;
                selected_permissions = null;

                let url = Util.pasteURL('/admin/', { select: d.values.key });
                go(null, url);
            } catch (err) {
                Log.error(err);
                d.refresh();
            }
        });
    });
}

function runConfigureInstanceDialog(e, instance) {
    return UI.dialog(e, `Configuration de ${instance.key}`, {}, (d, resolve, reject) => {
        d.pushOptions({ untoggle: false });

        if (instance.master == null) {
            d.tabs('tabs', () => {
                d.tab('Basique', () => {
                    d.text('*name', 'Nom', { value: instance.config.name });
                    d.boolean('*use_offline', 'Utilisation hors-ligne', { value: instance.config.use_offline });
                    d.boolean('*allow_guests', 'Autoriser les invités', { value: instance.config.allow_guests });
                });

                d.tab('Avancé', () => {
                    d.boolean('*data_remote', 'Données en ligne', { value: instance.config.data_remote });

                    d.text('token_key', 'Session par token', { value: instance.config.token_key });
                    if (d.values.token_key != null && !checkCryptoKey(d.values.token_key))
                        d.error('token_key', 'Format de clé non valide');
                    d.text('auto_key', 'Session de requête', { value: instance.config.auto_key });

                    d.number('fs_version', 'Version FS', {
                        suffix: 'Actuelle : ' + instance.config.fs_version,
                        help: 'Attention, ceci remet les développements à zéro'
                    });
                });

                if (profile.root && instance.legacy) {
                    d.tab('Migration', () => {
                        d.output(html`
                            <p>Attention, la migration en V3 est susceptible d'entrainer des <span style="color: red; font-weight: bold;">bugs et une perte de données</span> !</p>
                            <button type="button" @click=${UI.wrap(e => runMigrateDialog(e, instance))}>Migrer en V3</button>
                        `);
                    });
                }
            });

            d.action('Configurer', { disabled: !d.isValid() }, async () => {
                try {
                    await Net.post('/admin/api/instances/configure', {
                        instance: instance.key,
                        name: d.values.name,
                        use_offline: d.values.use_offline,
                        data_remote: d.values.data_remote,
                        token_key: d.values.token_key,
                        auto_key: d.values.auto_key,
                        allow_guests: d.values.allow_guests,
                        fs_version: d.values.fs_version
                    });

                    resolve();
                    Log.success(`Projet '${instance.key}' modifié`);

                    instances = null;

                    go();
                } catch (err) {
                    Log.error(err);
                    d.refresh();
                }
            });
        } else {
            d.text('*name', 'Nom', { value: instance.config.name });

            d.action('Configurer', { disabled: !d.isValid() }, async () => {
                try {
                    await Net.post('/admin/api/instances/configure', {
                        instance: instance.key,
                        name: d.values.name
                    });

                    resolve();
                    Log.success(`Projet '${instance.key}' modifié`);

                    instances = null;

                    go();
                } catch (err) {
                    Log.error(err);
                    d.refresh();
                }
            });
        }
    });
}

function checkCryptoKey(str) {
    try {
        let key = Base64.toBytes(str);
        return key.length === 32;
    } catch (err) {
        return false;
    }
}

async function runMigrateDialog(e, instance) {
    return UI.dialog(e, `Migration de ${instance.key}`, {}, (d, resolve, reject) => {
        d.output(`Voulez-vous vraiment migrer le projet '${instance.key}' ?`);

        d.action('Migrer', {}, async () => {
            try {
                await Net.post('/admin/api/instances/migrate', {
                    instance: instance.key
                }, { timeout: 180000 });

                resolve();
                Log.success(`Projet '${instance.key}' migré`);

                instances = null;

                go();
            } catch (err) {
                Log.error(err);
                d.refresh();
            }
        });
    });
}

function runDeleteInstanceDialog(e, instance) {
    return UI.dialog(e, `Suppression de ${instance.key}`, {}, (d, resolve, reject) => {
        d.output(`Voulez-vous vraiment supprimer le projet '${instance.key}' ?`);

        d.action('Supprimer', {}, async () => {
            try {
                await Net.post('/admin/api/instances/delete', {
                    instance: instance.key
                }, { timeout: 180000 });

                resolve();
                Log.success(`Projet '${instance.key}' supprimé`);

                instances = null;
                archives = null;

                go();
            } catch (err) {
                Log.error(err);
                d.refresh();
            }
        });
    });
}

function runSplitInstanceDialog(e, master) {
    return UI.dialog(e, `Division de ${master}`, {}, (d, resolve, reject) => {
        d.calc('instance', 'Projet', master);
        d.text('*key', 'Clé du sous-projet');
        d.text('name', 'Nom', { value: d.values.key });

        d.action('Créer', { disabled: !d.isValid() }, async () => {
            let full_key = master + '/' + d.values.key;

            try {
                await Net.post('/admin/api/instances/create', {
                    key: full_key,
                    name: d.values.name
                });

                resolve();
                Log.success(`Sous-projet '${full_key}' créé`);

                instances = null;
                selected_permissions = null;

                let url = Util.pasteURL('/admin/', { select: full_key });
                go(null, url);
            } catch (err) {
                Log.error(err);
                d.refresh();
            }
        });
    });
}

function runCreateUserDialog(e) {
    return UI.dialog(e, 'Création d\'un utilisateur', {}, (d, resolve, reject) => {
        let username = d.text('*username', 'Nom d\'utilisateur');

        d.password('*password', 'Mot de passe');
        d.password('*password2', null, { placeholder: 'Confirmation' });
        d.boolean('*change_password', 'Exiger un changement de mot de passe', {
            value: true, untoggle: false
        });
        if (d.values.password != null && d.values.password2 != null) {
            if (d.values.password !== d.values.password2) {
                d.error('password2', 'Les mots de passe sont différents');
            } else if (d.values.password.length < 8) {
                d.error('password2', 'Mot de passe trop court');
            }
        }
        d.boolean('*confirm', 'Authentification à 2 facteurs', {
            value: false, untoggle: false
        });

        d.text('email', 'Courriel');
        if (d.values.email != null && !d.values.email.includes('@'))
            d.error('email', 'Format non valide');
        d.text('phone', 'Téléphone');
        if (d.values.phone != null && !d.values.phone.startsWith('+'))
            d.error('phone', 'Format non valide (préfixe obligatoire)');

        if (profile.root)
            d.boolean('*root', 'Super-administrateur', { value: false, untoggle: false });

        d.action('Créer', { disabled: !d.isValid() }, async () => {
            try {
                await Net.post('/admin/api/users/create', {
                    username: d.values.username,
                    password: d.values.password,
                    change_password: d.values.change_password,
                    confirm: d.values.confirm,
                    email: d.values.email,
                    phone: d.values.phone,
                    root: profile.root ? d.values.root : false
                });

                resolve();
                Log.success(`Utilisateur '${d.values.username}' créé`);

                users = null;
                selected_permissions = null;

                go();
            } catch (err) {
                Log.error(err);
                d.refresh();
            }
        });
    });
}

function runAssignUserDialog(e, instance, user, prev_permissions) {
    return UI.dialog(e, `Droits de ${user.username} sur ${instance.key}`, {}, (d, resolve, reject) => {
        d.section('Projet', () => {
            let props = listPermissions('build_', instance.legacy);
            let value = (instance.master == null) ? prev_permissions.filter(perm => perm.startsWith('build_')) : null;

            d.multiCheck('build_permissions', null, props, {
                value: value,
                disabled: instance.master != null
            });
        }, { color: '#b518bf' });
        d.sameLine(true); d.section('Données', () => {
            let props = listPermissions('data_', instance.legacy);
            let value = !instance.slaves ? prev_permissions.filter(perm => perm.startsWith('data_')) : null;

            d.multiCheck('data_permissions', null, props, {
                value: value,
                disabled: instance.slaves > 0
            });
        }, { color: '#258264' });
        d.sameLine(true); d.section('Autres', () => {
            let props = listPermissions('misc_', instance.legacy);
            let value = !instance.slaves ? prev_permissions.filter(perm => perm.startsWith('misc_')) : null;

            d.multiCheck('misc_permissions', null, props, {
                value: value,
                disabled: instance.slaves > 0
            });
        }, { color: '#c97f1a' });

        // Now regroup permissions
        let permissions = [
            ...(d.values.build_permissions || []),
            ...(d.values.data_permissions || []),
            ...(d.values.misc_permissions || [])
        ];

        d.action('Modifier', { disabled: !d.isValid() }, async () => {
            try {
                await Net.post('/admin/api/instances/assign', {
                    instance: instance.key,
                    userid: user.userid,
                    permissions: permissions
                });

                resolve();
                Log.success(`Droits de '${user.username}' sur le projet '${instance.key}' ${permissions.length ? 'modifiés' : 'supprimés'}`);

                selected_permissions = null;

                go();
            } catch (err) {
                Log.error(err);
                d.refresh();
            }
        });
    });
}

function listPermissions(prefix, legacy) {
    let props = [];

    for (let perm in ENV.permissions) {
        if (legacy && !ENV.permissions[perm])
            continue;
        if (!perm.startsWith(prefix))
            continue;

        let name = Util.capitalize(perm.substr(prefix.length));
        let prop = [perm, name];

        props.push(prop);
    }

    return props;
}

function runEditUserDialog(e, user) {
    return UI.dialog(e, `Modification de ${user.username}`, {}, (d, resolve, reject) => {
        d.pushOptions({ untoggle: false });

        d.tabs('tabs', () => {
            d.tab('Identité', () => {
                d.text('username', 'Nom d\'utilisateur', { value: user.username });

                d.text('email', 'Courriel', { value: user.email });
                if (d.values.email != null && !d.values.email.includes('@'))
                    d.error('email', 'Format non valide');
                d.text('phone', 'Téléphone', { value: user.phone });
                if (d.values.phone != null && !d.values.phone.startsWith('+'))
                    d.error('phone', 'Format non valide (préfixe obligatoire)');

                if (profile.root)
                    d.boolean('*root', 'Super-administrateur', { value: user.root });
            });

            d.tab('Sécurité', () => {
                d.password('password', 'Mot de passe');
                d.password('password2', null, {
                    placeholder: 'Confirmation',
                    help: 'Laissez vide pour ne pas modifier',
                    mandatory: d.values.password != null
                });
                d.boolean('change_password', 'Exiger un changement de mot de passe', {
                    value: d.values.password != null,
                    untoggle: false
                });
                if (d.values.password != null && d.values.password2 != null) {
                    if (d.values.password !== d.values.password2) {
                        d.error('password2', 'Les mots de passe sont différents');
                    } else if (d.values.password.length < 8) {
                        d.error('password2', 'Mot de passe trop court');
                    }
                }
                d.boolean('confirm', 'Authentification à 2 facteurs', {
                    value: user.confirm, untoggle: false
                });
                d.boolean('reset_secret', 'Réinitialiser le secret TOTP', {
                    value: !user.confirm,
                    disabled: !d.values.confirm,
                    untoggle: false
                });
            });
        });

        d.action('Modifier', { disabled: !d.isValid() }, async () => {
            try {
                await Net.post('/admin/api/users/edit', {
                    userid: user.userid,
                    username: d.values.username,
                    password: d.values.password,
                    change_password: d.values.change_password,
                    confirm: d.values.confirm,
                    reset_secret: d.values.reset_secret,
                    email: d.values.email ?? '',
                    phone: d.values.phone ?? '',
                    root: profile.root ? d.values.root : false
                });

                resolve();
                Log.success(`Utilisateur '${d.values.username}' modifié`);

                users = null;

                go();
            } catch (err) {
                Log.error(err);
                d.refresh();
            }
        });
    });
}

function runDeleteUserDialog(e, user) {
    return UI.dialog(e, `Suppression de ${user.username}`, {}, (d, resolve, reject) => {
        d.output(`Voulez-vous vraiment supprimer l'utilisateur '${user.username}' ?`);

        d.action('Supprimer', {}, async () => {
            await Net.post('/admin/api/users/delete', { userid: user.userid });

            resolve();
            Log.success(`Utilisateur '${user.username}' supprimé`);

            users = null;

            go();
        });
    });
}

export {
    init,
    go
}
