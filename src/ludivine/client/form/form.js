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

function FormModule(db, study, page) {
    let div = null;

    let form = page.form;
    let part_idx = 0;

    let ctx = null;

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

        if (ctx == null)
            ctx = new FormContext;

        ctx.changeHandler = run;
    };

    this.stop = function() {
        // Nothing to do
    };

    this.render = function(section) {
        part_idx = parseInt(section, 10);

        run();

        return div;
    };

    this.hasUnsavedData = function() {
        return false;
    };

    function run() {
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
        let next = part_idx + 1;
        await app.navigateStudy(page, next);
    }

    async function finalize() {
        let data = ctx.raw;
        await app.saveTest(page, data);
    }
}

export { FormModule }
