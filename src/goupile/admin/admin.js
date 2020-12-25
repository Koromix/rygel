// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let admin = new function() {
    let self = this;

    let session_profile = {};
    let session_rnd;

    let select_instance;

    this.startApp = async function() {
        log.pushHandler(log.notifyHandler);
        self.run();
    };

    this.run = async function() {
        await syncProfile();

        if (isConnected()) {
            runAdmin();
        } else {
            runLogin();
        }
    };

    async function runAdmin() {
        let instances = await fetch('/admin/api/instances/list').then(response => response.json());
        let users = await fetch('/admin/api/users/list').then(response => response.json());

        // Server does not keep them ordered by key
        instances.sort((instance1, instance2) => util.compareValues(instance1.key, instance2.key));

        if (!instances.find(instance => instance.key === select_instance))
            select_instance = null;

        render(html`
            <section style="flex-grow: 2;">
                <h1>Instances</h1>

                <table>
                    <colgroup>
                        <col/>
                        <col style="width: 100px;"/>
                        <col style="width: 100px;"/>
                        <col style="width: 100px;"/>
                    </colgroup>

                    <tbody>
                        ${!instances.length ? html`<tr><td colspan="4">Aucune instance</td></tr>` : ''}
                        ${instances.map(instance => html`
                            <tr class=${instance.key === select_instance ? 'selected' : ''}>
                                <td>${instance.key} (<a href=${'/' + instance.key} target="_blank">accès</a>)</td>
                                <td><a @click=${e => runConfigureInstanceDialog(e, instance)}>Paramétrer</a></td>
                                <td><a @click=${e => toggleInstance(instance.key)}>Droits</a></td>
                                <td><a @click=${e => runDeleteInstanceDialog(e, instance.key)}>Fermer</a></td>
                            </tr>
                        `)}
                    </tbody>
                </table>

                <button @click=${runCreateInstanceDialog}>Créer une instance</button>
            </section>

            <section style="flex-grow: 3;">
                <h1>Utilisateurs</h1>

                <table>
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
                                    <td>${user.username}</td>
                                    <td>
                                        ${permissions.map(perm =>
                                            html`<span class="gp_tag" style="background: #555;">${perm}</span> `)}
                                        ${select_instance ?
                                            html`&nbsp;&nbsp;&nbsp;
                                                 <a @click=${e => runAssignUserDialog(e, select_instance, user.username,
                                                                                      permissions)}>${permissions.length ? 'Modifier' : 'Assigner'}</a>` : ''}
                                    </td>
                                    <td><a @click=${e => runDeleteUserDialog(e, user.username)}>Supprimer</a></td>
                                </tr>
                            `;
                        })}
                    </tbody>
                </table>

                <button @click=${runCreateUserDialog}>Créer un utilisateur</button>
            </section>
        `, document.querySelector('#gp_root'));
    }

    function toggleInstance(key) {
        if (key !== select_instance) {
            select_instance = key;
        } else {
            select_instance = null;
        }
        self.run();
    }

    function runCreateInstanceDialog(e) {
        return dialog.run(e, (d, resolve, reject) => {
            let key = d.text('*key', 'Clé de l\'instance');
            let name = d.text('name', 'Nom', {placeholder: key.value});

            d.action('Créer', {disabled: !d.isValid()}, async () => {
                let query = new URLSearchParams;
                query.set('key', key.value);
                query.set('app_name', name.value || key.value);

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

    function runDeleteInstanceDialog(e, key) {
        let msg = `Voulez-vous fermer l'instance '${key}' ?`;
        return dialog.confirm(e, msg, 'Fermer', async () => {
            let query = new URLSearchParams;
            query.set('key', key);

            let response = await net.fetch('/admin/api/instances/delete', {
                method: 'POST',
                body: query
            });

            if (response.ok) {
                log.success('Instance fermée');
                self.run();
            } else {
                let err = (await response.text()).trim();
                throw new Error(err);
            }
        });
    }

    function runConfigureInstanceDialog(e, instance) {
        return dialog.run(e, (d, resolve, reject) => {
            d.pushOptions({untoggle: false});

            let app_name = d.text('*app_name', 'Nom de l\'application', {value: instance.config.app_name});
            let use_offline = d.boolean('*use_offline', 'Utilisation hors-ligne', {value: instance.config.use_offline});
            let sync_mode = d.enum('*sync_mode', 'Mode de synchronisation', [
                ['offline', 'Hors ligne'],
                ['online', 'En ligne'],
                ['mirror', 'Mode miroir']
            ], {value: instance.config.sync_mode});

            d.action('Appliquer', {disabled: !d.isValid()}, async () => {
                let query = new URLSearchParams();
                query.set('key', instance.key);
                query.set('app_name', app_name.value);
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
            d.action('Annuler', {}, () => reject(new Error('Action annulée')));
        });
    }

    function runCreateUserDialog(e) {
        return dialog.run(e, (d, resolve, reject) => {
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
        return dialog.run(e, (d, resolve, reject) => {
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
        let msg = `Voulez-vous supprimer l'utilisateur '${username}' ?`;
        return dialog.confirm(e, msg, 'Supprimer', async () => {
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

    function runLogin() {
        return dialog.runScreen((d, resolve, reject) => {
            d.output(html`
                <img id="usr_logo" src="/admin/favicon.png" alt="" />
                <br/>
            `);

            let username = d.text('*username', 'Nom d\'utilisateur');
            let password = d.password('*password', 'Mot de passe');

            d.action('Se connecter', {disabled: !d.isValid()}, async () => {
                let success = await login(username.value, password.value);

                if (success) {
                    resolve(username.value);
                } else {
                    reject(new Error('Échec de la connexion'));
                }
            });
        });
    }

    async function login(username, password) {
        let entry = new log.Entry;

        entry.progress('Connexion en cours');

        try {
            let query = new URLSearchParams;
            query.set('username', username.toLowerCase());
            query.set('password', password);

            let response = await net.fetch('/admin/api/user/login', {
                method: 'POST',
                body: query
            });

            if (response.ok) {
                session_profile = await response.json();
                session_rnd = util.getCookie('session_rnd');

                entry.success('Connexion réussie');
                self.run();

                return true;
            } else {
                entry.error('Échec de la connexion');
                return false;
            }
        } catch (e) {
            entry.error(e);
            return false;
        }
    }

    async function syncProfile() {
        let new_rnd = util.getCookie('session_rnd');

        if (new_rnd !== session_rnd) {
            let response = await net.fetch('/admin/api/user/profile');
            let profile = await response.json();

            session_profile = profile;
            session_rnd = util.getCookie('session_rnd');
        }
    }

    function isConnected() { return !!session_profile.username; }
};
