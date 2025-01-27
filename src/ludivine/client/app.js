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
import { SmallCalendar } from '../../web/widgets/smallcalendar.js';
import { GENDERS } from './lib/constants.js';
import { computeAge, dateToString, renderProgress } from './lib/util.js';
import { PictureCropper } from './lib/picture.js';
import { NetworkModule } from './network/network.js';
import { TrackModule } from './track/track.js';
import { ASSETS } from '../assets/assets.js';
import { sos } from '../assets/shared/ldv.js';

import '../assets/app/app.css';

const DATABASE_FILENAME = 'LDV4.db';
const DATABASE_VERSION = 1;

const PROJECTS = [
    {
        index: 1,
        key: 'sociotrauma',
        title: 'SocioTrauma',
        online: true
    },

    {
        index: 2,
        key: 'calypsoT',
        title: 'CALYPSO',
        online: false
    }
];

const MODULES = [
    {
        key: 'network',
        title: 'Sociogramme',
        icon: 'app/main/network',
        historical: true,
        editable: true,

        prepare: (db, test, el) => new NetworkModule(db, test, el)
    },

    {
        key: 'arnaud',
        title: 'EyeTrack',
        icon: 'app/main/track',
        historical: false,
        editable: false,

        prepare: async (db, test, el) => {
            let url = BUNDLES['arnaud.js'];
            let experiment = await import(url);

            return new TrackModule(db, test, el, experiment);
        },
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
    accessories: 'Accessoires'
});

let db = null;

let identity = {
    name: '',
    birthdate: null,
    gender: null,
    picture: null
};

let root_el = null;
let active_mod = null;

let calendar = new SmallCalendar;

