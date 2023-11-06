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

function ApplicationInfo(profile) {
    this.head = null;

    this.pages = [];
    this.homepage = null;

    this.stores = [];

    this.panels = {
        editor: profile.develop,
        data: profile.userid > 0 && profile.lock == null
    };

    this.tags = [
        { key: 'incomplete', label: 'Incomplet', color: '#ef6e30' },
        { key: 'control', label: 'Contrôles', color: '#d9ab46' },
        { key: 'wait', label: 'En attente', color: '#3b96c8' },
        { key: 'check', label: 'À vérifier', color: '#44997c' },
        { key: 'locked', label: 'Terminé', color: '#d921e8' }
    ];
}

function ApplicationBuilder(app) {
    let self = this;

    let current_menu = null;
    let current_store = null;

    let options_stack = [
        {
            warn_unsaved: true,

            export_dialog: null,
            export_filter: null
        }
    ];

    Object.defineProperties(this, {
        head: { get: () => app.head, set: head => { app.head = head; }, enumerable: true },
        tags: { get: () => app.tags, set: tags => { app.tags = tags; }, enumerable: true },

        warnUnsaved: makeOptionProperty('warn_unsaved'),

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

        let page = {
            key: key,
            title: title,
            filename: `pages/${key}.js`,
            url: ENV.urls.instance + key,

            menu: null,
            store: current_store,

            options: options
        };

        let item = {
            key: key,
            title: title,
            url: page.url,

            chain: null,
            children: [],

            page: page
        };

        app.pages.push(page);

        if (current_menu != null) {
            item.chain = [...current_menu.chain, item];

            for (let item of current_menu.chain) {
                if (item.url == null)
                    item.url = page.url;
            }

            current_menu.children.push(item);
        } else {
            item.chain = [item];
        }
        page.menu = item;

        return page;
    };

    this.form = function(key, title, func = null, options = null) {
        checkKeySyntax(key);
        if (app.stores.some(store => store.key == key))
            throw new Error(`Store key '${key}' is already used`);

        title = title || key;

        if (options == null && typeof func == 'object') {
            options = func;
            func = null;
        }

        let prev_store = current_store;
        let prev_menu = current_menu;
        let prev_options = options_stack;

        try {
            current_menu = {
                key: key,
                title: title,
                url: null,

                chain: (current_menu != null) ? current_menu.chain.slice() : [],
                children: [],

                page: null
            };
            current_menu.chain.push(current_menu);
            current_store = key;

            options_stack = [expandOptions(options)];

            let store = {
                key: key,
                title: title,
                url: null
            };

            app.stores.push(store);

            if (typeof func == 'function') {
                func();
            } else {
                self.page(key, func || title);
            }

            if (prev_menu != null && current_menu.children.length) {
                if (current_menu.children.length > 1) {
                    prev_menu.children.push(current_menu);
                } else {
                    let item0 = current_menu.children[0];
                    item0.chain.splice(item0.chain.length - 2, 1);
                    prev_menu.children.push(item0);
                }
            }

            store.url = current_menu.url;
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
