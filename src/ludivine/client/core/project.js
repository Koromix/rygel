// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import { Util, Log } from '../../../web/core/base.js';

function ProjectInfo(project) {
    Object.assign(this, project);

    this.description = null;
    this.summary = null;

    this.root = null;
    this.pages = [];
    this.tests = [];
}

function ProjectBuilder(project) {
    let self = this;

    let current_page = null;

    Object.defineProperties(this, {
        description: { get: () => project.description, set: description => { project.description = description; }, enumerable: true },
        summary: { get: () => project.summary, set: summary => { project.summary = summary; }, enumerable: true }
    });

    this.module = function(key, title, func, options = {}) {
        if (current_page != null && current_page.tests.length)
            throw new Error('Cannot combine child modules and tests');

        if (!key.match(/^[a-z0-9_]+$/))
            throw new Error(`Invalid key '${key}'`);
        if (current_page != null && current_page.modules.some(child => child.key == key))
            throw new Error(`Module key '${key}' is already in use`);

        let prev_page = current_page;

        options = Object.assign({
            title: title,
            level: null,
            help: null,
            step: null
        }, options);

        try {
            current_page = {
                type: 'module',

                key: (current_page?.key ?? '') + '/' + key,
                title: null,
                chain: [],

                level: null,
                help: null,
                progress: null,

                modules: [],
                tests: []
            };

            if (prev_page != null)
                current_page.chain.push(...prev_page.chain);
            current_page.chain.push(current_page);

            project.pages.push(current_page);

            func(options);

            current_page.title = options.title;
            current_page.level = options.level;
            current_page.help = options.help;
            current_page.step = options.step;

            for (let child of current_page.modules)
                current_page.tests.push(...child.tests);

            if (prev_page != null) {
                prev_page.modules.push(current_page);
            } else {
                if (project.root != null)
                    throw new Error('Study must have only one root module');

                project.root = current_page;
            }
        } finally {
            current_page = prev_page;
        }
    };

    this.form = function(key, title, build, options = {}) {
        test(key, title, 'form', { build: build }, options);
    };

    this.network = function(key, title, options = {}) {
        test(key, title, 'network', {}, options);
    };

    function test(key, title, type, obj, options) {
        if (current_page == null)
            throw new Error('Cannot create test outside module');
        if (current_page.modules.length)
            throw new Error('Cannot combine child modules and tests');

        if (!key.match(/^[a-z0-9_]+$/))
            throw new Error(`Invalid key '${key}'`);
        if (current_page.tests.some(page => page.key == key))
            throw new Error(`Test key '${key}' is already in use`);

        let page = {
            type: type,

            key: current_page.key + '/' + key,
            title: title,
            chain: [],

            schedule: options.schedule ?? null,
            ...obj
        };

        page.chain.push(...current_page.chain);
        page.chain.push(page);

        current_page.tests.push(page);

        project.pages.push(page);
        project.tests.push(page);
    }
}

export {
    ProjectInfo,
    ProjectBuilder
}
