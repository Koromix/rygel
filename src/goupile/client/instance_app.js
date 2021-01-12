// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function ApplicationInfo() {
    this.forms = new Map;
    this.pages = new Map;
    this.home = null;
}

function FormInfo(key, title, parent) {
    this.key = key;
    this.title = title;
    this.parent = parent;
    // this.children = [];
    this.pages = new Map;
    this.children = new Map;
    this.url = null;
}

function PageInfo(key, title) {
    this.key = key;
    this.title = title;
    this.form = null;
    this.options = {};
    this.url = null;
    this.filename = null;
}

function ApplicationBuilder(app) {
    let self = this;

    let options_stack = [{
        dependencies: []
    }];
    let form_ref = null;

    this.pushOptions = function(options = {}) {
        options = expandOptions(options);
        options_stack.push(options);
    };
    this.popOptions = function() {
        if (options_stack.length < 2)
            throw new Error('Too many popOptions() operations');

        options_stack.pop();
    };

    this.form = function(key, title, func = null, options = {}) {
        if (func == null) {
            self.page(key, title);
        } else {
            checkKey(key);
            if (app.forms.has(key))
                throw new Error(`Form key '${key}' is already used`);

            let prev_options = options_stack;
            let prev_form = form_ref;

            try {
                options_stack = [expandOptions(options)];
                form_ref = new FormInfo(key, title, form_ref);

                func(self);

                if (!form_ref.pages.size)
                    throw new Error(`Form '${key}' must contain at least one page`);

                if (prev_form != null)
                    prev_form.children.set(key, form_ref);
                app.forms.set(key, form_ref);
            } finally {
                options_stack = prev_options;
                form_ref = prev_form;
            }
        }
    };

    this.page = function(key, title, options = {}) {
        checkKey(key);
        if (app.pages.has(key))
            throw new Error(`Page key '${key}' is already used`);

        options = expandOptions(options);

        let page = new PageInfo(key, title);

        page.form = form_ref != null ? form_ref : new FormInfo(key, title);
        page.dependencies = options.dependencies.filter(dep => dep !== key);
        page.url = `${ENV.base_url}main/${key}`;
        page.filename = `pages/${key}.js`;

        if (page.form.url == null)
            page.form.url = page.url;

        page.form.pages.set(key, page);
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
