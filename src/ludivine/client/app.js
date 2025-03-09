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

import { render, html, ref } from '../../../vendor/lit-html/lit-html.bundle.js';
import * as nacl from '../../../vendor/tweetnacl-js/nacl-fast.js';
import { Util, Log, Net, LocalDate } from '../../web/core/base.js';
import { Hex, Base64 } from '../../web/core/mixer.js';
import * as sqlite3 from '../../web/core/sqlite3.js';
import { computeAge, dateToString, niceDate,
         progressBar, progressCircle } from './lib/util.js';
import * as app from './app.js';
import * as UI from './ui.js';
import { SmallCalendar, EventProviders, createEvent } from './lib/calendar.js';
import { PictureCropper } from './lib/picture.js';
import { PROJECTS } from '../projects/projects.js';
import { ProjectInfo, ProjectBuilder } from './project.js';
import { FormModule } from './form/form.js';
import { NetworkModule } from './network/network.js';
import { ASSETS } from '../assets/assets.js';

// Needed for menu deploy and other UI stuff
import '../../web/flat/static.js';

import '../assets/client.css';

const DATABASE_VERSION = 3;

Object.assign(T, {
    cancel: 'Annuler',
    confirm: 'Confirmer',
    confirm_not_reversible: 'Attention, cette action ne peut pas être annulée',
    error_has_occured: 'Une erreur est survenue',
    filter: 'Filter',

    browse_for_image: 'Parcourir',
    clear_picture: 'Effacer l\'image',
    edit: 'Modifier',
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

let session = null;
let db = null;
let identity = null;

let root_el = null;
let calendar = null;

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

// ------------------------------------------------------------------------
// Init
// ------------------------------------------------------------------------

async function start() {
    UI.init(run, renderApp);

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
        if (UI.isDialogOpen())
            return 'Validez ou fermez la boîte de dialogue avant de continuer';
        if (ctx != null && ctx.hasUnsavedData())
            return 'Les modifications en cours seront perdues si vous continuez, êtes-vous sûr(e) de vouloir continuer ?';
    };

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

            await login(uid, tkey, registration);
        }

        let url = window.location.pathname + window.location.search;
        history.replaceState('', document.title, url);
    }

    // Open existing session (if any)
    try {
        let json = sessionStorage.getItem('session');
        session = JSON.parse(json);
    } catch (err) {
        console.error(err);
    }

    // Open database
    if (session?.vid != null && session?.vkey != null) {
        await initSQLite();

        let filename = 'ludivine/' + session.vid + '.db';
        let vkey = Hex.toBytes(session.vkey);

        db = await openDatabase(filename, vkey);
    }

    await go(window.location.href, false);

    UI.main();
    document.body.classList.remove('loading');
}

