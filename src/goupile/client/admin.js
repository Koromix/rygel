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
    let archives;

    let selected_instance;
    let selected_permissions;

    this.init = async function() {
        ui.setMenu(renderMenu);
        ui.createPanel('instances', 1, true, renderInstances);
        ui.createPanel('users', 0, true, renderUsers);
        ui.createPanel('archives', 0, false, renderArchives);
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
            <button class=${'icon' + (ui.isPanelEnabled('archives') ? ' active' : '')}
                    style="background-position-y: calc(-142px + 1.2em);"
                    @click=${ui.wrapAction(e => togglePanel(e, 'archives'))}>Archives</button>
            <div style="flex: 1;"></div>
            <div class="drop right" @click=${ui.deployMenu}>
                <button class="icon" style=${'background-position-y: calc(-' + (goupile.isLoggedOnline() ? 450 : 494) + 'px + 1.2em);'}>${profile.username}</button>
                <div>
                    <button @click=${ui.wrapAction(goupile.runChangePasswordDialog)}>Changer le mot de passe</button>
                    <button @click=${ui.wrapAction(goupile.runResetTOTP)}>Changer les codes TOTP</button>
                    <hr/>
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
                                <td><a role="button" tabindex="0" href=${util.pasteURL('/admin/', { select: instance.key })} @click=${e => ui.setPanelState('users', true)}>Droits</a></td>
                                <td><a role="button" tabindex="0" @click=${ui.wrapAction(e => runConfigureInstanceDialog(e, instance))}>Configurer</a></td>
                            </tr>
                        `)}
                    </tbody>
                </table>
            </div>
        `;
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
                            <col style="width: 160px;"/>
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
                                    ${selected_instance == null ? html`
                                        <td style="text-align: left;">${user.email != null ? html`<a href=${'mailto:' + user.email}>${user.email}</a>` : ''}</td>
                                        <td style="text-align: left;">${user.phone != null ? html`<a href=${'tel:' + user.phone}>${user.phone}</a>` : ''}</td>
                                    ` : ''}
                                    <td><a role="button" tabindex="0"
                                           @click=${ui.wrapAction(e => runEditUserDialog(e, user))}>Modifier</a></td>
                                    ${selected_instance != null ? html`
                                        <td class=${selected_instance.master != null ? 'missing' : ''}
                                            style="white-space: normal;">
                                            ${selected_instance.config.default_userid === user.userid ?
                                                html`<span class="ui_tag" style="background: #db0a0a;">Défaut</span>` : ''}
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

    function renderArchives() {
        return html`
            <div class="padded">
                <div class="ui_quick">
                    <a @click=${ui.wrapAction(createBackup)}>Créer une archive</a>
                    <div style="flex: 1;"></div>
                    Archives (<a @click=${ui.wrapAction(e => { archives = null; return self.go(); })}>rafraichir</a>)
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
                                <td style="text-align: left;"><a href=${util.pasteURL('/admin/api/archives/download', {filename: archive.filename})}
                                                                 download>${archive.filename}</a></td>
                                <td>${util.formatDiskSize(archive.size)}</td>
                                <td><a @click=${ui.wrapAction(e => runRestoreBackupDialog(e, archive.filename))}>Restaurer</a></td>
                                <td><a @click=${ui.wrapAction(e => runDeleteBackupDialog(e, archive.filename))}>Supprimer</a></td>
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
        let progress = log.progress('Archivage en cours');

        try {
            let response = await net.fetch('/admin/api/archives/create', {
                method: 'POST',
                timeout: 120000
            });

            if (response.ok) {
                progress.success('Archivage complété');

                archives = null;

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
        return ui.runDialog(e, 'Envoi d\'archive', {}, (d, resolve, reject) => {
            let archive = d.file('*archive', 'Archive');

            d.action('Envoyer', {disabled: !d.isValid()}, async () => {
                let progress = log.progress('Envoi en cours');

                try {
                    let response = await net.fetch('/admin/api/archives/upload', {
                        method: 'PUT',
                        body: archive.value,
                        timeout: null
                    });

                    if (response.ok) {
                        resolve();
                        progress.success('Envoi complété');

                        archives = null;

                        self.go();
                    } else {
                        let err = (await response.text()).trim();
                        throw new Error(err);
                    }
                } catch (err) {
                    progress.close();

                    log.error(err);
                    d.refresh();
                }
            });
        });
    }

    async function runRestoreBackupDialog(e, filename) {
        return ui.runDialog(e, `Restauration de '${filename}'`, {}, (d, resolve, reject) => {
            let key = d.password('*key', 'Clé de restauration');
            let restore_users = d.boolean('*restore_users', 'Restaurer les utilisateurs et leurs droits', { value: false, untoggle: false });

            d.action('Restaurer', {disabled: !d.isValid()}, async () => {
                let progress = log.progress('Restauration en cours');

                try {
                    let query = new URLSearchParams;
                    query.set('filename', filename);
                    query.set('key', key.value);
                    query.set('users', 0 + restore_users.value);

                    let response = await net.fetch('/admin/api/archives/restore', {
                        method: 'POST',
                        body: query
                    });

                    if (response.ok) {
                        resolve();
                        progress.success(`Archive '${filename}' restaurée`);

                        instances = null;
                        users = null;
                        archives = null;

                        self.go();
                    } else {
                        let err = (await response.text()).trim();
                        throw new Error(err);
                    }
                } catch (err) {
                    progress.close();

                    log.error(err);
                    d.refresh();
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
                log.success(`Archive '${filename}' supprimée`);

                archives = null;

                self.go();
            } else {
                let err = (await response.text()).trim();
                throw new Error(err);
            }
        });
    }

    this.go = async function(e, url = null, options = {}) {
        options = Object.assign({ push_history: true }, options);

        let new_instances = instances;
        let new_users = users;
        let new_archives = archives;
        let new_selected = selected_instance;
        let new_permissions = selected_permissions;
        let explicit_panels = false;

        if (new_instances == null)
            new_instances = await net.fetchJson('/admin/api/instances/list');
        if (new_users == null)
            new_users = await net.fetchJson('/admin/api/users/list');

        if (url != null) {
            url = new URL(url, window.location.href);

            let panels = url.searchParams.get('p');
            if (panels) {
                panels = panels.split('|');

                ui.setEnabledPanels(panels);
                explicit_panels = true;
            }

            if (url.searchParams.has('select')) {
                let select = url.searchParams.get('select');
                new_selected = new_instances.find(instance => instance.key === select);

                if (new_selected == null)
                    throw new Error(`Cannot select instance '${select}' (does not exist)`);
            } else {
                new_selected = null;
            }
        }

        if (ui.isPanelEnabled('archives') && new_archives == null) {
            new_archives = await net.fetchJson('/admin/api/archives/list');
            new_archives.sort(util.makeComparator(archive => archive.filename));
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

            if (!explicit_panels && (selected_instance == null ||
                                     new_selected.key !== selected_instance.key))
                ui.setPanelState('users', true);
        } else {
            new_permissions = null;
        }

        // Commit
        instances = new_instances;
        users = new_users;
        archives = new_archives;
        selected_instance = new_selected;
        selected_permissions = new_permissions;

        // Update browser URL
        {
            let params = {};

            if (selected_instance != null)
                params.select = selected_instance.key;
            params.p = ui.getEnabledPanels().join('|') || null;

            let url = util.pasteURL('/admin/', params);
            goupile.syncHistory(url, options.push_history);
        }

        ui.render();
    };
    this.go = util.serialize(this.go);

    function runCreateInstanceDialog(e) {
        return ui.runDialog(e, 'Création d\'un projet', {}, (d, resolve, reject) => {
            let key = d.text('*key', 'Clé du projet', {
                help: [
                    'Longueur maximale : 24 caractères',
                    'Caractères autorisés : a-z (minuscules), 0-9 et \'-\''
                ]
            });
            let name = d.text('name', 'Nom', {value: key.value});
            let demo = d.boolean('demo', 'Ajouter les pages par défaut', {value: true, untoggle: false});

            d.action('Créer', {disabled: !d.isValid()}, async () => {
                let query = new URLSearchParams;
                query.set('key', key.value);
                query.set('name', name.value);
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

                    let url = util.pasteURL('/admin/', { select: key.value });
                    self.go(null, url);
                } else {
                    let err = (await response.text()).trim();

                    log.error(err);
                    d.refresh();
                }
            });
        });
    }

    function runConfigureInstanceDialog(e, instance) {
        return ui.runDialog(e, `Configuration de ${instance.key}`, {}, (d, resolve, reject) => {
            d.pushOptions({untoggle: false});

            d.tabs('actions', () => {
                d.tab('Modifier', () => {
                    if (instance.master == null) {
                        let name = d.text('*name', 'Nom', {value: instance.config.name});

                        let use_offline = d.boolean('*use_offline', 'Utilisation hors-ligne', {value: instance.config.use_offline});
                        let sync_mode = d.enum('*sync_mode', 'Mode de synchronisation', [
                            ['online', 'En ligne'],
                            ['mirror', 'Miroir'],
                            ['offline', 'Hors ligne']
                        ], {value: instance.config.sync_mode});

                        let backup_key = (sync_mode.value == 'offline') ?
                                         d.text('backup_key', 'Clé d\'archivage', {value: instance.config.backup_key}) : {};
                        if (backup_key.value != null && !checkCryptoKey(backup_key.value))
                            backup_key.error('Format de clé non valide');
                        let shared_key = d.text('shared_key', 'Clé locale partagée', {
                            value: instance.config.shared_key,
                            hidden: instance.slaves > 0
                        });
                        if (instance.slaves > 0 && shared_key.value != null && !checkCryptoKey(shared_key.value))
                            shared_key.error('Format de clé non valide');
                        let token_key = d.text('token_key', 'Session par token', {value: instance.config.token_key});
                        if (token_key.value != null && !checkCryptoKey(token_key.value))
                            token_key.error('Format de clé non valide');
                        let auto_key = d.text('auto_key', 'Session de requête', {value: instance.config.auto_key});
                        let default_userid = d.enumDrop('default_userid', 'Session par défaut',
                                                        users.map(user => [user.userid, user.username]), {
                            value: instance.config.default_userid,
                            untoggle: true
                        });

                        d.action('Configurer', {disabled: !d.isValid()}, async () => {
                            let query = new URLSearchParams();
                            query.set('key', instance.key);
                            query.set('name', name.value);
                            query.set('use_offline', use_offline.value);
                            query.set('sync_mode', sync_mode.value);
                            if (sync_mode.value === 'offline')
                                query.set('backup_key', backup_key.value || '');
                            if (!instance.slaves)
                                query.set('shared_key', shared_key.value || '');
                            query.set('token_key', token_key.value || '');
                            query.set('auto_key', auto_key.value || '');
                            query.set('default_userid', default_userid.value || '');

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

                                log.error(err);
                                d.refresh();
                            }
                        });
                    } else {
                        let name = d.text('*name', 'Nom', {value: instance.config.name});

                        let shared_key = d.text('shared_key', 'Clé locale partagée', {value: instance.config.shared_key});
                        if (shared_key.value != null && !checkCryptoKey(shared_key.value))
                            shared_key.error('Format de clé non valide');

                        d.action('Configurer', {disabled: !d.isValid()}, async () => {
                            let query = new URLSearchParams();
                            query.set('key', instance.key);
                            query.set('name', name.value);
                            query.set('shared_key', shared_key.value || '');

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

                                log.error(err);
                                d.refresh();
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
                            body: query,
                            timeout: 30000
                        });

                        if (response.ok) {
                            resolve();
                            log.success(`Projet '${instance.key}' supprimé`);

                            instances = null;
                            archives = null;

                            self.go();
                        } else {
                            let err = (await response.text()).trim();

                            log.error(err);
                            d.refresh();
                        }
                    });
                });
            });
        });
    }

    function checkCryptoKey(str) {
        try {
            let key = base64ToBytes(str);
            return key.length === 32;
        } catch (err) {
            return false;
        }
    }

    function runSplitInstanceDialog(e, master) {
        return ui.runDialog(e, `Division de ${master}`, {}, (d, resolve, reject) => {
            d.calc('instance', 'Projet', master);
            let key = d.text('*key', 'Clé du sous-projet');
            let name = d.text('name', 'Nom', {value: key.value});

            d.action('Créer', {disabled: !d.isValid()}, async () => {
                let full_key = master + '/' + key.value;

                let query = new URLSearchParams;
                query.set('key', full_key);
                query.set('name', name.value);

                let response = await net.fetch('/admin/api/instances/create', {
                    method: 'POST',
                    body: query
                });

                if (response.ok) {
                    resolve();
                    log.success(`Sous-projet '${full_key}' créé`);

                    instances = null;
                    selected_permissions = null;

                    let url = util.pasteURL('/admin/', { select: full_key });
                    self.go(null, url);
                } else {
                    let err = (await response.text()).trim();

                    log.error(err);
                    d.refresh();
                }
            });
        });
    }

    function runCreateUserDialog(e) {
        return ui.runDialog(e, 'Création d\'un utilisateur', {}, (d, resolve, reject) => {
            let username = d.text('*username', 'Nom d\'utilisateur');

            let password = d.password('*password', 'Mot de passe');
            let password2 = d.password('*password2', null, {
                placeholder: 'Confirmation',
                help: 'Doit contenir au moins 8 caractères de 3 classes différentes'
            });
            if (password.value != null && password2.value != null) {
                if (password.value !== password2.value) {
                    password2.error('Les mots de passe sont différents');
                } else if (password.value.length < 8) {
                    password2.error('Mot de passe trop court', true);
                }
            }
            let confirm = d.enumDrop('confirm', 'Méthode de confirmation', [
                ['totp', 'TOTP'],
            ]);

            let email = d.text('email', 'Courriel');
            if (email.value != null && !email.value.includes('@'))
                email.error('Format non valide');
            let phone = d.text('phone', 'Téléphone');
            if (phone.value != null && !phone.value.startsWith('+'))
                phone.error('Format non valide (préfixe obligatoire)');

            let admin = d.boolean('*admin', 'Administrateur', {value: false, untoggle: false});

            d.action('Créer', {disabled: !d.isValid()}, async () => {
                let query = new URLSearchParams;
                query.set('username', username.value);
                query.set('password', password.value);
                query.set('confirm', confirm.value || '');
                if (email.value != null)
                    query.set('email', email.value);
                if (phone.value != null)
                    query.set('phone', phone.value);
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

                    log.error(err);
                    d.refresh();
                }
            });
        });
    }

    function runAssignUserDialog(e, instance, user, prev_permissions) {
        return ui.runDialog(e, `Droits de ${user.username} sur ${instance.key}`, {}, (d, resolve, reject) => {
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

                    log.error(err);
                    d.refresh();
                }
            });
        });
    }

    function makePermissionProp(perm) {
        let name = perm.substr(perm.indexOf('_') + 1);
        return [perm, util.capitalize(name)];
    }

    function runEditUserDialog(e, user) {
        return ui.runDialog(e, `Modification de ${user.username}`, {}, (d, resolve, reject) => {
            d.pushOptions({untoggle: false});

            d.tabs('actions', () => {
                d.tab('Modifier', () => {
                    let username = d.text('username', 'Nom d\'utilisateur', {value: user.username});

                    let password = d.password('password', 'Mot de passe');
                    let password2 = d.password('password2', null, {
                        placeholder: 'Confirmation',
                        help: [
                            'Laissez vide pour ne pas modifier',
                            'Doit contenir au moins 8 caractères de 3 classes différentes'
                        ],
                        mandatory: password.value != null
                    });
                    if (password.value != null && password2.value != null) {
                        if (password.value !== password2.value) {
                            password2.error('Les mots de passe sont différents');
                        } else if (password.value.length < 8) {
                            password2.error('Mot de passe trop court', true);
                        }
                    }
                    let confirm = d.enumDrop('confirm', 'Méthode de confirmation', [
                        ['totp', 'TOTP'],
                    ], { value: user.confirm, untoggle: true });
                    let reset_secret = d.boolean('reset_secret', 'Réinitialiser le secret TOTP', {
                        value: user.confirm == null,
                        disabled: confirm.value == null,
                        untoggle: false
                    });

                    let email = d.text('email', 'Courriel', {value: user.email});
                    if (email.value != null && !email.value.includes('@'))
                        email.error('Format non valide');
                    let phone = d.text('phone', 'Téléphone', {value: user.phone});
                    if (phone.value != null && !phone.value.startsWith('+'))
                        phone.error('Format non valide (préfixe obligatoire)');

                    let admin = d.boolean('*admin', 'Administrateur', {value: user.admin});

                    d.action('Modifier', {disabled: !d.isValid()}, async () => {
                        let query = new URLSearchParams;
                        query.set('userid', user.userid);
                        if (username.value != null)
                            query.set('username', username.value);
                        if (password.value != null)
                            query.set('password', password.value);
                        query.set('confirm', confirm.value || '');
                        query.set('reset_secret', 0 + reset_secret.value);
                        if (email.value != null)
                            query.set('email', email.value);
                        if (phone.value != null)
                            query.set('phone', phone.value);
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

                            log.error(err);
                            d.refresh();
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
