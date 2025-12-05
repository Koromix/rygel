// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import { render, html, ref } from 'vendor/lit-html/lit-html.bundle.js';
import * as nacl from 'vendor/tweetnacl-js/nacl-fast.js';
import { Util, Log, Net, HttpError, LocalDate } from 'lib/web/base/base.js';
import * as UI from 'lib/web/base/ui.js';
import { Hex, Base64 } from 'lib/web/base/mixer.js';
import { computeAge, dateToString, niceDate,
         progressBar, progressCircle, deflate, inflate,
         EventProviders, createEvent, safeTag } from './core/misc.js';
import { PictureCropper } from './util/picture.js';
import { PROJECTS } from '../projects/projects.js';
import { initSync, isSyncing, downloadVault, uploadVault, openVault } from './core/sync.js';
import { ProjectInfo, ProjectBuilder } from './core/project.js';
import { ConsentModule } from './form/consent.js';
import { FormModule } from './form/form.js';
import { FormState, FormModel, FormBuilder } from './form/builder.js';
import { NetworkModule } from './network/network.js';
import { deploy } from 'lib/web/flat/static.js';
import { ASSETS } from '../assets/assets.js';
import * as app from './app.js';

import '../assets/client.css';

const CHANNEL_NAME = 'ludivine';

const DATA_LOCK = 'data';
const RUN_LOCK = 'run';
const REMIND_LOCK = 'remind';

Object.assign(T, {
    cancel: 'Annuler',
    confirm: 'Confirmer',
    confirm_not_reversible: 'Attention, cette action ne peut pas être annulée',
    error_has_occured: 'Une erreur est survenue',
    filter: 'Filter',

    browse_for_image: 'Parcourir',
    clear_picture: 'Effacer l\'image',
    save: 'Enregistrer',
    drag_paste_or_browse_image: 'Déposez une image, copier-collez la ou utilisez le bouton « Parcourir »',
    upload_image: 'Choisir une image',
    zoom: 'Zoom',

    face: 'Visage',
    hair: 'Cheveux',
    eyes: 'Yeux',
    eyebrows: 'Sourcils',
    glasses: 'Lunettes',
    nose: 'Nez',
    mouth: 'Bouche',
    beard: 'Barbe',
    accessories: 'Accessoires',

    days: {
        1: 'Lundi',
        2: 'Mardi',
        3: 'Mercredi',
        4: 'Jeudi',
        5: 'Vendredi',
        6: 'Samedi',
        7: 'Dimanche'
    },
    months: {
        1: 'Janvier',
        2: 'Février',
        3: 'Mars',
        4: 'Avril',
        5: 'Mai',
        6: 'Juin',
        7: 'Juillet',
        8: 'Août',
        9: 'Septembre',
        10: 'Octobre',
        11: 'Novembre',
        12: 'Décembre'
    }
});

let DiaryModule = null;

let route = {
    mode: null,

    // study mode
    project: null,
    page: null,
    section: null,

    // diary mode
    entry: null
};
let route_url = null;
let poisoned = false;
let has_run = false;

let channel = null;
let session = null;
let db = null;
let identity = null;

let root_el = null;

let cache = {
    studies: null,
    events: null,

    project: null,
    study: null,
    page: null,
    section: null,

    tests: null
};

let ctx = null;
let ctx_key = null;

// ------------------------------------------------------------------------
// Init
// ------------------------------------------------------------------------

async function start() {
    UI.init(run, renderApp);
    initSync();

    // Handle internal links
    Util.interceptLocalAnchors((e, href) => {
        let url = new URL(href, window.location.href);

        let func = UI.wrap(e => go(url));
        func(e);

        e.preventDefault();
    });

    // Handle back navigation
    window.addEventListener('popstate', e => {
        UI.closeDialog();
        go(window.location.href, false);
    });

    // Prevent loss of data
    window.onbeforeunload = e => {
        if (isSyncing())
            return 'Synchronisation en cours, veuillez patienter';
        if (UI.isDialogOpen())
            return 'Validez ou fermez la boîte de dialogue avant de continuer';
        if (ctx?.hasUnsavedData?.())
            return 'Les modifications en cours seront perdues si vous continuez, êtes-vous sûr(e) de vouloir continuer ?';
    };

    // Keep this around for gesture emulation on desktop
    if (false) {
        let script = document.createElement('script');

        script.onload = () => TouchEmulator();
        script.src = BUNDLES['touch-emulator.js'];

        document.head.appendChild(script);
    }

    // Perform mail login
    if (window.location.hash) {
        let hash = window.location.hash.substr(1);
        let query = new URLSearchParams(hash);

        let uid = query.get('uid');
        let tkey = query.get('tk');
        let registration = query.get('r');

        if (uid && tkey && registration) {
            tkey = Hex.toBytes(tkey);
            registration = parseInt(registration, 10);

            await loginMagic(uid, tkey, registration);

            let url = window.location.pathname + window.location.search;
            history.replaceState('', document.title, url);
        }
    }

    // Open existing session (if any)
    if (session == null) {
        let obj = null;

        try {
            let json = sessionStorage.getItem('session');
            obj = JSON.parse(json);
        } catch (err) {
            console.error(err);
        }

        await open(obj);
    }

    // Share session data across tabs
    initChannel();

    await go(window.location.href, false);

    UI.main();
    document.body.classList.remove('loading');
}

function initChannel() {
    channel = new BroadcastChannel(CHANNEL_NAME);

    channel.addEventListener('message', async e => {
        switch (e.data.message) {
            case 'app': {
                if (session != null)
                    channel.postMessage({ message: 'session', session: session });
            } break;

            case 'web': {
                if (identity != null)
                    channel.postMessage({ message: 'picture', picture: identity.picture });
            } break;

            case 'session': {
                await open(e.data.session);
                await run();
            } break;

            case 'picture': {
                identity.picture = e.data.picture;
                sessionStorage.setItem('picture', e.data.picture ?? '');

                await run();
            } break;

            case 'logout': {
                if (session != null)
                    logout();
            } break;
        }
    });

    channel.postMessage({ message: 'app' });
}

