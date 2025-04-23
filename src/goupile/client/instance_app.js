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
    this.annotate = false;
    this.shortcuts = [];

    this.stores = [];

    this.panels = {
        editor: profile.develop,
        data: profile.userid > 0 && profile.lock == null
    };

    this.tags = [
        { key: 'incomplete', label: 'Incomplet', color: '#ef6e30', filter: true },
        { key: 'control', label: 'Contrôles', color: '#d9ab46', filter: true },
        { key: 'wait', label: 'En attente', color: '#3b96c8', filter: true },
        { key: 'check', label: 'À vérifier', color: '#44997c', filter: true },
        { key: 'error', label: 'Erreur', color: '#db0a0a', filter: true },
        { key: 'draft', label: 'Brouillon', color: '#d921e8', filter: true },
        { key: 'na', label: 'NA/ND/NSP', color: '#aaaaaa', filter: false }
    ];
}

function ApplicationBuilder(app) {
    let self = this;

    let current_page = null;
    let current_store = null;

    let options_stack = [
        {
            warn_unsaved: true,
            autosave: false,
            has_lock: false,
            sequence: false,
            progress: true,
            enabled: true,
            claim: true,
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

        warnUnsaved: makeOptionProperty('warn_unsaved'),
        autosave: makeOptionProperty('autosave'),
        hasLock: makeOptionProperty('has_lock'),
        sequence: makeOptionProperty('sequence'),
        progress: makeOptionProperty('progress'),
        enabled: makeOptionProperty('enabled'),
        claim: makeOptionProperty('claim'),
        confirm: makeOptionProperty('confirm'),
        icon: makeOptionProperty('icon'),
        help: makeOptionProperty('help')
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

        options = expandOptions(options);

        let prev_store = current_store;
        let prev_page = current_page;
        let prev_options = options_stack;

        try {
            current_store = {
                key: key,
                title: title,
                url: ENV.urls.instance + key
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
                    throw new Error(`Page key '${key}' is already used`);

                app.pages.push(current_page);

                func();
            } else {
                options_stack = [expandOptions(options)];
                self.page(key, func || title);
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
