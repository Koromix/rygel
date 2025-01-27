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

import { progressCircle } from '../lib/util.js';

function StudyModule(db, test, root_el, study) {
    let tests = null;

    let route = {
        mod: study.root,
        page: null,
        section: null
    };

    this.start = async function() {
        await db.transaction(async () => {
            await db.exec('UPDATE tests SET visible = 0 WHERE study = ?', study.id);

            for (let page of study.pages) {
                await db.exec(`INSERT INTO tests (study, key, title, visible, status, schedule)
                               VALUES (?, ?, ?, 1, 'empty', ?)
                               ON CONFLICT DO UPDATE SET title = excluded.title,
                                                         visible = excluded.visible,
                                                         schedule = excluded.schedule`,
                              study.id);
            }
        });

        await run();
    };

    this.stop = function() {
        // STOP
    };

    this.hasUnsavedData = function() {
        return true;
    };

    async function run() {
        tests = await db.fetchAll(`SELECT t.key, t.status, t.payload
                                   FROM tests t
                                   INNER JOIN studies s ON (s.id = t.study)
                                   WHERE s.key = ?`, study.key);

        let [progress, total] = computeProgress(null);

        render(html`
            <div class="tabbar">
                <a class="active">Participer</a>
            </div>

            <div class="tab">
                <div>
                    <img src=${study.picture} alt="" />
                    <p class="reference">${study.title}</p>
                    ${study.summary}
                    ${progressCircle(progress, total)}
                </div>

                ${route.page == null ? renderModule(route.mod) : null}
                ${route.page != null ? renderPage(route.page, route.section) : null}
            </div>
        `)
    }

    function renderModule(mod) {
        return html`
            ${Util.mapRange(0, mod.chain.length - 1, idx => {
                let parent = mod.chain[idx];
                let next = 

                return html`<div class="stu_step">${parent.title} - ${</div>`;
            })}
            ${mod.chain.length > 1 ? html`
                <div class="stu_step">
                    ${Util.mapRange(0, mod.chain.length - 1, idx => mod.chain[idx].title).join(' - ')}
                </div>
            ` : ''}
            <div class="stu_summary">
                <div class="stu_module">
                    ${mod.modules.map(child => {
                        let [progress, total] = computeProgress(child);

                        if (progress == total) {
                            return html`<div class="done">${child.title} Complet</div>`;
                        } else if (!progress) {
                            return html`<div class="empty">${child.title} A compléter</div>`;
                        } else {
                            return html`<div class="partial">${child.title} ${progressCircle(progress, total)}</div>`;
                        }
                    })}
                    ${mod.pages.map(page => {
                        let test = tests.find(test => test.key == page.key);

                        switch (test.status) {
                            case 'empty': return html`<div class="empty">${page.title} A compléter</div>`;
                            case 'draft': return html`<div class="partial">${page.title} A compléter</div>`;
                            case 'done': return html`<div class="done">${page.title} Complet</div>`;
                        }
                    })}
                </div>
            </div>
        `;
    }

    function renderStart(mod, page) {
        return html`
        `;
    }

    function renderForm(page, section) {
        return html`
            <div class="stu_progress"></div>
        `;
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
