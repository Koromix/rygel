// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function AdminController() {
    let self = this;

    let instances;
    let users;

    let selected_instance;

    this.start = async function() {
        initUI();
        self.go(null, window.location.href);
    };

    this.hasUnsavedData = function() {
        return false;
    };

    function initUI() {
        document.documentElement.className = 'admin';

        ui.setMenu(renderMenu);
        ui.createPanel('instances', 1, true, renderInstances);
        ui.createPanel('users', 0, true, renderUsers);
    }

    function renderMenu() {
        return html`
            <button class="icon" style="background-position-y: calc(-538px + 1.2em);">Admin</button>
            <button class=${'icon' + (ui.isPanelEnabled('instances') ? ' active' : '')}
                    style="background-position-y: calc(-362px + 1.2em);"
                    @click=${ui.wrapAction(e => togglePanel(e, 'instances'))}>Projets</button>
            <button class=${'icon' + (ui.isPanelEnabled('users') ? ' active' : '')}
                    style="background-position-y: calc(-406px + 1.2em);"
                    @click=${ui.wrapAction(e => togglePanel(e, 'users'))}>Utilisateurs</button>
            <div style="flex: 1;"></div>
            <div class="drop right">
                <button class="icon" style="background-position-y: calc(-494px + 1.2em)">${profile.username}</button>
                <div>
                    <button @click=${ui.wrapAction(goupile.logout)}>Se déconnecter</button>
                </div>
            </div>
        `;
    }

    function togglePanel(e, key) {
        ui.setPanelState(key, !ui.isPanelEnabled(key));
        return self.run();
    }

    function renderInstances() {
        return html`
            <div class="padded" style="background: #f8f8f8;">
                <div class="ui_quick">
                    <a @click=${ui.wrapAction(runCreateInstanceDialog)}>Créer un projet</a>
                    <div style="flex: 1;"></div>
                    Projets (<a @click=${ui.wrapAction(e => { instances = null; return self.run(); })}>rafraichir</a>)
                </div>

                <table class="ui_table">
                    <colgroup>
                        <col/>
                        <col style="width: 100px;"/>
                        <col style="width: 100px;"/>
                    </colgroup>

                    <tbody>
                        ${!instances.length ? html`<tr><td colspan="3">Aucun projet</td></tr>` : ''}
                        ${instances.map(instance => html`
                            <tr class=${instance.key === selected_instance ? 'active' : ''}>
                                <td style="text-align: left;">${instance.key} (<a href=${'/' + instance.key} target="_blank">accès</a>)</td>
                                <td><a role="button" tabindex="0" @click=${e => toggleSelectedInstance(e, instance.key)}>Droits</a></td>
                                <td><a role="button" tabindex="0" @click=${ui.wrapAction(e => runEditInstanceDialog(e, instance))}>Modifier</a></td>
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
                    Utilisateurs (<a @click=${ui.wrapAction(e => { users = null; return self.run(); })}>rafraichir</a>)
                </div>

                <table class="ui_table">
                    <colgroup>
                        ${selected_instance != null ? html`<col style="width: 100px;"/>` : ''}
                        <col/>
                        <col style="width: 100px;"/>
                    </colgroup>

                    <tbody>
                        ${!users.length ? html`<tr><td colspan="3">Aucun utilisateur</td></tr>` : ''}
                        ${users.map(user => {
                            let permissions = user.instances[selected_instance] || [];

                            return html`
                                <tr>
                                    <td style="text-align: left;">${user.username}</td>
                                    ${selected_instance != null ? html`
                                        <td>
                                            ${permissions.map(perm =>
                                                html`<span class="ui_tag" style="background: #777;">${perm}</span> `)}
                                            &nbsp;&nbsp;&nbsp;
                                            <a role="button" tabindex="0"
                                               @click=${ui.wrapAction(e => runAssignUserDialog(e, selected_instance, user,
                                                                                                  permissions))}>Assigner</a>
                                        </td>
                                    ` : ''}
                                    <td><a role="button" tabindex="0"
                                           @click=${ui.wrapAction(e => runEditUserDialog(e, user))}>Modifier</a></td>
                                </tr>
                            `;
                        })}
                    </tbody>
                </table>
            </div>
        `;
    }

    this.go = async function(e = null, url = null, push_history = true) {
        await goupile.syncProfile();
        if (!goupile.isAuthorized() || !profile.admin)
            await goupile.runLogin();

        await self.run(push_history);
    };
    this.go = util.serializeAsync(this.go);

    this.run = async function(push_history = false) {
        if (instances == null)
            instances = await net.fetchJson('/admin/api/instances/list');
        if (users == null)
            users = await net.fetchJson('/admin/api/users/list');
        if (!instances.find(instance => instance.key === selected_instance))
            selected_instance = null;

        ui.render();
    };
    this.run = util.serializeAsync(this.run);

    function toggleSelectedInstance(e, key) {
        if (key !== selected_instance) {
            selected_instance = key;
            ui.setPanelState('users', true, false);
        } else {
            selected_instance = null;
        }
        return self.run();
    }

    function runCreateInstanceDialog(e) {
        return ui.runDialog(e, (d, resolve, reject) => {
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

                    self.run();
                } else {
                    let err = (await response.text()).trim();
                    reject(new Error(err));
                }
            });
        });
    }

    function runEditInstanceDialog(e, instance) {
        return ui.runDialog(e, (d, resolve, reject) => {
            d.pushOptions({untoggle: false});

            d.tabs('actions', () => {
                d.tab('Modifier', () => {
                    let title = d.text('*title', 'Nom de l\'application', {value: instance.config.title});
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

                            self.run();
                        } else {
                            let err = (await response.text()).trim();
                            reject(new Error(err));
                        }
                    });
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

                            self.run();
                        } else {
                            let err = (await response.text()).trim();
                            reject(new Error(err));
                        }
                    });
                });
            });
        });
    }

    function runCreateUserDialog(e) {
        return ui.runDialog(e, (d, resolve, reject) => {
            let username = d.text('*username', 'Nom d\'utilisateur');

            let password = d.password('*password', 'Mot de passe');
            let password2 = d.password('*password2', 'Confirmation');
            if (password.value != null && password2.value != null && password.value !== password2.value)
                password2.error('Les mots de passe sont différents');

            let admin = d.boolean('*admin', 'Administrateur', {value: false, untoggle: false});

            d.action('Créer', {disabled: !d.isValid()}, async () => {
                let query = new URLSearchParams;
                query.set('username', username.value);
                query.set('password', password.value);
                query.set('admin', admin.value ? 1 : 0);

                let response = await net.fetch('/admin/api/users/create', {
                    method: 'POST',
                    body: query
                });

                if (response.ok) {
                    resolve();
                    log.success(`Utilisateur '${username.value}' créé`);

                    users = null;

                    self.run();
                } else {
                    let err = (await response.text()).trim();
                    reject(new Error(err));
                }
            });
        });
    }

    function runAssignUserDialog(e, instance, user, prev_permissions) {
        return ui.runDialog(e, (d, resolve, reject) => {
            d.calc('instance', 'Projet', instance);
            d.sameLine(); d.calc('username', 'Utilisateur', user.username);

            let permissions = d.textArea('permissions', 'Permissions', {
                rows: 7, cols: 16,
                value: prev_permissions.join('\n')
            });

            d.action('Modifier', {disabled: !d.isValid()}, async () => {
                let query = new URLSearchParams;
                query.set('instance', instance);
                query.set('userid', user.userid);
                query.set('permissions', permissions.value ? permissions.value.split('\n').join(',') : '');

                let response = await net.fetch('/admin/api/users/assign', {
                    method: 'POST',
                    body: query
                });

                if (response.ok) {
                    resolve();
                    log.success(`Droits de '${user.username}' sur le projet '${instance}' ${permissions.value ? 'modifiés' : 'supprimés'}`);

                    users = null;

                    self.run();
                } else {
                    let err = (await response.text()).trim();
                    reject(new Error(err));
                }
            });
        });
    }

    function runEditUserDialog(e, user) {
        return ui.runDialog(e, (d, resolve, reject) => {
            d.pushOptions({untoggle: false});

            d.tabs('actions', () => {
                d.tab('Modifier', () => {
                    let username = d.text('username', 'Nom d\'utilisateur', {value: user.username});

                    let password = d.password('password', 'Mot de passe', {
                        help: "Laissez vide pour ne pas modifier"
                    });
                    let password2 = d.password('password2', 'Confirmation');
                    if (password.value != null && password2.value != null && password.value !== password2.value)
                        password2.error('Les mots de passe sont différents');

                    let admin = d.boolean('*admin', 'Administrateur', {value: user.admin});

                    d.action('Modifier', {disabled: !d.isValid()}, async () => {
                        let query = new URLSearchParams;
                        query.set('userid', user.userid);
                        if (username.value != null)
                            query.set('username', username.value);
                        if (password.value != null)
                            query.set('password', password.value);
                        query.set('admin', admin.value ? 1 : 0);

                        let response = await net.fetch('/admin/api/users/edit', {
                            method: 'POST',
                            body: query
                        });

                        if (response.ok) {
                            resolve();
                            log.success(`Utilisateur '${username.value}' modifié`);

                            users = null;

                            self.run();
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
                            log.success(`Utilisateur '${user.username}' supprimé`);

                            users = null;

                            self.run();
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