async function login(uid, tkey, registration) {
    let vkey = new Uint8Array(32);
    crypto.getRandomValues(vkey);

    let session = {
        vid: crypto.randomUUID(),
        vkey: Hex.toHex(vkey),
        rid: crypto.randomUUID()
    };
    let token = encrypt(session, tkey);

    // Retrieve existing token (or create it if none is set) and session
    token = await Net.post('/api/login', {
        uid: uid,
        token: token,
        registration: registration
    });
    session = decrypt(token, tkey);

    let json = JSON.stringify(session);
    sessionStorage.setItem('session', json);

    return session;
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

async function logout() {
    await db.close();
    db = null;

    sessionStorage.removeItem('session');
    sessionStorage.removeItem('picture');

    window.onbeforeunload = null;
    window.location.href = '/';

    poisoned = true;
}

// ------------------------------------------------------------------------
// Run
// ------------------------------------------------------------------------

async function go(url = null, push = true) {
    if (url != null) {
        if (!(url instanceof URL))
            url = new URL(url, window.location.href);

        let path = url.pathname.replace(/\/+$/, '');
        let parts = path.slice(1).split('/');
        let mode = parts.shift();

        switch (mode) {
            case 'profil': { route.mode = 'profile'; } break;

            case 'participer': {
                route.mode = 'study';

                let project = PROJECTS.find(project => project.key == parts[0]);

                if (project != null) {
                    route.project = parts.shift();
                    route.page = '/' + parts.join('/');
                    route.section = null;
                } else {
                    route.project = null;
                    route.page = null;
                    route.section = null;
                }
            } break;

            case 'journal': {
                route.mode = 'diary';

                let entry = parseInt(parts[0], 10);

                if (!Number.isNaN() && entry >= 0) {
                    route.entry = entry;
                } else {
                    route.entry = null;
                }
            } break;

            default: {
                if (has_run) {
                    window.onbeforeunload = null;
                    window.location.href = url.href;

                    return;
                }

                route.mode = 'profile';
            } break;
        }
    }

    await run(push);
}

async function run(push = false) {
    if (poisoned)
        return;

    // We're running!
    has_run = true;

    // Don't run stateful code while dialog is running to avoid concurrency issues
    if (UI.isDialogOpen()) {
        UI.runDialog();
        return;
    }

    // Nothing to see
    if (db == null) {
        await runRegister();
        return;
    }

    // Fetch or create identity
    if (identity == null) {
        identity = await db.fetch1('SELECT picture FROM meta');

        if (identity == null) {
            identity = {
                picture: null
            };

            await db.exec('INSERT INTO meta (gender) VALUES (NULL)');
        }

        sessionStorage.setItem('picture', identity.picture ?? '');
    }

    // List active studies
    cache.studies = await db.fetchAll(`SELECT s.id, s.key, s.start,
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

    // Run module
    switch (route.mode) {
        case 'profile': { await runProfile(); } break;

        case 'study': {
            if (cache.project != null) {
                if (cache.study != null) {
                    await runProject();
                } else {
                    await runConsent();
                }
            } else {
                await runDashboard();
            }
        } break;

        case 'diary': { await runDiary(); } break;
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
}

function makeURL(values = {}) {
    let path = '/';

    values = Object.assign({}, route, values);

    switch (route.mode) {
        case 'profile': { path += 'profil'; } break;

        case 'study': {
            path += 'participer';

            if (route.project != null) {
                path += '/' + route.project;
                if (route.page != null)
                    path += route.page;
                if (route.section != null)
                    path += '/' + route.section;
            }
        } break;

        case 'diary': {
            path += 'journal';

            if (route.entry != null)
                path += '/' + route.entry;
        } break;
    }

    return path;
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
            <div id="deploy"></div>

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
                <div style="text-align: right; line-height: 1.2;">
                    ${ENV.title} © 2024<br>
                    <a href="/mentions" style="font-size: 0.8em;">Mentions légales</a>
                </div>
                <img src=${ASSETS['main/footer']} alt="" width="79" height="64">
                <div style="font-size: 0.8em;">
                    <a href="mailto:lignesdevie@cn2r.fr" style="font-weight: bold; color: inherit;">contact@ldv-recherche.fr</a>
                </div>
            </footer>

            ${isLogged() ? html`<a id="sos" @click=${UI.wrap(e => sos(event))}></a>` : ''}
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

        bundle.init(builder, start);
    }

    // Update study tests
    await db.transaction(async () => {
        await db.exec('UPDATE tests SET visible = 0 WHERE study = ?', study.id);

        for (let page of project.tests) {
            let schedule = page.schedule?.toString?.();

            await db.exec(`INSERT INTO tests (study, key, title, visible, status, schedule)
                           VALUES (?, ?, ?, 1, 'empty', ?)
                           ON CONFLICT DO UPDATE SET title = excluded.title,
                                                     visible = excluded.visible,
                                                     schedule = excluded.schedule`,
                          study.id, page.key, page.title, schedule);
        }
    });

    return project;
}

async function runRegister() {
    if (UI.isFullscreen)
        UI.toggleFullscreen(false);

    UI.main(html`
        <div class="tabbar">
            <a class="active">Enregistrez-vous</a>
        </div>

        <div class="tab">
            <div class="box" style="align-items: center;">
                <div class="header">Enregistrez-vous pour continuer</div>

                <div>
                    <p>La <b>création d’un compte</b> est essentielle pour participer aux études, elle permet de :
                    <ul>
                        <li>Garantir la protection de vos données
                        <li>Faciliter le suivi de votre progression
                        <li>Assurer la fiabilité des données recueillies
                    </ul>
                </div>

                <form style="text-align: center;" @submit=${UI.wrap(register)}>
                    <input type="text" name="email" style="width: 20em;" placeholder="adresse email" />
                    <div class="actions">
                        <button type="submit">Créer mon compte</button>
                    </div>
                </form>

                <div class="help">
                    <img src=${ASSETS['pictures/help1']} alt="" />
                    <div>
                        <p>Toutes vos données étant chiffrées et sécurisées, <b>conservez précieusement le lien de connexion</b> qui va vous être envoyé par e-mail !
                        <p>Nous ne serons <b>pas en mesure de recréer le lien de connexion</b> qui existe dans ce mail si vous le perdez !
                    </div>
                </div>
            </div>
        </div>
    `);
}

async function register(e) {
    let form = e.currentTarget;
    let elements = form.elements;

    if (!elements.email.value)
        throw new Error('Adresse email manquante');

    await Net.post('/api/register', { email: elements.email.value });

    form.innerHTML = '';
    render(html`<p>Consultez l'email qui <b>vous a été envoyé</b> pour continuer !</p>`, form);
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
            <a href="/participer">Tableau de bord</a>
        </div>

        <div class="tab">
            <div class="row">
                <div class="box profile">
                    <div class="header">
                        Profil
                        ${UI.safe('votre profil')}
                    </div>
                    <img class="avatar" src=${identity?.picture ?? ASSETS['ui/user']} alt=""/>

                    <div class="actions">
                        <button type="button" @click=${UI.wrap(changePicture)}>Modifier mon avatar</button>
                        <button type="button" class="secondary" @click=${UI.insist(logout)}>Se déconnecter</button>
                    </div>
                </div>

                <div class="column">
                    <div class="box">
                        <div>
                            <div class="header">Bienvenue sur ${ENV.title} !</div>
                            <p>Utilisez le bouton à gauche pour <b>personnaliser votre avatar</b>, si vous le désirez.
                            Une fois prêt(e), accéder à votre <b>tableau de bord et aux études</b> à l'aide du bouton ci-dessous.
                        </div>

                        <div class="actions">
                            <a href="/participer">Accéder aux études</a>
                        </div>
                    </div>

                    <div class="box">
                        <div class="header">
                            Mon journal
                            ${UI.safe('votre journal')}
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

    await run(true);
}

async function runDashboard() {
    if (UI.isFullscreen)
        UI.toggleFullscreen(false);

    UI.main(html`
        <div class="tabbar">
            <a href="/profil">Profil</a>
            <a href="/participer" class="active">Tableau de bord</a>
        </div>

        <div class="tab">
            <div class="box">
                <div>
                    <div class="header">Bienvenue sur ${ENV.title} !</div>
                    <p>Vous pouvez vous y <b>inscrire ou continuer</b> votre participation aux études ci-dessous le cas échéant.
                    Une fois inscrit à une étude, nous vous conseillons d'ajouter les <b>échéances à votre agenda</b> à l'aide du bouton dédié afin de revenir sur l'application le moment venu.
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
                        }

                        let online = (project.bundle != null);

                        return html`
                            <div class=${cls}>
                                <img src=${project.picture} alt="" />
                                <div class="info">
                                    <b>Étude n°${project.index}</b><br>
                                    Étude ${project.title}
                                </div>
                                <div class="progress">
                                    ${study != null && study.progress < study.total ? 'Participation en cours' : ''}
                                    ${study != null && study.progress == study.total ? 'Participation terminé' : ''}
                                </div>
                                ${study == null && online ?
                                    html`<button type="button"
                                                 @click=${UI.wrap(e => openStudy(project))}>Participer</button>` : ''}
                                ${study == null && !online ?
                                    html`<button type="button" disabled>Prochainement</button>` : ''}
                                ${study != null && study.progress < study.total ?
                                    html`<button type="button"
                                                 @click=${UI.wrap(e => openStudy(project))}>${study.progress ? 'Reprendre' : 'Commencer'}</button>` : ''}
                                ${study != null && study.progress == study.total ?
                                    html`<button type="button" disabled>Résultats</button>` : ''}
                            </div>
                        `;
                    })}

                    ${cache.events.length ? html`
                        <div class="help" style="align-self: end;">
                            <img src=${ASSETS['pictures/help1']} alt="" />
                            <div>
                                <p>Ajoutez les <b>prochaines étapes de l'étude à votre agenda</b> pour revenir le moment venu !
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
            sessionStorage.setItem('picture', url ?? '');
        });
    } catch (err) {
        if (err != null)
            Log.error(err);
    }

    await run(true);
}

