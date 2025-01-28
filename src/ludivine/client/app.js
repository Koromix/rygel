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
import { Util, Log, Net, LocalDate } from '../../web/core/base.js';
import * as UI from '../../web/flat/ui.js';
import * as sqlite3 from '../../web/core/sqlite3.js';
import { computeAge, dateToString, progressCircle } from './lib/util.js';
import { SmallCalendar } from './lib/calendar.js';
import { PictureCropper } from './lib/picture.js';
import { StudyModule } from './study/study.js';
import { NetworkModule } from './network/network.js';
import { TrackModule } from './track/track.js';
import { ASSETS } from '../assets/assets.js';
import { deploy, sos } from '../assets/shared/ldv.js';

import '../assets/app/app.css';

const DATABASE_VERSION = 1;

const PROJECTS = [
    {
        index: 1,
        key: 'sociotrauma',
        title: 'SocioTrauma',
        picture: ASSETS['web/illustrations/sociotrauma'],

        prepare: async (db, project, study) => {
            let url = BUNDLES['sociotrauma.js'];
            let { init } = await import(url);

            return new StudyModule(db, project, init, study);
        }
    },

    {
        index: 2,
        key: 'calypsoT',
        title: 'CALYPSO',
        picture: ASSETS['web/illustrations/calypso'],

        prepare: null
    }
];

