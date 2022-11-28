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
    let all_users;
    let selected_permissions;

    this.init = async function() {
        ui.setMenu(renderMenu);

        ui.createPanel('instances', ['users', 'archives'], 'users', renderInstances);
        ui.createPanel('users', [], null, renderUsers);
        ui.createPanel('archives', [], null, renderArchives);

        ui.setPanelState('instances', true);
        if (ui.allowTwoPanels())
            ui.setPanelState('users', true);
    }

    this.hasUnsavedData = function() {
        return false;
    };

    function renderMenu() {
        return html`
            <nav class="ui_toolbar" id="ui_top" style="z-index: 999999;">
                <button class="icon" style="background-position-y: calc(-538px + 1.2em);"
                        @click=${e => self.go(e, '/admin/')}>Admin</button>
                <button class=${'icon' + (ui.isPanelActive('instances') ? ' active' : '')}
                        style="background-position-y: calc(-362px + 1.2em);"
                        @click=${ui.wrapAction(e => togglePanel(e, 'instances'))}>Projets</button>
                <button class=${'icon' + (ui.isPanelActive('users') ? ' active' : '')}
                        style="background-position-y: calc(-406px + 1.2em);"
                        @click=${ui.wrapAction(e => togglePanel(e, 'users'))}>Utilisateurs</button>
                <button class=${'icon' + (ui.isPanelActive('archives') ? ' active' : '')}
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
            </nav>
        `;
    }

    function togglePanel(e, key) {
        ui.setPanelState(key, !ui.isPanelActive(key), key !== 'instances');
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
                        <col style="width: 100px;"/>
                    </colgroup>

                    <tbody>
                        ${!instances.length ? html`<tr><td colspan="5">Aucun projet</td></tr>` : ''}
                        ${instances.map(instance => html`
                            <tr class=${instance === selected_instance ? 'active' : ''}>
                                <td style="text-align: left;" class=${instance.master != null ? 'child' : ''}>
                                    ${instance.master != null ? html`<span style="color: #ccc;">${instance.master} / </span>${instance.key.replace(/^.*\//, '')}` : ''}
                                    ${instance.master == null ? instance.key : ''}
                                    (<a href=${'/' + instance.key} target="_blank">accès</a>)
                                </td>
                                <td>${instance.master == null ?
                                        html`<a role="button" tabindex="0" @click=${ui.wrapAction(e => runSplitInstanceDialog(e, instance.key))}>Diviser</a>` : ''}</td>
                                <td><a role="button" tabindex="0" href=${util.pasteURL('/admin/', { select: instance.key })} 
                                       @click=${ui.wrapAction(instance != selected_instance ? (e => ui.setPanelState('users', true))
                                                                                            : (e => { self.go(e, '/admin/'); e.preventDefault(); }))}>Droits</a></td>
                                <td><a role="button" tabindex="0" @click=${ui.wrapAction(e => runConfigureInstanceDialog(e, instance))}>Configurer</a></td>
                                <td><a role="button" tabindex="0" @click=${ui.wrapAction(e => runDeleteInstanceDialog(e, instance))}>Supprimer</a></td>
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
                    <a @click=${ui.wrapAction(runCreateUserDialog)}>Créer un utilisateur</a>
                    <div style="flex: 1;"></div>
                    ${selected_instance != null ? html`
                        <div class="fm_check">
                            <input id="all_users" type="checkbox" .checked=${all_users}
                                   @change=${ui.wrapAction(e => { all_users = e.target.checked; return self.go(); })} />
                            <label for="all_users">Afficher tout le monde</label>
                        </div>
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
                                    <td style=${'text-align: left;' + (user.admin ? ' color: #db0a0a;' : '')}>
                                        ${user.username}
                                        ${user.admin ? html`<span title="Administrateur">♛\uFE0E</span>` : ''}
                                    </td>
                                    ${selected_instance == null ? html`
                                        <td style="text-align: left;">${user.email != null ? html`<a href=${'mailto:' + user.email}>${user.email}</a>` : ''}</td>
                                        <td style="text-align: left;">${user.phone != null ? html`<a href=${'tel:' + user.phone}>${user.phone}</a>` : ''}</td>
                                    ` : ''}
                                    <td><a role="button" tabindex="0"
                                           @click=${ui.wrapAction(e => runEditUserDialog(e, user))}>Modifier</a></td>
                                    ${selected_instance != null ? html`
                                        <td class=${!permissions.length ? 'missing' : ''}
                                            style="white-space: normal;">
                                            ${selected_instance.master == null ? makePermissionsTag(permissions, 'admin_', '#b518bf') : ''}
                                            ${!selected_instance.slaves ? makePermissionsTag(permissions, 'data_', '#258264') : ''}
                                            ${!permissions.length ? 'Non assigné' : ''}
                                        </td>
                                        <td><a role="button" tabindex="0"
                                               @click=${ui.wrapAction(e => runAssignUserDialog(e, selected_instance, user,
                                                                                                  permissions))}>Assigner</a></td>
                                    ` : ''}
                                    ${selected_instance == null ?
                                        html`<td><a role="button" tabindex="0"
                                                    @click=${ui.wrapAction(e => runDeleteUserDialog(e, user))}>Supprimer</a></td>` : ''}
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
                <div style="margin-bottom: 2em;">
                    <p>Les archives créées manuellement ou automatiquement (chaque jour) sont gardées localement
                    pour une <span style="color: red; font-weight: bold;">période de ${ENV.retention} jours</span>.
                    Vous pouvez les télécharger et les enregistrer sur vos propres supports de stockage.</p>

                    <p>N'oubliez pas que <b>sans la clé de déchiffrement</b> qui vous a été confiée lors de l'ouverture
                    du domaine, le contenu de ces archives ne peut pas être restauré.</p>
                </div>

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
                                <td style="text-align: left;"><a href=${'/admin/api/archives/files/' + archive.filename}
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
                timeout: 180000
            });

            if (response.ok) {
                progress.success('Archivage complété');

                archives = null;

                self.go();
            } else {
                let err = await net.readError(response);
                throw new Error(err);
            }
        } catch (err) {
            progress.close();
            throw err;
        }
    }

    async function runUploadBackupDialog(e) {
        return ui.runDialog(e, 'Envoi d\'archive', {}, (d, resolve, reject) => {
            d.file('*archive', 'Archive');

            d.action('Envoyer', {disabled: !d.isValid()}, async () => {
                let progress = log.progress('Envoi en cours');

                try {
                    let url = '/admin/api/archives/files/' + d.values.archive.name;
                    let response = await net.fetch(url, {
                        method: 'PUT',
                        body: d.values.archive,
                        timeout: null
                    });

                    if (response.ok) {
                        resolve();
                        progress.success('Envoi complété');

                        archives = null;

                        self.go();
                    } else {
                        let err = await net.readError(response);
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
            d.password('*key', 'Clé de restauration');
            d.boolean('*restore_users', 'Restaurer les utilisateurs et leurs droits', { value: false, untoggle: false });

            d.action('Restaurer', {disabled: !d.isValid()}, async () => {
                let progress = log.progress('Restauration en cours');

                try {
                    let query = new URLSearchParams;
                    query.set('filename', filename);
                    query.set('key', d.values.key);
                    query.set('users', 0 + d.values.restore_users);

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
                        let err = await net.readError(response);
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
                let err = await net.readError(response);
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
        let new_all_users = all_users;
        let new_permissions = selected_permissions;
        let explicit_panels = false;

        if (new_instances == null)
            new_instances = await net.fetchJson('/admin/api/instances/list');
        if (new_users == null)
            new_users = await net.fetchJson('/admin/api/users/list');

        if (url != null) {
            if (!(url instanceof URL))
                url = new URL(url, window.location.href);

            let panels = url.searchParams.get('p');
            if (panels) {
                panels = panels.split('|');

                ui.restorePanels(panels);
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

            if (url.searchParams.has('all')) {
                new_all_users = (url.searchParams.get('all') == 1);
            } else if (new_all_users == null) {
                new_all_users = true;
            }
        }

        if (ui.isPanelActive('archives') && new_archives == null) {
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
                ui.setPanelState('users', true, true);
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
            params.p = ui.savePanels().join('|') || null;

            let url = util.pasteURL('/admin/', params);
            goupile.syncHistory(url, options.push_history);
        }

        ui.render();
    };
    this.go = util.serialize(this.go);

    function runCreateInstanceDialog(e) {
        return ui.runDialog(e, 'Création d\'un projet', {}, (d, resolve, reject) => {
            d.text('*key', 'Clé du projet', {
                help: [
                    'Longueur maximale : 24 caractères',
                    'Caractères autorisés : a-z (minuscules), 0-9 et \'-\''
                ]
            });
            d.text('name', 'Nom', {value: d.values.key});
            d.boolean('demo', 'Ajouter les pages par défaut', {value: true, untoggle: false});

            d.action('Créer', {disabled: !d.isValid()}, async () => {
                let query = new URLSearchParams;
                query.set('key', d.values.key);
                query.set('name', d.values.name);
                query.set('demo', 0 + d.values.demo);

                let response = await net.fetch('/admin/api/instances/create', {
                    method: 'POST',
                    body: query
                });

                if (response.ok) {
                    resolve();
                    log.success(`Projet '${d.values.key}' créé`);

                    instances = null;
                    selected_permissions = null;

                    let url = util.pasteURL('/admin/', { select: d.values.key });
                    self.go(null, url);
                } else {
                    let err = await net.readError(response);

                    log.error(err);
                    d.refresh();
                }
            });
        });
    }

    function runConfigureInstanceDialog(e, instance) {
        return ui.runDialog(e, `Configuration de ${instance.key}`, {}, (d, resolve, reject) => {
            d.pushOptions({untoggle: false});

            if (instance.master == null) {
                d.text('*name', 'Nom', {value: instance.config.name});

                d.boolean('*use_offline', 'Utilisation hors-ligne', {value: instance.config.use_offline});
                d.enum('*sync_mode', 'Mode de synchronisation', [
                    ['online', 'En ligne'],
                    ['mirror', 'Miroir'],
                    ['offline', 'Hors ligne']
                ], {value: instance.config.sync_mode});

                if (d.values.sync_mode == 'offline')
                    d.text('backup_key', 'Clé d\'archivage', {value: instance.config.backup_key});
                if (d.values.backup_key != null && !checkCryptoKey(d.values.backup_key))
                    d.error('backup_key', 'Format de clé non valide');
                d.text('shared_key', 'Clé locale partagée', {
                    value: instance.config.shared_key,
                    hidden: instance.slaves > 0
                });
                if (instance.slaves > 0 && d.values.shared_key != null && !checkCryptoKey(d.values.shared_key))
                    d.error('shared_key', 'Format de clé non valide');
                d.text('token_key', 'Session par token', {value: instance.config.token_key});
                if (d.values.token_key != null && !checkCryptoKey(d.values.token_key))
                    d.error('token_key', 'Format de clé non valide');
                d.text('auto_key', 'Session de requête', {value: instance.config.auto_key});
                d.boolean('allow_guests', 'Autoriser les invités', {value: instance.config.allow_guests});

                d.action('Configurer', {disabled: !d.isValid()}, async () => {
                    let query = new URLSearchParams();
                    query.set('key', instance.key);
                    query.set('name', d.values.name);
                    query.set('use_offline', 0 + d.values.use_offline);
                    query.set('sync_mode', d.values.sync_mode);
                    if (d.values.sync_mode === 'offline')
                        query.set('backup_key', d.values.backup_key || '');
                    if (!instance.slaves)
                        query.set('shared_key', d.values.shared_key || '');
                    query.set('token_key', d.values.token_key || '');
                    query.set('auto_key', d.values.auto_key || '');
                    query.set('allow_guests', 0 + d.values.allow_guests);

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
                        let err = await net.readError(response);

                        log.error(err);
                        d.refresh();
                    }
                });
            } else {
                d.text('*name', 'Nom', {value: instance.config.name});

                d.text('shared_key', 'Clé locale partagée', {value: instance.config.shared_key});
                if (d.values.shared_key != null && !checkCryptoKey(d.values.shared_key))
                    d.error('shared_key', 'Format de clé non valide');

                d.action('Configurer', {disabled: !d.isValid()}, async () => {
                    let query = new URLSearchParams();
                    query.set('key', instance.key);
                    query.set('name', d.values.name);
                    query.set('shared_key', d.values.shared_key || '');

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
                        let err = await net.readError(response);

                        log.error(err);
                        d.refresh();
                    }
                });
            }
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

    function runDeleteInstanceDialog(e, instance) {
        return ui.runDialog(e, `Suppression de ${instance.key}`, {}, (d, resolve, reject) => {
            d.output(`Voulez-vous vraiment supprimer le projet '${instance.key}' ?`);

            d.action('Supprimer', {}, async () => {
                let query = new URLSearchParams;
                query.set('key', instance.key);

                let response = await net.fetch('/admin/api/instances/delete', {
                    method: 'POST',
                    body: query,
                    timeout: 180000
                });

                if (response.ok) {
                    resolve();
                    log.success(`Projet '${instance.key}' supprimé`);

                    instances = null;
                    archives = null;

                    self.go();
                } else {
                    let err = await net.readError(response);

                    log.error(err);
                    d.refresh();
                }
            });
        });
    }

    function runSplitInstanceDialog(e, master) {
        return ui.runDialog(e, `Division de ${master}`, {}, (d, resolve, reject) => {
            d.calc('instance', 'Projet', master);
            d.text('*key', 'Clé du sous-projet');
            d.text('name', 'Nom', {value: d.values.key});

            d.action('Créer', {disabled: !d.isValid()}, async () => {
                let full_key = master + '/' + d.values.key;

                let query = new URLSearchParams;
                query.set('key', full_key);
                query.set('name', d.values.name);

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
                    let err = await net.readError(response);

                    log.error(err);
                    d.refresh();
                }
            });
        });
    }

    function runCreateUserDialog(e) {
        return ui.runDialog(e, 'Création d\'un utilisateur', {}, (d, resolve, reject) => {
            let username = d.text('*username', 'Nom d\'utilisateur');

            d.password('*password', 'Mot de passe');
            d.password('*password2', null, {
                placeholder: 'Confirmation',
                help: 'Doit contenir au moins 8 caractères de 3 classes différentes'
            });
            d.boolean('change_password', 'Exiger un changement de mot de passe', {
                value: true, untoggle: false
            });
            if (d.values.password != null && d.values.password2 != null) {
                if (d.values.password !== d.values.password2) {
                    d.error('password2', 'Les mots de passe sont différents');
                } else if (d.values.password.length < 8) {
                    d.error('password2', 'Mot de passe trop court');
                }
            }
            d.enumDrop('confirm', 'Méthode de confirmation', [
                ['totp', 'TOTP'],
            ]);

            d.text('email', 'Courriel');
            if (d.values.email != null && !d.values.email.includes('@'))
                d.error('email', 'Format non valide');
            d.text('phone', 'Téléphone');
            if (d.values.phone != null && !d.values.phone.startsWith('+'))
                d.error('phone', 'Format non valide (préfixe obligatoire)');

            d.boolean('*admin', 'Administrateur', {value: false, untoggle: false});

            d.action('Créer', {disabled: !d.isValid()}, async () => {
                let query = new URLSearchParams;
                query.set('username', d.values.username);
                query.set('password', d.values.password);
                query.set('change_password', d.values.change_password ? 1 : 0);
                query.set('confirm', d.values.confirm || '');
                if (d.values.email != null)
                    query.set('email', d.values.email);
                if (d.values.phone != null)
                    query.set('phone', d.values.phone);
                query.set('admin', d.values.admin ? 1 : 0);

                let response = await net.fetch('/admin/api/users/create', {
                    method: 'POST',
                    body: query
                });

                if (response.ok) {
                    resolve();
                    log.success(`Utilisateur '${d.values.username}' créé`);

                    users = null;
                    selected_permissions = null;

                    self.go();
                } else {
                    let err = await net.readError(response);

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

                d.multiCheck('admin_permissions', null, props, {
                    value: value,
                    disabled: instance.master != null
                });
            }, {color: '#b518bf'});
            d.sameLine(true); d.section("Enregistrements", () => {
                let props = ENV.permissions.filter(perm => perm.startsWith('data_')).map(makePermissionProp);
                let value = !instance.slaves ? prev_permissions.filter(perm => perm.startsWith('data_')) : null;

                d.multiCheck('data_permissions', null, props, {
                    value: value,
                    disabled: instance.slaves > 0
                });
            }, {color: '#258264'});

            // Now regroup permissions
            let permissions = [...(d.values.admin_permissions || []), ...(d.values.data_permissions || [])];

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
                    let err = await net.readError(response);

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

            d.text('username', 'Nom d\'utilisateur', {value: user.username});

            d.password('password', 'Mot de passe');
            d.password('password2', null, {
                placeholder: 'Confirmation',
                help: [
                    'Laissez vide pour ne pas modifier',
                    'Doit contenir au moins 8 caractères de 3 classes différentes'
                ],
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
            d.enumDrop('confirm', 'Méthode de confirmation', [
                ['totp', 'TOTP'],
            ], { value: user.confirm, untoggle: true });
            d.boolean('reset_secret', 'Réinitialiser le secret TOTP', {
                value: user.confirm == null,
                disabled: d.values.confirm == null,
                untoggle: false
            });

            d.text('email', 'Courriel', {value: user.email});
            if (d.values.email != null && !d.values.email.includes('@'))
                d.error('email', 'Format non valide');
            d.text('phone', 'Téléphone', {value: user.phone});
            if (d.values.phone != null && !d.values.phone.startsWith('+'))
                d.error('phone', 'Format non valide (préfixe obligatoire)');

            d.boolean('*admin', 'Administrateur', {value: user.admin});

            d.action('Modifier', {disabled: !d.isValid()}, async () => {
                let query = new URLSearchParams;
                query.set('userid', user.userid);
                if (d.values.username != null)
                    query.set('username', d.values.username);
                if (d.values.password != null)
                    query.set('password', d.values.password);
                query.set('change_password', 0 + d.values.change_password);
                query.set('confirm', d.values.confirm || '');
                query.set('reset_secret', 0 + d.values.reset_secret);
                if (d.values.email != null)
                    query.set('email', d.values.email);
                if (d.values.phone != null)
                    query.set('phone', d.values.phone);
                query.set('admin', 0 + d.values.admin);

                let response = await net.fetch('/admin/api/users/edit', {
                    method: 'POST',
                    body: query
                });

                if (response.ok) {
                    resolve();
                    log.success(`Utilisateur '${d.values.username}' modifié`);

                    users = null;

                    self.go();
                } else {
                    let err = await net.readError(response);

                    log.error(err);
                    d.refresh();
                }
            });
        });
    }

    function runDeleteUserDialog(e, user) {
        return ui.runDialog(e, `Suppression de ${user.username}`, {}, (d, resolve, reject) => {
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
                    let err = await net.readError(response);
                    throw new Error(err);
                }
            });
        });
    }
};
