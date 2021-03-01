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

// Global variables
let profile = {};
let db;

const goupile = new function() {
    let self = this;

    let session_rnd;
    let profile_keys = {};

    let controller;
    let current_url;

    this.start = async function() {
        if (ENV.urls.base === '/admin/') {
            controller = new AdminController;
            document.documentElement.className = 'admin';
        } else {
            controller = new InstanceController;
            document.documentElement.className = 'instance';
        }

        ui.init();
        await registerSW();
        await initDB();
        initNavigation();
        initTasks();

        await syncProfile();
        if (profile.permissions == null) {
            await runLoginScreen();
        } else {
            await initAfterAuthorization();
        }
    };

    async function initAfterAuthorization() {
        await controller.init();
        await runTasks();

        controller.go(null, window.location.href).catch(err => {
            log.error(err);

            // Now try home page... If that fails too, show login screen.
            // This will solve some situations such as overly restrictive locks.
            controller.go(null, ENV.urls.base).catch(async err => {
                runLoginScreen();
            });
        });
    };

    async function registerSW() {
        try {
            if (navigator.serviceWorker != null) {
                if (ENV.cache_offline) {
                    let registration = await navigator.serviceWorker.register(`${ENV.urls.base}sw.pk.js`);
                    let progress = new log.Entry;

                    if (registration.waiting) {
                        progress.error('Fermez tous les onglets pour terminer la mise à jour puis rafraichissez la page');
                        document.querySelector('#ui_root').classList.add('disabled');
                    } else {
                        registration.addEventListener('updatefound', () => {
                            if (registration.active) {
                                progress.progress('Mise à jour en cours, veuillez patienter');
                                document.querySelector('#ui_root').classList.add('disabled');

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
                        document.querySelector('#ui_root').classList.add('disabled');

                        await registration.unregister();

                        progress.success('Nettoyage effectué, l\'application va redémarrer');
                        setTimeout(() => document.location.reload(), 3000);
                    }
                }
            }
        } catch (err) {
            if (ENV.cache_offline) {
                console.log(err);
                console.log("Service worker API is not available");
            }
        }
    }

    async function initDB() {
        let db_name = `goupile:${ENV.urls.instance}`;

        db = await indexeddb.open(db_name, 8, (db, old_version) => {
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
                case 5: {
                    db.deleteIndex('rec_records', 'parent');
                    db.createIndex('rec_records', 'parent', 'pfkey', {unique: false});
                } // fallthrough
                case 6: {
                    db.deleteIndex('rec_records', 'form');
                    db.deleteIndex('rec_records', 'parent');
                    db.createIndex('rec_records', 'form', 'keys.form', {unique: false});
                    db.createIndex('rec_records', 'parent', 'keys.parent', {unique: false});
                } // fallthrough
                case 7: {
                    db.createIndex('rec_records', 'anchor', 'keys.anchor', {unique: false});
                    db.createIndex('rec_records', 'sync', 'keys.sync', {unique: false});
                } // fallthrough
            }
        });
    }

    async function syncProfile() {
        let new_rnd = util.getCookie('session_rnd', null);

        // Hack to force login screen to show up once when DemoUser setting is in use,
        // this cookie value is set in logout() just before page refresh.
        if (new_rnd === 'LOGIN') {
            util.deleteCookie('session_rnd', '/');
            session_rnd = null;
            return;
        }

        // Deal with lock, if any
        if (new_rnd == null) {
            let lock = await loadSessionValue('lock');

            if (lock != null) {
                util.deleteCookie('session_rnd', '/');

                profile = {
                    userid: lock.userid,
                    username: lock.username,
                    permissions: {
                        'edit': true
                    },
                    lock: lock.ctx
                };
                session_rnd = null;

                profile_keys = {};
                for (let key in lock.keys)
                    profile_keys[key] = base64ToBytes(lock.keys[key]);
            }
        }

        if (new_rnd !== session_rnd) {
            try {
                let response = await net.fetch(`${ENV.urls.instance}api/session/profile`);

                profile = await response.json();
                session_rnd = util.getCookie('session_rnd');

                profile_keys = {};
                for (let key in profile.keys)
                    profile_keys[key] = base64ToBytes(profile.keys[key]);
                delete profile.keys;
            } catch (err) {
                if (!ENV.cache_offline)
                    throw err;

                session_rnd = util.getCookie('session_rnd');
            }
        }
    }

    function runLoginScreen() {
        document.body.classList.remove('gp_loading');

        return ui.runScreen((d, resolve, reject) => {
            d.output(html`
                <img id="gp_logo" src=${ENV.urls.base + 'favicon.png'} alt="" />
                <br/>
            `);

            let username = d.text('*username', 'Nom d\'utilisateur');
            let password = d.password('*password', 'Mot de passe');

            d.action('Se connecter', {disabled: !d.isValid()}, async () => {
                try {
                    await login(null, username.value, password.value, false);
                    resolve();
                } catch (err) {
                    // Never reject because we want to keep the screen open
                    log.error(err);
                }
            });
        });
    }

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

        // Try to force all tabs to reload when instance is locked or unlocked
        window.addEventListener('storage', e => {
            if (e.key === ENV.urls.instance + 'lock' && !!e.newValue !== !!e.oldValue) {
                window.onbeforeunload = null;
                document.location.reload();
            }
        });
    }

    function initTasks() {
        setInterval(async () => {
            await net.fetch(`${ENV.urls.instance}api/session/ping`);
            runTasks();
        }, 300 * 1000);
    }

    async function runTasks() {
        let online = net.isOnline() &&
                     profile.userid != null &&
                     util.getCookie('session_rnd') != null;

        await controller.runTasks(online);
    }

    this.runLoginDialog = function(e) {
        return ui.runDialog(e, (d, resolve, reject) => {
            let username = d.text('*username', 'Nom d\'utilisateur');
            let password = d.password('*password', 'Mot de passe');

            d.action('Se connecter', {disabled: !d.isValid()}, async e => {
                try {
                    await login(e, username.value, password.value, true);
                } catch (err) {
                    reject(err);
                }
            });
        });
    };

    async function login(e, username, password, reload) {
        await self.confirmDangerousAction(e);

        let progress = log.progress('Connexion en cours');
        await tryLogin(username, password, progress, true);

        if (reload) {
            window.onbeforeunload = null;
            document.location.reload();
        } else {
            await initAfterAuthorization();
        }
    };

    async function tryLogin(username, password, progress, retry) {
        try {
            if (net.isOnline() || !ENV.cache_offline) {
                let query = new URLSearchParams;
                query.set('username', username.toLowerCase());
                query.set('password', password);

                let response = await net.fetch(`${ENV.urls.instance}api/session/login`, {
                    method: 'POST',
                    body: query
                });

                if (response.ok) {
                    let new_profile = await response.json();

                    // Save for offline login
                    if (ENV.cache_offline) {
                        let salt = nacl.randomBytes(24);
                        let key = await deriveKey(password, salt);
                        let enc = await encryptSecretBox(new_profile, key);

                        await db.saveWithKey('usr_profiles', username, {
                            salt: bytesToBase64(salt),
                            errors: 0,
                            profile: enc
                        });
                    }

                    profile = new_profile;
                    session_rnd = util.getCookie('session_rnd');

                    profile_keys = {};
                    for (let key in profile.keys)
                        profile_keys[key] = base64ToBytes(profile.keys[key]);
                    delete profile.keys;

                    await deleteSessionValue('lock');

                    progress.success('Connexion réussie');
                } else {
                    if (response.status === 403)
                        await db.delete('usr_profiles', username);

                    let err = (await response.text()).trim();
                    throw new Error(err);
                }
            } else if (ENV.cache_offline) {
                // Instantaneous login feels weird
                await util.waitFor(800);

                let obj = await db.load('usr_profiles', username);
                if (obj == null)
                    throw new Error('Profil hors ligne inconnu');

                let key = await deriveKey(password, base64ToBytes(obj.salt));

                try {
                    profile = await decryptSecretBox(obj.profile, key);
                    session_rnd = util.getCookie('session_rnd');

                    profile_keys = {};
                    for (let key in profile.keys)
                        profile_keys[key] = base64ToBytes(profile.keys[key]);
                    delete profile.keys;

                    await deleteSessionValue('lock');

                    progress.success('Connexion réussie (hors ligne)');
                } catch (err) {
                    obj.errors = (obj.errors || 0) + 1;

                    if (obj.errors >= 3) {
                        await db.delete('usr_profiles', username);
                    } else {
                        await db.saveWithKey('usr_profiles', username, obj);
                    }

                    throw new Error('Mot de passe hors ligne non reconnu');
                }
            }
        } catch (err) {
            if ((err instanceof NetworkError) && retry) {
                return tryLogin(username, password, progress, false);
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

    this.logout = async function(e) {
        await self.confirmDangerousAction(e);

        let progress = log.progress('Déconnexion en cours');

        try {
            let response = await net.fetch(`${ENV.urls.instance}api/session/logout`, {method: 'POST'})

            if (response.ok) {
                profile = {};
                session_rnd = undefined;
                profile_keys = {};

                util.setCookie('session_rnd', 'LOGIN', '/');
                await deleteSessionValue('lock');

                // Clear state and start from fresh as a precaution
                window.onbeforeunload = null;
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

    this.runLockDialog = function(e, ctx) {
        return ui.runDialog(e, (d, resolve, reject) => {
            let pin = d.pin('*pin', 'Code de déverrouillage');
            if (pin.value != null && pin.value.length < 4)
                pin.error('Ce code est trop court', true);

            d.action('Verrouiller', {disabled: !d.isValid()}, e => goupile.lock(e, pin.value, ctx));
        });
    };

    this.runUnlockDialog = function(e) {
        return ui.runDialog(e, (d, resolve, reject) => {
            let pin = d.pin('*pin', 'Code de déverrouillage');
            d.action('Déverrouiller', {disabled: !d.isValid()}, e => goupile.unlock(e, pin.value));
        });
    };

    this.lock = async function(e, password, ctx = null) {
        if (self.isLocked())
            throw new Error('Cannot lock unauthorized session');
        if (typeof ctx == undefined)
            throw new Error('Lock context must not be undefined');

        await self.confirmDangerousAction(e);

        let salt = nacl.randomBytes(24);
        let key = await deriveKey(password, salt);
        let enc = await encryptSecretBox(session_rnd, key);

        let lock = {
            userid: profile.userid,
            username: profile.username,
            salt: bytesToBase64(salt),
            errors: 0,
            keys: {},
            session_rnd: enc,
            ctx: ctx
        };
        for (let key in profile_keys)
            lock.keys[key] = bytesToBase64(profile_keys[key]);

        await storeSessionValue('lock', lock);
        util.deleteCookie('session_rnd', '/');

        window.onbeforeunload = null;
        document.location.reload();
        await util.waitFor(2000);
    };

    this.unlock = async function(e, password) {
        await self.confirmDangerousAction(e);

        let lock = await loadSessionValue('lock');
        if (lock == null)
            throw new Error('Session is not locked');

        let key = await deriveKey(password, base64ToBytes(lock.salt));

        try {
            session_rnd = await decryptSecretBox(lock.session_rnd, key);

            util.setCookie('session_rnd', session_rnd, '/');
            await deleteSessionValue('lock');
        } catch (err) {
            lock.errors = (lock.errors || 0) + 1;

            if (lock.errors >= 3) {
                log.error('Déverrouillage refusé, blocage de sécurité imminent');
                await deleteSessionValue('lock');

                setTimeout(() => {
                    window.onbeforeunload = null;
                    document.location.reload();
                }, 3000);
            } else {
                log.error('Déverrouillage refusé');
                await storeSessionValue('lock', lock);
            }

            return;
        }

        util.setCookie('session_rnd', session_rnd, '/');
        await deleteSessionValue('lock');

        window.onbeforeunload = null;
        document.location.reload();
        await util.waitFor(2000);
    };

    this.confirmDangerousAction = function(e) {
        if (controller == null)
            return;
        if (!controller.hasUnsavedData())
            return;

        return ui.runConfirm(e, "Si vous continuez, vous perdrez les modifications en cours. Voulez-vous continuer ?",
                             "Continuer", () => {});
    };

    this.isLocked = function() { return profile.lock !== undefined; };
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

    this.encryptLocal = function(obj) {
        if (profile_keys.local == null)
            throw new Error('Cannot encrypt without local key');

        return encryptSecretBox(obj, profile_keys.local);
    };

    this.decryptLocal = function(enc) {
        if (profile_keys.local == null)
            throw new Error('Cannot decrypt without local key');

        return decryptSecretBox(enc, profile_keys.local);
    };

    this.encryptBackup = function(obj) {
        if (ENV.backup_key == null)
            throw new Error('This instance is not configured for offline backups');
        if (profile_keys.local == null)
            throw new Error('Cannot encrypt backup without local key');

        let backup_key = base64ToBytes(ENV.backup_key);
        return encryptBox(obj, backup_key, profile_keys.local);
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

    async function loadSessionValue(key) {
        key = ENV.urls.instance + key;

        let json = localStorage.getItem(key);
        if (json == null)
            return null;

        return JSON.parse(json);
    }

    async function storeSessionValue(key, obj) {
        key = ENV.urls.instance + key;
        obj = JSON.stringify(obj);

        localStorage.setItem(key, obj);
    }

    async function deleteSessionValue(key) {
        key = ENV.urls.instance + key;
        localStorage.removeItem(key);
    }
};
