// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import { render, html } from 'vendor/lit-html/lit-html.bundle.js';
import { Util, Log } from 'lib/web/base/base.js';
import { FormState, FormModel, FormBuilder } from 'lib/web/ui/form.js';
import { ASSETS } from '../../assets/assets.js';

function ConsentModule(app, project) {
    let self = this;

    const UI = app.UI;
    const route = app.route;
    const db = app.db;

    let bundle = project.bundle;
    let consent = bundle.consent;

    let div = null;

    let downloaded = false;
    let state = null;
    let values = null;

    this.run = function() {
        if (state != null)
            return;

        div = document.createElement('div');
        div.className = 'column';

        downloaded = (consent.download == null);
        state = new FormState;
        values = state.values;

        state.changeHandler = runConsent;
    };

    this.stop = function() {
        // Nothing to do
    };

    this.render = function() {
        runConsent();
        return div;
    };

    this.hasUnsavedData = function() {
        return false;
    };

    function runConsent() {
        let model = new FormModel;
        let builder = new FormBuilder(state, model, { disabled: !downloaded });

        let valid = consent.accept(builder, values);

        render(html`
            <div class="box">
                <div class="header">Consentement</div>
                <div>${consent.text}</div>
                ${consent.download != null ? html`
                    <div class="actions">
                        <a href=${consent.download} download
                           @click=${UI.wrap(toggleDownload)}>Télécharger la lettre d'information</a>
                    </div>
                ` : ''}
            </div>
            <div class="box" style="align-items: center;">
                ${consent.download != null ? html`
                    <div class="help">
                        <img src=${ASSETS['pictures/help1']} alt="" />
                        <div>
                            <p>Vous devez <b>télécharger et lire la lettre d'informations</b> pour pouvoir commencer.
                            <p>Une fois cela fait, vous pourrez participer à l'étude !
                        </div>
                    </div>
                ` : ''}
                <form @submit=${UI.wrap(e => start(valid, values))}>
                    ${model.widgets.map(widget => widget.render())}

                    <div class="actions">
                        <button type="submit" ?disabled=${!downloaded}>Participer</button>
                    </div>
                </form>
            </div>
        `, div);
    }

    function toggleDownload() {
        if (downloaded)
            return;

        setTimeout(() => {
            downloaded = true;
            runConsent();
        }, 1000);
    }

    async function start(valid, values) {
        for (let key in values) {
            let value = values[key];
            if (value == null)
                throw new Error('Vous devez répondre aux différentes questions pour pouvoir participer');
        }
        if (!valid)
            throw new Error('Vous devez confirmé avoir lu et accepté la participation à ' + project.title);

        await app.startStudy(project, values);
    }
}

export { ConsentModule }
