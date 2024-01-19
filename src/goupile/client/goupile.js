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

import { render, html } from '../../../vendor/lit-html/lit-html.bundle.js';
import { Util, Log, Net, NetworkError } from '../../web/libjs/common.js';
import { Base64 } from '../../web/libjs/crypto.js';
import * as IDB from '../../web/libjs/indexedDB.js';
import * as UI from './ui.js';
import * as AdminController from './admin.js';
import * as InstanceController from './instance.js';

import '../../../vendor/opensans/OpenSans.css';
import './goupile.css';

// Global variables
let profile = {};

// Copied that crap from a random corner of the internet
let electron = (typeof process !== 'undefined' && typeof process.versions === 'object' &&
                !!process.versions.electron);

let profile_keys = {};
let confirm_promise;

let controller;
let current_url;

let current_hash = '';
let prev_hash = null;

async function start() {
    let url = new URL(window.location.href);

    // Select relevant controller
    if (ENV.urls.base === '/admin/') {
        controller = AdminController;
        document.documentElement.className = 'admin';
    } else {
        controller = InstanceController;
        document.documentElement.className = 'instance';
    }

    // Try to work around Safari bug:
    // https://bugs.webkit.org/show_bug.cgi?id=226547
    if (navigator.userAgent.toLowerCase().indexOf('safari') >= 0) {
        let req = indexedDB.open('dummy');

        req.onblocked = () => { console.log('Safari IDB workaround: blocked'); };
        req.onsuccess = () => { console.log('Safari IDB workaround: success'); };
        req.onerror = () => { console.log('Safari IDB workaround: error'); };
        req.onupgradeneeded = () => { console.log('Safari IDB workaround: upgrade needed'); };
    }

    // Initialize base subsystems
    UI.init();
    await registerSW();
    initNavigation();

    // Get current session profile (unless ?login=1 is present)
    if (url.searchParams.get('login')) {
        syncHistory(url.pathname, false);
        url = new URL(window.location.href);
    } else {
        if (url.searchParams.has('token')) {
            let query = new URLSearchParams;
            query.set('token', url.searchParams.get('token'));

            await Net.fetch(`${ENV.urls.instance}api/session/token`, {
                method: 'POST',
                body: query
            });
        } else if (ENV.auto_key && url.searchParams.has(ENV.auto_key)) {
            let query = new URLSearchParams;
            query.set('key', url.searchParams.get(ENV.auto_key));

            await Net.fetch(`${ENV.urls.instance}api/session/key`, {
                method: 'POST',
                body: query
            });
        }

        await syncProfile();
    }

    // Run login dialogs
    if (!profile.authorized)
        await runLoginScreen(null, true);

    // Adjust URLs based on user permissions
    if (profile.instances != null) {
        let instance = profile.instances.find(instance => url.pathname.startsWith(instance.url)) ||
                       profile.instances[0];

        ENV.key = instance.key;
        ENV.title = instance.title;
        ENV.urls.instance = instance.url;

        if (!url.pathname.startsWith(instance.url))
            url = new URL(instance.url, window.location.href);
    }

    // Run controller now
    try {
        await controller.init();
        await initTasks();

        if (url.hash)
            current_hash = url.hash;

        await controller.go(null, url.href);
    } catch (err) {
        Log.error(err);

        if (hasPermission('build_code')) {
            // Switch to dev mode for developers
            if (!profile.develop) {
                try {
                    await changeDevelopMode(true);
                } catch (err) {
                    Log.error(err);
                }
            }
        } else {
            await runLoginScreen(null, true);

            window.onbeforeunload = null;
            window.location.href = window.location.href;
        }
    }
};

async function registerSW() {
    try {
        if (navigator.serviceWorker != null) {
            if (ENV.use_offline) {
                let registration = await navigator.serviceWorker.register(`${ENV.urls.base}sw.js`);
                let progress = new Log.Entry;

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
                                    setTimeout(() => { window.location.href = window.location.href; }, 3000);
                                }
                            });
                        }
                    });
                }
            } else {
                let registration = await navigator.serviceWorker.getRegistration();
                let progress = new Log.Entry;

                if (registration != null) {
                    progress.progress('Nettoyage de l\'instance en cache, veuillez patienter');
                    document.querySelector('#ui_root').classList.add('disabled');

                    await registration.unregister();

                    progress.success('Nettoyage effectué, l\'application va redémarrer');
                    setTimeout(() => { window.location.href = window.location.href; }, 3000);
                }
            }
        }
    } catch (err) {
        if (ENV.use_offline) {
            console.log(err);
            console.log("Service worker API is not available");
        }
    }
}

