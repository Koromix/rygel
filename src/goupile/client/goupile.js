// Copyright (C) 2024  Niels Martignène <niels.martignene@protonmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

import { render, html } from '../../../vendor/lit-html/lit-html.bundle.js';
import { Util, Log, Net, NetworkError } from '../../web/libjs/common.js';
import { Base64 } from '../../web/libjs/mixer.js';
import * as IDB from '../../web/libjs/indexedDB.js';
import * as UI from './ui.js';
import * as AdminController from './admin.js';
import * as InstanceController from './instance.js';
import * as LegacyController from '../legacy/instance.js';
import * as nacl from '../../../vendor/tweetnacl-js/nacl-fast.js';

import '../../../vendor/opensans/OpenSans.css';
import './goupile.css';

// Global variables
let profile = {};

// Copied that crap from a random corner of the internet
let electron = (typeof process !== 'undefined' && typeof process.versions === 'object' &&
                !!process.versions.electron);

let online = true;

let can_unlock = false;
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
        controller = ENV.legacy ? LegacyController : InstanceController;
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
    } else if (url.searchParams.has('token')) {
        await Net.post(`${ENV.urls.instance}api/session/token`, {
            token: url.searchParams.get('token')
        });
        await syncProfile();
    } else if (ENV.auto_key && url.searchParams.has(ENV.auto_key)) {
        await Net.post(`${ENV.urls.instance}api/session/key`, {
            key: url.searchParams.get(ENV.auto_key)
        });
        await syncProfile();
    } else {
        await syncProfile();
    }

    // Authorize user
    if (profile.authorized) {
        url = await syncInstance();
    } else {
        url = await reAuthenticate();

        if (url == null)
            url = await runLoginScreen(null, true);
    }

    // Run controller now
    try {
        await controller.init();

        if (url.hash)
            current_hash = url.hash;

        if (controller.runTasks != null)
            await runTasks();
        await controller.go(null, url.href);

        await initTasks();
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
}

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
            if (protect && controller.hasUnsavedData?.()) {
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
            if (controller.hasUnsavedData?.())
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

async function syncInstance() {
    let url = new URL(window.location.href);

    if (profile.instances != null) {
        let instance = profile.instances.find(instance => url.pathname.startsWith(instance.url)) ||
                       profile.instances[0];

        ENV.key = instance.key;
        ENV.title = instance.title;
        ENV.urls.instance = instance.url;

        if (!url.pathname.startsWith(instance.url))
            url = new URL(instance.url, window.location.href);

        if (instance.key != profile.instance)
            await syncProfile();
    }

    return url;
}

async function syncProfile() {
    // Ask server (if needed)
    try {
        let response = await Net.fetch(`${ENV.urls.instance}api/session/profile`);

        let new_profile = await response.json();
        updateProfile(new_profile);
    } catch (err) {
        if (err instanceof NetworkError)
            online = false;

        if (ENV.use_offline) {
            if (!(err instanceof NetworkError))
                Log.error(err);
        } else {
            throw err;
        }
    }

    // Client-side lock?
    {
        let lock = loadLocal('lock');

        if (lock?.instances == null) {
            deleteLocal('lock');
            lock = null;
        }

        if (lock != null) {
            Util.deleteCookie('session_rnd', '/');

            let new_profile = {
                userid: lock.userid,
                username: lock.username,
                instances: lock.instances,
                online: false,
                authorized: true,
                permissions: {
                    data_save: true
                },
                namespaces: lock.namespaces,
                keys: lock.keys,
                lock: lock.lock
            };
            updateProfile(new_profile, true);

            return;
        }
    }
}

async function initTasks() {
    Net.retryHandler = async response => {
        if (response.status === 401) {
            try {
                await runConfirmIdentity();
                setTimeout(() => controller.go(null, null));

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
    }
}

async function runTasks() {
    try {
        let sync = online && (profile.online || profile.lock == null);
        await controller.runTasks(sync);
    } catch (err) {
        Log.error(err);
    }
}

async function pingServer() {
    try {
        let response = await Net.fetch(`${ENV.urls.instance}api/session/ping`);
        online = response.ok;
    } catch (err) {
        online = false;
    }
}

async function runLoginScreen(e, initial) {
    initial |= (profile.username == null);

    let password;
    {
        let new_profile = profile;

        do {
            try {
                if (new_profile.confirm == null)
                    [new_profile, password] = await runPasswordScreen(e, initial);
                if (new_profile.confirm == 'password') {
                    await runChangePassword(e, new_profile.username);
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

        updateProfile(new_profile);
    }

    let url = await syncInstance();

    // Save for offline login
    if (ENV.use_offline) {
        let db = await openLocalDB();

        if (profile.permissions['misc_offline']) {
            let data = Object.assign({}, profile);
            data.keys = Object.keys(profile_keys).reduce((obj, key) => { obj[key] = Base64.toBase64(profile_keys[key]); return obj }, {});

            let salt = nacl.randomBytes(24);
            let key = await deriveKey(password, salt);
            let enc = await encryptSecretBox(data, key);

            await db.saveWithKey('profiles', profile.username, {
                salt: Base64.toBase64(salt),
                errors: 0,
                profile: enc
            });
        } else {
            await db.delete('profiles', profile.username);
        }
    }

    return url;
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

async function reAuthenticate() {
    let remember = loadLocal('remember');

    if (!remember)
        return;

    let json = await Net.get(`${ENV.urls.instance}api/auth/challenge`);
    let challenge = json.challenge;

    let cred = null;

    try {
        cred = await navigator.credentials.get({
            publicKey: {
                rp: { name: ENV.title },
                allowCredentials: [],
                challenge: (new TextEncoder).encode(challenge),
                userVerification: 'required'
            }
        });
    } catch (err) {
        console.log(err);
        return;
    }

    try {
        let new_profile = await Net.post(`${ENV.urls.instance}api/auth/assert`, {
            challenge: challenge,
            credential_id: cred.id,
            signature: Base64.toBase64(cred.response.signature)
        });

        console.log(new_profile);

        updateProfile(new_profile);
    } catch (err) {
        console.error(err);
        return null;
    }

    let url = await syncInstance();
    return url;
}

async function rememberMe() {
    let remember = loadLocal('remember');

    if (remember)
        return;

    let json = await Net.get(`${ENV.urls.instance}api/auth/challenge`);
    let challenge = json.challenge;

    let id = new Int32Array([profile.userid]);
    let cred = null;

    try {
        cred = await navigator.credentials.create({
            publicKey: {
                rp: { name: ENV.title },
                user: { id: id, name: profile.username, displayName: profile.username },
                pubKeyCredParams: [{ type: 'public-key', alg: -7 }],
                challenge: (new TextEncoder).encode(challenge),
                authenticatorSelection: {
                    authenticatorAttachment: 'platform',
                    residentKey: 'required',
                    userVerification: 'required'
                }
            }
        });
    } catch (err) {
        console.error(err);
        return;
    }

    await Net.post(`${ENV.urls.instance}api/auth/register`, {
        challenge: challenge,
        credential_id: cred.id,
        public_key: Base64.toBase64(cred.response.getPublicKey()),
        algorithm: cred.response.getPublicKeyAlgorithm(),
        attestation: Base64.toBase64(cred.response.attestationObject)
    });

    storeLocal('remember', true);
}

function runChangePasswordDialog(e) { return runChangePassword(e); }

function runChangePassword(e, username = null) {
    let forced = (username != null);
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

        d.calc('username', 'Nom d\'utilisateur', username ?? profile.username);
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
    try {
        if (online || !ENV.use_offline) {
            let new_profile = await loginOnline(username, password);
            return new_profile;
        } else {
            online = false;

            let new_profile = await loginOffline(username, password);
            return new_profile;
        }
    } catch (err) {
        if ((err instanceof NetworkError) && online && ENV.use_offline) {
            online = false;

            let new_profile = await loginOffline(username, password);
            return new_profile;
        } else {
            throw err;
        }
    }
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
        deleteLocal('lock');

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
    deleteLocal('lock');

    return new_profile;
}

async function deriveKey(password, salt) {
    password = (new TextEncoder).encode(password);

    let key = await crypto.subtle.importKey('raw', password, { name: 'PBKDF2' }, false, ['deriveBits']);
    let derived = await crypto.subtle.deriveBits({ name: 'PBKDF2', salt, iterations: 100000, hash: 'SHA-256' }, key, 256);

    return new Uint8Array(derived);
}

function updateProfile(new_profile, unlock = false) {
    profile = Object.assign({}, new_profile);
    can_unlock = unlock;

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

        updateProfile({});
        deleteLocal('lock');

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
    let ret = online && profile.online;
    return ret;
}

async function changeDevelopMode(enable) {
    if (enable == profile.develop)
        return;

    await Net.post(`${ENV.urls.base}api/change/mode`, { develop: enable });

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

    let instance = profile.instances.find(instance => instance.url == ENV.urls.instance);

    let salt = nacl.randomBytes(16);
    let key = await deriveKey(password, salt);
    let enc = await encryptSecretBox(session_rnd, key);

    let lock = {
        userid: profile.userid,
        username: profile.username,
        instances: [instance],
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

    storeLocal('lock', lock);
    Util.deleteCookie('session_rnd', '/');

    window.onbeforeunload = null;
    window.location.href = ENV.urls.instance;
    await Util.waitFor(2000);
};

async function unlock(e, password) {
    await confirmDangerousAction(e);

    let lock = loadLocal('lock');
    if (lock == null)
        throw new Error('Session is not locked');

    let key = await deriveKey(password, Base64.toBytes(lock.salt));

    // Instantaneous unlock feels weird
    let progress = Log.progress('Déverrouillage en cours');
    await Util.waitFor(800);

    try {
        let session_rnd = await decryptSecretBox(lock.session_rnd, key);

        Util.setCookie('session_rnd', session_rnd, '/');
        deleteLocal('lock');

        progress.success('Déverrouillage effectué');
    } catch (err) {
        progress.close();

        lock.errors = (lock.errors || 0) + 1;

        if (lock.errors >= 5) {
            Log.error('Déverrouillage refusé, blocage de sécurité imminent');
            deleteLocal('lock');

            setTimeout(() => {
                window.onbeforeunload = null;
                window.location.href = window.location.href;
            }, 3000);
        } else {
            Log.error('Déverrouillage refusé');
            storeLocal('lock', lock);
        }

        return;
    }

    window.onbeforeunload = null;
    window.location.href = window.location.href;
    await Util.waitFor(2000);
};

function isLocked() {
    if (profile.lock != null)
        return true;
    if (!hasPermission('data_read'))
        return true;

    return false;
};

function canUnlock() {
    return can_unlock;
}

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

function loadLocal(key) {
    key = ENV.urls.base + key;

    let json = localStorage.getItem(key);
    if (json == null)
        return null;

    return JSON.parse(json);
}

function storeLocal(key, obj) {
    key = ENV.urls.base + key;

    obj = JSON.stringify(obj);
    localStorage.setItem(key, obj);
}

function deleteLocal(key) {
    key = ENV.urls.base + key;
    localStorage.removeItem(key);
}

function encryptSecretBox(obj, key) {
    let nonce = new Uint8Array(24);
    crypto.getRandomValues(nonce);

    let json = JSON.stringify(obj, (k, v) => v != null ? v : null);
    let message = (new TextEncoder()).encode(json);
    let box = nacl.secretbox(message, nonce, key);

    let enc = {
        format: 2,
        nonce: Base64.toBase64(nonce),
        box: Base64.toBase64(box)
    };
    return enc;
}

function decryptSecretBox(enc, key) {
    let nonce = Base64.toBytes(enc.nonce);
    let box = Base64.toBytes(enc.box);

    let message = nacl.secretbox.open(box, nonce, key);
    if (message == null)
        throw new Error('Failed to decrypt message: wrong key?');

    let json;
    if (enc.format >= 2) {
        json = (new TextDecoder()).decode(message);
    } else {
        json = window.atob(Base64.toBase64(message));
    }
    let obj = JSON.parse(json);

    return obj;
}

export {
    profile,
    profile_keys as profileKeys,

    start,
    syncProfile,

    rememberMe,
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
    canUnlock,

    hasPermission,
    confirmDangerousAction,

    syncHistory,
    setCurrentHash,

    openLocalDB
}
