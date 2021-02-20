// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see https://www.gnu.org/licenses/.

function ApplicationInfo() {
    this.head = null;
    this.forms = new Map;
    this.pages = new Map;
    this.home = null;
}

function FormInfo(key, title) {
    this.key = key;
    this.title = title;
    this.chain = [];
    this.pages = new Map;
    this.forms = new Map;
    this.menu = [];
    this.multi = false;
    this.zoned = true;
    this.url = null;
}

function PageInfo(key, title) {
    this.key = key;
    this.title = title;
    this.form = null;
    this.enabled = true;
    this.dictionaries = [];
    this.url = null;
    this.filename = null;
}

function ApplicationBuilder(app) {
    let self = this;

    let options_stack = [{}];
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

    this.head = function(head) {
        app.head = head;
    };

    this.form = function(key, title, func = null, options = {}) {
        checkKey(key);
        if (app.forms.has(key))
            throw new Error(`Form key '${key}' is already used`);

        let prev_options = options_stack;
        let prev_form = form_ref;

        try {
            options_stack = [expandOptions(options)];

            form_ref = new FormInfo(key, title);
            if (prev_form != null)
                form_ref.chain.push(...prev_form.chain);
            form_ref.chain.push(form_ref);

            if (typeof func === 'function') {
                func(self);
            } else {
                self.page(key, func || title);
            }
            if (options.zoned != null)
                form_ref.zoned = options.zoned;

            if (!form_ref.menu.length)
                throw new Error(`Form '${key}' must contain at least one page or child form`);

            if (prev_form != null) {
                let item = {
                    title: title,
                    type: 'form',
                    form: form_ref
                };
                prev_form.menu.push(item);

                prev_form.forms.set(key, form_ref);
            }
            app.forms.set(key, form_ref);

            return form_ref;
        } finally {
            options_stack = prev_options;
            form_ref = prev_form;
        }
    };

    this.formMulti = function(key, title, func = null, options = {}) {
        let form = self.form(key, title, func, options);
        form.multi = true;
        return form;
    };

    this.page = function(key, title, options = {}) {
        checkKey(key);
        if (app.pages.has(key))
            throw new Error(`Page key '${key}' is already used`);

        options = expandOptions(options);

        let page = new PageInfo(key, title);

        if (form_ref != null) {
            page.form = form_ref;
        } else {
            page.form = new FormInfo(key, title);
        }
        if (options.enabled != null)
            page.enabled = options.enabled;
        if (options.dictionaries != null)
            page.dictionaries = options.dictionaries;
        page.url = `${ENV.base_url}main/${key}`;
        page.filename = (options.filename != null) ? options.filename : `pages/${key}.js`;

        if (page.form.url == null)
            page.form.url = page.url;

        let item = {
            title: title,
            type: 'page',
            page: page
        };
        page.form.menu.push(item);

        page.form.pages.set(key, page);
        app.pages.set(key, page);

        return page;
    };

    function checkKey(key) {
        if (!key)
            throw new Error('Empty keys are not allowed');
        if (!key.match(/^[a-zA-Z_][a-zA-Z0-9_]*$/))
            throw new Error('Allowed key characters: a-z, _ and 0-9 (not as first character)');
        if (key.startsWith('__'))
            throw new Error('Keys must not start with \'__\'');
    }

    function expandOptions(options) {
        let expand_enabled = (typeof options.enabled === 'function');

        options = Object.assign({}, options_stack[options_stack.length - 1], options);

        if (expand_enabled) {
            if (form_ref == null)
                throw new Error('Enable callback cannot be used outside form definition');

            let form_key = form_ref.key;
            let enable_func = options.enabled;

            options.enabled = meta => {
                meta = meta.map[form_key];
                return enable_func(meta);
            };
        }

        return options;
    }
}
