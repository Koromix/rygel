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

function AdminController() {
    let self = this;

    let instances;
    let users;
    let backups;

    let selected_instance;
    let selected_permissions;

    this.init = async function() {
        ui.setMenu(renderMenu);
        ui.createPanel('instances', 1, true, renderInstances);
        ui.createPanel('users', 0, true, renderUsers);
        ui.createPanel('backups', 0, false, renderBackups);
    }

    this.hasUnsavedData = function() {
        return false;
    };

    function renderMenu() {
        return html`
            <button class="icon" style="background-position-y: calc(-538px + 1.2em);"
                    @click=${e => self.go(e, '/admin/')}>Admin</button>
            <button class=${'icon' + (ui.isPanelEnabled('instances') ? ' active' : '')}
                    style="background-position-y: calc(-362px + 1.2em);"
                    @click=${ui.wrapAction(e => togglePanel(e, 'instances'))}>Projets</button>
            <button class=${'icon' + (ui.isPanelEnabled('users') ? ' active' : '')}
                    style="background-position-y: calc(-406px + 1.2em);"
                    @click=${ui.wrapAction(e => togglePanel(e, 'users'))}>Utilisateurs</button>
            <button class=${'icon' + (ui.isPanelEnabled('backups') ? ' active' : '')}
                    style="background-position-y: calc(-142px + 1.2em);"
                    @click=${ui.wrapAction(e => togglePanel(e, 'backups'))}>Sauvegardes</button>
            <div style="flex: 1;"></div>
            <div class="drop right" @click=${ui.deployMenu}>
                <button class="icon" style=${'background-position-y: calc(-' + (goupile.isLoggedOnline() ? 450 : 494) + 'px + 1.2em);'}>${profile.username}</button>
                <div>
                    <button @click=${ui.wrapAction(goupile.logout)}>Se déconnecter</button>
                </div>
            </div>
        `;
    }

    function togglePanel(e, key) {
        ui.setPanelState(key, !ui.isPanelEnabled(key));
        return self.go();
    }

    function renderInstances() {
        return html`
            <div class="padded" style="background: #f8f8f8;">
                <div class="ui_quick">
                    <a @click=${ui.wrapAction(runCreateInstanceDialog)}>Créer un projet</a>
                    <div style="flex: 1;"></div>
                    Projets (<a @click=${ui.wrapAction(e => { instances = null; return self.go(); })}>rafraichir</a>)
                </div>

                <table class="ui_table fixed">
                    <colgroup>
                        <col/>
                        <col style="width: 100px;"/>
                        <col style="width: 100px;"/>
                        <col style="width: 100px;"/>
                    </colgroup>

                    <tbody>
                        ${!instances.length ? html`<tr><td colspan="4">Aucun projet</td></tr>` : ''}
                        ${instances.map(instance => html`
                            <tr class=${instance === selected_instance ? 'active' : ''}>
                                <td style="text-align: left;" class=${instance.master != null ? 'child' : ''}>
                                    ${instance.master != null ? html`<span style="color: #ccc;">${instance.master} / </span>${instance.key.replace(/^.*\//, '')}` : ''}
                                    ${instance.master == null ? instance.key : ''}
                                    (<a href=${'/' + instance.key} target="_blank">accès</a>)
                                </td>
                                <td>${instance.master == null ?
                                        html`<a role="button" tabindex="0" @click=${ui.wrapAction(e => runSplitInstanceDialog(e, instance.key))}>Diviser</a>` : ''}</td>
                                <td><a role="button" tabindex="0" href=${makeURL(instance.key)}>Droits</a></td>
                                <td><a role="button" tabindex="0" @click=${ui.wrapAction(e => runEditInstanceDialog(e, instance))}>Configurer</a></td>
                            </tr>
                        `)}
                    </tbody>
                </table>
            </div>
        `;
    }

    function makeURL(instance) {
        let url = util.pasteURL('/admin/', {
            select: instance
        });

        return url;
    }

    function renderUsers() {
        return html`
            <div class="padded" style="flex-grow: 1.5;">
                <div class="ui_quick">
                    <a @click=${ui.wrapAction(runCreateUserDialog)}>Créer un utilisateur</a>
                    <div style="flex: 1;"></div>
                    ${selected_instance != null ? html`
                        Droits sur ${selected_instance.key} (<a href="/admin/">déselectionner</a>)
                        <div style="flex: 1;"></div>
                    ` : ''}
                    Utilisateurs (<a @click=${ui.wrapAction(e => { users = null; selected_permissions = null; return self.go(); })}>rafraichir</a>)
                </div>

                <table class="ui_table fixed">
                    <colgroup>
                        ${selected_instance == null ? html`
                            <col style="width: 160px;"/>
                            <col/>
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
                        ${!users.length ? html`<tr><td colspan=${selected_instance != null ? 4 : 3}>Aucun utilisateur</td></tr>` : ''}
                        ${users.map(user => {
                            let permissions;
                            if (selected_instance != null) {
                                permissions = selected_permissions.permissions[user.userid] || [];
                            } else {
                                permissions = [];
                            }

                            return html`
                                <tr>
                                    <td style=${'text-align: left;' + (user.admin ? ' color: #db0a0a;' : '')}>
                                        ${user.username}
                                        ${user.admin ? html`<span title="Administrateur">♛\uFE0F</span>` : ''}
                                    </td>
                                    ${selected_instance == null && user.email != null ?
                                        html`<td style="text-align: left;"><a href=${'mailto:' + user.email}>${user.email}</a></td>` : ''}
                                    ${selected_instance == null && user.email == null ? html`<td></td>` : ''}
                                    <td><a role="button" tabindex="0"
                                           @click=${ui.wrapAction(e => runEditUserDialog(e, user))}>Modifier</a></td>
                                    ${selected_instance != null ? html`
                                        <td class=${selected_instance.master != null ? 'missing' : ''}
                                            style="white-space: normal;">
                                            ${selected_instance.master == null ? makePermissionsTag(permissions, 'admin_', '#b518bf') : ''}
                                            ${!selected_instance.slaves ? makePermissionsTag(permissions, 'data_', '#258264') : ''}
                                        </td>
                                        <td><a role="button" tabindex="0"
                                               @click=${ui.wrapAction(e => runAssignUserDialog(e, selected_instance, user,
                                                                                                  permissions))}>Assigner</a></td>
                                    ` : ''}
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
            let names = permissions.map(perm => util.capitalize(perm.substr(prefix.length)));
            return html`<span class="ui_tag" style=${'background: ' + background + ';'}>${names.join('|')}</span>`;
        } else {
            return '';
        }
    }

    function renderBackups() {
        return html`
            <div class="padded">
                <div class="ui_quick">
                    <a @click=${ui.wrapAction(createBackup)}>Faire une sauvegarde</a>
                    <div style="flex: 1;"></div>
                    Sauvegardes (<a @click=${ui.wrapAction(e => { backups = null; return self.go(); })}>rafraichir</a>)
                </div>

                <table class="ui_table fixed">
                    <colgroup>
                        <col/>
                        <col style="width: 100px;"/>
                        <col style="width: 100px;"/>
                        <col style="width: 100px;"/>
                    </colgroup>

                    <tbody>
                        ${!backups.length ? html`<tr><td colspan="4">Aucune sauvegarde</td></tr>` : ''}
                        ${backups.map(bak => html`
                            <tr>
                                <td style="text-align: left;"><a href=${util.pasteURL('/admin/api/archives/download', {filename: bak.filename})}
                                                                 download>${bak.filename}</a></td>
                                <td>${util.formatDiskSize(bak.size)}</td>
                                <td><a @click=${ui.wrapAction(e => runRestoreBackupDialog(e, bak.filename))}>Restaurer</a></td>
                                <td><a @click=${ui.wrapAction(e => runDeleteBackupDialog(e, bak.filename))}>Supprimer</a></td>
                            </tr>
                        `)}
                    </tbody>
                </table>

                <div class="ui_quick">
                    <div style="flex: 1;"></div>
                    <a @click=${ui.wrapAction(runUploadBackupDialog)}>Uploader une archive</a>
                </div>
            </div>
        `;
    }

    async function createBackup() {
        let progress = log.progress('Sauvegarde en cours');

        try {
            let response = await net.fetch('/admin/api/archives/create', {method: 'POST'});

            if (response.ok) {
                progress.success('Sauvegarde complétée');

                backups = null;

                self.go();
            } else {
                let err = (await response.text()).trim();
                throw new Error(err);
            }
        } catch (err) {
            progress.close();
            throw err;
        }
    }

    async function runUploadBackupDialog(e) {
        return ui.runDialog(e, 'Envoi d\'archive', (d, resolve, reject) => {
            let archive = d.file('*archive', 'Archive');

            d.action('Envoyer', {disabled: !d.isValid()}, async () => {
                let progress = log.progress('Envoi en cours');

                try {
                    let response = await net.fetch('/admin/api/archives/upload', {
                        method: 'PUT',
                        body: archive.value
                    });

                    if (response.ok) {
                        resolve();
                        progress.success('Envoi complété');

                        backups = null;

                        self.go();
                    } else {
                        let err = (await response.text()).trim();
                        throw new Error(err);
                    }
                } catch (err) {
                    progress.close();
                    reject(err);
                }
            });
        });
    }

    async function runRestoreBackupDialog(e, filename) {
        return ui.runDialog(e, `Restauration de '${filename}'`, (d, resolve, reject) => {
            let key = d.password('*key', 'Clé de restauration');

            d.action('Restaurer', {disabled: !d.isValid()}, async () => {
                let progress = log.progress('Restauration en cours');

                try {
                    let query = new URLSearchParams;
                    query.set('filename', filename);
                    query.set('key', key.value);

                    let response = await net.fetch('/admin/api/archives/restore', {
                        method: 'POST',
                        body: query
                    });

                    if (response.ok) {
                        resolve();
                        progress.success(`Sauvegarde '${filename}' restaurée`);

                        instances = null;
                        users = null;
                        backups = null;

                        self.go();
                    } else {
                        let err = (await response.text()).trim();
                        throw new Error(err);
                    }
                } catch (err) {
                    progress.close();
                    reject(err);
                }
            });
        });
    }

    function runDeleteBackupDialog(e, filename) {
        return ui.runConfirm(e, `Voulez-vous vraiment supprimer l'archive '${filename}' ?`,
                                'Supprimer', async () => {
            let query = new URLSearchParams;
            query.set('filename', filename);

            let response = await net.fetch('/admin/api/archives/delete', {
                method: 'POST',
                body: query
            });

            if (response.ok) {
                log.success(`Sauvegarde '${filename}' supprimée`);

                backups = null;

                self.go();
            } else {
                let err = (await response.text()).trim();
                throw new Error(err);
            }
        });
    }

    this.go = async function(e, url = null, push_history = true) {
        let new_instances = instances;
        let new_users = users;
        let new_backups = backups;
        let new_selected = selected_instance;
        let new_permissions = selected_permissions;

        if (new_instances == null)
            new_instances = await net.fetchJson('/admin/api/instances/list');
        if (ui.isPanelEnabled('users') && new_users == null)
            new_users = await net.fetchJson('/admin/api/users/list');
        if (ui.isPanelEnabled('backups') && new_backups == null) {
            new_backups = await net.fetchJson('/admin/api/archives/list');
            new_backups.sort((bak1, bak2) => util.compareValues(bak1.filename, bak2.filename));
        }

        if (url != null) {
            url = new URL(url, window.location.href);

            if (url.searchParams.has('select')) {
                let select = url.searchParams.get('select');
                new_selected = new_instances.find(instance => instance.key === select);

                if (new_selected == null)
                    throw new Error(`Cannot select instance '${select}' (does not exist)`);
            } else {
                new_selected = null;
            }
        }

        if (new_selected != null)
            new_selected = new_instances.find(instance => instance.key === new_selected.key);
        if (new_selected != null) {
            if (new_permissions == null || new_permissions.key != new_selected.key) {
                let url = util.pasteURL('/admin/api/instances/permissions', {key: new_selected.key});
                let permissions = await net.fetchJson(url);

                new_permissions = {
                    key: new_selected.key,
                    permissions: permissions
                };
            }

            if (selected_instance == null || new_selected.key !== selected_instance.key)
                ui.setPanelState('users', true);
        } else {
            new_permissions = null;
        }

        // Commit
        instances = new_instances;
        users = new_users;
        backups = new_backups;
        selected_instance = new_selected;
        selected_permissions = new_permissions;

        // Update browser URL
        {
            let url = makeURL(selected_instance != null ? selected_instance.key : null);
            goupile.syncHistory(url, push_history);
        }

        ui.render();
    };
    this.go = util.serializeAsync(this.go);

    function runCreateInstanceDialog(e) {
        return ui.runDialog(e, 'Création d\'un projet', (d, resolve, reject) => {
            let key = d.text('*key', 'Clé du projet');
            let name = d.text('name', 'Nom', {value: key.value});
            let demo = d.boolean('demo', 'Ajouter les pages par défaut', {value: true, untoggle: false});

            d.action('Créer', {disabled: !d.isValid()}, async () => {
                let query = new URLSearchParams;
                query.set('key', key.value);
                query.set('title', name.value);
                query.set('demo', 0 + demo.value);

                let response = await net.fetch('/admin/api/instances/create', {
                    method: 'POST',
                    body: query
                });

                if (response.ok) {
                    resolve();
                    log.success(`Projet '${key.value}' créé`);

                    instances = null;
                    selected_permissions = null;

                    self.go();
                } else {
                    let err = (await response.text()).trim();
                    reject(new Error(err));
                }
            });
        });
    }

    function runEditInstanceDialog(e, instance) {
        return ui.runDialog(e, `Modification de ${instance.key}`, (d, resolve, reject) => {
            d.pushOptions({untoggle: false});

            d.tabs('actions', () => {
                d.tab('Modifier', () => {
                    if (instance.master == null) {
                        let title = d.text('*title', 'Nom', {value: instance.config.title});
                        let use_offline = d.boolean('*use_offline', 'Utilisation hors-ligne', {value: instance.config.use_offline});
                        let sync_mode = d.enum('*sync_mode', 'Mode de synchronisation', [
                            ['offline', 'Hors ligne'],
                            ['online', 'En ligne'],
                            ['mirror', 'Mode miroir']
                        ], {value: instance.config.sync_mode});
                        let backup_key = (sync_mode.value == 'offline') ?
                                         d.text('backup_key', 'Clé d\'archivage', {value: instance.config.backup_key}) : {};
                        if (backup_key.value != null) {
                            try {
                                let key = base64ToBytes(backup_key.value);
                                if (key.length !== 32)
                                    throw new Error('Key length must be 32 bytes');
                            } catch (err) {
                                backup_key.error('Clé non valide');
                            }
                        }

                        d.action('Modifier', {disabled: !d.isValid()}, async () => {
                            let query = new URLSearchParams();
                            query.set('key', instance.key);
                            query.set('title', title.value);
                            query.set('use_offline', use_offline.value);
                            query.set('sync_mode', sync_mode.value);
                            if (sync_mode.value === 'offline')
                                query.set('backup_key', backup_key.value || '');

                            let response = await net.fetch('/admin/api/instances/configure', {
                                method: 'POST',
                                body: query
                            });

                            if (response.ok) {
                                resolve();
                                log.success(`Projet '${instance.key}' modifié`);

                                instances = null;

                                self.go();
                            } else {
                                let err = (await response.text()).trim();
                                reject(new Error(err));
                            }
                        });
                    } else {
                        let title = d.text('*title', 'Nom', {value: instance.config.title});

                        d.action('Modifier', {disabled: !d.isValid()}, async () => {
                            let query = new URLSearchParams();
                            query.set('key', instance.key);
                            query.set('title', title.value);

                            let response = await net.fetch('/admin/api/instances/configure', {
                                method: 'POST',
                                body: query
                            });

                            if (response.ok) {
                                resolve();
                                log.success(`Projet '${instance.key}' modifié`);

                                instances = null;

                                self.go();
                            } else {
                                let err = (await response.text()).trim();
                                reject(new Error(err));
                            }
                        });
                    }
                });

                d.tab('Supprimer', () => {
                    d.output(`Voulez-vous vraiment supprimer le projet '${instance.key}' ?`);

                    d.action('Supprimer', {}, async () => {
                        let query = new URLSearchParams;
                        query.set('key', instance.key);

                        let response = await net.fetch('/admin/api/instances/delete', {
                            method: 'POST',
                            body: query
                        });

                        if (response.ok) {
                            resolve();
                            log.success(`Projet '${instance.key}' supprimé`);

                            instances = null;
                            backups = null;

                            self.go();
                        } else {
                            let err = (await response.text()).trim();
                            reject(new Error(err));
                        }
                    });
                });
            });
        });
    }

    function runSplitInstanceDialog(e, master) {
        return ui.runDialog(e, `Division de ${master}`, (d, resolve, reject) => {
            d.calc('instance', 'Projet', master);
            let key = d.text('*key', 'Clé du sous-projet');
            let name = d.text('name', 'Nom', {value: key.value});

            d.action('Créer', {disabled: !d.isValid()}, async () => {
                let full_key = master + '/' + key.value;

                let query = new URLSearchParams;
                query.set('key', full_key);
                query.set('title', name.value);

                let response = await net.fetch('/admin/api/instances/create', {
                    method: 'POST',
                    body: query
                });

                if (response.ok) {
                    resolve();
                    log.success(`Sous-projet '${full_key}' créé`);

                    instances = null;
                    selected_permissions = null;

                    self.go();
                } else {
                    let err = (await response.text()).trim();
                    reject(new Error(err));
                }
            });
        });
    }

    function runCreateUserDialog(e) {
        return ui.runDialog(e, 'Création d\'un utilisateur', (d, resolve, reject) => {
            let username = d.text('*username', 'Nom d\'utilisateur');

            let password = d.password('*password', 'Mot de passe');
            let password2 = d.password('password2', null, {
                placeholder: 'Confirmation'
            });
            if (password.value != null && password2.value != null && password.value !== password2.value)
                password2.error('Les mots de passe sont différents');

            let email = d.text('email', 'Courriel');
            if (email.value != null && !email.value.includes('@'))
                email.error('Format non valide');
            let admin = d.boolean('*admin', 'Administrateur', {value: false, untoggle: false});

            d.action('Créer', {disabled: !d.isValid()}, async () => {
                let query = new URLSearchParams;
                query.set('username', username.value);
                query.set('password', password.value);
                if (email.value != null)
                    query.set('email', email.value);
                query.set('admin', admin.value ? 1 : 0);

                let response = await net.fetch('/admin/api/users/create', {
                    method: 'POST',
                    body: query
                });

                if (response.ok) {
                    resolve();
                    log.success(`Utilisateur '${username.value}' créé`);

                    users = null;
                    selected_permissions = null;

                    self.go();
                } else {
                    let err = (await response.text()).trim();
                    reject(new Error(err));
                }
            });
        });
    }

    function runAssignUserDialog(e, instance, user, prev_permissions) {
        return ui.runDialog(e, `Droits de ${user.username} sur ${instance.key}`, (d, resolve, reject) => {
            d.section("Développement", () => {
                let props = ENV.permissions.filter(perm => perm.startsWith('admin_')).map(makePermissionProp);
                let value = (instance.master == null) ? prev_permissions.filter(perm => perm.startsWith('admin_')) : null;

                d.multiCheck('admin_permissions', '', props, {
                    value: value,
                    disabled: instance.master != null
                });
            }, {color: '#b518bf'});
            d.sameLine(true); d.section("Enregistrements", () => {
                let props = ENV.permissions.filter(perm => perm.startsWith('data_')).map(makePermissionProp);
                let value = !instance.slaves ? prev_permissions.filter(perm => perm.startsWith('data_')) : null;

                d.multiCheck('data_permissions', '', props, {
                    value: value,
                    disabled: instance.slaves > 0
                });
            }, {color: '#258264'});

            // Now regroup permissions
            let permissions = [...d.value("admin_permissions", []), ...d.value("data_permissions", [])];

            d.action('Modifier', {disabled: !d.isValid()}, async () => {
                let query = new URLSearchParams;
                query.set('instance', instance.key);
                query.set('userid', user.userid);
                query.set('permissions', permissions.join(','));

                let response = await net.fetch('/admin/api/instances/assign', {
                    method: 'POST',
                    body: query
                });

                if (response.ok) {
                    resolve();
                    log.success(`Droits de '${user.username}' sur le projet '${instance.key}' ${permissions.length ? 'modifiés' : 'supprimés'}`);

                    selected_permissions = null;

                    self.go();
                } else {
                    let err = (await response.text()).trim();
                    reject(new Error(err));
                }
            });
        });
    }

    function makePermissionProp(perm) {
        let name = perm.substr(perm.indexOf('_') + 1);
        return [perm, util.capitalize(name)];
    }

    function runEditUserDialog(e, user) {
        return ui.runDialog(e, `Modification de ${user.username}`, (d, resolve, reject) => {
            d.pushOptions({untoggle: false});

            d.tabs('actions', () => {
                d.tab('Modifier', () => {
                    let username = d.text('username', 'Nom d\'utilisateur', {value: user.username});

                    let password = d.password('password', 'Mot de passe');
                    let password2 = d.password('password2', null, {
                        placeholder: 'Confirmation',
                        help: 'Laissez vide pour ne pas modifier'
                    });
                    if (password.value != null && password2.value != null && password.value !== password2.value)
                        password2.error('Les mots de passe sont différents');

                    let email = d.text('email', 'Courriel', {value: user.email});
                    if (email.value != null && !email.value.includes('@'))
                        email.error('Format non valide');

                    let admin = d.boolean('*admin', 'Administrateur', {value: user.admin});

                    d.action('Modifier', {disabled: !d.isValid()}, async () => {
                        let query = new URLSearchParams;
                        query.set('userid', user.userid);
                        if (username.value != null)
                            query.set('username', username.value);
                        if (password.value != null)
                            query.set('password', password.value);
                        if (email.value != null)
                            query.set('email', email.value);
                        query.set('admin', admin.value ? 1 : 0);

                        let response = await net.fetch('/admin/api/users/edit', {
                            method: 'POST',
                            body: query
                        });

                        if (response.ok) {
                            resolve();
                            log.success(`Utilisateur '${username.value}' modifié`);

                            users = null;

                            self.go();
                        } else {
                            let err = (await response.text()).trim();
                            reject(new Error(err));
                        }
                    });
                });

                d.tab('Supprimer', () => {
                    d.output(`Voulez-vous vraiment supprimer l'utilisateur '${user.username}' ?`);

                    d.action('Supprimer', {}, async () => {
                        let query = new URLSearchParams;
                        query.set('userid', user.userid);

                        let response = await net.fetch('/admin/api/users/delete', {
                            method: 'POST',
                            body: query
                        });

                        if (response.ok) {
                            resolve();
                            log.success(`Utilisateur '${user.username}' supprimé`);

                            users = null;

                            self.go();
                        } else {
                            let err = (await response.text()).trim();
                            throw new Error(err);
                        }
                    });
                });
            });
        });
    }
};