async function openStudy(project) {
    route.mode = 'study';
    route.project = project.key;

    await run(true);
}

function renderCalendar(events) {
    if (calendar == null) {
        calendar = new SmallCalendar;

        calendar.eventFunc = (start, end) => {
            let period = events.filter(evt => evt.schedule >= start && evt.schedule < end);

            let obj = period.reduce((obj, evt) => {
                let text = evt.count + (evt.count > 1 ? ' modules' : ' module');
                obj[evt.schedule] = [text];

                return obj;
            }, {});

            return obj;
        };
    }

    return calendar.render();
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

    let bundle = project.bundle;

    UI.main(html`
        <div class="tabbar">
            <a class="active">Participer</a>
        </div>

        <div class="tab">
            <div class="box">
                <div class="header">Consentement</div>
                ${bundle.consent}
            </div>
            <div class="box" style="align-items: center;">
                <form @submit=${UI.wrap(e => consent(e, cache.project))}>
                    <label>
                        <input name="consent" type="checkbox" />
                        <span>J’ai lu et je ne m’oppose pas à participer à l’étude ${project.title}</span>
                    </label>
                    <label>
                        <input name="reuse" type="checkbox" />
                        <span>J’accepte que mes données soient réutilisées dans le cadre d’autres études de ${ENV.title}.</span>
                    </label>
                    <div class="actions">
                        <button type="submit">Participer</button>
                    </div>
                </form>
            </div>
        </div>
    `);
}

