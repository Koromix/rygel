// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function ApplicationBuilder(app) {
    let self = this;

    let keys_set = new Set;

    this.form = function(key, func = null) {
        if (!key)
            throw new Error('Empty keys are not allowed');
        if (!key.match(/^[a-zA-Z_][a-zA-Z0-9_]*$/))
            throw new Error('Allowed key characters: a-z, _ and 0-9 (not as first character)');
        if (keys_set.has(key))
            throw new Error(`Asset '${key}' already exists`);

        let pages = [];

        let form_builder = new FormBuilder(pages);
        if (func) {
            func(form_builder);
        } else {
            form_builder.page(key);
        }

        let form = {
            key: key,
            pages: pages
        };
        app.forms.push(form);
        keys_set.add(key);
    };

    this.schedule = function(key) {
        if (!key)
            throw new Error('Empty keys are not allowed');
        if (!key.match(/^[a-zA-Z_][a-zA-Z0-9_]*$/))
            throw new Error('Allowed key characters: a-z, _ and 0-9 (not as first character)');
        if (keys_set.has(key))
            throw new Error(`Asset '${key}' already exists`);

        let schedule = {
            key: key
        };
        app.schedules.push(schedule);
        keys_set.add(key);
    };
}
