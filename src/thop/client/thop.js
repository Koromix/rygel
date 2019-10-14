// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Fetched from THOP server
let settings = {};

let thop = new function() {
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

        // Initialize module routes
        Object.assign(mco_info.route, mco_info.parseURL(''));
        Object.assign(mco_casemix.route, mco_casemix.parseURL(''));
        Object.assign(user.route, user.parseURL(''));

        // Start interface
        updateMenu('');
        self.go(window.location.href, {}, false);
    }

    function initNavigation() {
        window.addEventListener('popstate', e => self.go(window.location.href, {}, false));

        util.interceptLocalAnchors((e, href) => {
            self.go(href);
            e.preventDefault();
        });
    }

    this.go = async function(mod, args = {}, push_history = true) {
        let url = route(mod, args, push_history);
        if (!url)
            return;

        // Update URL quickly, even though we'll do it again after module run because some
        // parts may depend on fetched resources. Same thing for session.
        updateHistory(url, push_history && url !== route_url);
        await updateSettings();

        // Run!
        await run();
        await updateSettings();

        // Update again, even though we probably got it right earlier... but maybe not?
        url = route_mod.makeURL();
        updateHistory(url, false);
        updateScroll(route_url, url);
        updateMenu(url);

        route_url = url;
    };

    this.goFake = function(mod, args = {}, push_history = true) {
        let url = route(mod, args);
        if (!url)
            return;

        updateHistory(url, push_history);
        updateScroll(route_url, url);
        updateMenu(url);

        route_url = url;
    };

    function route(mod, args, push_history) {
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
                    updateScroll(route_url, mod);
                    updateMenu(mod);

                    log.error('Aucun module disponible pour cette adresse');
                    return null;
                } break;
            }

            util.assignDeep(route_mod.route, route_mod.parseURL(mod_path, params));
            util.assignDeep(route_mod.route, args);
        } else {
            route_mod = mod || route_mod;
            util.assignDeep(route_mod.route, args);
        }

        return route_mod.makeURL();
    }

    async function run(push_history) {
        let view_el = document.querySelector('#th_view');

        view_el.classList.add('th_view_busy');
        try {
            await route_mod.run();
        } catch (err) {
            render(err.message, view_el);
            log.error(err.message);
        }
        view_el.classList.remove('th_view_busy');
    }

    async function updateSettings() {
        if (env.has_users)
            user.readSessionCookies();
        if (settings_key === user.getUrlKey())
            return;

        // Clear cache, which may contain user-specific and even sensitive data
        data.clearCache();
        scroll_cache.clear();

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

    function updateHistory(url, push_history) {
        if (push_history) {
            window.history.pushState(null, null, url);
        } else {
            window.history.replaceState(null, null, url);
        }
    }

    function updateScroll(prev_url, new_url) {
        // Cache current scroll state
        let prev_target = [window.pageXOffset, window.pageYOffset];
        if (prev_target[0] || prev_target[1]) {
            scroll_cache.set(prev_url, prev_target);
        } else {
            scroll_cache.delete(prev_url);
        }

        // Restore scroll position for new URL
        let new_target = scroll_cache.get(new_url) || [0, 0];
        window.scrollTo(new_target[0], new_target[1]);
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

    function updateMenu(current_url) {
        render(html`
            <a class="category">Informations MCO</a>
            ${makeMenuLink('Racines de GHM', mco_info.makeURL({mode: 'ghm_roots'}), current_url)}
            ${makeMenuLink('Tarifs GHS', mco_info.makeURL({mode: 'ghs'}), current_url)}
            ${makeMenuLink('Arbre de groupage', mco_info.makeURL({mode: 'tree'}), current_url)}
            ${makeMenuLink('GHM / GHS', mco_info.makeURL({mode: 'ghmghs'}), current_url)}
            ${makeMenuLink('Diagnostics', mco_info.makeURL({mode: 'diagnoses'}), current_url)}
            ${makeMenuLink('Actes', mco_info.makeURL({mode: 'procedures'}), current_url)}

            ${settings.permissions.mco_casemix ? html`
                <a class="category">Activité MCO</a>
                ${makeMenuLink('GHM', mco_casemix.makeURL({mode: 'ghm'}), current_url)}
                ${makeMenuLink('Unités', mco_casemix.makeURL({mode: 'units'}), current_url)}
                ${settings.permissions.mco_results ?
                    makeMenuLink('Résumés (RSS)', mco_casemix.makeURL({mode: 'rss'}), current_url) : ''}
                ${makeMenuLink('Valorisation', mco_casemix.makeURL({mode: 'valorisation'}), current_url)}
            ` : ''}
        `, document.querySelector('#th_menu'));
    }

    function makeMenuLink(label, url, current_url) {
        let active = current_url.startsWith(url);
        return html`<a class=${active ? 'active': ''} href=${url}>${label}</a>`;
    }
};
