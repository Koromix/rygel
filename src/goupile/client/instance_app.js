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

    this.pages = [];
    this.homepage = null;

    this.stores = [];

    this.panels = {
        editor: profile.develop,
        data: profile.type == 'login' && profile.lock == null
    };

    this.tags = [
        { key: 'incomplete', label: 'Incomplet', color: '#ef6e30' },
        { key: 'control', label: 'Contrôles', color: '#d9ab46' },
        { key: 'wait', label: 'En attente', color: '#3b96c8' },
        { key: 'check', label: 'À vérifier', color: '#44997c' },
        { key: 'error', label: 'Erreur', color: '#db0a0a' },
        { key: 'draft', label: 'Brouillon', color: '#d921e8' }
    ];
}

function ApplicationBuilder(app) {
    let self = this;

    let current_menu = null;
    let current_store = null;

    let options_stack = [
        {
            warn_unsaved: true,
            has_lock: false,
            sequence: false,

            export_dialog: null,
            export_filter: null
        }
    ];

    Object.defineProperties(this, {
        homepage: { get: () => app.homepage, set: homepage => { app.homepage = homepage; }, enumerable: true },
        head: { get: () => app.head, set: head => { app.head = head; }, enumerable: true },
        tags: { get: () => app.tags, set: tags => { app.tags = tags; }, enumerable: true },

        warnUnsaved: makeOptionProperty('warn_unsaved'),
        hasLock: makeOptionProperty('has_lock'),
        sequence: makeOptionProperty('sequence'),

        exportDialog: makeOptionProperty('export_dialog'),
        exportFilter: makeOptionProperty('export_filter')
    });

    this.panel = function(panel, enable) {
        if (panel.startsWith('_') || !app.hasOwnProperty(panel))
            throw new Error(`Invalid panel key '${panel}'`);

        app.panels[panel] = enable;
    };

    // Keep public for backward compatibility
    this.pushOptions = function(options = {}) {
        options = expandOptions(options);
        options_stack.push(options);
    };
    this.popOptions = function() {
        if (options_stack.length < 2)
            throw new Error('Too many popOptions() operations');

        options_stack.pop();
    };

    this.page = function(key, title, options = null) {
        checkKeySyntax(key);
        if (app.pages.some(page => page.key == key))
            throw new Error(`Page key '${key}' is already used`);

        title = title || key;
        options = expandOptions(options);

        let item = {
            key: key,
            title: title,
            url: ENV.urls.instance + key,

            chain: null,
            children: [],

            store: current_store,
        };

        let page = {
            key: key,
            title: title,
            filename: options.filename ?? `pages/${key}.js`,
            url: item.url,

            menu: item,
            store: current_store,

            options: options
        };

        app.pages.push(page);

        if (current_menu != null) {
            item.chain = [...current_menu.chain, item];
            current_menu.children.push(item);
        } else {
            item.chain = [item];
        }

        return page;
    };

    this.form = function(key, title, func = null, options = null) {
        checkKeySyntax(key);
        if (app.stores.some(store => store.key == key))
            throw new Error(`Store key '${key}' is already used`);

        title = title || key;

        if (options == null) {
            if (typeof func == 'object') {
                options = func;
                func = null;
            } else {
                options = {};
            }
        }

        let prev_store = current_store;
        let prev_menu = current_menu;
        let prev_options = options_stack;

        try {
            options_stack = [expandOptions(options)];

            current_store = {
                key: key,
                title: title,
                url: ENV.urls.instance + key,

                menu: null
            };

            current_menu = {
                key: key,
                title: title,
                url: current_store.url,

                chain: current_menu?.chain.slice() ?? [],
                children: [],

                store: current_store,
                page: null
            };

            app.stores.push(current_store);
            current_menu.chain.push(current_menu);

            if (typeof func == 'function') {
                if (app.pages.some(page => page.key == key))
                    throw new Error(`Page key '${key}' is already used`);

                let page = {
                    key: key,
                    title: title,
                    filename: options.filename,
                    url: ENV.urls.instance + key,

                    menu: current_menu,
                    store: current_store,

                    options: options
                };

                app.pages.push(page);

                func();
            } else {
                self.page(key, func || title);
            }

            if (prev_menu != null) {
                let simplify = (current_menu.children.length == 1) &&
                               (typeof func != 'function');

                if (simplify) {
                    let child0 = current_menu.children[0];
                    child0.chain.splice(child0.chain.length - 2, 1);

                    prev_menu.children.push(child0);
                    current_store.menu = child0;
                } else {
                    prev_menu.children.push(current_menu);
                    current_store.menu = current_menu;
                }
            } else {
                current_store.menu = current_menu;
            }

            if (current_menu.children.length == 1) {
                current_menu.url = current_menu.children[0].url;
                current_store.url = current_menu.url;
            }
        } finally {
            current_menu = prev_menu;
            current_store = prev_store;
            options_stack = prev_options;
        }
    };

    function checkKeySyntax(key) {
        if (!key)
            throw new Error('Empty keys are not allowed');
        if (!key.match(/^[a-zA-Z_][a-zA-Z0-9_]*$/))
            throw new Error('Allowed key characters: a-z, _ and 0-9 (not as first character)');
        if (key.startsWith('__'))
            throw new Error('Keys must not start with \'__\'');
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
