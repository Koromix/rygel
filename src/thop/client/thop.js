// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Fetched from THOP server
let settings = {};

let thop = (function() {
    let self = this;

    let route_mod;

    function initLog() {
        // STUB
    }

    function initNavigation() {
        window.addEventListener('popstate', e => {
            self.go(window.location.href, {}, false);
        });

        document.body.addEventListener('click', e => {
            if (e.target && e.target.tagName == 'A' &&
                    !e.ctrlKey && !e.target.getAttribute('download')) {
                let href = e.target.getAttribute('href');
                if (href && !href.match(/^(?:[a-z]+:)?\/\//) && href[0] != '#') {
                    self.go(href);
                    e.preventDefault();
                }
            }
        });
    }

    async function initSettings() {
        settings = await fetch(`${env.base_url}api/settings.json`).then(response => response.json());

        if (settings.mco) {
            for (let version of settings.mco.versions) {
                version.begin_date = dates.fromString(version.begin_date);
                version.end_date = dates.fromString(version.end_date);
            }
        }
    }

    async function initThop() {
        initLog();
        initNavigation();
        await initSettings();

        // Initialize module routes
        mco_info.route = mco_info.parseURL('');
        mco_casemix.route = mco_casemix.parseURL('');

        self.go(window.location.href, {}, false);
    }

    this.go = async function(mod, args = {}, push_history = true) {
        // Parse URL if needed
        if (typeof mod === 'string') {
            let url = util.parseUrl(mod);
            let path = url.path.substr(env.base_url.length);

            // Who managed to f*ck the string split() limit parameter?
            let [mod_name, ...mod_path] = path.split('/');
            mod_path = mod_path.join('/');

            switch (mod_name || 'mco_info') {
                case 'mco_info': { route_mod = mco_info; } break;
                case 'mco_casemix': { route_mod = mco_casemix; } break;

                default: {
                    // Cannot make canonical URL (because it's invalid), but it's better than nothing
                    updateMenu(mod, push_history);
                    log.error('Aucun module disponible pour cette adresse');
                    return;
                } break;
            }

            args = {...route_mod.parseURL(mod_path, url.params), ...args};
        } else if (mod) {
            route_mod = mod;
        }

        Object.assign(route_mod.route, args);
        updateMenu(route_mod.makeURL(), push_history);

        // Run!
        try {
            await route_mod.run();
        } catch (err) {
            log.error(err.message);
        }
    };

    function updateMenu(current_url, push_history) {
        let links = [
            {category: 'Informations MCO', title: 'Tarifs GHS', func: () => mco_info.makeURL({mode: 'prices'})},
            {category: 'Informations MCO', title: 'Arbre de groupage', func: () => mco_info.makeURL({mode: 'tree'})}
        ];

        if (push_history) {
            window.history.pushState(null, null, current_url);
        } else {
            window.history.replaceState(null, null, current_url);
        }

        let prev_category = null;
        render(links.map(link => {
            let url = link.func();
            let active = current_url.startsWith(url);

            if (link.category === prev_category) {
                return html`<a class=${active ? 'active' : ''} href=${url}>${link.title}</a>`;
            } else {
                prev_category = link.category;
                return html`<a class="category">${link.category}</li>
                            <a class=${active ? 'active' : ''} href=${url}>${link.title}</a>`;
            }
        }), document.querySelector('#th_menu'));
    }

    document.addEventListener('readystatechange', e => {
        if (document.readyState === 'complete')
            initThop();
    });

    return this;
}).call({});
