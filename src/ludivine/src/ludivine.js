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
import { Util, Log } from '../../web/libjs/common.js';
import * as sqlite3 from '../../web/libjs/sqlite3.js';
import { GENDERS } from './lib/constants.js';
import { computeAge, dateToString } from './lib/util.js';
import * as UI from './lib/ui.js';
import { assets, loadAssets } from './lib/assets.js';
import { NetworkModule } from './network/network.js';

import './css/ludivine.css';

const MODULES = [
    {
        key: 'network',
        title: 'Sociogramme',
        cls: NetworkModule,

        historical: true,
        editable: true
    }
];

let db = null;

let identity = {
    name: '',
    birthdate: null,
    gender: null
};

let main_el = null;
let active_mod = null;

async function start(el) {
    Log.pushHandler(UI.notifyHandler);

    // Adapt to viewport
    window.addEventListener('resize', adaptToViewport);
    adaptToViewport();

    await loadAssets();

    await initSQLite();
    db = await openDatabase('ludivine.db', 'c');

    document.body.classList.remove('loading');

    // Load identity
    {
        let row = await db.fetch1('SELECT name, birthdate, gender FROM meta');

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

    main_el = el;
    await runDashboard();
}

function adaptToViewport() {
    let is_small = (window.innerWidth < 960);
    let is_medium = (window.innerWidth < 1280);

    document.documentElement.classList.toggle('small', is_small);
    document.documentElement.classList.toggle('medium', is_medium);
}

async function initSQLite() {
    let url = './build/sqlite3-worker1-bundler-friendly.js';
    await sqlite3.init(url);
}

async function openDatabase(filename, flags) {
    let db = await sqlite3.open(filename, flags);

    let version = await db.pluck('PRAGMA user_version');
    let target = 2;

    if (version == target)
        return db;
    if (version > target)
        throw new Error('Database model is too recent');

    await db.transaction(async () => {
        switch (version) {
            case 0: {
                await db.exec(`
                    CREATE TABLE meta (
                        name TEXT NOT NULL,
                        gender TEXT NOT NULL,
                        birthdate INTEGER NOT NULL
                    );

                    CREATE TABLE tests (
                        id INTEGER PRIMARY KEY,
                        date INTEGER NOT NULL,
                        timestamp INTEGER NOT NULL,
                        type TEXT NOT NULL,
                        payload BLOB
                    );

                    CREATE TABLE events (
                        test INTEGER NOT NULL REFERENCES tests (id) ON DELETE CASCADE,
                        sequence INTEGER NOT NULL,
                        timestamp INTEGER NOT NULL,
                        type TEXT NOT NULL,
                        data TEXT
                    );

                    CREATE TABLE images (
                        test INTEGER NOT NULL REFERENCES tests (id) ON DELETE CASCADE,
                        timestamp NOT NULL,
                        image BLOB NOT NULL
                    );
                `);
            } // fallthrough

            case 1: {
                await db.exec(`
                    DROP TABLE events;
                    DROP TABLE images;
                    DROP TABLE tests;

                    CREATE TABLE tests (
                        id INTEGER PRIMARY KEY,
                        type TEXT NOT NULL,
                        title TEXT NOT NULL,
                        date INTEGER NOT NULL,
                        payload BLOB
                    );

                    CREATE TABLE events (
                        test INTEGER NOT NULL REFERENCES tests (id) ON DELETE CASCADE,
                        sequence INTEGER NOT NULL,
                        timestamp INTEGER NOT NULL,
                        type TEXT NOT NULL,
                        data TEXT
                    );

                    CREATE TABLE images (
                        test INTEGER NOT NULL REFERENCES tests (id) ON DELETE CASCADE,
                        timestamp NOT NULL,
                        image BLOB NOT NULL
                    );
                `);
            } // fallthrough
        }

        await db.exec('PRAGMA user_version = ' + target);
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
                    ${init ? 'Créer le sujet' : 'Modifier le sujet'}
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

            let new_identity = {
                name: elements.name.value.trim(),
                gender: elements.gender.value,
                birthdate: elements.birthdate.valueAsDate.valueOf()
            };

            await db.transaction(async () => {
                await db.exec('DELETE FROM meta');

                await db.exec('INSERT INTO meta (name, gender, birthdate) VALUES (?, ?, ?)',
                              new_identity.name, new_identity.gender, new_identity.birthdate);
            });

            Object.assign(identity, new_identity);
        }
    });
}

async function runDashboard() {
    if (active_mod != null) {
        active_mod.stop();
        active_mod = null;
    }

    let tests = await db.fetchAll('SELECT id, type, title, date FROM tests ORDER BY id');

    render(html`
        <div class="dashboard">
            <div>
                <table class="tests">
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
                            let mod = MODULES.find(mod => mod.key == test.type);
                            let img = assets.main[mod.key];

                            return html`
                                <tr>
                                    <td class="picture" title=${mod.title}><img src=${img} alt=${mod.title}></td>
                                    ${mod.editable ? html`<td><a @click=${UI.wrap(e => openTest(test))}>${test.title}</a></td>` : ''}
                                    ${!mod.editable ? html`<td>${test.title}</td>` : ''}
                                    ${mod.historical ? html`<td>${(new Date(test.date)).toLocaleDateString()}</td>` : ''}
                                    ${!mod.historical ? html`<td>${(new Date(test.date)).toLocaleString()}</td>` : ''}
                                    <td class="right">
                                        <button type="button" class="small" @click=${UI.wrap(e => changeTest(test).then(runDashboard))}>Configurer</button>
                                        <button type="button" class="small danger" @click=${UI.confirm(`Suppression de ${test.title}`, e => deleteTest(test.id).then(runDashboard))}>Supprimer</button>
                                    </td>
                                </tr>
                            `;
                        })}
                        ${!tests.length ? html`<tr><td colspan="4" class="center">Aucun élément</td></tr>` : ''}
                    </tbody>
                </table>
                <div class="modules">
                    ${MODULES.map(mod =>
                        html`<button type="button" @click=${UI.wrap(e => createTest(mod.key))}>Nouveau ${mod.title.toLowerCase()}</button>`)}
                </div>
            </div>
        </div>
    `, main_el);
}

async function createTest(type) {
    let now = (new Date).valueOf();

    let test = {
        id: null,
        date: now,
        type: type
    };

    await changeTest(test);
    await openTest(test);
}

async function changeTest(test) {
    let init = (test.id == null);
    let mod = MODULES.find(mod => mod.key == test.type);

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
            let date = (mod.historical ? elements.date.valueAsDate : new Date).valueOf();

            let id = await db.pluck(`INSERT INTO tests (id, title, date, type) 
                                     VALUES (?, ?, ?, ?)
                                     ON CONFLICT DO UPDATE SET title = excluded.title,
                                                               date = excluded.date
                                     RETURNING id`,
                                    test.id, title, date, test.type);
            test.id = id;
        }
    });
}

async function deleteTest(id) {
    await db.exec('DELETE FROM tests WHERE id = ?', id);
}

async function openTest(test) {
    if (active_mod != null) {
        active_mod.stop();
        active_mod = null;
    }

    let mod = MODULES.find(mod => mod.key == test.type);

    active_mod = new mod.cls(db, test, main_el);
    await active_mod.start();
}

export {
    identity,

    start,
    changeIdentity
}