async function loginMagic(uid, tkey, registration) {
    let vkey = new Uint8Array(32);
    crypto.getRandomValues(vkey);

    let init = {
        vid: crypto.randomUUID(),
        vkey: Hex.toHex(vkey),
        rid: crypto.randomUUID()
    };

    // Retrieve existing token (or create it if none is set) and session
    let login = await Net.post('/api/token', {
        uid: uid,
        init: encrypt(init, tkey),
        vid: init.vid,
        rid: init.rid,
        registration: registration
    });

    let session = {
        uid: login.uid,
        ...decrypt(login.token, tkey),
        password: login.password
    };

    await open(session);
}

function encrypt(obj, key) {
    let nonce = new Uint8Array(24);
    crypto.getRandomValues(nonce);

    let json = JSON.stringify(obj, (k, v) => v != null ? v : null);
    let message = (new TextEncoder()).encode(json);
    let box = nacl.secretbox(message, nonce, key);

    let enc = {
        nonce: Base64.toBase64(nonce),
        box: Base64.toBase64(box)
    };

    return enc;
}

function decrypt(enc, key) {
    let nonce = Base64.toBytes(enc.nonce);
    let box = Base64.toBytes(enc.box);

    let message = nacl.secretbox.open(box, nonce, key);
    if (message == null)
        throw new Error('Failed to decrypt message: wrong key?');

    let json = (new TextDecoder()).decode(message);
    let obj = JSON.parse(json);

    return obj;
}

async function loginPassword(mail, password) {
    let key0 = await deriveKey(password, 'login');
    let key1 = await deriveKey(password, 'token');

    let login = await Net.post('/api/password', {
        mail: mail,
        password: Base64.toBase64(key0)
    });

    let session = {
        uid: login.uid,
        ...decrypt(login.token, key1),
        password: login.password
    };

    await open(session);
}

async function deriveKey(password, salt) {
    password = (new TextEncoder).encode(password);
    salt = (new TextEncoder).encode(salt);

    let key = await crypto.subtle.importKey('raw', password, { name: 'PBKDF2' }, false, ['deriveBits']);
    let derived = await crypto.subtle.deriveBits({ name: 'PBKDF2', salt, iterations: 100000, hash: 'SHA-256' }, key, 256);

    return new Uint8Array(derived);
}

async function open(obj) {
    if (session?.vid != obj?.vid) {
        if (db != null)
            await db.close();

        session = null;
        db = null;
        identity = null;
    }

    if (obj?.vid != null && obj?.vkey != null) {
        await downloadVault(obj.vid);

        let vkey = Hex.toBytes(obj.vkey);
        db = await openVault(obj.vid, vkey, DATA_LOCK);
    }

    session = obj;

    let json = JSON.stringify(session);
    sessionStorage.setItem('session', json);
}

async function logout() {
    await db.close();
    db = null;

    session = null;
    sessionStorage.removeItem('session');
    sessionStorage.removeItem('picture');

    channel.postMessage({ message: 'logout' });

    window.onbeforeunload = null;
    window.location.href = '/';

    poisoned = true;
}

// ------------------------------------------------------------------------
// Run
// ------------------------------------------------------------------------

function go(url = null, push = true) {
    if (url == null)
        return run({}, push);

    let changes = Object.assign({}, route);

    if (!(url instanceof URL))
        url = new URL(url, window.location.href);

    let path = url.pathname.replace(/\/+$/, '');
    let parts = path.slice(1).split('/');
    let mode = parts.shift();

    switch (mode) {
        case 'connexion': { changes.mode = 'login'; } break;

        case 'profil': { changes.mode = 'profile'; } break;

        case 'participer': { changes.mode = 'dashboard'; } break;

        case 'etude': {
            let project = PROJECTS.find(project => project.key == parts[0]);

            if (project == null) {
                changes.mode = 'dashboard';
                break;
            }

            changes.mode = 'study';
            changes.project = parts.shift();
            changes.page = '/' + parts.join('/');
            changes.section = null;
        } break;

        case 'journal': {
            changes.mode = 'diary';

            let entry = parseInt(parts[0], 10);

            if (!Number.isNaN() && entry >= 0) {
                changes.entry = entry;
            } else {
                changes.entry = null;
            }
        } break;

        case 'rappels': { changes.mode = 'ignore'; } break;

        default: {
            if (has_run) {
                window.onbeforeunload = null;
                window.location.href = url.href;

                return;
            }

            changes.mode = 'profile';
        } break;
    }

    return run(changes, push);
}

