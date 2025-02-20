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
import * as UI from './ui.js';
import { SmallCalendar } from './lib/calendar.js';
import { PictureCropper } from './lib/picture.js';
import { PROJECTS } from '../projects/projects.js';
import { ProjectInfo, ProjectBuilder } from './project.js';
import { FormModule } from './form/form.js';
import { NetworkModule } from './network/network.js';
import { ASSETS } from '../assets/assets.js';

import '../assets/client.css';

const DATABASE_VERSION = 1;

Object.assign(T, {
    browse_for_image: 'Parcourir',
    cancel: 'Annuler',
    clear_picture: 'Effacer l\'image',
    confirm: 'Confirmer',
    confirm_not_reversible: 'Attention, cette action n\'est pas réversible !',
    edit: 'Modifier',
    drag_paste_or_browse_image: 'Déposez une image, copier-collez la ou utilisez le bouton « Parcourir » ci-dessous',
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

let route = {
    mode: null,

    // study mode
    project: null,
    path: null,
    section: null
};
let route_url = null;
let poisoned = false;

let db = null;
let identity = null;

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
    UI.init(run);

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
    let session;
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

    window.onbeforeunload = null;
    window.location.href = ENV.urls.static;

    poisoned = true;
}

// ------------------------------------------------------------------------
// Run
// ------------------------------------------------------------------------

async function go(url = null, push = true) {
    if (url != null) {
        if (!(url instanceof URL))
            url = new URL(url, window.location.href);

        let parts = url.pathname.slice(1).split('/');
        let mode = parts.shift();

        switch (mode) {
            case 'profile': { route.mode = 'profile'; } break;

            case 'study': {
                route.mode = 'study';

                let project = PROJECTS.find(project => project.key == parts[0]);

                if (project != null) {
                    route.mode = 'study';
                    route.project = parts.shift();

                    if (parts.length > 0) {
                        let last = parts.pop();

                        route.path = '/' + parts.join('/');

                        if (last.startsWith('-')) {
                            route.section = last.substr(1);

                            if (route.section == 'edit')
                                route.section = 0;
                        } else {
                            route.path += '/' + last;
                            route.section = null;
                        }
                    }
                } else {
                    route.project = null;
                    route.path = null;
                    route.section = null;
                }
            } break;

            default: { route.mode = 'profile'; } break;
        }
    }

    await run(push);
}

async function run(push = true) {
    if (poisoned)
        return;

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

        cache.events = await db.fetchAll(`SELECT schedule, COUNT(id) AS count
                                          FROM tests
                                          WHERE schedule >= ? AND status <> 'done'
                                          GROUP BY schedule, study
                                          ORDER BY schedule, id`, start.toString());

        for (let evt of cache.events)
            evt.schedule = LocalDate.parse(evt.schedule);
    }

    // Sync state with expected route
    if (route.mode == 'study') {
        let project = PROJECTS.find(project => project.key == route.project);

        if (project != null) {
            if (project.key != cache.project?.key || project == cache.project) {
                if (typeof project.bundle == 'string')
                    project.bundle = await import(project.bundle);

                if (cache.study == null) {
                    let study = cache.studies.find(study => study.key == project.key);

                    if (study != null)
                        project = await initProject(project, study);

                    cache.study = study;
                }

                cache.project = project;
            }

            if (cache.study != null) {
                cache.page = cache.project.pages.find(page => page.key == route.path) ?? cache.project.root;
                cache.section = route.section;
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
    }

    // Update route values
    route.project = cache.project?.key;
    route.path = cache.page?.key;
    route.section = cache.section;

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

    path += route.mode;

    if (route.mode == 'study' && route.project != null) {
        path += '/' + route.project;
        if (route.path != null)
            path += route.path;
        if (route.section === 0) {
            path += '/-edit';
        } else if (route.section != null) {
            path += '/-' + route.section;
        }
    }

    return path;
}

async function initProject(project, study) {
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
    UI.main(html`
        <div class="tabbar">
            <a class="active">Enregistrez-vous</a>
        </div>

        <div class="tab">
            <div class="box" style="align-items: center;">
                <div class="header">Enregistrez-vous pour continuer</div>

                <form style="text-align: center;" @submit=${UI.wrap(register)}>
                    <input type="text" name="email" style="width: 20em;" placeholder="adresse email" />
                    <div class="actions">
                        <button type="submit">Enregistrer mon compte</button>
                    </div>
                </form>

                <div class="help">
                    <img src=${ASSETS['pictures/help1']} alt="" />
                    <div>
                        <p><b>Créer un compte</b> pour participer à nos études permet de :
                        <ul>
                            <li>Garantir la <b>protection de vos données</b>
                            <li>Faciliter le <b>suivi de votre progression</b>
                            <li>Assurer la <b>fiabilité des données</b> recueillies
                        </ul>
                        <p>Vous n’avez <b>pas encore de compte</b> ? Pas de souci !
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
    UI.main(html`
        <div class="tabbar">
            <a href="/profile" class="active">Profil</a>
            <a href="/study">Tableau de bord</a>
        </div>

        <div class="tab">
            <div class="row">
                <div class="box profile">
                    <img class="avatar" src=${identity?.picture ?? ASSETS['ui/user']} alt=""/>

                    <div class="actions">
                        <button type="button" @click=${UI.wrap(changePicture)}>Modifier mon avatar</button>
                        <button type="button" class="secondary" @click=${UI.insist(logout)}>Se déconnecter</button>
                    </div>
                </div>

                <div class="box">
                    <div class="header">Bienvenue sur <b>${ENV.title}</b> !</div>

                    <p>Vous pouvez modifier les informations de votre profil saisies précédemment à l’aide du bouton ci-dessous.
                    Par ailleurs, n’oubliez pas que vous pouvez <b>retirer votre consentement</b> à tout moment en vous rendant dans votre profil.
                    <p>N’hésitez pas à <b>faire des pauses</b> et à visiter la page <b>Se Détendre</b>. Vous pourrez reprendre à tout moment.</p>

                    <div class="actions">
                        <a href="/study">Accéder aux études</a>
                    </div>
                </div>
            </div>
        </div>
    `);
}

async function runDashboard() {
    UI.main(html`
        <div class="tabbar">
            <a href="/profile">Profil</a>
            <a href="/study" class="active">Tableau de bord</a>
        </div>

        <div class="tab">
            <div class="box">
                <div class="columns">
                    <div style="flex: 4;">
                        <div class="header">Bienvenue sur <b>${ENV.title}</b> !</div>

                        <p>Vous pouvez modifier les informations de votre profil saisies précédemment à l’aide du bouton ci-dessous.
                        Par ailleurs, n’oubliez pas que vous pouvez <b>retirer votre consentement</b> à tout moment en vous rendant dans votre profil.
                        <p>N’hésitez pas à <b>faire des pauses</b> et à visiter la page <b>Se Détendre</b>. Vous pourrez reprendre à tout moment.</p>
                    </div>
                    <img src=${ASSETS['pictures/kezako']} style="flex: 1;" alt="" />
                </div>
            </div>

            <div class="row">
                <div class="studies">
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
                                    ${project.title}
                                </div>
                                <div class=${online ? 'status' : 'status disabled'}>
                                    ${study != null ? progressCircle(study.progress, study.total) : ''}
                                    ${study == null && online ?
                                        html`<button type="button" class="small"
                                                     @click=${UI.wrap(e => openStudy(project))}>Participer</button>` : ''}
                                    ${study == null && !online ?
                                        html`<button type="button" class="small" disabled>Prochainement</button>` : ''}
                                    ${study != null && study.progress < study.total ?
                                        html`<button type="button" class="small"
                                                     @click=${UI.wrap(e => openStudy(project))}>Continuer</button>` : ''}
                                    ${study != null && study.progress == study.total ?
                                        html`<button type="button" class="small" disabled>Résultats</button>` : ''}
                                </div>
                            </div>
                        `;
                    })}
                </div>

                <div class="column">
                    <div class="box">
                        <div class="header">À venir</div>
                        ${cache.events.map(evt => {
                            return html`
                                <div class="event">
                                    <div class="date">${niceDate(evt.schedule, false)}</div>
                                    <div class="text">${evt.count} ${evt.count > 1 ? 'questionnaires' : 'questionnaire'}</div>
                                    <button type="button"><img src=${ASSETS['ui/calendar']} alt="Agenda" /></button>
                                </div>
                            `;
                        })}
                        ${!cache.events.length ? html`<p style="text-align: center;">Aucun évènement à venir</p>` : ''}
                    </div>

                    <div class="box">
                        <div class="header">Calendrier</div>
                        ${renderCalendar(cache.events)}
                    </div>
                </div>
            </div>
        </div>
    `);
}

function isLogged() {
    let logged = (db != null);
    return logged;
}

async function changePicture() {
    let meta = await db.fetch1('SELECT picture, avatar FROM meta');
    let settings = (meta.avatar != null) ? JSON.parse(meta.avatar) : meta.picture;

    let cropper = new PictureCropper('Modifier mon avatar', 256);

    let url = BUNDLES['notion.json'];
    let notion = await Net.get(url);

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
        });
    } catch (err) {
        if (err != null)
            Log.error(err);
    }

    run();
}

