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

import { render, html } from '../../../../vendor/lit-html/lit-html.bundle.js';
import { Util, Log, Net, LocalDate } from '../../../web/core/base.js';
import * as UI from '../../../web/flat/ui.js';
import { niceDate, progressBar, progressCircle } from '../lib/util.js';
import { ProjectInfo, ProjectBuilder } from './api.js';
import { FormState, FormModel, FormBuilder } from './form.js';
import { ASSETS } from '../../assets/assets.js';

function StudyModule(db, project, code, study) {
    let tests = null;

    let route = {
        mod: null,
        page: null,
        section: null
    };
    let state = null;

    let target_el = null;

    this.start = async function(el) {
        target_el = el;

        if (study.start != null)
            await setup();

        await run();
    };

    this.stop = function() {
        // STOP
    };

    this.hasUnsavedData = function() {
        return true;
    };

    async function setup() {
        // Init study schema
        {
            project = new ProjectInfo(project);

            let builder = new ProjectBuilder(project);
            let start = LocalDate.fromJSDate(study.start);

            code.init(builder, start);
        }

        route.mod = project.root;

        await db.transaction(async () => {
            await db.exec('UPDATE tests SET visible = 0 WHERE study = ?', study.id);

            for (let page of project.pages) {
                let schedule = page.schedule?.toString?.();

                await db.exec(`INSERT INTO tests (study, key, title, visible, status, schedule)
                               VALUES (?, ?, ?, 1, 'empty', ?)
                               ON CONFLICT DO UPDATE SET title = excluded.title,
                                                         visible = excluded.visible,
                                                         schedule = excluded.schedule`,
                              study.id, page.key, page.title, schedule);
            }
        });
    }

    async function run() {
        if (route.mod == null) {
            render(html`
                <div class="tabbar">
                    <a class="active">Participer</a>
                </div>

                <div class="tab">
                    <div class="box">
                        <div class="title">Consentement</div>
                        ${code.consent}
                    </div>
                    <div class="box" style="align-items: center;">
                        <form @submit=${UI.wrap(consent)}>
                            <label>
                                <input name="consent" type="checkbox" />
                                <span>J’ai lu et je ne m’oppose pas à participer à l’étude ${project.title}</span>
                            </label>
                            <button type="submit">Valider</button>
                        </form>
                    </div>
                </div>
            `, target_el);

            return;
        }

        tests = await db.fetchAll(`SELECT t.key, t.status, t.payload
                                   FROM tests t
                                   INNER JOIN studies s ON (s.id = t.study)
                                   WHERE s.id = ?`, study.id);

        for (let test of tests) {
            if (test.payload != null)
                test.payload = JSON.parse(test.payload);
        }

        let [progress, total] = computeProgress(project.root);

        render(html`
            <div class="tabbar">
                <a class="active">Participer</a>
            </div>

            <div class="tab">
                <div class="summary">
                    <img src=${project.picture} alt="" />
                    <div>
                        <div class="title">Étude ${project.index} - ${project.title}</div>
                        ${project.summary}
                    </div>
                    ${progressCircle(progress, total)}
                </div>

                ${route.page == null ? renderModule(route.mod) : null}
                ${route.page != null && route.section == null ? renderStart(route.mod, route.page) : null}
                ${route.page != null && route.section != null ? renderForm(route.mod, route.page, route.section) : null}
            </div>
        `, target_el);
    }

    async function consent(e) {
        let start = (new Date).valueOf();

        let form = e.currentTarget;
        let elements = form.elements;

        if (!elements.consent.checked)
            throw new Error('Vous devez confirmé avoir lu et accepté la participation à ' + project.title);

        study = await db.fetch1(`INSERT INTO studies (key, start)
                                 VALUES (?, ?)
                                 ON CONFLICT DO UPDATE SET start = excluded.start
                                 RETURNING id, start`,
                                project.key, start);

        await setup();
        await run();
    }

    function renderModule(mod) {
        let today = LocalDate.today();

        return html`
            ${Util.mapRange(0, mod.chain.length - 1, idx => {
                let parent = mod.chain[idx];
                let next = mod.chain[idx + 1];

                return html`
                    <div class="box level" @click=${UI.wrap(e => navigate(parent))}>
                        ${parent.level ?? ''}${parent.level ? ' - ' : ''}
                        ${next.title}
                    </div>
                `;
            })}
            <div class="box">
                <div class="title">${mod.level}</div>
                <div class="modules">
                    ${mod.modules.map(child => {
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
                            cls += ' empty';
                            status = 'À compléter';

                            let earliest = null;

                            for (let page of child.pages) {
                                if (page.status == 'done' || page.schedule == null)
                                    continue;

                                if (earliest == null || page.schedule < earliest)
                                    earliest = page.schedule;
                            }

                            if (earliest != null) {
                                status = niceDate(earliest, true);

                                if (earliest <= today)
                                    cls += ' draft';
                            }
                        }

                        return html`
                            <div class=${cls} @click=${UI.wrap(e => navigate(child))}>
                                <div class="title">${child.title}</div>
                                <div class="status">${status}</div>
                            </div>
                        `;
                    })}
                    ${!mod.modules.length ? mod.pages.map(page => {
                        let test = tests.find(test => test.key == page.key);

                        let cls = 'module ' + test.status;
                        let status = null;

                        if (test.status == 'done') {
                            status = 'Terminé';
                        } else if (page.schedule != null) {
                            status = niceDate(page.schedule, true);

                            if (page.schedule <= today)
                                cls += ' draft';
                        } else {
                            status = 'À compléter';
                        }

                        return html`
                            <div class=${cls} @click=${UI.wrap(e => navigate(mod, page))}>
                                <div class="title">${page.title}</div>
                                <div class="status">${status}</div>
                            </div>
                        `;
                    }) : ''}
                </div>

                ${mod.help != null ? html`<div class="help">${mod.help}</div>` : ''}
            </div>
        `;
    }

    function renderStart(mod, page) {
        return html`
            ${Util.mapRange(0, mod.chain.length - 1, idx => {
                let parent = mod.chain[idx];
                let next = mod.chain[idx + 1];

                return html`
                    <div class="box level" @click=${UI.wrap(e => navigate(parent))}>
                        ${parent.level ?? ''}${parent.level ? ' - ' : ''}
                        ${next.title}
                    </div>
                `;
            })}
            <div class="box level" @click=${UI.wrap(e => navigate(mod, page))}>
                Questionnaire - ${page.title}
            </div>
            <div class="start">
                <div class="help">
                    <img src=${ASSETS['pictures/help']} alt="" />
                    <div>
                        <p>Tout est prêt pour <b>commencer le questionnaire</b> !
                        <p>Pensez à <b>faire des pauses</b> si vous en ressentez le besoin, ou à faire un tour sur la page Se Détendre.
                        <p>Si vous êtes prêt, <b>on peut y aller</b> !
                    </div>
                </div>
                <button type="button" @click=${UI.wrap(e => navigate(mod, page, 0))}>Commencer le questionnaire</button>
            </div>
        `;
    }

    function renderForm(mod, page, section) {
        let model = new FormModel;
        let builder = new FormBuilder(state, model);

        page.form.run(builder, state.values);

        let widgets = model.widgets0; 
        let widget = widgets[section];

        if (widget == null)
            return '';

        let end = widgets.length - 1;

        return html`
            <div class="box">${page.form.intro}</div>

            ${progressBar(section, end, 'sections')}
            <div class="box">${widget?.render?.()}</div>

            <div class="actions">
                ${section < end ? html`<button @click=${UI.wrap(e => navigate(mod, page, section + 1))}>Continuer</button>`: ''}
                ${section == end ? html`<button @click=${UI.wrap(e => save(mod, page, state.values))}>Valider</button>`: ''}
            </div>
        `;
    }

    async function navigate(mod, page = null, section = null) {
        route.mod = mod;

        while (mod.modules.length == 1)
            mod = mod.modules[0];
        while (page == null && mod.pages.length == 1)
            page = mod.pages[0];

        if (page != route.page) {
            route.page = page;

            if (page != null) {
                let test = tests.find(test => test.key == page.key);
                let data = test?.payload ?? {};

                state = new FormState(data);
                state.changeHandler = run;
            } else {
                state = null;
            }
        }
        route.section = (page != null) ? section : null;

        await run();
    }

    async function save(mod, page, data) {
        let mtime = (new Date).valueOf();
        let json = JSON.stringify(data);

        await db.exec(`UPDATE tests SET status = 'done', mtime = ?, payload = ?
                       WHERE study = ? AND key = ?`, mtime, json, study.id, page.key);

        while (mod != project.root && isComplete(mod, [page]))
            mod = mod.chain[mod.chain.length - 2];

        await navigate(mod);
    }

    function isComplete(mod, saved = []) {
        for (let page of mod.pages) {
            let test = tests.find(test => test.key == page.key);

            if (test.status != 'done' && !saved.includes(page))
                return false;
        }

        return true;
    }

    function computeProgress(mod) {
        let progress = mod.pages.reduce((acc, page) => {
            let test = tests.find(test => test.key == page.key);
            return acc + (test.status == 'done');
        }, 0);
        let total = mod.pages.length;

        return [progress, total];
    }
}

export { StudyModule }