async function run(changes = {}, push = false) {
    if (poisoned)
        return;

    // Go back to top app when the context changes
    let scroll = (changes.mode != null && changes.mode != route.mode);

    await navigator.locks.request(RUN_LOCK, async () => {
        Object.assign(route, changes);

        // We're running!
        has_run = true;

        // Don't run stateful code while dialog is running to avoid concurrency issues
        if (UI.isDialogOpen()) {
            UI.runDialog();
            return;
        }

        // Fetch or create identity
        if (db != null) {
            if (identity == null) {
                identity = await db.fetch1('SELECT picture FROM meta');

                if (identity == null) {
                    identity = {
                        picture: null
                    };

                    await db.exec('INSERT INTO meta (gender) VALUES (NULL)');
                }

                sessionStorage.setItem('picture', identity.picture ?? '');
                channel.postMessage({ message: 'picture', picture: identity.picture });
            }

            // List active studies
            cache.studies = await db.fetchAll(`SELECT s.id, s.key, s.start, s.data,
                                                  SUM(IIF(t.status = 'done', 1, 0)) AS progress, COUNT(t.id) AS total
                                           FROM studies s
                                           LEFT JOIN tests t ON (t.study = s.id AND t.visible = 1)
                                           GROUP BY s.id`);

            // List future events
            {
                let start = LocalDate.today().minusMonths(1);

                cache.events = await db.fetchAll(`SELECT s.key AS title, t.schedule, COUNT(t.id) AS count
                                              FROM tests t
                                              INNER JOIN studies s ON (s.id = t.study)
                                              WHERE t.visible = 1 AND t.schedule >= ? AND t.status <> 'done'
                                              GROUP BY t.schedule, t.study
                                              ORDER BY t.schedule, t.id`, start.toString());

                for (let evt of cache.events) {
                    evt.title = PROJECTS.find(project => project.key == evt.title).title;
                    evt.schedule = LocalDate.parse(evt.schedule);
                }
            }

            // Sync state with expected route
            if (route.mode == 'study') {
                let project = PROJECTS.find(project => project.key == route.project);

                if (project != null) {
                    if (project.key != cache.project?.key || project == cache.project) {
                        if (cache.study == null) {
                            let study = cache.studies.find(study => study.key == project.key);

                            if (study != null)
                                project = await initProject(project, study);

                            cache.study = study;
                        }

                        cache.project = project;
                    }

                    if (cache.study != null) {
                        cache.page = cache.project.pages.find(page => page.key == route.page);

                        if (route.page != null && cache.page == null) {
                            let parts = route.page.split('/');
                            let section = parts.pop();
                            let path = parts.join('/');

                            cache.page = cache.project.pages.find(page => page.key == path);
                            cache.section = (cache.page != null) ? section : null;

                            if (cache.page != null)
                                cache.section = section;
                        } else {
                            cache.section = route.section;
                        }

                        if (cache.page == null)
                            cache.page = cache.project.root;
                    } else {
                        cache.page = null;
                        cache.section = null;
                    }
                } else {
                    cache.project = null;
                    cache.study = null;
                    cache.page = null;
                    cache.section = null;
                }
            }
        }

        // Run module
        switch (route.mode) {
            case 'login': {
                if (db != null) {
                    go('/profil');
                    return;
                }

                await runLogin();
            } break;

            case 'profile': {
                if (db == null) {
                    await runRegister();
                    return;
                }

                await runProfile();
            } break;

            case 'dashboard': {
                if (db == null) {
                    await runRegister();
                    return;
                }

                await runDashboard();
            } break;

            case 'study': {
                if (db == null) {
                    await runRegister();
                    return;
                }

                if (cache.study != null) {
                    await runProject();
                } else {
                    await runConsent();
                }

                // Update route values
                route.project = cache.project?.key;
                if (cache.page != null) {
                    route.page = cache.page.key;
                    route.section = cache.section;
                } else {
                    route.page = null;
                    route.section = null;
                }
            } break;

            case 'diary': {
                if (db == null) {
                    await runRegister();
                    return;
                }

                await runDiary();
            } break;

            case 'ignore': { await runIgnore(); } break;
        }

        // Update URL
        {
            let url = makeURL();

            if (url != route_url) {
                if (push) {
                    window.history.pushState(null, null, url);
                } else if (url != route_url) {
                    window.history.replaceState(null, null, url);
                }

                route_url = url;
            }
        }
    });

    if (scroll)
        window.scrollTo(0, 0);
}

function makeURL(changes = {}) {
    let path = '/';

    let values = Object.assign({}, route, changes);

    switch (values.mode) {
        case 'login': { path += 'connexion'; } break;

        case 'profile': { path += 'profil'; } break;

        case 'dashboard': { path += 'participer'; } break;

        case 'study': {
            path += 'etude';

            if (values.project != null) {
                path += '/' + values.project;
                if (values.page != null)
                    path += values.page;
                if (values.section != null)
                    path += '/' + values.section;
            }
        } break;

        case 'diary': {
            path += 'journal';

            if (values.entry != null)
                path += '/' + values.entry;
        } break;

        case 'ignore': { path += 'rappels'; } break;
    }

    if (path == window.location.pathname && window.location.hash)
        path += window.location.hash;

    return path;
}

async function syncContext(key, func = null) {
    if (ctx_key == key)
        return;

    if (ctx?.stop != null)
        await ctx.stop();

    if (func != null) {
        ctx = func();
    } else {
        ctx = null;
    }

    ctx_key = key;
}

function renderApp(el, fullscreen) {
    if (root_el == null) {
        root_el = document.createElement('div');
        root_el.id = 'root';

        document.body.appendChild(root_el);
    }

    if (fullscreen) {
        render(el, root_el);
    } else {
        render(html`
            <div class="deploy" @click=${deploy}></div>

            <nav id="top">
                <menu>
                    <a id="logo" href="/"><img src=${ASSETS['main/logo']} alt=${'Logo ' + ENV.title} /></a>
                    ${ENV.pages.map((page, idx) => {
                        if (idx > 0 && ENV.pages[idx - 1].title == page.title)
                            return '';

                        let active = isPageActive(page.url);

                        for (let i = idx + 1; i < ENV.pages.length; i++) {
                            let next = ENV.pages[i];
                            if (next.title != page.title)
                                break;
                            active ||= isPageActive(next.url);
                        }

                        return html`<li><a href=${page.url} class=${active ? 'active' : ''} style="margin-left: 0em;">${page.title}</a>`;
                    })}
                    <div style="flex: 1;"></div>
                    <a href="/profil"><img class=${'avatar' + (identity?.picture == null ? ' anonymous' : '')}
                                           src=${identity?.picture ?? ASSETS['ui/anonymous']} alt="" /></a>
                </menu>
            </nav>

            ${el}

            <footer>
                <div style="text-align: right;">
                    ${ENV.title} © 2025
                    <a class="legal" href="/mentions">Mentions légales</a>
                </div>
                <img src=${ASSETS['main/footer']} alt="" width="79" height="64">
                <div style="font-size: 0.8em;">
                    <a href=${'mailto:' + ENV.contact} style="font-weight: bold; color: inherit;">${ENV.contact}</a>
                </div>
            </footer>
        `, root_el);
    }
}

function isPageActive(url) {
    let path = window.location.pathname;

    if (path == url)
        return true;
    if (path.startsWith(url + '/'))
        return true;

    return false;
}

