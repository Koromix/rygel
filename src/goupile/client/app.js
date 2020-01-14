// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function Application() {
    let self = this;

    this.forms = [];
    this.schedules = [];

    // These assets are always available, even in broken apps
    this.assets = [
        {
            type: 'main',
            url: `${env.base_url}main/js/`,

            category: 'Paramétrage',
            label: 'Application',
            overview: 'Application',

            path: '/files/main.js',
            edit: true
        }, {
            type: 'main',
            url: `${env.base_url}main/css/`,

            category: 'Paramétrage',
            label: 'Feuille de style',
            overview: 'Feuille de style',

            path: '/files/main.css',
            edit: true
        }
    ];

    this.home = null;

    // Initialized in goupile.js
    this.urls_map = null;
    this.paths_map = null;

    // Used for user globals
    this.data = {};
    this.route = {};
}

function FormInfo(key) {
    this.key = key;
    this.pages = [];
    this.links = [];
}

function PageInfo(form, key) {
    this.key = key;
    this.url = `${env.base_url}app/${form.key}/${key}/`;
}

function ScheduleInfo(key) {
    this.key = key;
    this.url = `${env.base_url}app/${key}/`;
}

function ApplicationBuilder(app) {
    let self = this;

    let used_keys = new Set;
    let forms_map = {};
    let used_links = new Set;

    this.start = function(url) {
        app.home = url;
    };

    this.form = function(key, func = null) {
        checkKey(key);

        let form = new FormInfo(key);
        let form_builder = new FormBuilder(app, form);
        if (func) {
            func(form_builder);
        } else {
            form_builder.page(key);
        }

        app.forms.push(form);
        forms_map[key] = form;
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

    this.schedule = function(key) {
        checkKey(key);

        let schedule = new ScheduleInfo(key);
        app.schedules.push(schedule);

        app.assets.push({
            type: 'schedule',
            url: schedule.url,

            category: 'Agendas',
            label: schedule.key,
            overview: 'Agenda',

            schedule: schedule
        });
        app.assets.push({
            type: 'schedule_settings',
            url: `${schedule.url}settings/`,

            category: 'Agendas',
            label: schedule.key,
            overview: 'Créneaux',
            secondary: true,

            schedule: schedule
        });
    };

    this.file = function(file) {
        app.assets.push({
            type: 'blob',
            url: `${env.base_url}blob${file.path}`,

            category: 'Fichiers',
            label: file.path,
            overview: 'Contenu',

            path: file.path
        });
    };

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

function FormBuilder(app, form) {
    let self = this;

    let used_keys = new Set;

    this.page = function(key) {
        if (!key)
            throw new Error('Empty keys are not allowed');
        if (!key.match(/^[a-zA-Z_][a-zA-Z0-9_]*$/))
            throw new Error('Allowed key characters: a-z, _ and 0-9 (not as first character)');
        if (used_keys.has(key))
            throw new Error(`Page '${key}' is already used in this form`);

        let page = new PageInfo(form, key);

        form.pages.push(page);
        used_keys.add(key);

        app.assets.push({
            type: 'page',
            url: page.url,

            category: `Formulaire ${form.key}`,
            label: (form.key !== page.key) ? `${form.key}/${page.key}` : form.key,
            overview: 'Formulaire',

            form: form,
            page: page,

            path: `/files/pages/${page.key}.js`,
            edit: true
        });
    };
}
