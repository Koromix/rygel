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

    // Copied that crap from a random corner of the internet
    let electron = (typeof process !== 'undefined' && typeof process.versions === 'object' &&
                    !!process.versions.electron);

    let profile_keys = {};
    let online = true;

    let controller;
    let current_url;

    this.start = async function() {
        let url = new URL(window.location.href);

        if (ENV.urls.base === '/admin/') {
            controller = new AdminController;
            document.documentElement.className = 'admin';
        } else {
            controller = new InstanceController;
            document.documentElement.className = 'instance';

            // Detect base URLs
            {
                let parts = url.pathname.split('/', 3);

                if (parts[2] && parts[2] !== 'main')
                    ENV.urls.instance += `${parts[2]}/`;
            }
        }

        // Try to work around Safari bug:
        // https://bugs.webkit.org/show_bug.cgi?id=226547
        if (navigator.userAgent.toLowerCase().indexOf('Safari') >= 0)
            await indexeddb.open('dummy');

        ui.init();
        await registerSW();
        initNavigation();

        {
            let login = url.searchParams.get('login');
            let token = url.searchParams.get('token');

            if (login) {
                self.syncHistory(url.pathname, false);
            } else if (token) {
                try {
                    await sendToken(token);
                } catch (err) {
                    log.error(err);
                }
                self.syncHistory(url.pathname, false);
            } else {
                await syncProfile();
            }
        }

        if (profile.authorized) {
            await initAfterAuthorization();
        } else if (profile.confirm == 'SMS') {
            await runConfirmScreen();
        } else {
            await runLoginScreen();
        }
    };

    async function initAfterAuthorization() {
        let url = new URL(window.location.href);

        if (profile.instances != null) {
            let instance = profile.instances.find(instance => url.pathname.startsWith(instance.url)) ||
                           profile.instances[0];

            ENV.title = instance.title;
            ENV.urls.instance = instance.url;

            if (!url.pathname.startsWith(instance.url))
                url = new URL(instance.url, window.location.href);
        }

        await controller.init();
        await initTasks();

        controller.go(null, url.href).catch(err => {
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
        let session_rnd = util.getCookie('session_rnd');

        if (session_rnd != null) {
            try {
                let response = await net.fetch(`${ENV.urls.instance}api/session/profile`);

                let new_profile = await response.json();
                await updateProfile(new_profile, true);
            } catch (err) {
                if (ENV.cache_offline) {
                    if (!(err instanceof NetworkError))
                        log.error(err);
                } else {
                    throw err;
                }
            }
        }

        // Client-side lock?
        {
            let lock = await loadSessionValue('lock');

            if (lock != null) {
                util.deleteCookie('session_rnd', '/');

                let new_profile = {
                    userid: lock.userid,
                    username: lock.username,
                    authorized: true,
                    permissions: {
                        data_load: true,
                        data_save: true
                    },
                    namespaces: lock.namespaces,
                    keys: lock.keys,
                    lock: {
                        unlockable: true,
                        ctx: lock.ctx
                    }
                };
                await updateProfile(new_profile, false);

                return;
            }
        }
    }

    async function runLoginScreen(e) {
        return ui.runScreen((d, resolve, reject) => {
            d.output(html`
                <img id="gp_logo" src=${ENV.urls.base + 'favicon.png'} alt="" />
                <br/>
            `);

            let username = d.text('*username', 'Nom d\'utilisateur');
            let password = d.password('*password', 'Mot de passe');

            d.action('Se connecter', {disabled: !d.isValid()}, async () => {
                try {
                    let progress = log.progress('Connexion en cours');

                    await tryLogin(username.value, password.value, progress, net.isOnline());
                    await initAfterAuthorization();

                    resolve();
                } catch (err) {
                    log.error(err);
                    d.refresh();
                }
            });
        });
    }

    async function sendToken(token) {
        let query = new URLSearchParams();
        query.set('token', token);

        let response = await net.fetch(`${ENV.urls.instance}api/session/token`, {
            method: 'POST',
            body: query
        })

        if (response.ok) {
            let new_profile = await response.json();
            await updateProfile(new_profile, true);
        } else {
            let err = (await response.text()).trim();
            throw new Error(err);
        }
    }

    function runConfirmScreen() {
        return ui.runScreen((d, resolve, reject) => {
            d.output(html`
                <img id="gp_logo" src=${ENV.urls.base + 'favicon.png'} alt="" />
                <br/><br/>
                Veuillez entrer le code secret qui vous a été <b>envoyé par SMS</b>.
                <br/>
            `);
            let code = d.password('*code', 'Code secret');

            d.action('Continuer', {disabled: !d.isValid()}, async () => {
                let progress = log.progress('Connexion en cours');

                try {
                    let query = new URLSearchParams;
                    query.set('code', code.value);

                    let response = await net.fetch(`${ENV.urls.instance}api/session/confirm`, {
                        method: 'POST',
                        body: query
                    });

                    if (response.ok) {
                        let new_profile = await response.json();
                        await updateProfile(new_profile, true);

                        await initAfterAuthorization();

                        progress.success('Connexion réussie');
                        resolve();
                    } else {
                        let err = (await response.text()).trim();
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

    this.runChangePasswordDialog = function(e) {
        return ui.runDialog(e, 'Changement de mot de passe', (d, resolve, reject) => {
            let old_password = d.password('*old_password', 'Ancien mot de passe');
            let new_password = d.password('*new_password', 'Nouveau mot de passe');
            let new_password2 = d.password('*new_password2', null, {
                placeholder: 'Confirmation',
                help: 'Doit contenir au moins 8 caractères de 3 classes différentes'
            });
            if (new_password.value != null && new_password2.value != null && new_password.value !== new_password2.value)
                new_password2.error('Les mots de passe sont différents');

            d.action('Modifier', {disabled: !d.isValid()}, async () => {
                let progress = log.progress('Modification du mot de passe');

                try {
                    let query = new URLSearchParams;
                    query.set('old_password', old_password.value);
                    query.set('new_password', new_password.value);

                    let response = await net.fetch(`${ENV.urls.instance}api/password/change`, {
                        method: 'POST',
                        body: query
                    });

                    if (response.ok) {
                        resolve();
                        progress.success('Mot de passe modifié');
                    } else {
                        let err = (await response.text()).trim();
                        throw new Error(err);
                    }
                } catch (err) {
                    progress.close();

                    log.error(err);
                    d.refresh();
                }
            });
        });
    };

    function initNavigation() {
        window.addEventListener('popstate', e => controller.go(null, window.location.href, { push_history: false }));

        util.interceptLocalAnchors((e, href) => {
            let func = ui.wrapAction(e => controller.go(e, href));
            func(e);

            e.preventDefault();
        });

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

    async function initTasks() {
        net.retryHandler = async response => {
            if (response.status === 401) {
                try {
                    await self.confirmIdentity();
                    return true;
                } catch (err) {
                    return false;
                }
            } else {
                return false;
            }
        };

        if (controller.runTasks != null) {
            let ignore_ping = false;
            setInterval(async () => {
                if (ignore_ping)
                    return;

                try {
                    ignore_ping = true;

                    await pingServer();
                    await runTasks();
                } finally {
                    ignore_ping = false;
                }
            }, 60 * 1000);

            document.addEventListener('visibilitychange', () => {
                if (document.visibilityState === 'visible')
                    pingServer();
            });
            window.addEventListener('online', pingServer);
            window.addEventListener('offline', pingServer);
            net.changeHandler = async online => {
                await runTasks();
                controller.go();
            };

            await runTasks();
        }
    }

    async function runTasks() {
        try {
            let online = net.isOnline() && util.getCookie('session_rnd') != null;
            await controller.runTasks(online);
        } catch (err) {
            console.log(err);
        }
    }

    async function pingServer() {
        try {
            let response = await net.fetch(`${ENV.urls.instance}api/session/ping`);
            net.setOnline(response.ok);
        } catch (err) {
            // Automatically set to offline
        }
    }

    async function tryLogin(username, password, progress, online) {
        try {
            if (online || !ENV.cache_offline) {
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

                    await updateProfile(new_profile, true);
                    await deleteSessionValue('lock');

                    if (progress != null)
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
                let new_profile;

                try {
                    new_profile = await decryptSecretBox(obj.profile, key);

                    // Reset errors after successful offline login
                    if (obj.errors) {
                        obj.errors = 0;
                        await db.saveWithKey('usr_profiles', username, obj);
                    }

                    if (progress != null)
                        progress.success('Connexion réussie (hors ligne)');
                } catch (err) {
                    obj.errors = (obj.errors || 0) + 1;

                    if (obj.errors >= 5) {
                        await db.delete('usr_profiles', username);
                        throw new Error('Mot de passe non reconnu, connexion hors ligne désactivée');
                    } else {
                        await db.saveWithKey('usr_profiles', username, obj);
                        throw new Error('Mot de passe non reconnu');
                    }
                }

                await updateProfile(new_profile, false);
                await deleteSessionValue('lock');
            }
        } catch (err) {
            if ((err instanceof NetworkError) && online && ENV.cache_offline) {
                return tryLogin(username, password, progress, false);
            } else {
                if (progress != null)
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

    async function updateProfile(new_profile, online) {
        let usb_key;
        if (new_profile.encrypt_usb) {
            if (!electron)
                throw new Error('La sécurisation USB n\'est disponible que sur le client Electron');

            let ipcRenderer = require('electron').ipcRenderer;
            let id = Sha256(ENV.urls.instance);

            usb_key = ipcRenderer.sendSync('get_usb_key', id);
            if (usb_key == null)
                throw new Error('Insérez la clé USB de sécurité avant de continuer');
            usb_key = base64ToBytes(usb_key);

            let waiting = log.progress('Veuillez retirer la clé pour continuer');

            try {
                while (ipcRenderer.sendSync('has_usb_key', id))
                    await util.waitFor(2000);
            } finally {
                waiting.close();
            }
        }

        profile = Object.assign({}, new_profile);
        profile.online = online;

        // Keep keys local to "hide" (as much as JS allows..) them
        if (profile.keys != null) {
            profile_keys = profile.keys;
            delete profile.keys;

            for (let key in profile_keys) {
                let value = base64ToBytes(profile_keys[key]);
                if (usb_key != null) {
                    let salt = value.slice(0, 24);
                    let combined = new Uint8Array(value.length + usb_key.length);
                    combined.set(value, 0);
                    combined.set(usb_key, value.length);
                    value = await deriveKey(combined, salt);
                }
                profile_keys[key] = value;
            }
        } else {
            profile_keys = new Map;
        }
    }

    this.logout = async function(e) {
        await self.confirmDangerousAction(e);

        let progress = log.progress('Déconnexion en cours');

        try {
            let response = await net.fetch(`${ENV.urls.instance}api/session/logout`, {method: 'POST'})

            if (response.ok) {
                await updateProfile({}, false);
                await deleteSessionValue('lock');

                // Clear state and start from fresh as a precaution
                let url = new URL(window.location.href);
                let reload = util.pasteURL(url.pathname, {login: 1});

                window.onbeforeunload = null;
                document.location.href = reload;
            } else {
                let err = (await response.text()).trim();
                throw new Error(err);
            }
        } catch (err) {
            progress.close();
            throw err;
        }
    }

    this.goToLogin = async function(e) {
        await self.confirmDangerousAction(e);

        let url = new URL(window.location.href);
        let reload = util.pasteURL(url.pathname, {login: 1});

        window.onbeforeunload = null;
        document.location.href = reload;
    }

    this.runLockDialog = function(e, ctx) {
        return ui.runDialog(e, 'Verrouillage', (d, resolve, reject) => {
            let pin = d.pin('*pin', 'Code de déverrouillage');
            if (pin.value != null && pin.value.length < 4)
                pin.error('Ce code est trop court', true);

            d.action('Verrouiller', {disabled: !d.isValid()}, e => goupile.lock(e, pin.value, ctx));
        });
    };

    this.runUnlockDialog = function(e) {
        return ui.runDialog(e, 'Déverrouillage', (d, resolve, reject) => {
            let pin = d.pin('*pin', 'Code de déverrouillage');
            d.action('Déverrouiller', {disabled: !d.isValid()}, e => goupile.unlock(e, pin.value));
        });
    };

    this.lock = async function(e, password, ctx = null) {
        let session_rnd = util.getCookie('session_rnd');

        if (self.isLocked())
            throw new Error('Cannot lock unauthorized session');
        if (typeof ctx == undefined)
            throw new Error('Lock context must not be undefined');
        if (session_rnd == null)
            throw new Error('Cannot lock without session cookie');

        await self.confirmDangerousAction(e);

        let salt = nacl.randomBytes(24);
        let key = await deriveKey(password, salt);
        let enc = await encryptSecretBox(session_rnd, key);

        let lock = {
            userid: profile.userid,
            username: profile.username,
            salt: bytesToBase64(salt),
            errors: 0,
            namespaces: profile.namespaces,
            keys: {},
            session_rnd: enc,
            ctx: ctx
        };
        for (let key in lock.namespaces)
            lock.keys[key] = bytesToBase64(profile_keys[key]);

        await storeSessionValue('lock', lock);
        util.deleteCookie('session_rnd', '/');

        window.onbeforeunload = null;
        document.location.href = ENV.urls.instance;
        await util.waitFor(2000);
    };

    this.unlock = async function(e, password) {
        await self.confirmDangerousAction(e);

        let lock = await loadSessionValue('lock');
        if (lock == null)
            throw new Error('Session is not locked');

        let key = await deriveKey(password, base64ToBytes(lock.salt));

        // Instantaneous unlock feels weird
        let progress = log.progress('Déverrouillage en cours');
        await util.waitFor(800);

        try {
            let session_rnd = await decryptSecretBox(lock.session_rnd, key);

            util.setCookie('session_rnd', session_rnd, '/');
            await deleteSessionValue('lock');

            progress.success('Déverrouillage effectué');
        } catch (err) {
            progress.close();

            lock.errors = (lock.errors || 0) + 1;

            if (lock.errors >= 5) {
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

        window.onbeforeunload = null;
        document.location.reload();
        await util.waitFor(2000);
    };

    this.confirmIdentity = async function(e) {
        let session_rnd = util.getCookie('session_rnd');

        // We're OK!
        if (session_rnd != null)
            return;

        if (profile.restore != null) {
            let url = util.pasteURL(`${ENV.urls.instance}api/session/profile`, profile.restore);
            let response = await net.fetch(url);

            if (!response.ok) {
                let err = (await response.text()).trim();
                throw new Error(err);
            }
        } else if (profile.userid >= 0) {
            return ui.runDialog(e, 'Confirmation d\'identité', (d, resolve, reject) => {
                d.calc('username', 'Nom d\'utilisateur', profile.username);
                let password = d.password('*password', 'Mot de passe');

                d.action('Confirmer', {disabled: !d.isValid()}, async e => {
                    let progress = log.progress('Confirmation en cours');

                    try {
                        await tryLogin(profile.username, password.value, progress, true);
                        resolve();
                    } catch (err) {
                        log.error(err);
                        d.refresh();
                    }
                });
            });
        } else {
            throw new Error('Cette session n\'est plus valide veuillez vous reconnecter');
        }
    };
    this.confirmIdentity = util.serialize(this.confirmIdentity);

    this.confirmDangerousAction = function(e) {
        if (controller == null)
            return;
        if (!controller.hasUnsavedData())
            return;

        return ui.runConfirm(e, html`Si vous continuez, vous <span style="color: red; font-weight: bold;">PERDREZ les modifications en cours</span>.
                                     Voulez-vous continuer ?<br/>
                                     Si vous souhaitez les conserver, <b>annulez cette action</b> et cliquez sur le bouton d'enregistrement.`,
                                "Abandonner les modifications", () => {});
    };

    this.isLocked = function() { return profile.lock !== undefined; };
    this.hasPermission = function(perm) { return profile.permissions != null &&
                                                 profile.permissions[perm]; };
    this.isLoggedOnline = function() { return net.isOnline() && profile.online; };

    this.syncHistory = function(url, push = true) {
        if (profile.restore != null) {
            let query = new URLSearchParams;
            for (let key in profile.restore)
                query.set(key, profile.restore[key]);
            url += '?' + query;
        }

        if (push && current_url != null && url !== current_url) {
            window.history.pushState(null, null, url);
        } else {
            window.history.replaceState(null, null, url);
        }

        current_url = url;
    };

    this.encryptSymmetric = function(obj, namespace) {
        let key = profile_keys[namespace];
        if (key == null)
            throw new Error(`Cannot encrypt without '${namespace}' key`);

        return encryptSecretBox(obj, key);
    };

    this.decryptSymmetric = function(enc, namespace) {
        let key = profile_keys[namespace];
        if (key == null)
            throw new Error(`Cannot decrypt without '${namespace}' key`);

        return decryptSecretBox(enc, key);
    };

    this.encryptBackup = function(obj) {
        if (ENV.backup_key == null)
            throw new Error('This instance is not configured for offline backups');

        let key = profile_keys.records;
        if (key == null)
            throw new Error('Cannot encrypt backup without local key');

        let backup_key = base64ToBytes(ENV.backup_key);
        return encryptBox(obj, backup_key, key);
    }

    async function encryptSecretBox(obj, key) {
        let nonce = new Uint8Array(24);
        crypto.getRandomValues(nonce);

        let json = JSON.stringify(obj, (k, v) => v != null ? v : null);
        let message = (new TextEncoder()).encode(json);
        let box = nacl.secretbox(message, nonce, key);

        let enc = {
            format: 2,
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

        let json;
        if (enc.format >= 2) {
            json = (new TextDecoder()).decode(message);
        } else {
            json = window.atob(bytesToBase64(message));
        }
        let obj = JSON.parse(json);

        return obj;
    }

    async function encryptBox(obj, public_key, secret_key) {
        let nonce = new Uint8Array(24);
        crypto.getRandomValues(nonce);

        let json = JSON.stringify(obj, (k, v) => v != null ? v : null);
        let message = (new TextEncoder()).encode(json);
        let box = nacl.box(message, nonce, public_key, secret_key);

        let enc = {
            format: 2,
            nonce: bytesToBase64(nonce),
            box: bytesToBase64(box)
        };
        return enc;
    }

    async function loadSessionValue(key) {
        key = ENV.urls.base + key;

        let json = localStorage.getItem(key);
        if (json == null)
            return null;

        return JSON.parse(json);
    }

    async function storeSessionValue(key, obj) {
        key = ENV.urls.base + key;
        obj = JSON.stringify(obj);

        localStorage.setItem(key, obj);
    }

    async function deleteSessionValue(key) {
        key = ENV.urls.base + key;
        localStorage.removeItem(key);
    }
};
