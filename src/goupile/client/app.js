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

    this.pages = new Map;
    this.homepage = null;

    this.stores = new Set;

    this.panels = {
        editor: profile.develop,
        data: profile.userid > 0 && profile.lock == null,
        view: true
    };
}

function ApplicationBuilder(app) {
    let self = this;

    let current_menu = null;
    let current_store = null;

    let options_stack = [
        {
            warn_unsaved: true
        }
    ];

    this.panel = function(panel, enable) {
        if (panel.startsWith('_') || !app.hasOwnProperty(panel))
            throw new Error(`Invalid panel key '${panel}'`);

        app.panels[panel] = enable;
    };

    this.pushOptions = function(options = {}) {
        options = expandOptions(options);
        options_stack.push(options);
    };
    this.popOptions = function() {
        if (options_stack.length < 2)
            throw new Error('Too many popOptions() operations');

        options_stack.pop();
    };

    this.head = function(head) { app.head = head; };

    this.page = function(key, title, options = null) {
        checkKeySyntax(key);
        if (app.pages.has(key))
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

        app.pages.set(key, page);

        return page;
    };

    this.form = function(key, title, func = null, options = null) {
        checkKeySyntax(key);
        if (app.stores.has(key))
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

            app.stores.add(key);
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
}
