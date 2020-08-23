// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function UserManager(db) {
    let self = this;

    let session_profile = {};
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

        let success;
        try {
            if (net.isOnline()) {
                success = await loginOnline(username, password);
            } else {
                success = await loginOffline(username, password);
            }
        } catch (err) {
            if (env.use_offline) {
                success = await loginOffline(username, password);
            } else {
                success = false;
            }
        }

        if (success) {
            // Emergency unlocking
            deleteLock();

            entry.success('Connexion réussie');
            await goupile.initMain();
        } else {
            entry.error('Échec de la connexion');
        }

        return success;
    };

    async function loginOnline(username, password) {
        let response = await net.fetch(`${env.base_url}api/login.json`, {
            method: 'POST',
            body: new URLSearchParams({
                username: username.toLowerCase(),
                password: password
            })
        });

        if (response.ok) {
            let profile = await response.json();
            if (env.use_offline)
                await saveOfflineProfile(profile, password);

            session_profile = profile;
            session_rnd = util.getCookie('session_rnd');

            return true;
        } else {
            if (env.use_offline)
                await db.delete('usr_passports', username);

            return false;
        }
    }

    async function saveOfflineProfile(profile, password) {
        let pwd_utf8 = new TextEncoder().encode(password);
        let pwd_hash = await crypto.subtle.digest('SHA-256', pwd_utf8);

        let iv = crypto.getRandomValues(new Uint8Array(12));
        let algorithm = {
            name: 'AES-GCM',
            iv: iv
        };
        let key = await crypto.subtle.importKey('raw', pwd_hash, algorithm.name, false, ['encrypt']);

        let profile_utf8 = new TextEncoder().encode(JSON.stringify(profile));
        let profile_enc = await crypto.subtle.encrypt(algorithm, key, profile_utf8);

        let passport = {
            username: profile.username,
            iv: iv,
            profile: profile_enc
        };
        await db.save('usr_passports', passport);
    }

    async function loginOffline(username, password, entry) {
        // Instantaneous login feels weird
        await util.waitFor(800);

        let passport = await db.load('usr_passports', username);
        if (passport == null)
            return false;

        try {
            let pwd_utf8 = new TextEncoder().encode(password);
            let pwd_hash = await crypto.subtle.digest('SHA-256', pwd_utf8);

            let algorithm = {
                name: 'AES-GCM',
                iv: passport.iv
            };
            let key = await crypto.subtle.importKey('raw', pwd_hash, algorithm.name, false, ['decrypt']);

            let profile_utf8 = await crypto.subtle.decrypt(algorithm, key, passport.profile);

            session_profile = JSON.parse(new TextDecoder().decode(profile_utf8));
            session_rnd = undefined;

            return true;
        } catch (err) {
            await util.waitFor(1200);
            return false;
        }
    }

    this.logout = async function() {
        let entry = new log.Entry;

        entry.progress('Déconnexion en cours');

        let success;
        try {
            if (net.isOnline()) {
                success = await logoutOnline();
            } else {
                success = await logoutOffline();
            }
        } catch (err) {
            if (env.use_offline) {
                success = await logoutOffline();
            } else {
                success = false;
            }
        }

        if (success) {
            session_profile = {};
            session_rnd = undefined;

            entry.success('Déconnexion réussie');
            await goupile.initMain();
        } else {
            entry.error('Échec de déconnexion');
        }
    };

    async function logoutOnline() {
        let response = await net.fetch(`${env.base_url}api/logout.json`, {method: 'POST'});
        return response.ok;
    }

    function logoutOffline() {
        // We can't delete session_key (HttpOnly), nor can we delete the session on
        // the server. But if we delete session_rnd, the mismatch will cause the
        // server to close the session as soon as possible.
        util.deleteCookie('session_rnd', env.base_url);
        return true;
    }

    this.isSynced = function() {
        if (net.isOnline()) {
            let new_rnd = util.getCookie('session_rnd');
            return new_rnd === session_rnd;
        } else {
            return true;
        }
    };

    this.fetchProfile = async function() {
        if (net.isOnline()) {
            let response = await net.fetch(`${env.base_url}api/profile.json`);
            let profile = await response.json();

            if (!env.use_offline || profile.username != null) {
                session_profile = profile;
                session_rnd = util.getCookie('session_rnd');
            }
        }
    };

    this.isConnected = function() { return !!session_profile.username; };
    this.getUserName = function() { return session_profile.username; };
    this.hasPermission = function(perm) {
        return session_profile.permissions &&
               !!session_profile.permissions[perm];
   };

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
