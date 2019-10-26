// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function FormInfo(key) {
    this.key = key;
    this.pages = [];
    this.links = [];
}

function FormBuilder(form) {
    let self = this;

    let used_keys = new Set;

    this.page = function(key) {
        if (!key)
            throw new Error('Empty keys are not allowed');
        if (!key.match(/^[a-zA-Z_][a-zA-Z0-9_]*$/))
            throw new Error('Allowed key characters: a-z, _ and 0-9 (not as first character)');
        if (used_keys.has(key))
            throw new Error(`Page '${key}' is already used in this form`);

        let page = Object.freeze({
            key: key
        });

        form.pages.push(page);
        used_keys.add(key);
    };
}
