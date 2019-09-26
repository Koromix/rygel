// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Fetched from THOP server
let settings = {};

let thop = (function() {
    let self = this;

    let route_mod;
    let route_url = '';
    let scroll_cache = new LruMap(32);

    document.addEventListener('readystatechange', e => {
        if (document.readyState === 'complete')
            initThop();
    });

    async function initThop() {
        log.pushHandler(log.notifyHandler);
        initNavigation();
        await initSettings();

        // We deal with this
        if (window.history.scrollRestoration)
            window.history.scrollRestoration = 'manual';

        // Initialize module routes
        Object.assign(mco_info.route, mco_info.parseURL(''));
        Object.assign(mco_casemix.route, mco_casemix.parseURL(''));
        Object.assign(user.route, user.parseURL(''));

        // Initialize some UI elements
        updateMenu();
        updateSession();

        self.go(window.location.href, {}, false);
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

    this.go = async function(mod, args = {}, push_history = true) {
        // Update module and route
        if (typeof mod === 'string') {
            let url = util.parseUrl(mod);
            let path = url.path.substr(env.base_url.length);

            // Who managed to f*ck the string split() limit parameter?
            let [mod_name, ...mod_path] = path.split('/');
            mod_path = mod_path.join('/');

            switch (mod_name || 'mco_info') {
                case 'mco_info': { route_mod = mco_info; } break;
                case 'mco_casemix': { route_mod = mco_casemix; } break;
                case 'user': { route_mod = user; } break;

                default: {
                    // Cannot make canonical URL (because it's invalid), but it's better than nothing
                    route_url = mod;
                    updateHistory(push_history);
                    updateMenu();

                    log.error('Aucun module disponible pour cette adresse');
                    return;
                } break;
            }

            util.assignDeep(route_mod.route, route_mod.parseURL(mod_path, url.params));
            util.assignDeep(route_mod.route, args);
        } else {
            route_mod = mod || route_mod;
            util.assignDeep(route_mod.route, args);
        }

        // Memorize current scroll state
        if (window.history.scrollRestoration || push_history) {
            let target = [window.pageXOffset, window.pageYOffset];
            if (target[0] || target[1]) {
                scroll_cache.set(route_url, target);
            } else {
                scroll_cache.delete(route_url);
            }
        }

        // Update URL quickly, even though we'll do it again after module run because some
        // parts may depend on fetched resources.
        route_url = route_mod.makeURL();
        updateHistory(push_history);

        let view_el = document.querySelector('#th_view');

        // Run!
        view_el.classList.add('th_view_busy');
        try {
            await route_mod.run();
        } catch (err) {
            render(err.message, view_el);
            log.error(err.message);
        }
        view_el.classList.remove('th_view_busy');

        // Set appropriate scroll state
        if (window.history.scrollRestoration || push_history) {
            let target = scroll_cache.get(route_url) || [0, 0];
            window.scrollTo(target[0], target[1]);
        }

        // Update shared state and UI
        route_url = route_mod.makeURL();
        updateHistory(false);
        updateSession();
        updateMenu();
    };

    function updateSession() {
        if (env.has_users) {
            user.readSessionCookies();

            render(html`
                ${!user.isConnected() ?
                    html`<a href=${user.makeURL({mode: 'login'})}>Se connecter</a>` : html``}
                ${user.isConnected() ?
                    html`${user.getUserName()} (<a href=${user.makeURL({mode: 'login'})}>changer</a>,
                                                <a href="#" @click=${e => { user.logout(); e.preventDefault(); }}>déconnexion</a>)` : html``}
            `, document.querySelector('#th_session'));
        }
    }

    function updateHistory(push_history) {
        if (push_history) {
            window.history.pushState(null, null, route_url);
        } else {
            window.history.replaceState(null, null, route_url);
        }
    }

    function updateMenu() {
        let links = [
            {category: 'Informations MCO', title: 'Tarifs GHS', func: () => mco_info.makeURL({mode: 'ghs'})},
            {category: 'Informations MCO', title: 'Arbre de groupage', func: () => mco_info.makeURL({mode: 'tree'})}
        ];

        let prev_category = null;
        render(links.map(link => {
            let url = link.func();
            let active = route_url.startsWith(url);

            if (link.category === prev_category) {
                return html`<a class=${active ? 'active' : ''} href=${url}>${link.title}</a>`;
            } else {
                prev_category = link.category;
                return html`<a class="category">${link.category}</li>
                            <a class=${active ? 'active' : ''} href=${url}>${link.title}</a>`;
            }
        }), document.querySelector('#th_menu'));
    }

    return this;
}).call({});