async function consent(e, project) {
    let start = (new Date).valueOf();

    let form = e.currentTarget;
    let elements = form.elements;

    if (!elements.consent.checked)
        throw new Error('Vous devez confirmé avoir lu et accepté la participation à ' + project.title);

    let reuse = elements.reuse.checked;

    let study = await db.fetch1(`INSERT INTO studies (key, start, reuse)
                                 VALUES (?, ?, ?)
                                 ON CONFLICT DO UPDATE SET start = excluded.start,
                                                           reuse = excluded.reuse
                                 RETURNING id, key, start`,
                                project.key, start, 0 + reuse);

    await initProject(project, study);

    // Go back to dashboard;
    route.mode = 'study';
    route.project = null;

    await run(true);
}

async function runProject() {
    // Sync tests
    cache.tests = await db.fetchAll(`SELECT t.id, t.key, t.status
                                     FROM tests t
                                     INNER JOIN studies s ON (s.id = t.study)
                                     WHERE s.id = ? AND t.visible = 1`, cache.study.id);

    // Prepare page data context
    if (cache.page.type != 'module') {
        let page = cache.page;

        if (ctx?.page != page) {
            let test = cache.tests.find(test => test.key == page.key);

            if (ctx != null)
                ctx.stop();

            switch (page.type) {
                case 'form': { ctx = new FormModule(app, cache.study, page); } break;
                case 'network': { ctx = new NetworkModule(app, cache.study, page); } break;
            }

            await ctx.start();
        }
    } else {
        if (ctx != null)
            ctx.stop();
        ctx = null;
    }

    // Render tab
    {
        let step = cache.page.chain.findLast(it => it.type == 'module' && it.step != null);

        let [progress, total] = computeProgress(step ?? cache.project.root);
        let cls = 'summary ' + (progress == total ? 'done' : 'draft');

        UI.main(html`
            <div class="tabbar">
                <a class="active">Participer</a>
            </div>

            <div class="tab" style="flex: 1;">
                <div class=${cls}>
                    <img src=${cache.project.picture} alt="" />
                    <div>
                        ${step != null ? html`
                            <div class="header">Étude ${cache.project.index} - ${cache.project.title} - ${step.title}</div>
                            ${step.step}
                        ` : ''}
                        ${step == null ? html`
                            <div class="header">Étude ${cache.project.index} - ${cache.project.title}</div>
                            ${cache.project.summary}
                        ` : ''}
                    </div>
                    ${step != null ? progressCircle(progress, total) : ''}
                </div>

                ${cache.page.type == 'module' ? renderModule() : null}
                ${cache.page.type != 'module' ? ctx.render(cache.section) : null}
            </div>
        `);
    }
}