function initNavigation() {
    window.addEventListener('popstate', e => controller.go(null, window.location.href, { push_history: false }));

    Util.interceptLocalAnchors((e, href) => {
        let url = new URL(href, window.location.href);

        let func = UI.wrap(e => controller.go(e, url));
        func(e);

        e.preventDefault();
    });

    if (electron) {
        let protect = true;

        window.onbeforeunload = e => {
            if (protect && controller.hasUnsavedData != null && controller.hasUnsavedData()) {
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
            if (controller.hasUnsavedData != null && controller.hasUnsavedData())
                return 'Si vous confirmez vouloir quitter la page, les modifications en cours seront perdues !';
        };
    }

    // Try to force all tabs to reload when instance is locked or unlocked
    window.addEventListener('storage', e => {
        if (e.key === ENV.urls.instance + 'lock' && !!e.newValue !== !!e.oldValue) {
            window.onbeforeunload = null;
            window.location.href = window.location.href;
        }
    });
}

async function syncProfile() {
    // Ask server (if needed)
    try {
        let new_profile = await Net.get(`${ENV.urls.instance}api/session/profile`);
        await updateProfile(new_profile, true);
    } catch (err) {
        throw err;
    }

    // Client-side lock?
    {
        let lock = await loadSessionValue('lock');

        if (lock != null) {
            Util.deleteCookie('session_rnd', '/');

            let new_profile = {
                userid: lock.userid,
                username: lock.username,
                online: false,
                authorized: true,
                permissions: {
                    data_load: true,
                    data_save: true
                },
                namespaces: lock.namespaces,
                keys: lock.keys,
                lock: lock.lock
            };
            await updateProfile(new_profile);

            return;
        }
    }
}

async function initTasks() {
    Net.retryHandler = async response => {
        if (response.status === 401) {
            try {
                await runConfirmIdentity();
                return true;
            } catch (err) {
                if (err != null)
                    Log.error(err);
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
        Net.changeHandler = async online => {
            await runTasks();
            controller.go();
        };

        await runTasks();
    }
}

async function runTasks() {
    try {
        let online = Net.isOnline() && Util.getCookie('session_rnd') != null;
        await controller.runTasks(online);
    } catch (err) {
        Log.error(err);
    }
}

async function pingServer() {
    try {
        let response = await Net.fetch(`${ENV.urls.instance}api/session/ping`);
        Net.setOnline(response.ok);
    } catch (err) {
        // Automatically set to offline
    }
}

async function runLoginScreen(e, initial) {
    initial |= (profile.username == null);

    let new_profile = profile;
    let password;

    do {
        try {
            if (new_profile.confirm == null)
                [new_profile, password] = await runPasswordScreen(e, initial);
            if (new_profile.confirm == 'password') {
                await runChangePassword(e, true);

                new_profile = await Net.get(`${ENV.urls.instance}api/session/profile`);
            }
            if (new_profile.confirm != null)
                new_profile = await runConfirmScreen(e, initial, new_profile.confirm);
        } catch (err) {
            if (!err)
                throw err;
            Log.error(err);
        }
    } while (!new_profile.authorized);

    await updateProfile(new_profile);
}

async function runPasswordScreen(e, initial) {
    let title = initial ? null : 'Confirmation d\'identité';

    return UI.dialog(e, title, { fixed: true }, (d, resolve, reject) => {
        if (initial) {
            d.output(html`
                <img id="gp_logo" src=${ENV.urls.base + 'favicon.png'} alt="" />
                <div id="gp_title">${ENV.title}</div>
                <br/>
            `);
            d.text('*username', 'Nom d\'utilisateur');
        } else {
            d.output(html`Veuillez <b>confirmer votre identité</b> pour continuer.`);
            d.calc('*username', 'Nom d\'utilisateur', profile.username);
        }

        d.password('*password', 'Mot de passe');

        d.action('Se connecter', { disabled: !d.isValid() }, async () => {
            try {
                let username = (d.values.username || '').trim();
                let password = (d.values.password || '').trim();

                let new_profile = await login(username, password);
                resolve([new_profile, password]);
            } catch (err) {
                Log.error(err);
                d.refresh();
            }
        });
    });
}

async function runConfirmScreen(e, initial, method) {
    let qrcode;
    if (method === 'qrcode') {
        let url = Util.pasteURL(ENV.urls.instance + 'api/change/qrcode', { buster: Util.getRandomInt(0, Number.MAX_SAFE_INTEGER) });

        let response = await Net.fetch(url);
        let secret = response.headers.get('X-TOTP-SecretKey');
        let buf = await response.arrayBuffer();

        qrcode = {
            secret: secret,
            image: 'data:image/png;base64,' + Base64.toBase64(buf)
        };
    }

    let title = initial ? null : 'Confirmation d\'identité';
    let errors = 0;

    return UI.dialog(e, title, { fixed: true }, (d, resolve, reject) => {
        d.output(html`
            ${initial ? html`
                <img id="gp_logo" src=${ENV.urls.base + 'favicon.png'} alt="" />
                <div id="gp_title">${ENV.title}</div>
                <br/><br/>
            ` : ''}
            ${method === 'mail' ? html`Entrez le code secret qui vous a été <b>envoyé par mail</b>.` : ''}
            ${method === 'sms' ? html`Entrez le code secret qui vous a été <b>envoyé par SMS</b>.` : ''}
            ${method === 'totp' ? html`Entrez le code secret affiché <b>par l'application d'authentification</b>.` : ''}
            ${method === 'qrcode' ? html`
                <p>Scannez ce QR code avec une <b>application d'authentification
                pour mobile</b> puis saississez le code donné par cette application.</p>
                <p><i>Applications : andOTP, FreeOTP, etc.</i></p>

                <div style="text-align: center; margin-top: 2em;"><img src=${qrcode.image} alt="" /></div>
                <p style="text-align: center; font-size: 0.8em">${qrcode.secret}</p>
            ` : ''}
            <br/>
        `);
        d.text('*code', 'Code secret', { help: '6 chiffres' });

        if (method === 'mail' || method === 'sms')
            d.output(html`<i>Si vous ne trouvez pas ce message, pensez à vérifiez vos spams.</i>`);
        if (errors >= 2) {
            d.output(html`
                <span style="color: red; font-style: italic">
                    En cas de difficulté, vérifiez que l'heure de votre téléphone
                    est précisément réglée, que le fuseau horaire paramétré est
                    le bon ainsi que l'heure d'été.
                </span>
            `);
        }

        d.action('Continuer', { disabled: !d.isValid() }, async () => {
            try {
                let new_profile = await Net.post(`${ENV.urls.instance}api/session/confirm`, { code: d.values.code });
                resolve(new_profile);
            } catch (err) {
                errors++;
                d.refresh();

                throw new Error(err);
            }
        });
    });
}

async function runConfirmIdentity(e) {
    if (confirm_promise != null) {
        await confirm_promise;
        return;
    }

    if (profile.type === 'key') {
        await Net.post(`${ENV.urls.instance}api/session/key`, { key: profile.username });
    } else if (profile.type == 'login') {
        confirm_promise = runLoginScreen(e, false);
        confirm_promise.finally(() => confirm_promise = null);

        return confirm_promise;
    } else if (profile.type === 'token') {
        throw new Error('Veuillez vous reconnecter à l\'aide du lien fourni par mail pour continuer');
    } else {
        throw new Error('Vous devez vous reconnecter pour continuer');
    }
}

function runChangePasswordDialog(e) { return runChangePassword(e, false); };

function runChangePassword(e, forced) {
    let title = forced ? null : 'Changement de mot de passe';

    return UI.dialog(e, title, { fixed: forced }, (d, resolve, reject) => {
        if (forced) {
            d.output(html`
                <img id="gp_logo" src=${ENV.urls.base + 'favicon.png'} alt="" />
                <div id="gp_title">${ENV.title}</div>
                <br/><br/>
                Veuillez saisir un nouveau mot de passe.
            `);
        }

        if (!forced)
            d.password('*old_password', 'Ancien mot de passe');
        d.password('*new_password', 'Nouveau mot de passe');
        d.password('*new_password2', null, {
            placeholder: 'Confirmation',
            help: 'Votre mot de passe doit être suffisamment complexe (longueur, lettres, chiffres, symboles)'
        });
        if (d.values.new_password != null && d.values.new_password2 != null &&
                                             d.values.new_password !== d.values.new_password2)
            d.error('new_password2', 'Les mots de passe sont différents');

        d.action('Modifier', { disabled: !d.isValid() }, async () => {
            let progress = Log.progress('Modification du mot de passe');

            try {
                await Net.post(`${ENV.urls.instance}api/change/password`, {
                    old_password: d.values.old_password,
                    new_password: d.values.new_password
                });

                resolve();
                progress.success('Mot de passe modifié');
            } catch (err) {
                progress.close();

                Log.error(err);
                d.refresh();
            }
        });
    });
}

async function runResetTOTP(e) {
    let qrcode;
    {
        let url = Util.pasteURL(ENV.urls.instance + 'api/change/qrcode', { buster: Util.getRandomInt(0, Number.MAX_SAFE_INTEGER) });

        let response = await Net.fetch(url);
        let secret = response.headers.get('X-TOTP-SecretKey');
        let buf = await response.arrayBuffer();

        qrcode = {
            secret: secret,
            image: 'data:image/png;base64,' + Base64.toBase64(buf)
        };
    }

    let errors = 0;

    return UI.dialog(e, 'Changement de codes TOTP', { fixed: true }, (d, resolve, reject) => {
        d.output(html`
            <p>Scannez ce QR code avec une <b>application d'authentification
            pour mobile</b> puis saississez le code donné par cette application.</p>
            <p><i>Applications : FreeOTP, Authy, etc.</i></p>

            <div style="text-align: center; margin-top: 2em;"><img src="${qrcode.image}" alt="" /></div>
            <p style="text-align: center; font-size: 0.8em">${qrcode.secret}</p>
        `);

        d.text('*code', 'Code secret', { help : '6 chiffres' });
        d.password('*password', 'Mot de passe du compte');

        if (errors >= 2) {
            d.output(html`
                <span style="color: red; font-style: italic">
                    En cas de difficulté, vérifiez que l'heure de votre téléphone
                    est précisément réglée, que le fuseau horaire paramétré est
                    le bon ainsi que l'heure d'été.
                </span>
            `);
        }

        d.action('Modifier', { disabled: !d.isValid() }, async () => {
            let progress = Log.progress('Modification des codes TOTP');

            try {
                await Net.post(`${ENV.urls.instance}api/change/totp`, {
                    password: d.values.password,
                    code: d.values.code
                });

                resolve();
                progress.success('Codes TOTP modifiés');
            } catch (err) {
                progress.close();

                Log.error(err);
                d.refresh();
            }
        });
    });
};

function runLockDialog(e, ctx) {
    return UI.dialog(e, 'Verrouillage', {}, (d, resolve, reject) => {
        d.pin('*pin', 'Code de déverrouillage');
        if (d.values.pin != null && d.values.pin.length < 4)
            d.error('pin', 'Ce code est trop court', true);
        d.action('Verrouiller', { disabled: !d.isValid() }, e => lock(e, d.values.pin, ctx));
    });
};

function runUnlockDialog(e) {
    return UI.dialog(e, 'Déverrouillage', {}, (d, resolve, reject) => {
        d.pin('*pin', 'Code de déverrouillage');
        d.action('Déverrouiller', { disabled: !d.isValid() }, e => unlock(e, d.values.pin));
    });
};

async function login(username, password) {
    let new_profile = await loginOnline(username, password);
    return new_profile;
}

async function loginOnline(username, password) {
    let response = await Net.fetch(`${ENV.urls.instance}api/session/login`, {
        method: 'POST',
        body: JSON.stringify({
            username: username.toLowerCase(),
            password: password
        })
    });

    if (response.ok) {
        let new_profile = await response.json();

        // Release client-side lock
        await deleteSessionValue('lock');

        return new_profile;
    } else {
        if (response.status === 403) {
            let db = await openLocalDB();
            await db.delete('profiles', username);
        }

        let err = await Net.readError(response);
        throw new Error(err);
    }
}

async function loginOffline(username, password) {
    let db = await openLocalDB();

    // Instantaneous login feels weird
    await Util.waitFor(800);

    let obj = await db.load('profiles', username);
    if (obj == null)
        throw new Error('Profil hors ligne inconnu');

    let key = await deriveKey(password, Base64.toBytes(obj.salt));
    let new_profile;

    try {
        new_profile = await decryptSecretBox(obj.profile, key);
        new_profile.online = false;

        // Reset errors after successful offline login
        if (obj.errors) {
            obj.errors = 0;
            await db.saveWithKey('profiles', username, obj);
        }
    } catch (err) {
        obj.errors = (obj.errors || 0) + 1;

        if (obj.errors >= 5) {
            await db.delete('profiles', username);
            throw new Error('Mot de passe non reconnu, connexion hors ligne désactivée');
        } else {
            await db.saveWithKey('profiles', username, obj);
            throw new Error('Mot de passe non reconnu');
        }
    }

    // Release client-side lock
    await deleteSessionValue('lock');

    return new_profile;
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

async function updateProfile(new_profile) {
    profile = Object.assign({}, new_profile);

    // Keep keys local to "hide" (as much as JS allows..) them
    if (profile.keys != null) {
        profile_keys = profile.keys;
        delete profile.keys;

        for (let key in profile_keys) {
            let value = Base64.toBytes(profile_keys[key]);
            profile_keys[key] = value;
        }
    } else {
        profile_keys = new Map;
    }
}

async function logout(e) {
    await confirmDangerousAction(e);

    let progress = Log.progress('Déconnexion en cours');

    try {
        await Net.post(`${ENV.urls.instance}api/session/logout`);

        await updateProfile({});
        await deleteSessionValue('lock');

        // Clear state and start from fresh as a precaution
        let url = new URL(window.location.href);
        let reload = Util.pasteURL(url.pathname, { login: 1 });

        window.onbeforeunload = null;
        window.location.href = reload;
    } catch (err) {
        progress.close();
        throw err;
    }
}

async function goToLogin(e) {
    await confirmDangerousAction(e);

    let url = new URL(window.location.href);
    let reload = Util.pasteURL(url.pathname, { login: 1 });

    window.onbeforeunload = null;
    window.location.href = reload;
}

function isLoggedOnline() {
    let online = Net.isOnline() && profile.online;
    return online;
}

async function changeDevelopMode(enable) {
    if (enable == profile.develop)
        return;

    await Net.post(`${ENV.urls.instance}api/change/mode`, { develop: enable });

    // We want to reload no matter what, because the mode has changed
    let url = new URL(window.location.href);
    window.location.href = url.pathname;
}

async function lock(e, password, ctx = null) {
    let session_rnd = Util.getCookie('session_rnd');

    if (isLocked())
        throw new Error('Cannot lock unauthorized session');
    if (typeof ctx == undefined)
        throw new Error('Lock context must not be undefined');
    if (session_rnd == null)
        throw new Error('Cannot lock without session cookie');
    if (profile_keys.lock == null)
        throw new Error('Cannot lock session without lock key');

    await confirmDangerousAction(e);

    let salt = nacl.randomBytes(24);
    let key = await deriveKey(password, salt);
    let enc = await encryptSecretBox(session_rnd, key);

    let lock = {
        userid: profile.userid,
        username: profile.username,
        salt: Base64.toBase64(salt),
        errors: 0,
        namespaces: {
            records: profile.namespaces.records
        },
        keys: {
            records: Base64.toBase64(profile_keys.records)
        },
        session_rnd: enc,
        lock: ctx
    };

    await storeSessionValue('lock', lock);
    Util.deleteCookie('session_rnd', '/');

    window.onbeforeunload = null;
    window.location.href = ENV.urls.instance;
    await Util.waitFor(2000);
};

async function unlock(e, password) {
    await confirmDangerousAction(e);

    let lock = await loadSessionValue('lock');
    if (lock == null)
        throw new Error('Session is not locked');

    let key = await deriveKey(password, Base64.toBytes(lock.salt));

    // Instantaneous unlock feels weird
    let progress = Log.progress('Déverrouillage en cours');
    await Util.waitFor(800);

    try {
        let session_rnd = await decryptSecretBox(lock.session_rnd, key);

        Util.setCookie('session_rnd', session_rnd, '/');
        await deleteSessionValue('lock');

        progress.success('Déverrouillage effectué');
    } catch (err) {
        progress.close();

        lock.errors = (lock.errors || 0) + 1;

        if (lock.errors >= 5) {
            Log.error('Déverrouillage refusé, blocage de sécurité imminent');
            await deleteSessionValue('lock');

            setTimeout(() => {
                window.onbeforeunload = null;
                window.location.href = window.location.href;
            }, 3000);
        } else {
            Log.error('Déverrouillage refusé');
            await storeSessionValue('lock', lock);
        }

        return;
    }

    window.onbeforeunload = null;
    window.location.href = window.location.href;
    await Util.waitFor(2000);
};

function isLocked() {
    if (controller != InstanceController)
        return false;

    if (profile.lock != null)
        return true;
    if (!hasPermission('data_load'))
        return true;

    return false;
};

function confirmDangerousAction(e) {
    if (controller == null)
        return;
    if (controller.hasUnsavedData == null || !controller.hasUnsavedData())
        return;

    return UI.confirm(e, html`Si vous continuez, vous <span style="color: red; font-weight: bold;">PERDREZ les modifications en cours</span>.
                                 Voulez-vous continuer ?<br/>
                                 Si vous souhaitez les conserver, <b>annulez cette action</b> et cliquez sur le bouton d'enregistrement.`,
                            "Abandonner les modifications", () => {});
}

function hasPermission(perm) {
    if (profile.permissions == null)
        return false;
    if (!profile.permissions[perm])
        return false;

    return true;
}

function syncHistory(url, push = true) {
    push &= (current_url != null && url !== current_url);

    if (profile.restore != null) {
        url = new URL(url, window.location.href);

        let query = url.searchParams;
        for (let key in profile.restore)
            query.set(key, profile.restore[key]);

        url = url.toString();
    }

    if (push) {
        window.history.pushState(null, null, url + current_hash);
    } else {
        window.history.replaceState(null, null, url + current_hash);
    }

    current_url = url;

    setTimeout(() => {
        if (current_hash) {
            let el = document.querySelector(current_hash);

            if (el != null) {
                if (current_hash != prev_hash)
                    el.scrollIntoView();
            } else {
                window.history.replaceState(null, null, url);
                current_hash = '';
            }

            prev_hash = current_hash;
        }
    }, 0);
}

function setCurrentHash(hash) {
    current_hash = hash || '';
    prev_hash = null;
}

async function openLocalDB() {
    let db_name = `goupile+${ENV.urls.base}`;

    let db = await IDB.open(db_name, 2, (db, old_version) => {
        switch (old_version) {
            case null: {
                db.createStore('usr_profiles');
            } // fallthrough

            case 1: {
                db.deleteStore('usr_profiles');

                db.createStore('profiles');
                db.createStore('changes');
            } // fallthrough
        }
    });

    return db;
}

function loadSessionValue(key) {
    key = ENV.urls.base + key;

    let json = localStorage.getItem(key);
    if (json == null)
        return null;

    return JSON.parse(json);
}

function storeSessionValue(key, obj) {
    key = ENV.urls.base + key;
    obj = JSON.stringify(obj);

    localStorage.setItem(key, obj);
}

function deleteSessionValue(key) {
    key = ENV.urls.base + key;
    localStorage.removeItem(key);
}

export {
    profile,

    start,

    runChangePasswordDialog,
    runResetTOTP,
    runLockDialog,
    runUnlockDialog,

    logout,
    goToLogin,
    isLoggedOnline,

    changeDevelopMode,

    lock,
    unlock,
    isLocked,

    hasPermission,
    confirmDangerousAction,

    syncHistory,
    setCurrentHash,

    openLocalDB
}