async function initProject(project, study) {
    if (typeof project.bundle == 'string')
        project.bundle = await import(project.bundle);

    let bundle = project.bundle;

    // Init study schema
    {
        project = new ProjectInfo(project);

        let builder = new ProjectBuilder(project);
        let start = LocalDate.fromJSDate(study.start);
        let values = JSON.parse(study.data);

        bundle.init(builder, start, values);
    }

    // Update study tests
    await db.transaction(async t => {
        let prevs = await t.fetchAll(`SELECT id, key, title, schedule
                                      FROM tests
                                      WHERE study = ? AND visible = 1`, study.id);
        let map = new Map(prevs.map(prev => [prev.key, prev]));

        for (let page of project.tests) {
            let prev = map.get(page.key);
            let schedule = page.schedule?.toString?.();

            let skip = (prev != null && page.title == prev.title && schedule == prev.schedule);

            if (!skip) {
                await t.exec(`INSERT INTO tests (study, key, title, visible, status, schedule)
                              VALUES (?, ?, ?, 1, 'empty', ?)
                              ON CONFLICT DO UPDATE SET title = excluded.title,
                                                        visible = excluded.visible,
                                                        schedule = excluded.schedule`,
                             study.id, page.key, page.title, schedule);
            }

            map.delete(page.key);
        }

        for (let prev of map.values())
            await t.exec('UPDATE tests SET visible = 0 WHERE id = ?', prev.id);
    });

    return project;
}

async function runRegister() {
    if (UI.isFullscreen)
        UI.toggleFullscreen(false);

    UI.main(html`
        <div class="tabbar">
            <a class="active">Connectez-vous</a>
        </div>

        <div class="tab">
            <div class="box" style="align-items: center;">
                <div class="header">Vous avez déjà un compte</div>

                <div style="max-width: 700px;">
                    <p>Pour vous connecter, vous devez utiliser le lien présent dans le mail d'inscription. Cherchez le mail portant l'objet <b>« Connexion à ${ENV.title} ! »</b> et cliquez sur le lien à l'intérieur.
                </div>

                <div class="help">
                    <img src=${ASSETS['pictures/help1']} alt="" />
                    <div>
                        <p>Toutes vos données étant chiffrées et sécurisées, vous devez <b>conserver précieusement le lien de connexion</b> qui est présent dans le mail d'inscription !
                        <p>Nous ne serons <b>pas en mesure de recréer le lien de connexion</b> qui existe dans ce mail si vous le perdez !
                    </div>
                </div>

                <div style="max-width: 700px;">
                    <p>Vous pouvez aussi vous <a href="/connexion">connecter avec votre mot de passe</a> si vous en avez créé un après votre première connexion.
                </div>
            </div>

            <div class="box" style="align-items: center;">
                <div class="header">Créez votre compte</div>

                <div style="max-width: 700px;">
                    <p>La <b>création d’un compte</b> est essentielle pour participer aux études, elle permet de :
                    <ul>
                        <li>Garantir la protection de vos données
                        <li>Faciliter le suivi de votre progression
                        <li>Assurer la fiabilité des données recueillies
                    </ul>
                </div>

                <form style="text-align: center;" @submit=${UI.wrap(register)}>
                    <input type="text" name="mail" style="width: 20em;" placeholder="adresse email" />
                    <div class="actions">
                        <button type="submit">Créer mon compte</button>
                    </div>
                </form>
            </div>
        </div>
    `);
}

async function register(e) {
    let form = e.currentTarget;
    let elements = form.elements;

    let mail = elements.mail.value.trim();

    if (!mail)
        throw new Error('Adresse email manquante');

    await Net.post('/api/register', { mail: mail });

    form.innerHTML = '';
    render(html`<p>Consultez l'email qui <b>vous a été envoyé</b> pour continuer !</p>`, form);
}

async function runLogin() {
    if (UI.isFullscreen)
        UI.toggleFullscreen(false);

    UI.main(html`
        <div class="tabbar">
            <a class="active">Connectez-vous</a>
        </div>

        <div class="tab" style="align-items: center;">
            <div class="header">Connectez-vous pour continuer</div>

            <form style="text-align: center;" @submit=${UI.wrap(login)}>
                <label>
                    <input type="text" name="mail" style="width: 20em;" placeholder="adresse email" />
                </label>
                <label>
                    <input type="password" name="password" style="width: 20em;" placeholder="mot de passe" />
                </label>
                <div class="actions">
                    <button type="submit">Continuer</button>
                </div>
            </form>
        </div>
    `);
}

async function login(e) {
    let form = e.currentTarget;
    let elements = form.elements;

    let mail = elements.mail.value.trim();
    let password = elements.password.value.trim();

    if (!mail)
        throw new Error('Adresse email manquante');
    if (!password)
        throw new Error('Mot de passe manquant');

    await loginPassword(mail, password);
    await go('/profil');
}

async function runProfile() {
    if (UI.isFullscreen)
        UI.toggleFullscreen(false);

    let entries = await db.fetchAll('SELECT id, date, title FROM diary ORDER BY id DESC');

    for (let entry of entries)
        entry.date = LocalDate.parse(entry.date);

    UI.main(html`
        <div class="tabbar">
            <a href="/profil" class="active">Profil</a>
            <a href="/participer">Études</a>
            ${cache.project != null ? html`<a href=${makeURL({ mode: 'study' })}>${cache.project.title}</a>` : ''}

            <div style="flex: 1;"></div>
            <a id="sos" @click=${UI.wrap(e => sos(event))}></a>
        </div>

        <div class="tab">
            <div class="row">
                <div class="box profile">
                    <div class="header">
                        Profil
                        ${safeTag('Votre avatar est privé.')}
                    </div>
                    <img class="avatar" src=${identity?.picture ?? ASSETS['ui/user']} alt=""/>

                    <div class="actions vertical">
                        <button type="button" @click=${UI.wrap(changePicture)}>Modifier mon avatar</button>
                        <button type="button" @click=${UI.wrap(changePassword)}>${session.password ? 'Modifier mon mot de passe' : 'Créer un mot de passe'}</button>
                        <button type="button" class="secondary" @click=${UI.insist(logout)}>Se déconnecter</button>
                    </div>
                </div>

                <div class="column">
                    <div class="box">
                        <div class="header">Bienvenue sur ${ENV.title} !</div>
                        ${!session.password ? html`
                            <div class="help">
                                <img src=${ASSETS['pictures/help1']} alt="" />
                                <div>
                                    <p>Vous devrez utiliser le mail d'inscription qui vous a été envoyé initialement pour vous connecter à nouveau.
                                    <p>Cependant, nous vous recommandons de <b>créer un mot de passe</b> pour vous permettre de vous connecter sans avoir besoin de ce mail !
                                    <div class="actions">
                                        <button @click=${UI.wrap(changePassword)}>Je souhaite créer un mot de passe</button>
                                    </div>
                                </div>
                            </div>
                        ` : ''}
                        <p>Utilisez le bouton à gauche pour <b>personnaliser votre avatar</b>, si vous le désirez.
                        Une fois prêt(e), accéder à votre <b>tableau de bord et aux études</b> à l'aide du bouton ci-dessous.

                        <div class="actions">
                            <a href="/participer">Accéder aux études</a>
                        </div>
                    </div>

                    <div class="box">
                        <div class="header">
                            Mon journal
                            ${safeTag('Les entrées de votre journal sont privées.')}
                        </div>
                        ${entries.map(entry => html`
                            <div class="diary">
                                <img src=${ASSETS['ui/diary']} alt="" />
                                <div class="text">
                                    <b>${entry.title || html`<i>Entrée sans titre</i>`}</b><br>
                                    ${niceDate(entry.date, true)}
                                </div>
                                <button type="button" @click=${UI.wrap(e => navigateDiary(entry.id))}>Éditer</button>
                            </div>
                        `)}
                        ${!entries.length ? html`
                            <div class="help right">
                                <div>
                                    <p>Utilisez librement votre journal, ces <b>données sont privées</b> et nous ne nous en servirons pas !
                                    <p>Pour ajouter une première entrée, cliquez sur le bouton d'ajout ci-dessous.
                                </div>
                                <img src=${ASSETS['pictures/help2']} alt="" />
                            </div>
                        ` : ''}
                        <div class="actions">
                            <a href="/journal">Ajouter une entrée</a>
                        </div>
                    </div>
                </div>
            </div>
        </div>
    `);
}

