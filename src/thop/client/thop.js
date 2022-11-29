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

// Fetched from THOP server
let settings = {};

const thop = new function() {
    let self = this;

    let deployer_targets = [];

    let route_mod;
    let route_url = '';
    let prev_url;
    let scroll_cache = new LruMap(128);

    let settings_rnd;

    let log_entries = [];

    this.start = async function() {
        log.pushHandler(notifyHandler);
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
            // Close all mobile menus (just in case)
            closeAllDeployedElements();

            self.go(href);
            e.preventDefault();
        });

        // Handle deploy buttons (mobile only)
        document.querySelectorAll('.th_deploy').forEach(deployer => {
            deployer.addEventListener('click', handleDeployerClick);

            let target = document.querySelector(deployer.dataset.target);
            deployer_targets.push(target);
        });
        document.querySelector('#th_view').addEventListener('click', closeAllDeployedElements);
    }

    function handleDeployerClick(e) {
        let target = document.querySelector(e.target.dataset.target);
        let open = !target.classList.contains('active');

        closeAllDeployedElements();
        target.classList.toggle('active', open);

        e.stopPropagation();
    }

    function closeAllDeployedElements() {
        deployer_targets.forEach(el => el.classList.remove('active'));
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
        {
            let hash = url.substr(url.indexOf('#'));
            url = `${route_mod.makeURL()}${hash.startsWith('#') ? hash : ''}`;
        }

        updateHistory(url, false);
        updateScroll(route_url, url);
        updateMenu(url);

        route_url = url;
    };
    this.go = util.serialize(this.go);

    this.goBack = function() { self.go(prev_url || '/'); };

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

            let path_str = url.pathname.substr(ENV.base_url.length);
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

            return `${route_mod.makeURL()}${url.hash || ''}`;
        } else {
            route_mod = mod || route_mod;
            util.assignDeep(route_mod.route, args);

            return route_mod.makeURL();
        }
    }

    async function run(push_history) {
        let view_el = document.querySelector('#th_view');

        try {
            if (!window.document.documentMode)
                view_el.classList.add('busy');

            await route_mod.run();

            view_el.classList.remove('error');
        } catch (err) {
            document.title = 'THOP (erreur)';

            render(html`⚠\uFE0E ${err.message.split('\n').map(line => [line, html`<br/>`])}`, view_el);
            log.error(err);

            view_el.classList.add('error');
        }
        view_el.classList.remove('busy');
    }

    async function updateSettings() {
        if (ENV.has_users)
            user.readSessionCookies();
        if (settings_rnd === user.getSessionRnd())
            return;

        // Clear cache, which may contain user-specific and even sensitive data
        data.clearCache();
        scroll_cache.clear();

        // Fetch new settings
        {
            // We'll parse it manually to revive dates. It's relatively small anyway.
            let url = util.pasteURL(`${ENV.base_url}api/user/settings`, {rnd: user.getSessionRnd()});
            let json = await net.fetch(url).then(response => response.text());

            settings = JSON.parse(json, (key, value) => {
                if (typeof value === 'string' && value.match(/^[0-9]{4}-[0-9]{2}-[0-9]{2}$/)) {
                    return dates.parse(value);
                } else {
                    return value;
                }
            });
            settings_rnd = user.getSessionRnd();
        }

        // Update session information
        if (ENV.has_users) {
            render(html`
                ${!user.isConnected() ?
                    html`<a href=${user.makeURL({mode: 'login'})}>Se connecter</a>` : ''}
                ${user.isConnected() ?
                    html`${settings.username} (<a href=${user.makeURL({mode: 'login'})}>changer</a>,
                                               <a @click=${handleLogoutClick}>déconnexion</a>)` : ''}
            `, document.querySelector('#th_session'));
        }
    }

    function updateHistory(url, push_history) {
        if (push_history && url !== route_url) {
            prev_url = route_url;
            window.history.pushState(null, null, url);
        } else {
            window.history.replaceState(null, null, url);
        }
    }

    function updateScroll(prev_url, new_url) {
        // Cache current scroll state
        if (!prev_url.includes('#')) {
            let prev_target = [window.pageXOffset, window.pageYOffset];
            if (prev_target[0] || prev_target[1]) {
                scroll_cache.set(prev_url, prev_target);
            } else {
                scroll_cache.delete(prev_url);
            }
        }

        // Set new scroll position: try URL hash first, use cache otherwise
        let new_hash = new_url.substr(new_url.indexOf('#'));
        if (new_hash.startsWith('#')) {
            let el = document.querySelector(new_hash);

            if (el) {
                el.scrollIntoView();
            } else {
                window.scrollTo(0, 0);
            }
        } else {
            let new_target = scroll_cache.get(new_url) || [0, 0];
            window.scrollTo(new_target[0], new_target[1]);
        }
    }

    function handleLogoutClick(e) {
        let p = user.logout();

        p.then(success => {
            if (success)
                self.go();
        });
        p.catch(log.error);
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

    function notifyHandler(action, entry) {
        if (typeof lithtml !== 'undefined' && entry.type !== 'debug') {
            switch (action) {
                case 'open': {
                    log_entries.unshift(entry);

                    if (entry.type === 'progress') {
                        // Wait a bit to show progress entries to prevent quick actions from showing up
                        setTimeout(renderLog, 300);
                    } else {
                        renderLog();
                    }
                } break;
                case 'edit': {
                    renderLog();
                } break;
                case 'close': {
                    log_entries = log_entries.filter(it => it !== entry);
                    renderLog();
                } break;
            }
        }

        log.defaultHandler(action, entry);
    };

    function renderLog() {
        let log_el = document.querySelector('#th_log');
        if (!log_el) {
            log_el = document.createElement('div');
            log_el.id = 'th_log';
            document.body.appendChild(log_el);
        }

        render(log_entries.map((entry, idx) => {
            let msg = (entry.msg instanceof Error) ? entry.msg.message : entry.msg;

            if (entry.type === 'progress') {
                return html`<div class="th_log_entry progress">
                    <div class="th_log_spin"></div>
                    ${msg.split('\n').map(line => [line, html`<br/>`])}
                </div>`;
            } else if (entry.type === 'error') {
                return html`<div class="error" @click=${e => entry.close()}>
                    <button class="ui_log_close">X</button>
                    <b>Une erreur est survenue</b><br/>
                    ${msg}
                </div>`;
            } else {
                return html`<div class=${'th_log_entry ' + entry.type} @click=${e => entry.close()}>
                    <button class="th_log_close">X</button>
                    ${msg.split('\n').map(line => [line, html`<br/>`])}
                </div>`;
            }
        }), log_el);
    }
};
