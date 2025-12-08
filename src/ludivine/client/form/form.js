// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import { render, html } from 'vendor/lit-html/lit-html.bundle.js';
import { Util, Log } from 'lib/web/base/base.js';
import * as Data from 'lib/web/ui/data.js';
import { progressBar, deflate, inflate } from '../core/misc.js';
import { FormState, FormModel, FormBuilder } from 'lib/web/ui/form.js';
import { ASSETS } from '../../assets/assets.js';

function FormModule(app, study, page) {
    let self = this;

    const UI = app.UI;
    const db = app.db;

    let div = null;

    let build = page.build;
    let part_idx = null;

    let state = null;
    let tests = {};
    let model = null;

    let is_new = false;
    let has_changed = false;

    let rendered_idx = null;

    this.run = async function() {
        if (state != null)
            return;

        div = document.createElement('div');
        div.className = 'column';

        // Load existing data
        {
            let load = [page.key];

            if (Array.isArray(page.load))
                load.push(...page.load);

            for (let key of load) {
                let payload = await db.pluck('SELECT payload FROM tests WHERE study = ? AND key = ?', study.id, key);

                if (payload != null) {
                    try {
                        let buf = await inflate(payload);
                        let json = (new TextDecoder).decode(buf);
                        let obj = JSON.parse(json);

                        if (key == page.key) {
                            state = new FormState(obj);
                            tests[key] = state.values;
                        } else {
                            let [raw, values] = Data.wrap(obj);
                            tests[key] = values;
                        }
                    } catch (err) {
                        console.error(err);
                    }
                } else {
                    tests[key] = null;
                }
            }
        }

        if (state == null) {
            state = new FormState;
            is_new = true;
        }

        state.changeHandler = () => {
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
            <div class="help">
                <img src=${ASSETS['pictures/help1']} alt="" />
                <div>
                    <p>Tout est prêt pour <b>commencer le questionnaire</b> !
                    <p>Pensez à <b>faire des pauses</b> si vous en ressentez le besoin, ou à faire un tour sur la page Se Détendre.
                    <p>Si vous êtes prêt, <b>on peut y aller</b> !
                </div>
            </div>
            <div class="actions">
                <button type="button" @click=${UI.wrap(e => app.navigateStudy(page, 0))}>${is_new ? 'Commencer' : 'Continuer'}</button>
            </div>
        `, div);
    }

    function runForm() {
        // Reset model
        model = new FormModel;

        let builder = new FormBuilder(state, model);

        build(builder, {
            page: page,
            data: tests,
            annotate: Data.annotate,
            mask: mask
        });

        let end = model.parts.length - 1;
        if (part_idx > end)
            part_idx = end;
        let part = model.parts[part_idx];

        render(html`
            ${progressBar(part_idx, end, 'parts')}

            <div id="intro">
                ${wrapIntro(part.intro, part.introIndex)}
            </div>

            <div id="part" class="box">
                <div class="header">Partie ${part_idx + 1}</div>
                <form @submit=${UI.wrap(next)}>
                    ${part.widgets.map(widget => widget.render())}
                </form>
            </div>

            <div class="actions">
                <button type="button" class="secondary" @click=${UI.wrap(previous)}>Précédent</button>
                ${part_idx < end ? html`<button type="button" @click=${UI.wrap(next)}>Continuer</button>` : ''}
                ${part_idx == end ? html`
                    <button type="button" class="confirm" @click=${UI.wrap(next)}>
                        <img src=${ASSETS['ui/confirm']} alt="" />
                        <span style="display: inline;">Finaliser</span>
                    </button>
                ` : ''}
            </div>
        `, div);

        if (part_idx != rendered_idx) {
            let prev_part = model.parts[rendered_idx];
            let new_intro = (part.introIndex !== prev_part?.introIndex);

            let el = div.querySelector(new_intro ? '#intro' : '#part');
            el?.scrollIntoView?.({ behavior: 'smooth', block: 'start' });

            rendered_idx = part_idx;
        }
    }

    function mask(values, key, mask) {
        if (typeof values == 'string') {
            mask = key;
            key = values;
            values = state.values;
        }

        let notes = Data.annotate(values, key);
        notes.mask = mask;
    }

    function wrapIntro(intro, idx) {
        if (!intro)
            return '';

        let left = (idx % 2 == 0);

        if (left) {
            return html`
                <div class="box">
                    <div class="row">
                        <img style="width: 100px; align-self: center;" src=${ASSETS['pictures/help1']} alt="" />
                        <div style="flex: 1; margin-left: 1em;">${intro}</div>
                    </div>
                </div>
            `;
        } else {
            return html`
                <div class="box">
                    <div class="row">
                        <div style="flex: 1; margin-right: 1em;">${intro}</div>
                        <img style="width: 100px; align-self: center;" src=${ASSETS['pictures/help2']} alt="" />
                    </div>
                </div>
            `;
        }
    }

    async function previous() {
        if (has_changed)
            await app.saveTest(page, state.raw);
        has_changed = false;

        let section = part_idx - 1;
        if (section < 0)
            section = null;
        await app.navigateStudy(page, section);
    }

    async function next() {
        validate();

        let end = model.parts.length - 1;

        if (part_idx < end) {
            if (has_changed)
                await app.saveTest(page, state.raw);
            has_changed = false;

            let section = part_idx + 1;
            await app.navigateStudy(page, section);
        } else {
            let raw = state.raw;
            let results = anonymize(state.raw);

            await app.finalizeTest(page, raw, results);
            has_changed = false;
        }
    }

    function anonymize(raw) {
        let obj = {};

        for (let key in raw) {
            let ptr = Object.assign({}, raw[key]);

            if (Util.isPodObject(ptr.$v))
                ptr.$v = anonymize(ptr.$v);

            if (Object.hasOwn(ptr, '$n') && Object.hasOwn(ptr.$n, 'mask')) {
                ptr.$n = Object.assign({}, ptr.$n);
                ptr.$v = ptr.$n.mask;
                delete ptr.$n.mask;
            }

            obj[key] = ptr;
        }

        return obj;
    }

    function validate() {
        let part = model.parts[part_idx];

        let valid = true;

        for (let widget of part.widgets) {
            if (widget.key == null)
                continue;

            let notes = widget.notes;

            if (widget.value != null || widget.optional || widget.disabled) {
                delete notes.skip;
            } else if (!notes.skip) {
                notes.skip = false;
                valid = false;
            }

            if (notes.error != null)
                valid = false;
        }

        if (!valid) {
            state.refresh();
            throw new Error('Certaines réponses sont manquantes ou erronées, veuillez vérifier vos réponses');
        }
    }
}

export { FormModule }