Object.assign(T, {
    browse_for_image: 'Parcourir',
    cancel: 'Annuler',
    clear_picture: 'Effacer l\'image',
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

let db = null;

let identity = {
    picture: null
};

let root_el = null;
let main_el = null;

let active_mod = null;

let studies = [];
let events = [];

let calendar = null;

async function start(root) {
    Log.pushHandler(UI.notifyHandler);

    // Adapt to viewport
    window.addEventListener('resize', adaptToViewport);
    adaptToViewport();

    // Prevent unsaved data loss
    window.onbeforeunload = () => {
        if (active_mod != null && active_mod.hasUnsavedData())
            return 'Si vous confirmez vouloir quitter la page, les modifications en cours seront perdues !';
    };

    // Open database
    {
        let session = null;

        try {
            let json = sessionStorage.getItem('session');
            session = JSON.parse(json);
        } catch (err) {
            console.error(err);
        }

        if (window.location.hash) {
            let hash = window.location.hash.substr(1);
            let query = new URLSearchParams(hash);

            let uuid = query.get('uuid');
            let mkey = query.get('mk');

            if (uuid && mkey) {
                session = {
                    uuid: uuid,
                    mkey: mkey
                };

                let json = JSON.stringify(session);
                sessionStorage.setItem('session', json);
            }

            let url = window.location.pathname + window.location.search;
            history.replaceState('', document.title, url);
        }

        if (session?.uuid != null && session?.mkey != null) {
            await initSQLite();

            let db_filename = 'ludivine/' + session.uuid + '.db';
            db = await openDatabase(db_filename, 'c');
        }
    }

    root_el = root;
    main_el = document.createElement('main');

    if (db == null) {
        await runRegister();
    } else {
        let row = await db.fetch1('SELECT picture FROM meta');

        if (row != null) {
            Object.assign(identity, row);
        } else {
            await db.exec('INSERT INTO meta (gender) VALUES (NULL)');
        }

        await runDashboard();
    }

    renderFull();
    document.body.classList.remove('loading');
}

function renderFull() {
    render(html`
        <div id="deploy" @click=${deploy}></div>

        <nav id="top">
            <menu>
                <a id="logo" href=${ENV.urls.static}><img src=${ASSETS['logo']} alt="Logo Lignes de Vie" /></a>
                <li><a href=${ENV.urls.static} style="margin-left: 0em;">Accueil</a></li>
                <li><a href=${ENV.urls.app} class="active" style="margin-left: 0em;">Participer</a></li>
                <li><a href=${ENV.urls.static + '/etudes'} style="margin-left: 0em;">Études</a></li>
                <li><a href=${ENV.urls.static + '/livres'} style="margin-left: 0em;">Ressources</a></li>
                <li><a href=${ENV.urls.static + '/detente'} style="margin-left: 0em;">Se détendre</a></li>
                <li><a href=${ENV.urls.static + '/equipe'} style="margin-left: 0em;">Qui sommes-nous ?</a></li>
                <div style="flex: 1;"></div>
                <img class="picture" src=${identity.picture ?? ASSETS['app/main/user']} alt="" />
            </menu>
        </nav>

        ${main_el}

        <footer>
            <div>Lignes de Vie © 2024</div>
            <img src=${ASSETS['footer']} alt="" width="79" height="64">
            <div style="font-size: 0.8em;">
                Cn2r, 103 Bld de la liberté, 59000 LILLE<br>
                <a href="mailto:lignesdevie@cn2r.fr" style="font-weight: bold; color: inherit;">lignesdevie@cn2r.fr</a>
            </div>
        </footer>

        <a id="sos" @click=${UI.wrap(e => sos(event))}>SOS</a>
    `, root_el);
}

function adaptToViewport() {
    let is_small = (window.innerWidth < 960);
    let is_medium = (window.innerWidth < 1280);

    document.documentElement.classList.toggle('small', is_small);
    document.documentElement.classList.toggle('medium', is_medium);
}

async function initSQLite() {
    let url = BUNDLES['sqlite3-worker1-bundler-friendly.mjs'];
    await sqlite3.init(url);
}

async function openDatabase(filename, flags) {
    let db = await sqlite3.open(filename, flags);
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

async function runDashboard() {
    // List active studies
    studies = await db.fetchAll(`SELECT s.id, s.key, s.start,
                                        COUNT(t1.id) AS progress, COUNT(t2.id) AS total
                                 FROM studies s
                                 LEFT JOIN tests t1 ON (t1.study = s.id AND t1.visible = 1 AND t1.status = 'done')
                                 LEFT JOIN tests t2 ON (t2.study = s.id AND t2.visible = 1)
                                 GROUP BY s.id`);

    // List future events
    {
        let start = LocalDate.today().minusMonths(1);

        events = await db.fetchAll(`SELECT schedule, COUNT(id) AS count
                                    FROM tests
                                    WHERE schedule >= ? AND status <> 'done'
                                    GROUP BY schedule, study
                                    ORDER BY schedule, id`, start.toString());

        for (let evt of events)
            evt.schedule = LocalDate.parse(evt.schedule);
    }

    render(html`
        <div class="tabbar">
            <a class="active">Tableau de bord</a>
        </div>

        <div class="tab">
            <div class="box profile">
                <img class="picture" src=${identity.picture ?? ASSETS['app/main/user']} alt=""/>
                <div>
                    <p>Bienvenue sur <b>${ENV.title}</b> !

                    <p>Vous pouvez modifier les informations de votre profil saisies précédemment à l’aide du bouton ci-dessous.
                    Par ailleurs, n’oubliez pas que vous pouvez <b>retirer votre consentement</b> à tout moment en vous rendant dans votre profil.
                    <p>N’hésitez pas à <b>faire des pauses</b> et à visiter la page <b>Se Détendre</b>. Vous pourrez reprendre à tout moment.</p>

                    <div class="actions">
                        <button type="button" @click=${UI.wrap(changePicture)}>Modifier mon avatar</button>
                    </div>
                </div>
            </div>

            <div class="row">
                <div class="studies">
                    ${PROJECTS.map(project => {
                        let study = studies.find(study => study.key == project.key);

                        let cls = 'study';
                        if (study != null) {
                            if (study.progress == study.total) {
                                cls += ' complete';
                            } else {
                                cls += ' draft';
                            }
                        }

                        let online = (project.prepare != null);

                        return html`
                            <div class=${cls}>
                                <img src=${project.picture} alt="" />
                                <div class="info">
                                    <b>Étude n°${project.index}</b><br>
                                    ${project.title}
                                </div>
                                <div class=${online ? 'status' : 'status disabled'}>
                                    ${study == null ? progressCircle(0, 0) : ''}
                                    ${study != null ? progressCircle(study.progress, study.total) : ''}
                                    ${study == null && online ?
                                        html`<button type="button" class="small"
                                                     @click=${UI.insist(e => openStudy(project))}>Participer</button>` : ''}
                                    ${study == null && !online ?
                                        html`<button type="button" class="small" disabled>Prochainement</button>` : ''}
                                    ${study != null ?
                                        html`<button type="button" class="small"
                                                     @click=${UI.wrap(e => openStudy(project))}>Reprendre</button>` : ''}
                                    ${study != null && study.progress == study.total ?
                                        html`<button type="button" class="small" disabled>Résultats</button>` : ''}
                                </div>
                            </div>
                        `;
                    })}
                </div>

                <div class="column">
                    <div class="box">
                        <div class="title">À venir</div>
                        ${events.map(evt => {
                            let date = html`
                                ${evt.schedule.day}<br>
                                ${T.months[evt.schedule.month].substr(0, 3)}
                            `;

                            return html`
                                <div class="event">
                                    <div class="date">${date}</div>
                                    <div class="text">${evt.count} ${evt.count > 1 ? 'questionnaires' : 'questionnaire'}</div>
                                    <button type="button"><img src=${ASSETS['app/main/calendar']} alt="Agenda" /></button>
                                </div>
                            `;
                        })}
                        ${!events.length ? html`<p style="text-align: center;">Aucun évènement à venir</p>` : ''}
                    </div>

                    <div class="box">
                        <div class="title">Calendrier</div>
                        ${renderCalendar()}
                    </div>
                </div>
            </div>
        </div>
    `, main_el);
}

function renderCalendar() {
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

async function runRegister() {
    render(html`
        <div class="tabbar">
            <a class="active">Enregistrez-vous</a>
        </div>

        <div class="tab">
            <div class="box" style="align-items: center;">
                <div class="title">Enregistrez-vous pour continuer</div>

                <form @submit=${UI.wrap(register)}>
                    <input type="text" name="email" placeholder="adresse email" />
                    <button type="submit">Enregistrer mon compte</button>
                </form>

                <div class="help">
                    <img src=${ASSETS['web/illustrations/participer']} alt="" />
                    <div>
                        <p><b>Créer un compte pour participer</b> à nos études est essentiel pour :
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
    `, main_el);
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

async function changePicture() {
    let meta = await db.fetch1('SELECT picture, avatar FROM meta');
    let settings = (meta.avatar != null) ? JSON.parse(meta.avatar) : meta.picture;

    let cropper = new PictureCropper('Modifier mon avatar', 256);

    let url = BUNDLES['notion.json'];
    let notion = await Net.get(url);

    cropper.defaultURL = ASSETS['app/main/user'];
    cropper.notionAssets = notion;
    cropper.imageFormat = 'image/webp';

    await cropper.run(settings, async (blob, settings) => {
        if (blob == meta.picture)
            return;

        let url = await blobToDataURL(blob);

        if (settings != null)
            settings = JSON.stringify(settings);
        await db.exec('UPDATE meta SET picture = ?, avatar = ?', url, settings);

        identity.picture = url;
    });

    renderFull();
    runDashboard();
}

async function blobToDataURL(blob) {
    if (blob == null)
        return null;

    let url = await new Promise((resolve, reject) => {
        let reader = new FileReader;
        reader.onload = e => resolve(e.target.result);
        reader.readAsDataURL(blob);
    });

    return url;
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
                <div class="title">
                    ${title}
                    <div style="flex: 1;"></div>
                    <button type="button" class="secondary" @click=${UI.wrap(close)}>✖\uFE0E</button>
                </div>

                <div class="main">
                    <progress style="width: 300px;" max=${src.size} value="0"
                              ${ref(el => { progress = el ?? progress; })} />
                </div>

                <div class="footer"></span>
                    <button type="button" class="secondary" @click=${UI.insist(e => { controller.abort(`L'utilisateur a annulé`); close(); })}>${T.cancel}</button>
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

async function openStudy(project) {
    let start = (new Date).valueOf();

    let study = await db.fetch1(`INSERT INTO studies (key, start)
                                 VALUES (?, ?)
                                 ON CONFLICT DO UPDATE SET start = start
                                 RETURNING id, start`,
                                project.key, start);
    let mod = await project.prepare(db, project, study);

    await mod.start(main_el);
}

export {
    start,
    runDashboard
}
