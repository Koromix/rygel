// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function ApplicationInfo() {
    let self = this;

    this.pages = [];
    this.pages_map = {};

    this.shared = {};
}

function ApplicationBuilder(app) {
    let self = this;

    let used_keys = new Set;
    let options_stack = [{
        show_id: true,
        show_data: true,
        make_seq: record => record.sequence || 'local',
        default_actions: true,
        float_actions: true,
        use_validation: false,
        dependencies: []
    }];
    let store_ref = null;
    let menu_ref = null;

    // Proxy to ApplicationInfo
    self.shared = app.shared;

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

            let prev_options = options_stack;

            try {
                options_stack = [expandOptions(options)];
                menu_ref = [];
                store_ref = key;

                func(self);

                if (!menu_ref.length)
                    throw new Error('Forms must contain at least one page');
            } finally {
                options_stack = prev_options;
                store_ref = null;
                menu_ref = null;
            }
        }
    };

    this.page = function(key, title, options = {}) {
        checkKey(key);
        options = expandOptions(options);

        let page = {
            key: key,
            store: store_ref != null ? store_ref : key,

            title: title,
            menu: menu_ref != null ? menu_ref : [],
            options: options
        };
        let item = {
            key: key,
            title: title,
            dependencies: options.dependencies
        }
        page.menu.push(item);

        app.pages.push(page);
        app.pages_map[page.key] = page;
    };

    function checkKey(key) {
        if (!key)
            throw new Error('Empty keys are not allowed');
        if (!key.match(/^[a-zA-Z_][a-zA-Z0-9_]*$/))
            throw new Error('Allowed key characters: a-z, _ and 0-9 (not as first character)');
        if (used_keys.has(key))
            throw new Error(`Key '${key}' is already used`);

        used_keys.add(key);
    }

    function expandOptions(options) {
        options = Object.assign({}, options_stack[options_stack.length - 1], options);
        return options;
    }
}