function renderModule() {
    if (UI.isFullscreen)
        UI.toggleFullscreen(false);

    let today = LocalDate.today();
    let page = cache.page;

    return html`
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
                    let [progress, total] = computeProgress(child);

                    let cls = 'module';
                    let status = null;

                    if (progress == total) {
                        cls += ' done';
                        status = 'Terminé';
                    } else if (progress) {
                        cls += ' draft';
                        status = progressBar(progress, total);
                    } else {
                        status = 'À compléter';

                        let earliest = null;

                        for (let it of child.tests) {
                            if (it.status == 'done' || it.schedule == null)
                                continue;

                            if (earliest == null || it.schedule < earliest)
                                earliest = it.schedule;
                        }

                        if (earliest != null) {
                            status = niceDate(earliest, true);

                            if (earliest <= today)
                                cls += (earliest <= today) ? ' draft' : ' empty';
                        } else {
                            cls += ' draft';
                        }
                    }

                    return html`
                        <div class=${cls} @click=${UI.wrap(e => navigateStudy(child))}>
                            <div class="title">${child.title}</div>
                            <div class="status">${status}</div>
                        </div>
                    `;
                })}
                ${!page.modules.length ? page.tests.map(child => {
                    let test = cache.tests.find(test => test.key == child.key);

                    let cls = 'module ' + test.status;
                    let status = null;

                    if (test.status == 'done') {
                        status = 'Terminé';
                    } else if (child.schedule != null) {
                        status = niceDate(child.schedule, true);

                        if (child.schedule <= today)
                            cls += ' draft';
                    } else {
                        status = 'À compléter';
                        cls += ' draft';
                    }

                    return html`
                        <div class=${cls} @click=${UI.wrap(e => navigateStudy(child))}>
                            <div class="title">${child.title}</div>
                            <div class="status">${status}</div>
                        </div>
                    `;
                }) : ''}
            </div>

            ${wrapHelp(page)}
        </div>
    `;
}

function wrapHelp(page) {
    if (!page.help)
        return '';

    let left = (page.chain.length % 2 != 0);

    if (left) {
        return html`
            <div class="help">
                <img src=${ASSETS['pictures/help1']} alt="" />
                <div>${page.help}</div>
            </div>
        `;
    } else {
        return html`
            <div class="help right">
                <div>${page.help}</div>
                <img src=${ASSETS['pictures/help2']} alt="" />
            </div>
        `;
    }
}

async function navigateStudy(page, section = null) {
    while (page.type == 'module' && page.modules.length == 1)
        page = page.modules[1];
    if (page.type == 'module' && page.tests.length == 1)
        page = page.tests[0];

    route.page = page.key;
    route.section = (page != null) ? section : null;

    await run(true);
}

async function saveTest(page, data, status = 'draft') {
    let mtime = (new Date).valueOf();
    let json = JSON.stringify(data);

    await db.exec(`UPDATE tests SET status = ?, mtime = ?, payload = ?
                   WHERE study = ? AND key = ?`, status, mtime, json, cache.study.id, page.key);
}

async function finalizeTest(page, data) {
    await saveTest(page, data, 'done');

    let next = page;

    do {
        next = next.chain[next.chain.length - 2];
    } while (next != cache.project.root && isComplete(next, [page]));

    await navigateStudy(next);
}

function isComplete(page, saved = []) {
    for (let child of page.tests) {
        let test = cache.tests.find(test => test.key == child.key);

        if (test.status != 'done' && !saved.includes(child))
            return false;
    }

    return true;
}

function computeProgress(page) {
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

    if (!(ctx instanceof DiaryModule)) {
        if (ctx != null)
            ctx.stop();

        ctx = new DiaryModule(app);
    }
    await ctx.run();

    UI.main(html`
        <div class="tabbar">
            <a href="/profil">Profil</a>
            <a href=${makeURL()} class="active">Mon journal</a>
        </div>

        <div class="tab">
            ${ctx.render()}
        </div>
    `);
}

// ------------------------------------------------------------------------
// Database
// ------------------------------------------------------------------------

async function initSQLite() {
    let url = BUNDLES['sqlite3-worker1-bundler-friendly.mjs'];
    await sqlite3.init(url);
}

