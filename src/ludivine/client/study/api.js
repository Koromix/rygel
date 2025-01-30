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

function ProjectInfo(project) {
    Object.assign(this, project);
    delete this.prepare;

    this.summary = null;

    this.root = null;
    this.modules = [];
    this.pages = [];
}

function ProjectBuilder(project) {
    let self = this;

    let current_mod = null;

    Object.defineProperties(this, {
        summary: { get: () => project.summary, set: summary => { project.summary = summary; }, enumerable: true }
    });

    this.module = function(key, title, func, options = {}) {
        if (current_mod != null && current_mod.pages.length)
            throw new Error('Cannot combine child modules and tests');

        if (!key.match(/^[a-z0-9_]+$/))
            throw new Error(`Invalid key '${key}'`);
        if (current_mod != null && current_mod.modules.some(mod => mod.key == key))
            throw new Error(`Module key '${key}' is already in use`);

        let prev_mod = current_mod;

        options = Object.assign({
            title: title,
            level: null,
            help: null
        }, options);

        try {
            current_mod = {
                key: (prev_mod?.key ?? '') + '/' + key,
                title: null,

                level: null,
                help: null,

                modules: [],
                pages: [],

                chain: []
            };

            if (prev_mod != null)
                current_mod.chain.push(...prev_mod.chain);
            current_mod.chain.push(current_mod);

            project.modules.push(current_mod);

            func(options);

            current_mod.title = options.title;
            current_mod.level = options.level;
            current_mod.help = options.help;

            for (let mod of current_mod.modules)
                current_mod.pages.push(...mod.pages);

            if (prev_mod != null) {
                prev_mod.modules.push(current_mod);
            } else {
                if (project.root != null)
                    throw new Error('Study must have only one root module');

                project.root = current_mod;
            }
        } finally {
            current_mod = prev_mod;
        }
    };

    this.test = function(key, title, form, options = {}) {
        if (current_mod == null)
            throw new Error('Cannot create test outside module');
        if (current_mod.modules.length)
            throw new Error('Cannot combine child modules and tests');

        if (!key.match(/^[a-z0-9_]+$/))
            throw new Error(`Invalid key '${key}'`);
        if (current_mod.pages.some(page => page.key == key))
            throw new Error(`Test key '${key}' is already in use`);

        let page = {
            key: current_mod.key + '/' + key,
            title: title,
            form: form,

            schedule: options.schedule ?? null
        };

        current_mod.pages.push(page);

        project.pages.push(page);
    };
}

export {
    ProjectInfo,
    ProjectBuilder
}
