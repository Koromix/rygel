// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function UserManager(db) {
    let self = this;

    let profile = {};
    let session_rnd;

    this.runLogin = function() {
        let state = new PageState;

        let update = () => {
            let model = new PageModel('@login');

            let builder = new PageBuilder(state, model);
            builder.changeHandler = update;
            builder.pushOptions({
                missing_mode: 'disable',
                wide: true
            });

            makeLoginForm(builder);
            builder.action('Connexion', {disabled: !builder.isValid()}, builder.submit);

            let focus = !document.querySelector('#usr_login');

            render(html`
                <div style="flex: 1;"></div>
                <form id="usr_login" @submit=${e => e.preventDefault()}>
                    <img id="usr_logo" src=${`${env.base_url}favicon.png`} alt="" />
                    ${model.render()}
                </form>
                <div style="flex: 1;"></div>
            `, document.querySelector('#gp_all'));

            if (focus) {
                let el = document.querySelector('#usr_login input');
                setTimeout(() => el.focus(), 0);
            }
        };
        update();
    };

    this.showLoginDialog = function(e) {
        dialog.popup(e, 'Connexion', makeLoginForm);
    };

    function makeLoginForm(page, close = null) {
        let username = page.text('*username', 'Nom d\'utilisateur');
        let password = page.password('*password', 'Mot de passe');

        page.submitHandler = async () => {
            let success = await self.login(username.value, password.value);
            if (success && close != null)
                close();
        };
    }

    this.login = async function(username, password) {
        let entry = new log.Entry;

        entry.progress('Connexion en cours');

        if (net.isOnline()) {
            return await loginOnline(username, password, entry);
        } else if (env.use_offline) {
            return await loginOffline(username, password, entry);
        }
    };

    async function loginOnline(username, password, entry) {
        try {
            let response = await net.fetch(`${env.base_url}api/login.json`, {
                method: 'POST',
                body: new URLSearchParams({
                    username: username.toLowerCase(),
                    password: password
                })
            });

            if (response.ok) {
                let json = await response.json();

                if (env.use_offline) {
                    await saveOfflineToken(username, password, json.token);
                    sessionStorage.setItem('offline_username', username);
                }

                profile = json.profile;
                session_rnd = util.getCookie('session_rnd');

                // Emergency unlocking
                deleteLock();

                entry.success('Connexion réussie');
                await goupile.initMain();

                return true;
            } else {
                let msg = (await response.text()).trim();
                entry.error(msg);

                return false;
            }
        } catch (err) {
            entry.error(err);
            return false;
        }
    }

    async function saveOfflineToken(username, password, token) {
        let salt;
        {
            let digits = '0123456789ABCDEF';

            salt = window.crypto.getRandomValues(new Uint8Array(32));
            salt = Array.from(salt).map(value => digits[Math.floor(value / 16)] + digits[value % 16]);
            salt = salt.join('');
        }

        let offline = {
            username: username,
            password_salt: salt,
            password_sha256: Sha256(salt + password),
            token: token
        };

        await db.save('usr_offline', offline);
    }

    async function loginOffline(username, password, entry) {
        try {
            let [offline, new_profile] = await Promise.all([
                db.load('usr_offline', username),
                db.load('usr_profiles', username)
            ]);

            if (offline == null || new_profile == null) {
                entry.error('Ce compte n\'est pas disponible dans le cache hors ligne');
                return false;
            }

            let password_sha256 = Sha256(offline.password_salt + password);
            if (password_sha256 !== offline.password_sha256) {
                entry.error('Ce compte n\'est pas disponible dans le cache hors ligne');
                return false;
            }

            profile = new_profile;
            sessionStorage.setItem('offline_username', username);

            entry.success('Connexion réussie (hors ligne)');
            await goupile.initMain();

            return true;
        } catch (err) {
            entry.error(err);
            return false;
        }
    }

    this.logout = async function() {
        let entry = new log.Entry;

        entry.progress('Déconnexion en cours');
        try {
            if (env.use_offline) {
                let username = sessionStorage.getItem('offline_username');

                if (username != null) {
                    await Promise.all([
                        db.delete('usr_offline', username),
                        db.delete('usr_profiles', username)
                    ]);
                }

                sessionStorage.removeItem('offline_username');
            }

            if (net.isOnline()) {
                let response = await net.fetch(`${env.base_url}api/logout.json`, {method: 'POST'});

                if (!response.ok) {
                    let msg = (await response.text()).trim();
                    entry.error(msg);

                    return false;
                }
            } else {
                // XXX: Detect this on the server!
                util.deleteCookie('session_rnd', env.base_url);
            }

            entry.success('Déconnexion réussie');
            await goupile.initMain();

            return true;
        } catch (err) {
            entry.error(err);
            return false;
        }
    };

    this.fetchProfile = async function() {
        if (net.isOnline()) {
            // Try online first
            let response = await net.fetch(`${env.base_url}api/profile.json`);
            if (response.ok) {
                let new_profile = await response.json();

                if (new_profile.username != null) {
                    profile = new_profile;
                    session_rnd = util.getCookie('session_rnd');

                    if (env.use_offline)
                        await db.save('usr_profiles', profile);

                    return;
                }
            }

            // It failed, but in offline mode we can try to reconnect using offline token (if any)
            if (env.use_offline) {
                let username = sessionStorage.getItem('offline_username');

                if (username != null) {
                    let offline = await db.load('usr_offline', username);

                    if (offline != null) {
                        let response = await net.fetch(`${env.base_url}api/reconnect.json`, {
                            method: 'POST',
                            body: new URLSearchParams({token: offline.token})
                        });

                        if (response.ok) {
                            profile = await response.json();
                            session_rnd = util.getCookie('session_rnd');

                            return;
                        }
                    }
                }
            }

            // Well, too bad. Erase all traces.
            profile = {};
            session_rnd = undefined;
            util.deleteCookie('session_rnd', env.base_url);
            sessionStorage.removeItem('offline_username');
        } else {
            let username = sessionStorage.getItem('offline_username');

            if (username != null && profile.username !== username) {
                profile = await db.load('usr_profiles', username);
                if (profile == null)
                    profile = {};
            }
        }
    };

    this.testCookies = function() {
        if (net.isOnline()) {
            let new_rnd = util.getCookie('session_rnd');
            return new_rnd !== session_rnd;
        } else {
            return false;
        }
    };

    this.isConnected = function() { return !!profile.username; };
    this.getUserName = function() { return profile.username; };
    this.hasPermission = function(perm) { return profile.permissions && !!profile.permissions[perm]; };

    this.getLockURL = function() {
        let url = localStorage.getItem('lock_url');
        return url;
    };

    this.showLockDialog = function(e, url) {
        dialog.popup(e, 'Verrouiller', (page, close) => {
            page.output('Entrez le code de verrouillage');
            let pin = page.pin('*code');

            if (pin.value && pin.value.length < 4)
                pin.error('Le code doit comporter au moins 4 chiffres', true);

            page.submitHandler = () => {
                close();

                localStorage.setItem('lock_url', url);
                localStorage.setItem('lock_pin', pin.value);

                log.success('Application verrouillée !');
                goupile.go();
            };
        });
    };

    this.showUnlockDialog = function(e) {
        dialog.popup(e, null, (page, close) => {
            page.output('Entrez le code de déverrouillage');
            let pin = page.pin('code');

            if (pin.value && pin.value.length >= 4) {
                let code = localStorage.getItem('lock_pin');

                if (pin.value === code) {
                    setTimeout(close, 0);

                    deleteLock();

                    log.success('Application déverrouillée !');
                    goupile.go();
                } else if (pin.value.length >= code.length) {
                    pin.error('Code erroné');
                }
            }
        });
    };

    function deleteLock() {
        localStorage.removeItem('lock_url');
        localStorage.removeItem('lock_pin');
    }
};
