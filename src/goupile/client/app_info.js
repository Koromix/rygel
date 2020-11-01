// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function ApplicationInfo() {
    let self = this;

    this.forms = [];
    this.assets = [];

    // Initialized by goupile.js and main script
    this.home = null;
    this.urls_map = util.arrayToObject(this.assets, asset => asset.url);
    this.aliases_map = {};
    this.paths_map = {};

    // Used for user globals
    this.shared = {};
}

function ApplicationBuilder(app) {
    let self = this;

    let options_stack = [{
        show_id: true,
        show_data: true,
        make_seq: record => record.sequence || 'local',
        default_actions: true,
        float_actions: true,
        use_validation: false,
        dependencies: []
    }];

    let used_keys = new Set;
    let forms_map = {};
    let used_links = new Set;

    this.pushOptions = function(options = {}) {
        options = expandOptions(options);
        options_stack.push(options);
    };
    this.popOptions = function() {
        if (options_stack.length < 2)
            throw new Error('Too many popOptions() operations');

        options_stack.pop();
    };

    this.start = function(url) {
        app.home = url;
    };

    this.form = function(key, func = null) {
        checkKey(key);

        let form = {
            key: key,
            url: `${env.base_url}app/${key}/`,

            pages: [],
            links: []
        };

        let form_builder = new FormBuilder(form, expandOptions);
        if (typeof func === 'function') {
            let prev_options = options_stack;

            try {
                options_stack = [options_stack[options_stack.length - 1]];
                func(form_builder);
            } finally {
                options_stack = prev_options;
            }

            if (!form.pages.length)
                throw new Error(`Impossible de créer le formulaire '${key}' sans page`);
        } else {
            form_builder.page(key, func);
        }

        app.forms.push(form);
        forms_map[key] = form;

        for (let page of form.pages) {
            pushAsset({
                type: 'page',
                url: page.url,

                category: `Formulaire ${form.key}`,
                label: page.label,
                overview: 'Formulaire',

                form: form,
                page: page,

                path: `/files/pages/${page.key}.js`
            });
        }

        app.aliases_map[`${env.base_url}app/${key}/`] = app.urls_map[form.pages[0].url];
    };

    this.link = function(key1, key2) {
        let [form1, form2] = findLinkForms(key1, key2);

        form1.links.push(form2);
        form2.links.push(form1);
    };

    this.linkMany = function(key1, key2) {
        let [form1, form2] = findLinkForms(key1, key2);

        // XXX: Handle the reverse link (one-to-many)
        form2.links.push(form1);
    };

    function findLinkForms(key1, key2) {
        if (!key1 || !key2)
            throw new Error('Form key is missing');
        if (key1 === key2)
            throw new Error(`Cannot link form '${key1}' to itself`);

        if (key1 > key2)
            [key1, key2] = [key2, key1];
        if (used_links.has(`${key1}::${key2}`))
            throw new Error(`There is already a link between '${key1}' and '${key2}'`);
        used_links.add(`${key1}::${key2}`);

        let form1 = forms_map[key1];
        let form2 = forms_map[key2];
        if (!form1)
            throw new Error(`Form '${key1}' does not exist`);
        if (!form2)
            throw new Error(`Form '${key2}' does not exist`);

        return [form1, form2];
    }

    function expandOptions(options) {
        options = Object.assign({}, options_stack[options_stack.length - 1], options);
        return options;
    }

    function pushAsset(asset) {
        app.assets.push(asset);

        app.urls_map[asset.url] = asset;
        app.paths_map[asset.path] = asset;
    }

    function checkKey(key) {
        if (!key)
            throw new Error('Empty keys are not allowed');
        if (!key.match(/^[a-zA-Z_][a-zA-Z0-9_]*$/))
            throw new Error('Allowed key characters: a-z, _ and 0-9 (not as first character)');
        if (used_keys.has(key))
            throw new Error(`Asset '${key}' already exists`);

        used_keys.add(key);
    }
}

function FormBuilder(form, expand_func = options => options) {
    let self = this;

    let used_keys = new Set;

    this.page = function(key, label = undefined, options = {}) {
        if (!key)
            throw new Error('Empty keys are not allowed');
        if (!key.match(/^[a-zA-Z_][a-zA-Z0-9_]*$/))
            throw new Error('Allowed key characters: a-z, _ and 0-9 (not as first character)');
        if (used_keys.has(key))
            throw new Error(`Page '${key}' is already used in this form`);

        options = expand_func(options);
        options.dependencies = options.dependencies.filter(dep => dep !== key);

        let page = {
            key: key,
            label: label || key,
            url: `${env.base_url}app/${form.key}/${key}/`,

            form: form,
            options: options
        };
        form.pages.push(page);

        used_keys.add(key);
    };
}
