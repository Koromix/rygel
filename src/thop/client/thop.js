// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Fetched from THOP server
let settings = {};

let thop = (function() {
    let self = this;

    let route_mod;
    let route_url = '';
    let scroll_cache = new LruMap(128);

    let settings_key;

    document.addEventListener('readystatechange', e => {
        if (document.readyState === 'complete')
            initThop();
    });

    async function initThop() {
        log.pushHandler(log.notifyHandler);
        initNavigation();

        // We deal with this
        if (window.history.scrollRestoration)
            window.history.scrollRestoration = 'manual';

        // Update settings
        await updateSettings();

        updateMenu();
        self.go(window.location.href, {}, false);
    }

    function initNavigation() {
        window.addEventListener('popstate', e => {
            self.go(window.location.href, {}, false);
        });

        document.body.addEventListener('click', e => {
            if (e.target && e.target.tagName == 'A' && !e.defaultPrevented &&
                    !e.ctrlKey && !e.target.getAttribute('download')) {
                let href = e.target.getAttribute('href');
                if (href && !href.match(/^(?:[a-z]+:)?\/\//) && href[0] != '#') {
                    self.go(href);
                    e.preventDefault();
                }
            }
        });
    }

    this.go = async function(mod, args = {}, push_history = true) {
        // Update module and route
        if (typeof mod === 'string') {
            let url = new URL(mod, window.location.href);

            let path_str = url.pathname.substr(env.base_url.length);
            let [mod_name, ...mod_path] = path_str.split('/');

            let params = {};
            for (let [key, value] of url.searchParams)
                params[key] = value;

            switch (mod_name || 'mco_info') {
                case 'mco_info': { route_mod = mco_info; } break;
                case 'mco_casemix': { route_mod = mco_casemix; } break;
                case 'user': { route_mod = user; } break;

                default: {
                    // Cannot make canonical URL (because it's invalid), but it's better than nothing
                    updateHistory(mod, push_history);
                    updateMenu();

                    log.error('Aucun module disponible pour cette adresse');
                    return;
                } break;
            }

            util.assignDeep(route_mod.route, route_mod.parseURL(mod_path, params));
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
        // parts may depend on fetched resources. Same thing for session.
        updateHistory(route_mod.makeURL(), push_history);
        await updateSettings();

        let view_el = document.querySelector('#th_view');

        // Run!
        view_el.classList.add('th_view_busy');
        try {
            await route_mod.run();
        } catch (err) {
            render('', document.querySelector('#th_options'));
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
        updateHistory(route_mod.makeURL(), false);
        await updateSettings();
        updateMenu();
    };

    function updateHistory(url, push_history) {
        if (push_history && url !== route_url) {
            window.history.pushState(null, null, url);
        } else {
            window.history.replaceState(null, null, url);
        }

        route_url = url;
    }

    async function updateSettings() {
        if (env.has_users)
            user.readSessionCookies();
        if (settings_key === user.getUrlKey())
            return;

        // Clear cache, which may contain user-specific and even sensitive data
        data.clearCache();

        // Fetch new settings
        {
            // We'll parse it manually to revive dates. It's relatively small anyway.
            let json = await fetch(`${env.base_url}api/settings.json?key=${user.getUrlKey()}`).then(response => response.text());

            settings = JSON.parse(json, (key, value) => {
                if (typeof value === 'string' && value.match(/^[0-9]{4}-[0-9]{2}-[0-9]{2}$/)) {
                    return dates.fromString(value);
                } else {
                    return value;
                }
            });
            settings_key = user.getUrlKey();
        }

        // Initialize module routes
        for (let mod of [mco_info, mco_casemix, user]) {
            util.clearObject(mod.route);
            Object.assign(mod.route, mod.parseURL(''));
        }

        // Update session information
        if (env.has_users) {
            render(html`
                ${!user.isConnected() ?
                    html`<a href=${user.makeURL({mode: 'login'})}>Se connecter</a>` : ''}
                ${user.isConnected() ?
                    html`${user.getUserName()} (<a href=${user.makeURL({mode: 'login'})}>changer</a>,
                                                <a href="#" @click=${handleLogoutClick}>déconnexion</a>)` : ''}
            `, document.querySelector('#th_session'));
        }
    }

    function handleLogoutClick(e) {
        let p = user.logout();

        p.then(success => {
            if (success)
                self.go();
        });
        p.catch(log.error);

        e.preventDefault();
    }

    function updateMenu() {
        render(html`
            <a class="category">Informations MCO</a>
            ${makeMenuLink('Tarifs GHS', mco_info.makeURL({mode: 'ghs'}))}
            ${makeMenuLink('Arbre de groupage', mco_info.makeURL({mode: 'tree'}))}
            ${makeMenuLink('GHM / GHS', mco_info.makeURL({mode: 'ghmghs'}))}
            ${makeMenuLink('Diagnostics', mco_info.makeURL({mode: 'diagnoses'}))}
            ${makeMenuLink('Actes', mco_info.makeURL({mode: 'procedures'}))}

            ${settings.permissions.mco_casemix ? html`
                <a class="category">Activité MCO</a>
                ${makeMenuLink('GHM', mco_casemix.makeURL({mode: 'ghm'}))}
                ${makeMenuLink('Unités', mco_casemix.makeURL({mode: 'units'}))}
                ${settings.permissions.mco_results ?
                    makeMenuLink('Résumés (RSS)', mco_casemix.makeURL({mode: 'rss'})) : ''}
                ${makeMenuLink('Valorisation', mco_casemix.makeURL({mode: 'valorisation'}))}
            ` : ''}
        `, document.querySelector('#th_menu'));
    }

    function makeMenuLink(label, url) {
        let active = route_url.startsWith(url);
        return html`<a class=${active ? 'active': ''} href=${url}>${label}</a>`;
    }

    return this;
}).call({});