async function navigateDiary(id) {
    route.mode = 'diary';
    route.entry = id;

    await go();
}

async function runDashboard() {
    if (UI.isFullscreen)
        UI.toggleFullscreen(false);

    UI.main(html`
        <div class="tabbar">
            <a href="/profil">Profil</a>
            <a href="/participer" class="active">Études</a>
            ${cache.project != null ? html`<a href=${makeURL({ mode: 'study' })}>${cache.project.title}</a>` : ''}

            <div style="flex: 1;"></div>
            <a id="sos" @click=${UI.wrap(e => sos(event))}></a>
        </div>

        <div class="tab">
            <div class="box">
                <div>
                    <div class="header">Bienvenue sur ${ENV.title} !</div>
                    <p>Vous pouvez vous <b>inscrire ou continuer</b> votre participation aux études ci-dessous le cas échéant, ou retourner à votre profil pour utiliser votre journal ou modifier votre avatar.
                </div>

                <div class="actions">
                    <a href="/profil">Accéder au profil</a>
                </div>
            </div>

            <div class="row">
                <div class="column" style="flex: 1;">
                    ${PROJECTS.map(project => {
                        let study = cache.studies.find(study => study.key == project.key);
                        let cls = 'study';

                        if (study != null) {
                            if (study.progress == study.total) {
                                cls += ' done';
                            } else {
                                cls += ' draft';
                            }
                        } else {
                            cls += ' unsubscribed';
                        }

                        return html`
                            <div class=${cls}>
                                <div class="title">
                                    Étude ${project.title}
                                    <img src=${project.picture} alt="" />
                                </div>
                                ${study == null ? html`<div class="description">${project.description}</div>` : ''}
                                ${study != null ? html`
                                    <div class="progress">
                                        ${study?.total && !study.progress ? 'Participation acceptée' : ''}
                                        ${study?.total && study.progress && study.progress < study.total ? 'Participation en cours' : ''}
                                        ${study?.total && study.progress == study.total ? 'Participation terminée' : ''}
                                    </div>
                                ` : ''}
                                <button type="button" @click=${UI.wrap(e => openStudy(project))}>
                                    ${study == null ? 'Participer' : ''}
                                    ${study?.total && !study.progress ? 'Commencer' : ''}
                                    ${study?.total && study.progress && study.progress < study.total ? 'Reprendre' : ''}
                                    ${study?.total && study.progress == study.total ? 'Accéder' : ''}
                                    ${study != null && !study.total ? 'Ouvrir' : ''}
                                </button>
                            </div>
                        `;
                    })}

                    ${cache.events.length ? html`
                        <div class="help" style="align-self: end;">
                            <img src=${ASSETS['pictures/help1']} alt="" />
                            <div>
                                <p>Ajoutez les <b>prochaines étapes de l'étude à votre agenda</b> pour revenir le moment venu !
                                <p>Utilisez les boutons <img class="shortcut" src=${ASSETS['ui/calendar']} alt="Agenda" /> dans la section <b>« À venir »</b>.
                            </div>
                        </div>
                    ` : ''}
                </div>

                ${cache.studies.length ? html`
                    <div class="box">
                        <div class="header">À venir</div>
                        ${cache.events.map(evt => html`
                            <div class="event">
                                <div class="text">
                                    <b>${niceDate(evt.schedule, true)}</b><br>
                                    ${evt.title}
                                </div>
                                <button type="button" @click=${UI.wrap(e => addToCalendar(e, evt))}><img src=${ASSETS['ui/calendar']} alt="Agenda" /></button>
                            </div>
                        `)}
                        ${!cache.events.length ? html`<p style="text-align: center;">Aucun évènement à venir</p>` : ''}
                    </div>
                ` : ''}
            </div>
        </div>
    `);
}

function addToCalendar(e, evt) {
    UI.popup(e, html`
        <div class="calendars">
            ${EventProviders.map(provider => html`
                <a @click=${UI.wrap(e => add(provider))}>
                    <img src=${ASSETS[provider.icon]} alt=${provider.title} title=${provider.title} />
                </a>
            `)}
        </div>
    `);

    function add(provider) {
        let title = ENV.title + ' - ' + evt.title;

        let description = html`
            Participation à <b>${ENV.title}</b> :
            <a href=${ENV.url}>Étude ${evt.title}</a>
        `;

        createEvent(provider.key, evt.schedule, title, description);
    }
}

function isLogged() {
    let logged = (db != null);
    return logged;
}

