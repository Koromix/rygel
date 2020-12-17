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

        if (!instances.includes(select_instance))
            select_instance = null;

        render(html`
            <section style="flex-grow: 2;">
                <h1>Instances</h1>

                <table>
                    <colgroup>
                        <col"/>
                        <col style="width: 100px;"/>
                        <col style="width: 100px;"/>
                        <col style="width: 100px;"/>
                    </colgroup>

                    <tbody>
                        ${!instances.length ? html`<tr><td colspan="4">Aucune instance</td></tr>` : ''}
                        ${instances.map(instance => html`
                            <tr class=${instance === select_instance ? 'selected' : ''}>
                                <td>${instance}</td>
                                <td><a href=${'/' + instance}>Accéder</a></td>
                                <td><a @click=${e => toggleInstance(instance)}>Droits</a></td>
                                <td><a @click=${e => runDeleteInstanceDialog(e, instance)}>Fermer</a></td>
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

    function toggleInstance(instance) {
        if (instance !== select_instance) {
            select_instance = instance;
        } else {
            select_instance = null;
        }
        self.run();
    }

    function runCreateInstanceDialog(e) {
        return dialog.run(e, (d, resolve, reject) => {
            let key = d.text('*key', 'Nom de l\'instance');

            d.action('Créer', {disabled: !d.isValid()}, async () => {
                let query = new URLSearchParams;
                query.set('key', key.value);

                let response = await net.fetch('/admin/api/instances/create', {
                    method: 'POST',
                    body: query
                });

                if (response.ok) {
                    resolve();

                    log.success('Instance créée');
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

    function runDeleteInstanceDialog(e, instance) {
        let msg = `Voulez-vous fermer l'instance '${instance}' ?`;
        return dialog.confirm(e, msg, 'Fermer', async () => {
            let query = new URLSearchParams;
            query.set('key', instance);

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

    function runCreateUserDialog(e) {
        return dialog.run(e, (d, resolve, reject) => {
            let username = d.text('*username', 'Nom d\'utilisateur');
            let password = d.text('*password', 'Mot de passe');
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
            d.calc("instance", "Instance", instance);
            d.sameLine(); d.calc("username", "Utilisateur", username);

            let permissions = d.textArea("permissions", "Permissions", {
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
        render(html`
            <form class="gp_form" @submit=${handleLoginSubmit}>
                <fieldset id="usr_fieldset" style="margin: 0; padding: 0; border: 0;" ?disabled=${false}>
                    <label>Utilisateur : <input id="usr_username" type="text"/></label>
                    <label>Mot de passe : <input id="usr_password" type="password"/></label>

                    <br/><button>Se connecter</button>
                </fieldset>
            </form>
        `, document.querySelector('#gp_root'));
    }

    function handleLoginSubmit(e) {
        let fieldset_el = document.querySelector('#usr_fieldset');
        let username_el = document.querySelector('#usr_username');
        let password_el = document.querySelector('#usr_password');

        fieldset_el.disabled = true;
        let p = login(username_el.value, password_el.value);

        p.then(success => {
            if (success) {
                self.run();
            } else {
                fieldset_el.disabled = false;
                password_el.focus();
            }
        });
        p.catch(err => {
            fieldset_el.disabled = false;
            log.error(err);
        });

        e.preventDefault();
    }

    async function login(username, password) {
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

            return true;
        } else {
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
