// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function AdminController() {
    let self = this;

    let instances;
    let users;

    let select_instance;

    this.start = async function() {
        initUI();
        self.run();
    };

    function initUI() {
        document.documentElement.className = 'admin';

        ui.setMenu(el => html`
            <button>Admin</button>
            <button class=${ui.isPanelEnabled('instances') ? 'active' : ''}
                    @click=${e => ui.togglePanel('instances')}>Instances</button>
            <button class=${ui.isPanelEnabled('users') ? 'active' : ''}
                    @click=${e => ui.togglePanel('users')}>Utilisateurs</button>
            <div style="flex: 1;"></div>
            <button @click=${ui.wrapClick(goupile.logout)}>Se déconnecter</button>
        `);

        ui.createPanel('instances', () => html`
            <div class="adm_panel">
                <table class="ui_table">
                    <colgroup>
                        <col style="width: 200px;"/>
                        <col/>
                        <col style="width: 100px;"/>
                        <col style="width: 100px;"/>
                    </colgroup>

                    <tbody>
                        ${!instances.length ? html`<tr><td colspan="4">Aucune instance</td></tr>` : ''}
                        ${instances.map(instance => html`
                            <tr class=${instance.key === select_instance ? 'active' : ''}>
                                <td style="text-align: left;">${instance.key} (<a href=${'/' + instance.key} target="_blank">accès</a>)</td>
                                <td></td>
                                <td><a role="button" tabindex="0" @click=${e => toggleInstance(instance.key)}>Droits</a></td>
                                <td><a role="button" tabindex="0" @click=${ui.wrapClick(e => runEditInstanceDialog(e, instance))}>Modifier</a></td>
                            </tr>
                        `)}
                    </tbody>
                </table>

                <div class="ui_actions">
                    <button @click=${ui.wrapClick(runCreateInstanceDialog)}>Créer une instance</button>
                </div>
            </div>
        `);

        ui.createPanel('users', () => html`
            <div class="adm_panel" style="flex-grow: 1.5;">
                <table class="ui_table">
                    <colgroup>
                        <col style="width: 100px;"/>
                        <col/>
                        <col style="width: 100px;"/>
                    </colgroup>

                    <tbody>
                        ${!users.length ? html`<tr><td colspan="2">Aucun utilisateur</td></tr>` : ''}
                        ${users.map(user => {
                            let permissions = user.instances[select_instance] || [];

                            return html`
                                <tr>
                                    <td style="text-align: left;">${user.username}</td>
                                    <td>
                                        ${permissions.map(perm =>
                                            html`<span class="gp_tag" style="background: #666;">${perm}</span> `)}
                                        ${select_instance ?
                                            html`&nbsp;&nbsp;&nbsp;
                                                 <a role="button" tabindex="0"
                                                    @click=${ui.wrapClick(e => runAssignUserDialog(e, select_instance, user.username,
                                                                                                   permissions))}>Assigner</a>` : ''}
                                    </td>
                                    <td><a role="button" tabindex="0"
                                           @click=${ui.wrapClick(e => runDeleteUserDialog(e, user.username))}>Supprimer</a></td>
                                </tr>
                            `;
                        })}
                    </tbody>
                </table>

                <div class="ui_actions">
                    <button @click=${ui.wrapClick(runCreateUserDialog)}>Créer un utilisateur</button>
                <div>
            </div>
        `);
    }

    this.run = async function() {
        await goupile.syncProfile();

        if (!goupile.isAuthorized())
            await goupile.runLogin();

        instances = await fetch('/admin/api/instances/list').then(response => response.json());
        users = await fetch('/admin/api/users/list').then(response => response.json());
        if (!instances.find(instance => instance.key === select_instance))
            select_instance = null;

        ui.render();
    };

    function toggleInstance(key) {
        if (key !== select_instance) {
            select_instance = key;
        } else {
            select_instance = null;
        }
        self.run();
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

                    log.success('Instance créée');
                    select_instance = key.value;
                    self.run();
                } else {
                    let err = (await response.text()).trim();
                    log.error(err);
                    reject(new Error(err));
                }
            });
            d.action('Annuler', {}, () => reject(new Error('Action annulée')));
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

                            log.success('Instance configurée');
                            self.run();
                        } else {
                            let err = (await response.text()).trim();
                            log.error(err);
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

                            log.success('Instance supprimée');
                            self.run();
                        } else {
                            let err = (await response.text()).trim();
                            log.error(err);
                            reject(new Error(err));
                        }
                    });
                });

                d.action('Annuler', {}, () => reject(new Error('Action annulée')));
            })
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

                    log.success('Utilisateur créé');
                    self.run();
                } else {
                    let err = (await response.text()).trim();
                    log.error(err);
                    reject(new Error(err));
                }
            });
            d.action('Annuler', {}, () => reject(new Error('Action annulée')));
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

                    log.success('Droits modifiés');
                    self.run();
                } else {
                    let err = (await response.text()).trim();
                    log.error(err);
                    reject(new Error(err));
                }
            });
            d.action('Annuler', {}, () => reject(new Error('Action annulée')));
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
                log.success('Utilisateur supprimé');
                self.run();
            } else {
                let err = (await response.text()).trim();
                throw new Error(err);
            }
        });
    }
};
