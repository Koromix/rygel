// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
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

    this.pages = [];
    this.homepage = null;
    this.annotate = false;
    this.shortcuts = [];

    this.stores = [];

    this.panels = {
        editor: profile.develop,
        data: !profile.single
    };

    this.tags = [
        { key: 'draft', label: T.label_draft, color: '#d921e8', filter: true },
        { key: 'wait', label: T.label_wait, color: '#3b96c8', filter: true },
        { key: 'check', label: T.label_check, color: '#2eb5be', filter: true },
        { key: 'na', label: T.label_na, color: '#ef6e30', filter: true },
        { key: 'incomplete', label: T.label_incomplete, color: '#aaaaaa', filter: true },
        { key: 'error', label: T.label_error, color: '#db0a0a', filter: true }
    ];
}

function ApplicationBuilder(app) {
    let self = this;

    let current_page = null;
    let current_store = null;

    let options_stack = [
        {
            menu: null,
            autosave: false,
            sequence: null,
            progress: true,
            enabled: true,
            claim: true,
            lock: null,
            confirm: null,
            icon: null,
            help: null
        }
    ];

    Object.defineProperties(this, {
        homepage: { get: () => app.homepage, set: homepage => { app.homepage = homepage; }, enumerable: true },
        annotate: { get: () => app.annotate, set: annotate => { app.annotate = annotate; }, enumerable: true },

        head: { get: () => app.head, set: head => { app.head = head; }, enumerable: true },
        tags: { get: () => app.tags, set: tags => { app.tags = tags; }, enumerable: true },

        menu: makeOptionProperty('menu'),
        autosave: makeOptionProperty('autosave'),
        sequence: makeOptionProperty('sequence'),
        progress: makeOptionProperty('progress'),
        enabled: makeOptionProperty('enabled'),
        claim: makeOptionProperty('claim'),
        confirm: makeOptionProperty('confirm'),
        lock: makeOptionProperty('lock'),
        icon: makeOptionProperty('icon'),
        help: makeOptionProperty('help')
    });

    this.panel = function(panel, enable) {
        if (panel.startsWith('_') || !app.hasOwnProperty(panel))
            throw new Error(T.message(`Unknown panel key '{1}'`, panel));

        app.panels[panel] = enable;
    };

    // Keep public for backward compatibility
    this.pushOptions = function(options = {}) {
        options = expandOptions(options);
        options_stack.push(options);
    };
    this.popOptions = function() {
        if (options_stack.length < 2)
            throw new Error(T.message(`Too many calls to function popOptions()`));

        options_stack.pop();
    };

    this.page = function(key, title, options = null) {
        checkKeySyntax(key);
        if (app.pages.some(page => page.key == key))
            throw new Error(T.message(`Page '{1}' already exists`, key));

        title = title || key;
        options = expandOptions(options);

        if (options.menu == null)
            options.menu = title;

        let page = {
            key: key,
            title: title,
            filename: options.filename ?? `pages/${key}.js`,
            url: ENV.urls.instance + key,

            chain: null,
            children: [],

            store: current_store,

            ...options
        };

        app.pages.push(page);

        if (current_page != null) {
            page.chain = [...current_page.chain, page];
            current_page.children.push(page);
        } else {
            page.chain = [page];
        }

        return page;
    };

    this.form = function(key, title, func = null, options = null) {
        checkKeySyntax(key);
        if (app.stores.some(store => store.key == key))
            throw new Error(T.message(`Form '{1}' already exists`, key));

        if (options == null) {
            if (typeof func == 'object') {
                options = func;
                func = null;
            } else {
                options = {};
            }
        }

        title = title || key;
        options = expandOptions(options);

        if (options.menu == null)
            options.menu = title;

        let prev_store = current_store;
        let prev_page = current_page;
        let prev_options = options_stack;

        try {
            current_store = {
                key: key,
                title: title,
                url: ENV.urls.instance + key,
                many: null
            };

            app.stores.push(current_store);

            current_page = {
                key: key,
                title: title,
                filename: options.filename,
                url: current_store.url,

                chain: current_page?.chain.slice() ?? [],
                children: [],

                store: current_store,

                ...options
            };

            if (current_page.filename === true)
                current_page.filename = `pages/${key}.js`;

            current_page.chain.push(current_page);

            if (typeof func == 'function') {
                if (app.pages.some(page => page.key == key))
                    throw new Error(T.message(`Page '{1}' already exists`, key));

                app.pages.push(current_page);

                func();
            } else {
                options_stack = [];
                self.page(key, func || title, options);
            }

            if (prev_page != null) {
                let simplify = (current_page.children.length == 1) &&
                               (typeof func != 'function');

                if (simplify) {
                    let child0 = current_page.children[0];
                    child0.chain.splice(child0.chain.length - 2, 1);

                    prev_page.children.push(child0);
                } else {
                    prev_page.children.push(current_page);
                }
            }

            if (current_page.children.length == 1) {
                current_page.url = current_page.children[0].url;
                current_store.url = current_page.url;
            }
        } finally {
            current_page = prev_page;
            current_store = prev_store;
            options_stack = prev_options;
        }
    };

    this.many = function(key, title, plural, func = null, options = null) {
        self.form(key, title, func, options);

        let store = app.stores[app.stores.length - 1];
        store.many = plural;
    };

    this.shortcut = function(label, options = {}, func = null) {
        let shortcut = {
            label: label,
            click: func,
            icon: options.icon ?? null,
            color: options.color ?? null
        };

        app.shortcuts.push(shortcut);
    };

    function checkKeySyntax(key) {
        if (!key)
            throw new Error(T.message(`Empty keys are not allowed`));
        if (!key.match(/^[a-zA-Z_][a-zA-Z0-9_]*$/))
            throw new Error(T.message(`Characters allowed in keys: a-z, ‘_’ and 0-9 (except in the first position)`));
        if (key.startsWith('__'))
            throw new Error(T.message(`Keys must not start with '__'`));
    }

    function expandOptions(options) {
        options = Object.assign({}, options_stack[options_stack.length - 1], options);
        return options;
    }

    function makeOptionProperty(key) {
        let prop = {
            get: () => {
                let options = options_stack[options_stack.length - 1];
                return options[key];
            },

            set: value => {
                let options = {};
                options[key] = value;
                self.pushOptions(options);
            },

            enumerable: true
        };

        return prop;
    }
}

export {
    ApplicationInfo,
    ApplicationBuilder
}