async function start(root) {
    Log.pushHandler(UI.notifyHandler);

    // Adapt to viewport
    window.addEventListener('resize', adaptToViewport);
    adaptToViewport();

    await initSQLite();
    db = await openDatabase(DATABASE_FILENAME, 'c');

    document.body.classList.remove('loading');

    // Load identity
    {
        let row = await db.fetch1('SELECT name, birthdate, gender, picture FROM meta');

        if (row != null) {
            Object.assign(identity, row);
        } else {
            await changeIdentity();
        }
    }

    window.onbeforeunload = () => {
        if (active_mod?.hasUnsavedData())
            return 'Si vous confirmez vouloir quitter la page, les modifications en cours seront perdues !';
    };

    root_el = root;
    await runDashboard();
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
                        name TEXT NOT NULL,
                        gender TEXT NOT NULL,
                        birthdate INTEGER NOT NULL,
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

async function changeIdentity() {
    let init = !identity.name;

    let time = identity.birthdate;
    let age = null;

    await UI.dialog({
        can_be_closed: !init,

        run: (render, close) => {
            let now = (new Date).valueOf();
            let new_age = (time != null) ? computeAge(time, now) : null;

            if (age == null || new_age >= 0)
                age = new_age;

            return html`
                <div class="title">
                    ${init ? 'Créer mon compte' : 'Modifier mon identité'}
                    ${!init ? html`
                        <div style="flex: 1;"></div>
                        <button type="button" class="secondary" @click=${UI.wrap(close)}>✖\uFE0E</button>
                    ` : ''}
                </div>

                <div class="main">
                    <label>
                        <span>Identité</span>
                        <input name="name" value=${identity.name} />
                    </label>
                    <label>
                        <span>Genre</span>
                        ${Object.keys(GENDERS).map(gender => {
                            let info = GENDERS[gender];
                            return html`<label><input name="gender" type="radio" value=${gender} ?checked=${gender == identity.gender}>${info.text}</label>`;
                        })}
                    </label>

                    <label>
                        <span>Date de naissance</span>
                        <input name="birthdate" type="date" value=${dateToString(identity.birthdate)}
                               @input=${e => { time = e.target.valueAsDate.valueOf(); render(); }} />
                    </label>
                    <label>
                        <span>Âge</span>
                        <input readonly value=${(age != null) ? (age + (age > 1 ? ' ans' : ' an')) : ''} />
                    </label>
                </div>

                <div class="footer">
                    ${init ? html`
                        <div style="flex: 1;"></div>
                        <button type="submit">Créer</button>
                        <div style="flex: 1;"></div>
                    ` : ''}
                    ${!init ? html`
                        <div style="flex: 1;"></div>
                        <button type="button" class="secondary" @click=${UI.insist(close)}>Annuler</button>
                        <button type="submit">Modifier</button>
                    ` : ''}
                </div>
            `;
        },

        submit: async (elements) => {
            if (!elements.name.value.trim())
                throw new Error('Le nom ne peut pas être vide');
            if (!elements.birthdate.value)
                throw new Error('La date de naissance ne peut pas être vide');

            let changes = {
                name: elements.name.value.trim(),
                gender: elements.gender.value,
                birthdate: elements.birthdate.valueAsDate.valueOf()
            };

            await db.transaction(async () => {
                await db.exec('DELETE FROM meta');

                await db.exec('INSERT INTO meta (name, gender, birthdate) VALUES (?, ?, ?)',
                              changes.name, changes.gender, changes.birthdate);
            });

            Object.assign(identity, changes);
        }
    });
}

async function runDashboard() {
    stopTest();

    let studies = await db.fetchAll(`SELECT s.id, s.key, s.start,
                                            COUNT(t1.id) AS progress, COUNT(t2.id) AS total
                                     FROM studies s
                                     LEFT JOIN tests t1 ON (t1.study = s.id AND t1.visible = 1 AND t1.status = 'done')
                                     LEFT JOIN tests t2 ON (t2.study = s.id AND t2.visible = 1)
                                     GROUP BY s.id`);
    let tests = await db.fetchAll('SELECT id, key, title, mtime FROM tests WHERE study IS NULL ORDER BY id');

    let now = (new Date).valueOf();
    let age = computeAge(identity.birthdate, now);

    render(html`
        <nav id="top">
            <menu>
                <a id="logo" href="https://ldv-recherche.fr/"><img src=${ASSETS['logo']} alt="Logo Lignes de Vie" /></a>
                <div style="flex: 1;"></div>
                <img class="picture" src=${identity.picture ?? ASSETS['app/main/user']} alt="" />
            </menu>
        </nav>

       <main>
            <div class="tabbar">
                <a class="active">Profil</a>
                <a>Tableau de bord</a>
            </div>

            <div class="tab dashboard">
                <div class="column">
                    <div class="box profile">
                        <div class="title">${identity.name} (${age} ${age > 1 ? 'ans' : 'an'})</div>
                        <img class="picture" src=${identity.picture ?? ASSETS['app/main/user']} alt=""/>
                        <button type="button"
                                @click=${UI.wrap(e => changeIdentity().then(runDashboard))}>Modifier mon identité</button>
                        <button type="button"
                                @click=${UI.wrap(e => changePicture().then(runDashboard))}>Modifier mon avatar</button>
                    </div>

                    <div class="box">
                        <button type="button"
                                @click=${UI.insist(e => exportDatabase(identity.name))}>Exporter mes données</button>
                        <button type="button" class="danger"
                                @click=${UI.confirm('Suppression définitive de toutes les données', deleteDatabase)}>Supprimer mon profil</button>
                    </div>
                </div>

                <div class="column">
                    <div class="box">
                        <div class="title">Études</div>
                        <div class="studies">
                            ${PROJECTS.map(project => {
                                let study = studies.find(study => study.key == project.key);

                                return html`
                                    <div class="study">
                                        <div class="info">
                                            <b>Étude n°${project.index}</b><br>
                                            ${project.title}
                                        </div>
                                        <div class=${project.online ? 'status' : 'status disabled'}>
                                            ${study != null ? renderProgress(study.progress, study.total) : ''}
                                            ${study == null ? renderProgress(0, 0) : ''}
                                            ${study != null ?
                                                html`<button type="button" class="secondary small"
                                                             @click=${UI.wrap(e => openStudy(project))}>Reprendre</button>` : ''}
                                            ${study == null && project.online ?
                                                html`<button type="button" class="secondary small"
                                                             @click=${UI.insist(e => openStudy(project))}>Participer</button>` : ''}
                                            ${study == null && !project.online ?
                                                html`<button type="button" class="secondary small" disabled>Prochainement</button>` : ''}
                                        </div>
                                    </div>
                                `;
                            })}
                        </div>
                    </div>

                    <div class="row">
                        <div class="box">
                            <div class="title">Calendrier</div>
                            ${calendar.render()}
                        </div>
                        <div class="box" style="flex: 1;">
                            <div class="title">Évènements à venir</div>
                            <p style="text-align: center;">Aucun évènement à venir</p>
                        </div>
                    </div>

                    <div class="box">
                        <div class="title">Tests libres</div>
                        <table style="table-layout: fixed;">
                            <colgroup>
                                <col class="check"/>
                                <col/>
                                <col style="width: 200px;"/>
                                <col style="width: 160px;"/>
                            </colgroup>

                            <thead>
                                <tr>
                                    <th></th>
                                    <th>Titre</th>
                                    <th>Date</th>
                                    <th></th>
                                </tr>
                            </thead>

                            <tbody>
                                ${tests.map(test => {
                                    let mod = MODULES.find(mod => mod.key == test.key);

                                    if (mod == null)
                                        return '';

                                    let img = ASSETS[mod.icon];

                                    return html`
                                        <tr>
                                            <td class="picture" title=${mod.title}><img src=${img} alt=${mod.title}></td>
                                            ${mod.editable ? html`<td><a @click=${UI.wrap(e => openTest(test))}>${test.title}</a></td>` : ''}
                                            ${!mod.editable ? html`<td>${test.title}</td>` : ''}
                                            ${mod.historical ? html`<td>${(new Date(test.ctime)).toLocaleDateString()}</td>` : ''}
                                            ${!mod.historical ? html`<td>${(new Date(test.ctime)).toLocaleString()}</td>` : ''}
                                            <td class="right">
                                                <button type="button" class="secondary small" @click=${UI.wrap(e => changeTest(test).then(runDashboard))}>Configurer</button>
                                                <button type="button" class="danger small" @click=${UI.confirm(`Suppression de ${test.title}`, e => deleteTest(test.id).then(runDashboard))}>Supprimer</button>
                                            </td>
                                        </tr>
                                    `;
                                })}
                                ${!tests.length ? html`<tr><td colspan="4" class="center">Aucun test libre</td></tr>` : ''}
                            </tbody>
                        </table>
                        <div class="actions">
                            ${MODULES.map(mod =>
                                html`<button type="button" @click=${UI.wrap(e => createTest(mod.key))}>Nouveau ${mod.title}</button>`)}
                        </div>
                    </div>
                </div>
            </div>
        </main>

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

async function exportDatabase(name) {
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

    await db.exec(`INSERT INTO studies (key, start)
                   VALUES (?, ?)
                   ON CONFLICT DO NOTHING`,
                  project.key, start);

    Log.info('Bientôt !');
    runDashboard();
}

async function createTest(key) {
    let now = (new Date).valueOf();

    let test = {
        id: null,
        key: key,
        title: '',
        mtime: now
    };

    await changeTest(test);
    await openTest(test);
}

async function changeTest(test) {
    let init = (test.id == null);
    let mod = MODULES.find(mod => mod.key == test.key);

    await UI.dialog({
        run: (render, close) => {
            return html`
                <div class="title">
                    ${init ? 'Nouveau' : 'Modification de'} ${mod.title}
                    <div style="flex: 1;"></div>
                    <button type="button" class="secondary" @click=${UI.wrap(close)}>✖\uFE0E</button>
                </div>

                <div class="main">
                    <label>
                        <span>Titre</span>
                        <input name="title" value=${test.title} />
                    </label>
                    ${mod.historical ? html`
                        <label>
                            <span>Date de l'évènement</span>
                            <input name="date" type="date" value=${dateToString(test.date)} />
                        </label>
                    ` : ''}
                </div>

                <div class="footer">
                    <div style="flex: 1;"></div>
                    <button type="button" class="secondary" @click=${UI.insist(close)}>Annuler</button>
                    <button type="submit">${init ? 'Créer' : 'Modifier'}</button>
                </div>
            `;
        },

        submit: async (elements) => {
            if (!elements.title.value.trim())
                throw new Error('Le titre ne peut pas être vide');
            if (mod.historical && !elements.date.value)
                throw new Error('La date ne peut pas être vide');

            let title = elements.title.value.trim();
            let mtime = (mod.historical ? elements.date.valueAsDate : new Date).valueOf();

            let ret = await db.fetch1(`INSERT INTO tests (id, key, title, visible, status, mtime)
                                       VALUES (?, ?, ?, 1, 'draft', ?)
                                       ON CONFLICT DO UPDATE SET title = excluded.title,
                                                                 mtime = excluded.mtime
                                       RETURNING id, mtime`,
                                      test.id, test.key, title, mtime);

            test.id = ret.id;
            test.mtime = ret.mtime;
        }
    });
}

async function deleteTest(id) {
    await db.exec('DELETE FROM tests WHERE id = ?', id);
}

async function openTest(test) {
    stopTest();

    let mod = MODULES.find(mod => mod.key == test.key);

    active_mod = await mod.prepare(db, test, root_el);
    await active_mod.start();
}

function stopTest() {
    if (active_mod == null)
        return;

    active_mod.stop();
    active_mod = null;
}

export {
    identity,

    start,
    runDashboard,

    changeIdentity
}
