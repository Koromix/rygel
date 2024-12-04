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
import { NetworkModule } from './network/network.js';

const MODULES = [
    {
        key: 'network',
        title: 'Sociogramme',
        cls: NetworkModule
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

async function initSQLite() {
    let url = './build/sqlite3-worker1-bundler-friendly.js';
    await sqlite3.init(url);
}

async function openDatabase(filename, flags) {
    let db = await sqlite3.open(filename, flags);

    let version = await db.pluck('PRAGMA user_version');
    let target = 1;

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

    let tests = await db.fetchAll('SELECT id, date, timestamp, type FROM tests ORDER BY id');

    render(html`
        <div>
            ${tests.map(test => html`
                <a @click=${UI.wrap(e => openTest(test))}>${test.type} ${(new Date(test.date)).toLocaleString()}</a><br/>
            `)}
        </div>

        <div>
            ${MODULES.map(mod => html`
                <button @click=${UI.wrap(e => createTest(mod.key))}>${mod.title}</button><br/>
            `)}
        </div>
    `, main_el);
}

async function createTest(type) {
    let now = (new Date).valueOf();

    let test = {
        id: null,
        date: now,
        timestamp: now,
        type: type
    };

    test.id = await db.pluck(`INSERT INTO tests (date, timestamp, type)
                              VALUES (?, ?, ?)
                              RETURNING id`,
                             test.date, test.timestamp, test.type);

    await openTest(test);
}

async function openTest(test) {
    let mod = MODULES.find(mod => mod.key == test.type);

    active_mod = new mod.cls(db, test, main_el);
    await active_mod.start();
}

export {
    identity,

    start,
    changeIdentity
}