async function openDatabase(filename, key) {
    let db = await sqlite3.open(filename, 'multipleciphers-opfs');

    let sql = `
        PRAGMA cipher = 'sqlcipher';
        PRAGMA key = "x'${Hex.toHex(key)}'";
    `;
    await db.exec(sql);

    let version = await db.pluck('PRAGMA user_version');

    if (version == DATABASE_VERSION)
        return db;
    if (version > DATABASE_VERSION)
        throw new Error('Database model is too recent');

    await db.transaction(async () => {
        switch (version) {
            case 0: {
                await db.exec(`
                    CREATE TABLE meta (
                        gender TEXT,
                        picture TEXT,
                        avatar TEXT
                    );

                    CREATE TABLE studies (
                        id INTEGER PRIMARY KEY,
                        key TEXT NOT NULL,
                        start INTEGER NOT NULL
                    );
                    CREATE UNIQUE INDEX studies_k ON studies (key);

                    CREATE TABLE tests (
                        id INTEGER PRIMARY KEY,
                        study INTEGER REFERENCES studies (id) ON DELETE CASCADE,
                        key TEXT NOT NULL,
                        title TEXT NOT NULL,
                        visible INTEGER CHECK (visible IN (0, 1)) NOT NULL,
                        status TEXT CHECK (status IN ('empty', 'draft', 'done')) NOT NULL,
                        schedule TEXT,
                        mtime INTEGER,
                        payload BLOB
                    );
                    CREATE UNIQUE INDEX tests_sk ON tests (study, key);

                    CREATE TABLE events (
                        test INTEGER NOT NULL REFERENCES tests (id) ON DELETE CASCADE,
                        sequence INTEGER NOT NULL,
                        timestamp INTEGER NOT NULL,
                        type TEXT NOT NULL,
                        data TEXT
                    );

                    CREATE TABLE snapshots (
                        test INTEGER NOT NULL REFERENCES tests (id) ON DELETE CASCADE,
                        timestamp NOT NULL,
                        image BLOB NOT NULL
                    );
                `);
            } // fallthrough

            case 1: {
                await db.exec(`
                    DROP TABLE events;
                    DROP TABLE snapshots;

                    PRAGMA foreign_keys = 0;

                    DROP INDEX studies_k;
                    ALTER TABLE studies RENAME TO studies_BAK;

                    CREATE TABLE studies (
                        id INTEGER PRIMARY KEY,
                        key TEXT NOT NULL,
                        start INTEGER NOT NULL,
                        reuse INTEGER CHECK (reuse IN (0, 1)) NOT NULL
                    );
                    CREATE UNIQUE INDEX studies_k ON studies (key);

                    INSERT INTO studies (id, key, start, reuse)
                        SELECT id, key, start, 0 FROM studies_BAK;

                    DROP TABLE studies_BAK;

                    PRAGMA foreign_keys = 1;
                `);
            } // fallthrough

            case 2: {
                await db.exec(`
                    CREATE TABLE diary (
                        id INTEGER PRIMARY KEY,
                        date TEXT NOT NULL,
                        title TEXT,
                        content TEXT NOT NULL
                    );
                `);
            } // fallthrough
        }

        await db.exec('PRAGMA user_version = ' + DATABASE_VERSION);
    });

    return db;
}

async function exportDatabase(filename) {
    let name = (new Date).toLocaleDateString();

    let root = await navigator.storage.getDirectory();
    let handle = await root.getFileHandle(filename);
    let src = await handle.getFile();

    if (window.showSaveFilePicker != null) {
        let dest = null;

        try {
            dest = await window.showSaveFilePicker({ suggestedName: name + '.db' });
        } catch (err) {
            if (err.name == 'AbortError')
                return;
            throw err;
        }

        try {
            let title = `Export de ${name}`;
            await copyWithProgress(title, src, dest);
        } catch (err) {
            dest.remove();
            throw err;
        }
    } else {
        Util.saveFile(src, name + '.db');
    }
}

async function copyWithProgress(title, src, dest) {
    let writer = await dest.createWritable();

    let controller = new AbortController;
    let progress = null;

    let dialog = UI.dialog({
        run: (render, close) => {
            return html`
                <div class="tabbar">
                    <a class="active">${title}</a>
                </div>

                <div class="tab">
                    <div class="box">
                        <progress style="width: 100%;" max=${src.size} value="0"
                                  ${ref(el => { progress = el ?? progress; })} />
                    </div>

                    <div class="actions">
                        <button type="button" class="secondary" @click=${UI.insist(e => { controller.abort(`L'utilisateur a annulé`); close(); })}>${T.cancel}</button>
                    </div>
                </div>
            `;
        }
    });

    try {
        let copied = 0;
        let through = new TransformStream({
            transform(chunk, controller) {
                copied += chunk.length;
                controller.enqueue(chunk);

                if (progress != null)
                    progress.value = copied;
            }
        });

        await src.stream().pipeThrough(through).pipeTo(writer, { signal: controller.signal });
    } catch (err) {
        throw err;
    } finally {
        dialog.close();
    }
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
            <img src=${ASSETS['ui/3114']} width="348" height="86" alt="">
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

    navigateStudy,
    saveTest,
    finalizeTest
}
