// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Global variables
let profile = {};
let db;

const goupile = new function() {
    let self = this;

    let session_rnd;
    let passport;

    let controller;
    let current_url;

    this.start = async function() {
        ui.init();

        await registerSW();
        await initDB();

        initNavigation();

        if (ENV.base_url === '/admin/') {
            controller = new AdminController;
        } else {
            controller = new InstanceController;
        }
        await controller.start();
    };

    function initNavigation() {
        window.addEventListener('popstate', e => controller.go(null, window.location.href, false));

        util.interceptLocalAnchors((e, href) => {
            let func = ui.wrapAction(e => controller.go(e, href));
            func(e);

            e.preventDefault();
        });

        // Copied that crap from a random corner of the internet
        let electron = (typeof process !== 'undefined' && typeof process.versions === 'object' &&
                        !!process.versions.electron);
        if (electron) {
            let protect = true;

            window.onbeforeunload = e => {
                if (protect && controller.hasUnsavedData()) {
                    e.returnValue = "NO!";

                    let remote = require('electron').remote;
                    let dialog = remote.dialog;

                    let win = remote.getCurrentWindow();
                    let p = dialog.showMessageBox(win, {
                        type: 'warning',
                        buttons: ['Quitter', 'Annuler'],
                        title: 'Données non enregistrées',
                        message: 'Si vous continuer vous allez perdre les modifications non enregistrées, voulez-vous continuer ?'
                    });

                    p.then(r => {
                        if (r.response === 0) {
                            protect = false;
                            win.close();
                        }
                    });
                }
            };
        } else {
            window.onbeforeunload = e => {
                if (controller.hasUnsavedData())
                    return 'Si vous confirmez vouloir quitter la page, les modifications en cours seront perdues !';
            };
        }
    }

    async function registerSW() {
        if (navigator.serviceWorker != null) {
            if (ENV.cache_offline) {
                let registration = await navigator.serviceWorker.register(`${ENV.base_url}sw.pk.js`);
                let progress = new log.Entry;

                if (registration.waiting) {
                    progress.error('Fermez tous les onglets pour terminer la mise à jour');
                } else {
                    registration.addEventListener('updatefound', () => {
                        if (registration.active) {
                            progress.progress('Mise à jour en cours, veuillez patienter');

                            registration.installing.addEventListener('statechange', e => {
                                if (e.target.state === 'installed') {
                                    progress.success('Mise à jour effectuée, l\'application va redémarrer');
                                    setTimeout(() => document.location.reload(), 3000);
                                }
                            });
                        }
                    });
                }
            } else {
                let registration = await navigator.serviceWorker.getRegistration();
                let progress = new log.Entry;

                if (registration != null) {
                    progress.progress('Nettoyage de l\'instance en cache, veuillez patienter');

                    await registration.unregister();

                    progress.success('Nettoyage effectué, l\'application va redémarrer');
                    setTimeout(() => document.location.reload(), 3000);
                }
            }
        }
    }

    async function initDB() {
        let db_name = `goupile:${ENV.base_url}`;

        db = await indexeddb.open(db_name, 5, (db, old_version) => {
            switch (old_version) {
                case null: {
                    db.createStore('usr_profiles');
                } // fallthrough
                case 1: {
                    db.createStore('fs_files');
                } // fallthrough
                case 2: {
                    db.createStore('rec_records');
                } // fallthrough
                case 3: {
                    db.createIndex('rec_records', 'form', 'fkey', {unique: false});
                } // fallthrough
                case 4: {
                    db.createIndex('rec_records', 'parent', 'pkey', {unique: false});
                } // fallthrough
            }
        });
    }

    this.runLogin = function() {
        return ui.runScreen((d, resolve, reject) => {
            d.output(html`
                <img id="gp_logo" src=${ENV.base_url + 'favicon.png'} alt="" />
                <br/>
            `);

            let username = d.text('*username', 'Nom d\'utilisateur');
            let password = d.password('*password', 'Mot de passe');

            d.action('Se connecter', {disabled: !d.isValid()}, async () => {
                try {
                    await self.login(username.value, password.value);
                    resolve(username.value);
                } catch (err) {
                    // Never reject because we want to keep the screen open
                    log.error(err);
                }
            });
        });
    };

    this.login = function(username, password) {
        let progress = log.progress('Connexion en cours');
        return login(username, password, progress, true);
    };

    async function login(username, password, progress, retry) {
        try {
            if (net.isOnline() || !ENV.cache_offline) {
                let query = new URLSearchParams;
                query.set('username', username.toLowerCase());
                query.set('password', password);

                let response = await net.fetch(`${ENV.base_url}api/user/login`, {
                    method: 'POST',
                    body: query
                });

                if (response.ok) {
                    profile = await response.json();
                    session_rnd = util.getCookie('session_rnd');
                    passport = (profile.passport != null) ? base64ToBytes(profile.passport) : null;

                    // Save for offline login
                    if (ENV.cache_offline) {
                        let salt = nacl.randomBytes(24);
                        let key = await deriveKey(password, salt);

                        let enc = await encryptSecretBox(profile, key);

                        await db.saveWithKey('usr_profiles', username, {
                            salt: bytesToBase64(salt),
                            profile: enc
                        });
                    }

                    progress.success('Connexion réussie');
                } else {
                    let err = (await response.text()).trim();
                    throw new Error(err);
                }
            } else if (ENV.cache_offline) {
                // Instantaneous login feels weird
                await util.waitFor(800);

                let enc = await db.load('usr_profiles', username);
                if (enc == null)
                    throw new Error('Profil hors ligne inconnu');

                let key = await deriveKey(password, base64ToBytes(enc.salt));

                try {
                    profile = await decryptSecretBox(enc.profile, key);
                    session_rnd = util.getCookie('session_rnd');
                    passport = (profile.passport != null) ? base64ToBytes(profile.passport) : null;

                    progress.success('Connexion réussie (hors ligne)');
                } catch (err) {
                    throw new Error('Mot de passe hors ligne non reconnu');
                }
            }
        } catch (err) {
            if ((err instanceof NetworkError) && retry) {
                return login(username, password, progress, false);
            } else {
                progress.close();
                throw err;
            }
        }
    }

    function deriveKey(password, salt) {
        return new Promise((resolve, reject) => {
            scrypt(password, salt, {
                N: 16384,
                r: 8,
                p: 1,
                dkLen: 32,
                encoding: 'binary'
            }, resolve);
        });
    }

    this.logout = async function() {
        let progress = log.progress('Déconnexion en cours');

        try {
            let response = await net.fetch(`${ENV.base_url}api/user/logout`, {method: 'POST'})

            if (response.ok) {
                profile = {};
                session_rnd = undefined;
                passport = undefined;

                util.setCookie('session_rnd', 'LOGIN', ENV.base_url);

                // Clear state and start from fresh as a precaution
                document.location.reload();
            } else {
                let err = (await response.text()).trim();
                throw new Error(err);
            }
        } catch (err) {
            progress.close();
            throw err;
        }
    }

    // XXX: Exponential backoff
    this.syncProfile = async function() {
        let new_rnd = util.getCookie('session_rnd');

        // Hack to force login screen to show up once when DemoUser setting is in use,
        // this cookie value is set in logout() just before page refresh.
        if (new_rnd === 'LOGIN') {
            util.deleteCookie('session_rnd', ENV.base_url);
            return;
        }

        if (new_rnd !== session_rnd) {
            try {
                let response = await net.fetch(`${ENV.base_url}api/user/profile`);

                profile = await response.json();
                session_rnd = util.getCookie('session_rnd');
                passport = (profile.passport != null) ? base64ToBytes(profile.passport) : null;
            } catch (err) {
                if (!ENV.cache_offline)
                    throw err;
            }
        }
    };

    this.isAuthorized = function() { return !!profile.username; };
    this.hasPermission = function(perm) {
        return profile.permissions != null &&
               profile.permissions[perm];
    }

    this.syncHistory = function(url, push = true) {
        if (push && current_url != null && url !== current_url) {
            window.history.pushState(null, null, url);
        } else {
            window.history.replaceState(null, null, url);
        }

        current_url = url;
    };

    this.encryptWithPassport = function(obj) {
        if (passport == null)
            throw new Error('Cannot encrypt without passport');

        return encryptSecretBox(obj, passport);
    };

    this.decryptWithPassport = function(enc) {
        if (passport == null)
            throw new Error('Cannot decrypt without passport');

        return decryptSecretBox(enc, passport);
    };

    this.encryptBackup = function(obj) {
        if (ENV.backup_key == null)
            throw new Error('This instance is not configured for offline backups');
        if (passport == null)
            throw new Error('Cannot encrypt without passport');

        let backup_key = base64ToBytes(ENV.backup_key);
        return encryptBox(obj, backup_key, passport);
    }

    async function encryptSecretBox(obj, key) {
        let nonce = new Uint8Array(24);
        crypto.getRandomValues(nonce);

        let json = JSON.stringify(obj);
        let message = base64ToBytes(window.btoa(json));
        let box = nacl.secretbox(message, nonce, key);

        let enc = {
            nonce: bytesToBase64(nonce),
            box: bytesToBase64(box)
        };
        return enc;
    }

    async function decryptSecretBox(enc, key) {
        let nonce = base64ToBytes(enc.nonce);
        let box = base64ToBytes(enc.box);

        let message = nacl.secretbox.open(box, nonce, key);
        if (message == null)
            throw new Error('Failed to decrypt message: wrong key?');

        let json = window.atob(bytesToBase64(message));
        let obj = JSON.parse(json);

        return obj;
    }

    async function encryptBox(obj, public_key, secret_key) {
        let nonce = new Uint8Array(24);
        crypto.getRandomValues(nonce);

        let json = JSON.stringify(obj);
        let message = base64ToBytes(window.btoa(json));
        let box = nacl.box(message, nonce, public_key, secret_key);

        let enc = {
            nonce: bytesToBase64(nonce),
            box: bytesToBase64(box)
        };
        return enc;
    }
};