async function openStudy(project) {
    route.mode = 'study';
    route.project = project.key;

    await run();
}

function renderCalendar(events) {
    if (calendar == null) {
        calendar = new SmallCalendar;

        calendar.eventFunc = (start, end) => {
            let period = events.filter(evt => evt.schedule >= start && evt.schedule < end);

            let obj = period.reduce((obj, evt) => {
                let text = evt.count + (evt.count > 1 ? ' questionnaires' : ' questionnaire');
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
    let project = cache.project;
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

    await db.fetch1(`INSERT INTO studies (key, start)
                     VALUES (?, ?)
                     ON CONFLICT DO UPDATE SET start = excluded.start
                     RETURNING id, start`, project.key, start);

    await run();
}

async function runProject() {
    // Sync tests
    cache.tests = await db.fetchAll(`SELECT t.id, t.key, t.status
                                     FROM tests t
                                     INNER JOIN studies s ON (s.id = t.study)
                                     WHERE s.id = ?`, cache.study.id);

    // Prepare page data context
    if (cache.page.type != 'module') {
        let page = cache.page;

        if (ctx?.page != page) {
            let test = cache.tests.find(test => test.key == page.key);

            ctx = {
                page: page,
                mod: null
            };

            switch (page.type) {
                case 'form': { ctx.mod = new FormModule(db, cache.study, page); } break;
                case 'network': { ctx.mod = new NetworkModule(db, cache.study, page); } break;
            }

            await ctx.mod.start();
        }
    } else {
        ctx = null;
    }

    // Render tab
    {
        let [progress, total] = computeProgress(cache.project.root);
        let cls = 'summary ' + (progress == total ? 'done' : 'draft');

        UI.main(html`
            <div class="tabbar">
                <a class="active">Participer</a>
            </div>

            <div class="tab" style="flex: 1;">
                <div class=${cls}>
                    <img src=${cache.project.picture} alt="" />
                    <div>
                        <div class="header">Étude ${cache.project.index} - ${cache.project.title}</div>
                        ${cache.project.summary}
                    </div>
                    ${progressCircle(progress, total)}
                </div>

                ${cache.page.type == 'module' ? renderModule() : null}
                ${cache.page.type != 'module' && cache.section == null ? renderStart() : null}
                ${cache.page.type != 'module' && cache.section != null ? ctx.mod.render(route.section) : null}
            </div>
        `);
    }
}

function renderModule() {
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
                    }

                    return html`
                        <div class=${cls} @click=${UI.wrap(e => navigateStudy(child))}>
                            <div class="title">${child.title}</div>
                            <div class="status">${status}</div>
                        </div>
                    `;
                }) : ''}
            </div>

            ${page.help ?? ''}
        </div>
    `;
}

function renderStart() {
    let page = cache.page;

    return html`
        ${Util.mapRange(0, page.chain.length - 2, idx => {
            let parent = page.chain[idx];
            let next = page.chain[idx + 1];

            return html`
                <div class="box level" @click=${UI.wrap(e => navigateStudy(parent))}>
                    ${parent.level ?? ''}${parent.level ? ' - ' : ''}
                    ${next.title}
                </div>
            `;
        })}
        <div class="box level" @click=${UI.wrap(e => navigateStudy(page))}>
            Questionnaire - ${page.title}
        </div>
        <div class="start">
            <div class="help">
                <img src=${ASSETS['pictures/help1']} alt="" />
                <div>
                    <p>Tout est prêt pour <b>commencer le questionnaire</b> !
                    <p>Pensez à <b>faire des pauses</b> si vous en ressentez le besoin, ou à faire un tour sur la page Se Détendre.
                    <p>Si vous êtes prêt, <b>on peut y aller</b> !
                </div>
            </div>
            <button type="button" @click=${UI.wrap(e => navigateStudy(page, 0))}>Commencer !</button>
        </div>
    `;
}

async function navigateStudy(page, section = null) {
    while (page.type == 'module' && page.modules.length == 1)
        page = page.modules[1];
    if (page.type == 'module' && page.tests.length == 1)
        page = page.tests[0];

    route.path = page.key;
    route.section = (page != null) ? section : null;

    await run();
}

async function saveTest(page, data) {
    let mtime = (new Date).valueOf();
    let json = JSON.stringify(data);

    await db.exec(`UPDATE tests SET status = 'done', mtime = ?, payload = ?
                   WHERE study = ? AND key = ?`, mtime, json, cache.study.id, page.key);

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
// Database
// ------------------------------------------------------------------------

async function initSQLite() {
    let url = BUNDLES['sqlite3-worker1-bundler-friendly.mjs'];
    await sqlite3.init(url);
}

async function openDatabase(filename, key) {
    let db = await sqlite3.open(filename, 'multipleciphers-opfs');

    await db.exec(`
        PRAGMA cipher = 'sqlcipher';
        PRAGMA key = 'raw:${Hex.toHex(key)}'
    `);

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
        }

        await db.exec('PRAGMA user_version = ' + DATABASE_VERSION);
    });

    return db;
}

async function exportDatabase() {
    let name = (new Date).toLocaleDateString();

    let root = await navigator.storage.getDirectory();
    let handle = await root.getFileHandle(DATABASE_FILENAME);
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

async function deleteDatabase() {
    await db.close();

    let root = await navigator.storage.getDirectory();
    root.removeEntry(DATABASE_FILENAME);

    window.onbeforeunload = null;
    window.location.href = window.location.href;
}

export {
    identity,

    start,

    run,

    isLogged,
    changePicture,

    navigateStudy,
    saveTest
}