async function changePassword() {
    await UI.dialog({
        overlay: true,

        run: (render, close) => {
            return html`
                <div class="tabbar">
                    <a class="active">Mot de passe</a>
                </div>

                <div class="tab" style="align-items: center;">
                    <div style="max-width: 700px;">
                        <p>Pour vous connecter à Lignes de Vie, vous avez besoin du lien qui vous a été envoyé par mail lors de votre inscription.
                        <p>Cependant, si vous le souhaitez, vous pouvez définir un mot de passe qui vous fournira un <b>moyen de connexion supplémentaire</b> !
                    </div>

                    <input type="password" name="password1" placeholder="mot de passe" />
                    <input type="password" name="password2" placeholder="confirmation" />
                    <div class="tip">Le mot de passe doit contenir au moins 10 caractères, des lettres, des chiffres et un symbole spécial.</div>
                    <div class="actions">
                        <button type="button" class="secondary" @click=${UI.insist(close)}>Annuler</button>
                        <button type="submit">${session.password ? 'Modifier le mot de passe' : 'Définir le mot de passe'}</button>
                    </div>

                    <div class="help">
                        <img src=${ASSETS['pictures/help1']} alt="" />
                        <div>
                            <p>Toutes vos données étant chiffrées et sécurisées, nous ne serons <b>pas en mesure de vous aider</b> si vous perdez à la fois le mail d'inscription et le mot de passe que vous avez défini !
                        </div>
                    </div>
                </div>
            `;
        },

        submit: async (elements) => {
            let password1 = elements.password1.value.trim();
            let password2 = elements.password2.value.trim();

            if (!password1 || !password2)
                throw new Error('Password is missing');
            if (password2 != password1)
                throw new Error('Passwords do not match');

            let key0 = await deriveKey(password1, 'login');
            let key1 = await deriveKey(password1, 'token');

            let obj = {
                vid: session.vid,
                vkey: session.vkey,
                rid: session.rid
            };

            await Net.post('/api/protect', {
                uid: session.uid,
                password: Base64.toBase64(key0),
                token: encrypt(obj, key1)
            });

            session.password = true;

            let json = JSON.stringify(session);
            sessionStorage.setItem('session', json);
        }
    });
}

async function changePicture() {
    let meta = await db.fetch1('SELECT picture, avatar FROM meta');
    let settings = (meta.avatar != null) ? JSON.parse(meta.avatar) : meta.picture;

    let cropper = new PictureCropper('Modifier mon avatar', 256);

    let notion = await Net.get(BUNDLES['notion.json']);

    cropper.defaultURL = ASSETS['ui/user'];
    cropper.notionAssets = notion;
    cropper.imageFormat = 'image/webp';

    try {
        await cropper.change(settings, async (blob, settings) => {
            if (blob == meta.picture)
                return;

            let url = null;

            if (blob != null) {
                url = await new Promise((resolve, reject) => {
                    let reader = new FileReader;
                    reader.onload = e => resolve(e.target.result);
                    reader.readAsDataURL(blob);
                });
            }

            if (settings != null)
                settings = JSON.stringify(settings);
            await db.exec('UPDATE meta SET picture = ?, avatar = ?', url, settings);

            identity.picture = url;

            sessionStorage.setItem('picture', identity.picture ?? '');
            channel.postMessage({ message: 'picture', picture: url });
        });
    } catch (err) {
        if (err != null)
            Log.error(err);
    }

    await go();
}

async function openStudy(project) {
    route.mode = 'study';
    route.project = project.key;

    await go();
}

async function runIgnore() {
    if (UI.isFullscreen)
        UI.toggleFullscreen(false);

    let uid = session?.uid;
    let project = cache.project;

    if (uid == null || project == null) {
        if (window.location.hash) {
            let hash = window.location.hash.substr(1);
            let query = new URLSearchParams(hash);

            uid = query.get('uid');

            let study = query.get('study');
            project = PROJECTS.find(project => project.index == study);
        }

        if (uid == null || project == null)
            throw new Error('Paramètres manquants');
    }

    UI.main(html`
        <div class="tabbar">
            <a class="active">Notifications ${project.title}</a>
        </div>

        <div class="tab" style="align-items: center;">
            <form style="text-align: center;" @submit=${UI.wrap(e => ignore(e, uid, project))}>
                <label>
                    <input type="checkbox" name="ignore" />
                    <span>Je ne souhaite plus recevoir de rappels pour ${project.title}</span>
                </label>
                <div class="actions">
                    <button type="submit">Désactiver les rappels</button>
                </div>
            </form>
        </div>
    `);
}

async function ignore(e, uid, project) {
    let form = e.currentTarget;
    let elements = form.elements;

    if (!elements.ignore.checked)
        throw new Error('Vous devez confirmer la désactivation des rappels pour contineur !');

    await Net.post('/api/ignore', {
        uid: uid,
        study: project.index
    });

    form.innerHTML = '';
    render(html`<p>Vous ne <b>recevrez plus de rappels</b> pour ${project.title} !</p>`, form);
}

// ------------------------------------------------------------------------
// Study
// ------------------------------------------------------------------------

async function runConsent() {
    if (UI.isFullscreen)
        UI.toggleFullscreen(false);

    let project = cache.project;

    if (typeof project.bundle == 'string')
        project.bundle = await import(project.bundle);

    await syncContext(project, () => new ConsentModule(app, project));
    await ctx.run();

    UI.main(html`
        <div class="tabbar">
            <a href="/profil">Profil</a>
            <a href="/participer">Études</a>
            <a class="active">${project.title}</a>

            <div style="flex: 1;"></div>
            <a id="sos" @click=${UI.wrap(e => sos(event))}></a>
        </div>

        <div class="tab">
            ${ctx.render()}
        </div>
    `);
}

async function runProject() {
    // Sync tests
    cache.tests = await db.fetchAll(`SELECT t.id, t.key, t.status, t.notify
                                     FROM tests t
                                     INNER JOIN studies s ON (s.id = t.study)
                                     WHERE s.id = ? AND t.visible = 1`, cache.study.id);

    // Best effort, don't wait
    syncEvents();

    await syncContext(cache.page, () => {
        let page = cache.page;
        let test = cache.tests.find(test => test.key == page.key);

        switch (page.type) {
            case 'module': return null;
            case 'form': return new FormModule(app, cache.study, page);
            case 'network': return new NetworkModule(app, cache.study, page);
        }
    });
    if (ctx?.run != null)
        await ctx.run();

    // Render tab
    {
        let project = cache.project;
        let page = cache.page;

        UI.main(html`
            <div class="tabbar">
                <a href="/profil">Profil</a>
                <a href="/participer">Études</a>
                <a class="active">${project.title}</a>

                <div style="flex: 1;"></div>
                <a id="sos" @click=${UI.wrap(e => sos(event))}></a>
            </div>

            <div class="tab" style="flex: 1;">
                ${page.type == 'module' ? renderModule() : null}
                ${page.type != 'module' ? renderTest() : null}
            </div>
        `);
    }
}

