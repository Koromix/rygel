// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function UserManager(db) {
    let self = this;

    let session_profile = {};
    let session_rnd;

    this.runLoginScreen = function() {
        return dialog.runScreen((d, resolve, reject) => {
            d.output(html`
                <img id="usr_logo" src=${`${env.base_url}favicon.png`} alt="" />
                <br/>
            `);

            let username = d.text('*username', 'Nom d\'utilisateur');
            let password = d.password('*password', 'Mot de passe');

            d.action('Se connecter', {disabled: !d.isValid()}, async () => {
                let success = await self.login(username.value, password.value);
                if (success)
                    resolve(username.value);
            });
        });
    };

    this.runLoginDialog = function(e = null, front) {
        return dialog.run(e, (d, resolve, reject) => {
            if (e == null) {
                d.output(html`
                    <img id="usr_logo" src=${`${env.base_url}favicon.png`} alt="" />
                    <br/>
                `);
            }

            let username = d.text('*username', 'Nom d\'utilisateur');
            let password = d.password('*password', 'Mot de passe');

            d.action('Connexion', {disabled: !d.isValid()}, async () => {
                let success = await self.login(username.value, password.value);
                if (success)
                    resolve(username.value);
            });
            d.action('Annuler', {}, () => reject(new Error('Action annulée')));
        });
    };

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
            goupile.initMain();
        } else {
            entry.error('Échec de la connexion');
        }

        return success;
    };

    async function loginOnline(username, password) {
        let query = new URLSearchParams;
        query.set('username', username.toLowerCase());
        query.set('password', password);

        let response = await net.fetch(`${env.base_url}api/user/login`, {
            method: 'POST',
            body: query
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
        let salt = crypto.getRandomValues(new Uint8Array(16));

        let key;
        {
            let algorithm = {
                name: 'PBKDF2',
                hash: 'SHA-256',
                salt: salt,
                iterations: 1000
            };

            key = await crypto.subtle.importKey('raw', pwd_hash, 'PBKDF2', false, ['deriveKey']);
            key = await crypto.subtle.deriveKey(algorithm, key, {name: 'AES-GCM', length: 256}, false, ['encrypt']);
        }

        let profile_enc;
        {
            let algorithm = {
                name: 'AES-GCM',
                iv: iv
            };

            let profile_utf8 = new TextEncoder().encode(JSON.stringify(profile));
            profile_enc = await crypto.subtle.encrypt(algorithm, key, profile_utf8);
        }

        let passport = {
            username: profile.username,
            iv: iv,
            salt: salt,
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

            let key;
            {
                let algorithm = {
                    name: 'PBKDF2',
                    hash: 'SHA-256',
                    salt: passport.salt,
                    iterations: 1000
                };

                key = await crypto.subtle.importKey('raw', pwd_hash, 'PBKDF2', false, ['deriveKey']);
                key = await crypto.subtle.deriveKey(algorithm, key, {name: 'AES-GCM', length: 256}, false, ['decrypt']);
            }

            let profile_utf8;
            {
                let algorithm = {
                    name: 'AES-GCM',
                    iv: passport.iv
                };

                profile_utf8 = await crypto.subtle.decrypt(algorithm, key, passport.profile);
            }

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
            goupile.initMain();
        } else {
            entry.error('Échec de déconnexion');
        }
    };

    async function logoutOnline() {
        let response = await net.fetch(`${env.base_url}api/user/logout`, {method: 'POST'});
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
            let response = await net.fetch(`${env.base_url}api/user/profile`);
            let profile = await response.json();

            if (!env.use_offline || profile.username != null)
                session_profile = profile;
            session_rnd = util.getCookie('session_rnd');
        }
    };

    this.isConnected = function() { return !!session_profile.username; };
    this.isConnectedOnline = function() { return session_rnd != null; }
    this.getUserName = function() { return session_profile.username; };
    this.getZone = function() { return session_profile.zone; };
    this.hasPermission = function(perm) {
        return session_profile.permissions &&
               !!session_profile.permissions[perm];
    };

    this.getLockURL = function() {
        let url = localStorage.getItem('lock_url');
        return url;
    };

    this.runLockDialog = function(e, url) {
        return dialog.run(e, (d, resolve, reject) => {
            d.output('Entrez le code de verrouillage');
            let pin = d.pin('*code');

            if (pin.value && pin.value.length < 4)
                pin.error('Le code doit comporter au moins 4 chiffres', true);

            d.action('Verrouiller', {disabled: !d.isValid()}, () => {
                localStorage.setItem('lock_url', url);
                localStorage.setItem('lock_pin', pin.value);

                log.success('Application verrouillée !');
                goupile.go();

                resolve();
            });
            d.action('Annuler', {}, () => reject(new Error('Action annulée')));
        });
    };

    this.runUnlockDialog = function(e) {
        return dialog.run(e, (d, resolve, reject) => {
            d.output('Entrez le code de déverrouillage');
            let pin = d.pin('code');

            if (pin.value && pin.value.length >= 4) {
                let code = localStorage.getItem('lock_pin');

                if (pin.value === code) {
                    setTimeout(resolve, 0);

                    deleteLock();

                    log.success('Application déverrouillée !');
                    goupile.go();
                } else if (pin.value.length >= code.length) {
                    pin.error('Code erroné');
                }
            }

            d.action('Annuler', {}, () => reject(new Error('Action annulée')));
        });
    };

    function deleteLock() {
        localStorage.removeItem('lock_url');
        localStorage.removeItem('lock_pin');
    }
};
