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
import { Util, Log } from '../../../web/core/base.js';
import { progressBar } from '../lib/util.js';
import { FormContext, FormModel, FormBuilder } from './builder.js';
import * as app from '../app.js';
import * as UI from '../ui.js';
import { ASSETS } from '../../assets/assets.js';

function FormModule(db, study, page) {
    let div = null;

    let form = page.form;
    let part_idx = null;

    let ctx = null;

    let is_new = false;
    let has_changed = false;

    this.start = async function() {
        div = document.createElement('div');
        div.className = 'column';

        // Load existing data
        {
            let payload = await db.pluck('SELECT payload FROM tests WHERE study = ? AND key = ?', study.id, page.key);

            if (payload != null) {
                try {
                    let json = JSON.parse(payload);
                    ctx = new FormContext(json);
                } catch (err) {
                    console.error(err);
                }
            }
        }

        if (ctx == null) {
            ctx = new FormContext;
            is_new = true;
        }

        ctx.changeHandler = () => {
            has_changed = true;
            runForm();
        };
    };

    this.stop = function() {
        // Nothing to do
    };

    this.render = function(section) {
        if (section != null) {
            part_idx = parseInt(section, 10);
        } else {
            part_idx = null;
        }

        if (part_idx != null) {
            runForm();
        } else {
            runStart();
        }

        return div;
    };

    this.hasUnsavedData = function() {
        return has_changed;
    };

    function runStart() {
        render(html`
            ${Util.mapRange(0, page.chain.length - 2, idx => {
                let parent = page.chain[idx];
                let next = page.chain[idx + 1];

                return html`
                    <div class="box level" @click=${UI.wrap(e => app.navigateStudy(parent))}>
                        ${parent.level ?? ''}${parent.level ? ' - ' : ''}
                        ${next.title}
                    </div>
                `;
            })}
            <div class="box level" @click=${UI.wrap(e => app.navigateStudy(page))}>
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
                <button type="button" @click=${UI.wrap(e => app.navigateStudy(page, 0))}>${is_new ? 'Commencer' : 'Continuer'}</button>
            </div>
        `, div);
    }

    function runForm() {
        let model = new FormModel;
        let builder = new FormBuilder(ctx, model);

        form.run(builder, ctx.values);

        let end = model.parts.length - 1;
        let idx = Math.min(part_idx, end);

        let part = model.parts[idx];

        render(html`
            <div class="box">${form.intro}</div>
            ${progressBar(idx, end, 'parts')}

            <div class="box">
                <form>
                    ${model.parts[idx].map(widget => widget.render())}
                </form>
            </div>

            <div class="actions">
                ${idx < end ? html`<button @click=${UI.wrap(next)}>Continuer</button>` : ''}
                ${idx == end ? html`<button @click=${UI.wrap(finalize)}>Finaliser</button>` : ''}
            </div>
        `, div);
    }

    async function next() {
        await app.saveTest(page, ctx.raw);
        has_changed = false;

        let section = part_idx + 1;
        await app.navigateStudy(page, section);
    }

    async function finalize() {
        await app.finalizeTest(page, ctx.raw);
        has_changed = false;
    }
}

export { FormModule }