async function syncEvents() {
    let project = cache.project;
    let study = cache.study;
    let tests = cache.tests;

    await navigator.locks.request(REMIND_LOCK, async () => {
        let sync = tests.some(test => test.notify != test.status);

        if (!sync)
            return;

        // Make copy in case it's refreshed while this runs
        tests = tests.slice();

        let start = LocalDate.fromJSDate(study.start);
        let offset = -(new Date).getTimezoneOffset();

        let events = new Map;

        for (let page of project.tests) {
            let schedule = page.schedule?.toString?.();
            let test = tests.find(test => test.key == page.key);

            if (!schedule)
                continue;

            let evt = events.get(schedule);

            if (evt == null) {
                evt = {
                    empty: 0,
                    draft: 0,
                    done: 0
                };

                events.set(schedule, evt);
            }

            evt[test.status]++;
        }

        // Simplify events
        events = Array.from(Util.map(events.entries(), ([date, evt]) => ({
            date: date,
            ...evt
        })));
        events = events.filter(evt => evt.empty || evt.draft);
        events = events.map(evt => ({
            date: evt.date,
            partial: evt.draft > 0 || evt.done > 0
        }));

        await Net.post('/api/remind', {
            uid: session.uid,
            study: project.index,
            title: project.title,
            start: start,
            events: events,
            offset: offset
        });

        await db.transaction(async t => {
            for (let test of tests)
                await t.exec('UPDATE tests SET notify = ? WHERE id = ?', test.status, test.id);
        });
    });
}

function renderModule() {
    if (UI.isFullscreen)
        UI.toggleFullscreen(false);

    let project = cache.project;
    let page = cache.page;

    let today = LocalDate.today();

    let step = page.chain.findLast(it => it.type == 'module' && it.step != null);
    let [progress, total] = computeProgress(step ?? project.root, today);
    let cls = 'summary ' + (progress == total ? 'done' : 'draft');

    return html`
        <div class=${cls}>
            <img src=${project.picture} alt="" />
            <div>
                <div class="header">
                    ${step == null ? `Étude ${project.title}` : ''}
                    ${step != null ? step.title : ''}
                </div>
                ${step != null ? step.step : project.summary}
            </div>
            ${step != null ? progressCircle(progress, total) : ''}
        </div>

        ${Util.mapRange(0, page.chain.length - 1, idx => {
            let parent = page.chain[idx];
            let next = page.chain[idx + 1];

            return html`
                <div class="box level" @click=${UI.wrap(e => navigateStudy(parent))}>
                    ${parent.level ?? ''}${parent.level ? ' - ' : ''}
                    ${next.title}
                </div>
            `;
        })}

        <div class="box">
            <div class="header">${page.level}</div>
            <div class="modules">
                ${page.modules.map(child => {
                    let [progress, total] = computeProgress(child, today);

                    let cls = 'module';
                    let status = null;
                    let img = null;
                    let available = true;

                    if (total && progress == total) {
                        cls += ' done';
                        status = 'Terminé';
                        img = ASSETS['ui/validate'];
                    } else if (progress) {
                        cls += ' draft';
                        status = progressBar(progress, total);
                    } else if (total) {
                        status = 'À compléter';

                        let earliest = null;

                        for (let it of child.tests) {
                            if (it.status == 'done' || it.schedule == null)
                                continue;

                            if (earliest == null || it.schedule < earliest)
                                earliest = it.schedule;
                        }

                        if (earliest != null && earliest > today) {
                            status = niceDate(earliest, true);

                            if (!ENV.test) {
                                cls += ' disabled';
                                available = false;
                            }
                        } else {
                            cls += ' draft';
                        }
                    } else {
                        status = 'Non disponible';
                        cls += ' disabled';
                        available = false;
                    }

                    return html`
                        <div class=${cls} @click=${available ? UI.wrap(e => navigateStudy(child)) : null}>
                            <div class="title">${child.title}</div>
                            ${img != null ? html`<img src=${img} alt="" />` : ''}
                            <div class="status">${status}</div>
                        </div>
                    `;
                })}
                ${!page.modules.length ? page.tests.map(child => {
                    let test = cache.tests.find(test => test.key == child.key);

                    let cls = 'module ' + test.status;
                    let status = null;
                    let img = null;
                    let available = true;

                    if (test.status == 'done') {
                        status = 'Terminé';
                        img = ASSETS['ui/validate'];
                    } else if (child.schedule != null && child.schedule > today) {
                        status = niceDate(child.schedule, true);

                        if (!ENV.test) {
                            cls += ' disabled';
                            available = false;
                        }
                    } else {
                        status = 'À compléter';
                        cls += ' draft';
                    }

                    return html`
                        <div class=${cls} @click=${available ? UI.wrap(ae => navigateStudy(child)) : null}>
                            <div class="title">${child.title}</div>
                            ${img != null ? html`<img src=${img} alt="" />` : ''}
                            <div class="status">${status}</div>
                        </div>
                    `;
                }) : ''}
            </div>

            ${wrapHelp(page, progress, total)}
        </div>
    `;
}

function wrapHelp(page, progress, total) {
    let help = page.help;
    let left = (page.chain.length % 2 != 0);

    if (typeof help == 'function')
        help = help(progress, total);
    if (!help)
        return '';

    if (left) {
        return html`
            <div class="help">
                <img src=${ASSETS['pictures/help1']} alt="" />
                <div>${help}</div>
            </div>
        `;
    } else {
        return html`
            <div class="help right">
                <div>${help}</div>
                <img src=${ASSETS['pictures/help2']} alt="" />
            </div>
        `;
    }
}

