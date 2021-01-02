// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function ApplicationInfo() {
    this.forms = new Set;
    this.pages = new Map;
    this.home = null;
}

function PageInfo() {
    this.key = null;
    this.title = null;

    this.store = null;
    this.menu = null;
    this.options = {};
}

function ApplicationBuilder(app) {
    let self = this;

    let options_stack = [{}];
    let store_ref = null;
    let menu_ref = null;

    this.pushOptions = function(options = {}) {
        options = expandOptions(options);
        options_stack.push(options);
    };
    this.popOptions = function() {
        if (options_stack.length < 2)
            throw new Error('Too many popOptions() operations');

        options_stack.pop();
    };

    this.form = function(key, func = null, options = {}) {
        if (typeof func === 'string') {
            self.page(key, func, options);
        } else {
            checkKey(key);
            if (app.forms.has(key))
                throw new Error(`Form key '${key}' is already used`);

            let prev_options = options_stack;

            try {
                options_stack = [expandOptions(options)];
                menu_ref = [];
                store_ref = key;

                func(self);

                if (!menu_ref.length)
                    throw new Error(`Form '${key}' must contain at least one page`);
            } finally {
                options_stack = prev_options;
                store_ref = null;
                menu_ref = null;
            }

            app.forms.add(key);
        }
    };

    this.page = function(key, title, options = {}) {
        checkKey(key);
        if (app.pages.has(key))
            throw new Error(`Page key '${key}' is already used`);

        let page = new PageInfo;

        page.key = key;
        page.title = title;
        page.store = store_ref != null ? store_ref : key;
        page.menu = menu_ref != null ? menu_ref : [];
        page.options = expandOptions(options);

        page.menu.push(page);
        app.pages.set(key, page);
    };

    function checkKey(key) {
        if (!key)
            throw new Error('Empty keys are not allowed');
        if (!key.match(/^[a-zA-Z_][a-zA-Z0-9_]*$/))
            throw new Error('Allowed key characters: a-z, _ and 0-9 (not as first character)');
    }

    function expandOptions(options) {
        options = Object.assign({}, options_stack[options_stack.length - 1], options);
        return options;
    }
}
