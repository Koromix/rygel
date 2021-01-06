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
        self.go();
    };

    function initUI() {
        document.documentElement.className = 'admin';

        ui.setMenu(el => html`
            <button class="icon" style="background-position-y: calc(-538px + 1.2em);">Admin</button>
            <button class=${ui.isPanelEnabled('instances') ? 'active' : ''}
                    @click=${e => togglePanel('instances')}>Instances</button>
            <button class=${ui.isPanelEnabled('users') ? 'active' : ''}
                    @click=${e => togglePanel('users')}>Utilisateurs</button>
            <div style="flex: 1;"></div>
            <div class="drop right">
                <button class="icon" style="background-position-y: calc(-494px + 1.2em)">${profile.username}</button>
                <div>
                    <button @click=${ui.wrapAction(goupile.logout)}>Se déconnecter</button>
                </div>
            </div>
        `);

        ui.createPanel('instances', true, () => html`
            <div class="adm_panel" style="background: #f8f8f8;">
                <div class="ui_quick">
                    Instances
                    <div style="flex: 1;"></div>
                    <a @click=${ui.wrapAction(e => { instances = null; return self.go(); })}>🗘</a>
                </div>

                <table class="ui_table">
                    <colgroup>
                        <col/>
                        <col style="width: 100px;"/>
                        <col style="width: 100px;"/>
                    </colgroup>

                    <tbody>
                        ${!instances.length ? html`<tr><td colspan="3">Aucune instance</td></tr>` : ''}
                        ${instances.map(instance => html`
                            <tr class=${instance.key === selected_instance ? 'active' : ''}>
                                <td style="text-align: left;">${instance.key} (<a href=${'/' + instance.key} target="_blank">accès</a>)</td>
                                <td><a role="button" tabindex="0" @click=${e => toggleSelectedInstance(instance.key)}>Droits</a></td>
                                <td><a role="button" tabindex="0" @click=${ui.wrapAction(e => runEditInstanceDialog(e, instance))}>Modifier</a></td>
                            </tr>
                        `)}
                    </tbody>
                </table>

                <div class="ui_actions">
                    <button @click=${ui.wrapAction(runCreateInstanceDialog)}>Créer une instance</button>
                </div>
            </div>
        `);

        ui.createPanel('users', true, () => html`
            <div class="adm_panel" style="flex-grow: 1.5;">
                <div class="ui_quick">
                    Utilisateurs
                    <div style="flex: 1;"></div>
                    <a @click=${ui.wrapAction(e => { users = null; return self.go(); })}>🗘</a>
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
                                                html`<span class="gp_tag" style="background: #777;">${perm}</span> `)}
                                            &nbsp;&nbsp;&nbsp;
                                            <a role="button" tabindex="0"
                                               @click=${ui.wrapAction(e => runAssignUserDialog(e, selected_instance, user.username,
                                                                                               permissions))}>Assigner</a>
                                        </td>
                                    ` : ''}
                                    <td><a role="button" tabindex="0"
                                           @click=${ui.wrapAction(e => runDeleteUserDialog(e, user.username))}>Supprimer</a></td>
                                </tr>
                            `;
                        })}
                    </tbody>
                </table>

                <div class="ui_actions">
                    <button @click=${ui.wrapAction(runCreateUserDialog)}>Créer un utilisateur</button>
                <div>
            </div>
        `);

        // Push to the top of priority list
        ui.setPanelState('instances', true);
    }

    this.go = async function(url = null, push_history = true) {
        await goupile.syncProfile();
        if (!goupile.isAuthorized())
            await goupile.runLogin();

        if (instances == null)
            instances = await fetch('/admin/api/instances/list').then(response => response.json());
        if (users == null)
            users = await fetch('/admin/api/users/list').then(response => response.json());
        if (!instances.find(instance => instance.key === selected_instance))
            selected_instance = null;

        ui.render();
    };
    this.go = util.serializeAsync(this.go);

    function togglePanel(key) {
        ui.setPanelState(key, !ui.isPanelEnabled(key));
        self.go();
    }

    function toggleSelectedInstance(key) {
        if (key !== selected_instance) {
            selected_instance = key;
            ui.setPanelState('users', true, false);
        } else {
            selected_instance = null;
        }
        self.go();
    }

    function runCreateInstanceDialog(e) {
        return ui.runDialog(e, (d, resolve, reject) => {
            let key = d.text('*key', 'Clé de l\'instance');
            let name = d.text('name', 'Nom', {value: key.value});

            d.action('Créer', {disabled: !d.isValid()}, async () => {
                let query = new URLSearchParams;
                query.set('key', key.value);
                query.set('title', name.value);

                let response = await net.fetch('/admin/api/instances/create', {
                    method: 'POST',
                    body: query
                });

                if (response.ok) {
                    resolve();
                    log.success(`Instance '${key.value}' créée`);

                    instances = null;

                    self.go();
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

                    d.action('Modifier', {disabled: !d.isValid()}, async () => {
                        let query = new URLSearchParams();
                        query.set('key', instance.key);
                        query.set('title', title.value);
                        query.set('use_offline', use_offline.value);
                        query.set('sync_mode', sync_mode.value);

                        let response = await net.fetch('/admin/api/instances/configure', {
                            method: 'POST',
                            body: query
                        });

                        if (response.ok) {
                            resolve();
                            log.success(`Instance '${instance.key}' modifiée`);

                            instances = null;

                            self.go();
                        } else {
                            let err = (await response.text()).trim();
                            reject(new Error(err));
                        }
                    });
                });

                d.tab('Supprimer', () => {
                    d.output(`Voulez-vous vraiment supprimer l'instance '${instance.key}' ?`);

                    d.action('Supprimer', {}, async () => {
                        let query = new URLSearchParams;
                        query.set('key', instance.key);

                        let response = await net.fetch('/admin/api/instances/delete', {
                            method: 'POST',
                            body: query
                        });

                        if (response.ok) {
                            resolve();
                            log.success(`Instance '${instance.key}' supprimée`);

                            instances = null;

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

                    self.go();
                } else {
                    let err = (await response.text()).trim();
                    reject(new Error(err));
                }
            });
        });
    }

    function runAssignUserDialog(e, instance, username, prev_permissions) {
        return ui.runDialog(e, (d, resolve, reject) => {
            d.calc('instance', 'Instance', instance);
            d.sameLine(); d.calc('username', 'Utilisateur', username);

            let permissions = d.textArea('permissions', 'Permissions', {
                rows: 7, cols: 16,
                value: prev_permissions.join('\n')
            });

            d.action('Modifier', {disabled: !d.isValid()}, async () => {
                let query = new URLSearchParams;
                query.set('instance', instance);
                query.set('user', username);
                query.set('permissions', permissions.value ? permissions.value.split('\n').join(',') : '');

                let response = await net.fetch('/admin/api/users/assign', {
                    method: 'POST',
                    body: query
                });

                if (response.ok) {
                    resolve();
                    log.success(`Droits de '${username}' sur l'instance '${instance}' ${permissions.value ? 'modifiés' : 'supprimés'}`);

                    users = null;

                    self.go();
                } else {
                    let err = (await response.text()).trim();
                    reject(new Error(err));
                }
            });
        });
    }

    function runDeleteUserDialog(e, username) {
        return ui.runConfirm(e, `Voulez-vous supprimer l'utilisateur '${username}' ?`,
                             'Supprimer', async () => {
            let query = new URLSearchParams;
            query.set('username', username);

            let response = await net.fetch('/admin/api/users/delete', {
                method: 'POST',
                body: query
            });

            if (response.ok) {
                log.success(`Utilisateur '${username}' supprimé`);

                users = null;

                self.go();
            } else {
                let err = (await response.text()).trim();
                throw new Error(err);
            }
        });
    }
};