function renderTest() {
    let project = cache.project;
    let page = cache.page;

    let today = LocalDate.today();

    let step = page.chain.findLast(it => it.type == 'module' && it.step != null);
    let [progress, total] = computeProgress(step ?? project.root, today);
    let cls = 'summary ' + (progress == total ? 'done' : 'draft');

    return html`
        <div class=${cls}>
            <img src=${project.picture} alt="" />
            <div>
                <div class="header">
                    ${step == null ? `Étude ${project.title}` : ''}
                    ${step != null ? step.title : ''}
                </div>
                <div class="header">${page.title}</div>
                <div class="actions">
                    <button type="button" class="secondary"
                            @click=${UI.wrap(e => exitTest(page))}>Retourner au menu</button>
                </div>
            </div>
            ${step != null ? progressCircle(progress, total) : ''}
        </div>

        ${ctx.render(cache.section)}
    `;
}

async function startStudy(project, values) {
    let start = (new Date).valueOf();
    let data = JSON.stringify(values);

    let study = await db.fetch1(`INSERT INTO studies (key, start, data)
                                 VALUES (?, ?, ?)
                                 ON CONFLICT DO UPDATE SET start = excluded.start
                                 RETURNING id, key, start, data`,
                                project.key, start, data);

    await Net.post('/api/publish', {
        rid: session.rid,
        study: project.index,
        key: '/',
        values: values
    });

    project = await initProject(project, study);

    // Go back to dashboard if there are scheduled tests, open study otherwise
    if (!project.tests.some(test => test.schedule != null)) {
        route.mode = 'study';
        route.project = project.key;
    } else {
        route.mode = 'dashboard';
        route.project = null;
    }

    await go();
}

async function navigateStudy(page, section = null) {
    while (page.type == 'module' && page.modules.length == 1)
        page = page.modules[1];
    if (page.type == 'module' && page.tests.length == 1)
        page = page.tests[0];

    route.page = page.key;
    route.section = (page != null) ? section : null;

    await go();
}

async function saveTest(page, data, status = 'draft') {
    let mtime = (new Date).valueOf();
    let json = JSON.stringify(data);
    let payload = await deflate(json);

    await db.exec(`UPDATE tests SET status = ?, mtime = ?, payload = ?
                   WHERE study = ? AND key = ?`, status, mtime, payload, cache.study.id, page.key);

    await go();
}

async function finalizeTest(page, data, results) {
    let project = cache.project;

    await saveTest(page, data, 'done');

    await Net.post('/api/publish', {
        rid: session.rid,
        study: project.index,
        key: page.key,
        values: results
    });

    await exitTest(page);
}

async function exitTest(page) {
    let today = LocalDate.today();

    let next = page;

    do {
        next = next.chain[next.chain.length - 2];
    } while (next != cache.project.root && isComplete(next, today, [page]));

    await navigateStudy(next);
}

function isComplete(page, date, saved = []) {
    for (let child of page.tests) {
        let test = cache.tests.find(test => test.key == child.key);

        if (test.schedule != null && test.schedule > date)
            continue;
        if (test.status != 'done' && !saved.includes(child))
            return false;
    }

    return true;
}

function computeProgress(page, date) {
    let progress = page.tests.reduce((acc, child) => {
        let test = cache.tests.find(test => test.key == child.key);
        return acc + (test.status == 'done');
    }, 0);
    let total = page.tests.length;

    return [progress, total];
}

// ------------------------------------------------------------------------
// Diary
// ------------------------------------------------------------------------

async function runDiary() {
    if (DiaryModule == null) {
        let js = await import(BUNDLES['diary.js']);

        let css = await Net.fetch(BUNDLES['diary.css']).then(r => r.text());
        let style = await (new CSSStyleSheet).replace(css);

        DiaryModule = js.DiaryModule;
        document.adoptedStyleSheets.push(style);
    }

    if (UI.isFullscreen)
        UI.toggleFullscreen(false);

    await syncContext(DiaryModule, () => new DiaryModule(app));
    await ctx.run();

    UI.main(html`
        <div class="tabbar">
            <a href="/profil">Profil</a>
            <a href="/participer">Études</a>
            <a href=${makeURL()} class="active">Mon journal</a>

            <div style="flex: 1;"></div>
            <a id="sos" @click=${UI.wrap(e => sos(event))}></a>
        </div>

        <div class="tab">
            ${ctx.render()}
        </div>
    `);
}

// ------------------------------------------------------------------------
// SOS
// ------------------------------------------------------------------------

function sos(e) {
    let dlg = document.querySelector('#help');

    if (dlg == null) {
        dlg = document.createElement('dialog');
        dlg.id = 'help';
    }

    render(html`
        <div @click=${stop}>
            <img src=${ASSETS['ui/3114']} width="232" height="58" alt="">
            <p>Le <b>3114</b> est le numéro national de prévention de suicide. Consultez le <a href="https://3114.fr/" target="_blank">site du 3114</a> pour plus d'informations.</p>

            <img src=${ASSETS['ui/116006']} width="292" height="150" alt="">
            <p>Le <b>116 006</b> est un numéro d’aide aux victimes géré par France Victimes. L'appel est gratuit 7j/7 24h/24. Consultez le <a href="https://www.france-victimes.fr/index.php/categories-inavem/105-actualites/824-116006-un-nouveau-numero-pour-l-aide-aux-victimes-enparlerpouravancer" target="_blank">site de France Victimes</a> pour plus d'informations.</p>

            <img src=${ASSETS['ui/15']} width="237" height="114" alt="">
            <p>Le <b>15</b> est le numéro du SAMU, disponible 7j/7 24h/24.</p>

            <button type="button" class="secondary" @click=${close}>Fermer</button>
        </div>
    `, dlg);

    if (!dlg.open) {
        document.body.appendChild(dlg);
        dlg.show();
    } else {
        close();
    }

    if (e.target == e.currentTarget)
        e.preventDefault();

    function close() {
        dlg.close();
        dlg.parentNode.removeChild(dlg);
    }

    function stop(e) {
        if (e.target != e.currentTarget && e.target.tagName == 'A')
            e.stopPropagation();
    }
}

export {
    UI,
    route,
    identity,
    db,

    start,
    go,
    run,

    isLogged,
    changePicture,

    startStudy,
    navigateStudy,

    saveTest,
    finalizeTest,
    exitTest
}
