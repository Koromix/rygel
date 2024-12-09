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

function ApplicationInfo(profile) {
    this.head = null;
    this.forms = new Map;
    this.pages = new Map;
    this.home = null;
    this.panels = {
        editor: profile.develop,
        data: profile.userid > 0 && profile.lock == null,
        view: true
    };
    this.mtime = true;
    this.dashboard = null;
}

function FormInfo(key, title) {
    this.key = key;
    this.title = title;
    this.chain = [];
    this.pages = new Map;
    this.forms = new Map;
    this.menu = [];
    this.multi = null;
    this.url = null;
}

function PageInfo(key, title, stack) {
    let self = this;

    this.key = key;
    this.title = title;
    this.form = null;
    this.url = null;

    this.getOption = function(key, record, default_value = undefined) {
        for (let i = stack.length - 1; i >= 0; i--) {
            let options = stack[i];

            if (typeof options === 'function') {
                if (record == null)
                    continue;

                options = options(record, self);
            }

            let value = options[key];

            if (typeof value === 'function') {
                if (record == null)
                    continue;

                value = value(record, self);
            }

            if (value != null)
                return value;
        }

        return default_value;
    };
}

function ApplicationBuilder(app) {
    let self = this;

    let options_stack = [
        // {
        //     enabled: true,
        //     restrict: false,
        //     dictionaries: null,
        //     load: null,
        //     default_actions: true,
        //     autosave: false,
        //     confirm: false,
        //     claim: true
        // }
    ];
    let form_ref = null;

    this.home = function(home) { app.home = home; };
    this.panel = function(panel, enable) { app.panels[panel] = enable; };

    this.mtime = function(mtime) { app.mtime = mtime; };
    this.dashboard = function(url) { app.dashboard = url; };

    this.pushOptions = function(options = {}) {
        options_stack = expandOptions(options);
    };
    this.popOptions = function() {
        if (options_stack.length < 2)
            throw new Error('Too many popOptions() operations');

        options_stack.pop();
    };

    this.head = function(head) { app.head = head; };

    this.form = function(key, title, func = null, options = null) {
        checkKey(key);
        if (app.forms.has(key))
            throw new Error(`Form key '${key}' is already used`);

        title = title || key;

        if (options == null && typeof func === 'object') {
            options = func;
            func = null;
        }

        let prev_options = Array.from(options_stack);
        let prev_form = form_ref;

        try {
            options_stack = expandOptions(options);

            form_ref = new FormInfo(key, title);
            if (prev_form != null)
                form_ref.chain.push(...prev_form.chain);
            form_ref.chain.push(form_ref);

            if (typeof func === 'function') {
                func(self);

                if (!form_ref.menu.length)
                    self.page(key, title);
            } else {
                self.page(key, func || title);
            }

            // Determine URL
            if (form_ref.pages.size) {
                form_ref.url = form_ref.pages.values().next().value.url;
            } else {
                form_ref.url = form_ref.forms.values().next().value.url;
            }

            if (prev_form != null && showMenuRec(form_ref)) {
                let item = {
                    key: key,
                    title: title,
                    type: 'form',
                    form: form_ref,
                    url: form_ref.url
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

    function showMenuRec(form) {
        for (let page of form.pages.values()) {
            if (page.getOption('menu', null, true))
                return true;
        }

        for (let child of form.forms.values()) {
            if (showMenuRec(child))
                return true;
        }

        return false;
    }

    this.many = function(key, multi, title, func = null, options = {}) {
        if (form_ref == null)
            throw new Error('many cannot be used for top-level forms');

        let form = self.form(key, title, func, options);
        form.multi = multi;
        return form;
    };
    this.formMulti = this.many;

    this.page = function(key, title, options = null) {
        checkKey(key);
        if (app.pages.has(key))
            throw new Error(`Page key '${key}' is already used`);
        if (form_ref == null)
            throw new Error('Cannot make page without enclosing form');

        title = title || key;

        options = expandOptions(options, {
            filename: `pages/${key}.js`
        });

        let page = new PageInfo(key, title, options);

        if (form_ref != null) {
            page.form = form_ref;
        } else {
            page.form = new FormInfo(key, title);
        }
        page.url = `${ENV.urls.instance}main/${key}`;

        if (page.getOption('menu', null, true)) {
            let item = {
                key: key,
                title: title,
                type: 'page',
                page: page,
                url: page.url
            };
            page.form.menu.push(item);
        }

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

    function expandOptions(options, defaults = null) {
        let stack = options_stack;

        if (defaults != null)
            stack = [defaults, ...stack];
        if (options != null)
            stack = [...stack, options];

        return stack;
    }
}

export {
    ApplicationInfo,
    FormInfo,
    PageInfo,
    ApplicationBuilder
}
