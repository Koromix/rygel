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

const goupile = new function() {
    let self = this;

    let session_rnd;
    let profile_keys = new Map;

    let controller;
    let current_url;

    this.start = async function() {
        if (ENV.urls.base === '/admin/') {
            controller = new AdminController;
            document.documentElement.className = 'admin';
        } else {
            // Detect base URLs
            {
                let url = new URL(window.location.href);
                let parts = url.pathname.split('/', 3);

                if (parts[2] && parts[2] !== 'main')
                    ENV.urls.instance += `${parts[2]}/`;
            }

            controller = new InstanceController;
            document.documentElement.className = 'instance';
        }

        ui.init();
        await registerSW();
        initNavigation();
        initTasks();

        await syncProfile();
        if (profile.authorized) {
            await initAfterAuthorization();
        } else {
            await runLoginScreen();
        }
    };

    async function initAfterAuthorization() {
        let url = new URL(window.location.href);

        if (profile.instance != null) {
            ENV.title = profile.instance.title;
            ENV.urls.instance = profile.instance.url;

            if (!url.pathname.startsWith(ENV.urls.instance))
                url = new URL(ENV.urls.instance, window.location.href);
        }

        await controller.init();
        await runTasks();

        controller.go(null, url.pathname).catch(err => {
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

                let new_profile = {
                    userid: lock.userid,
                    username: lock.username,
                    permissions: {
                        'edit': true
                    },
                    keys: lock.keys,
                    lock: lock.ctx
                };
                updateProfile(new_profile);
            }
        }

        if (new_rnd !== session_rnd) {
            try {
                let response = await net.fetch(`${ENV.urls.instance}api/session/profile`);
                let new_profile = await response.json();
                updateProfile(new_profile);
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
                        let db = await openProfileDB();

                        let salt = nacl.randomBytes(24);
                        let key = await deriveKey(password, salt);
                        let enc = await encryptSecretBox(new_profile, key);

                        await db.saveWithKey('usr_profiles', username, {
                            salt: bytesToBase64(salt),
                            errors: 0,
                            profile: enc
                        });
                    }

                    updateProfile(new_profile);
                    await deleteSessionValue('lock');

                    progress.success('Connexion réussie');
                } else {
                    if (response.status === 403) {
                        let db = await openProfileDB();
                        await db.delete('usr_profiles', username);
                    }

                    let err = (await response.text()).trim();
                    throw new Error(err);
                }
            } else if (ENV.cache_offline) {
                let db = await openProfileDB();

                // Instantaneous login feels weird
                await util.waitFor(800);

                let obj = await db.load('usr_profiles', username);
                if (obj == null)
                    throw new Error('Profil hors ligne inconnu');

                let key = await deriveKey(password, base64ToBytes(obj.salt));

                try {
                    let new_profile = await decryptSecretBox(obj.profile, key);

                    updateProfile(new_profile);
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

    async function openProfileDB() {
        let db_name = `goupile+${ENV.urls.base}`;
        let db = await indexeddb.open(db_name, 1, (db, old_version) => {
            switch (old_version) {
                case null: {
                    db.createStore('usr_profiles');
                } // fallthrough
            }
        });

        return db;
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

    function updateProfile(new_profile) {
        profile = new_profile;

        // Keep keys local to "hide" (as much as JS allows..) them
        if (profile.keys != null) {
            profile_keys = new Map(profile.keys.map(it => [it[0], base64ToBytes(it[1])]));
            profile.keys = profile.keys.map(key => key[0]);
        } else {
            profile_keys = new Map;
        }

        session_rnd = util.getCookie('session_rnd');
    }

    this.logout = async function(e) {
        await self.confirmDangerousAction(e);

        let progress = log.progress('Déconnexion en cours');

        try {
            let response = await net.fetch(`${ENV.urls.instance}api/session/logout`, {method: 'POST'})

            if (response.ok) {
                updateProfile({});
                await deleteSessionValue('lock');

                // Force login after reload
                util.setCookie('session_rnd', 'LOGIN', '/');

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
            keys: Array.from(profile_keys.items()),
            session_rnd: enc,
            ctx: ctx
        };
        for (let key of lock.keys)
            key[1] = bytesToBase64(key[1]);

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

    this.encryptSymmetric = function(obj, type) {
        let key = profile_keys.get(type);
        if (key == null)
            throw new Error(`Cannot encrypt without '${type}' key`);

        return encryptSecretBox(obj, key);
    };

    this.decryptSymmetric = function(enc, type) {
        let key = profile_keys.get(type);
        if (key == null)
            throw new Error(`Cannot decrypt without '${type}' key`);

        return decryptSecretBox(enc, key);
    };

    this.encryptBackup = function(obj) {
        if (ENV.backup_key == null)
            throw new Error('This instance is not configured for offline backups');

        let key = profile_keys.get(profile.userid);
        if (key == null)
            throw new Error('Cannot encrypt backup without local key');

        let backup_key = base64ToBytes(ENV.backup_key);
        return encryptBox(obj, backup_key, key);
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
