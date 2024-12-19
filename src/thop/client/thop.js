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

import { render, html } from '../../../vendor/lit-html/lit-html.bundle.js';
import { Util, Log, Net, Mutex, LruMap, LocalDate } from '../../web/core/base.js';
import { DataCache } from './utility.js';
import * as McoInfoMod from './mco_info.js';
import * as McoCasemixMod from './mco_casemix.js';
import * as SessionMod from './session.js';

import '../../../vendor/opensans/OpenSans.css';
import './thop.css';

// Fetched from THOP server
let settings = {};
let settings_rnd;

let deployer_targets = [];

let route_mod;
let route_url = '';
let prev_url;
let scroll_cache = new LruMap(128);

let log_entries = [];

// Explicit mutex to serialize (global) state-changing functions
let mutex = new Mutex;

async function start() {
    Log.pushHandler(notifyHandler);
    initNavigation();

    // We deal with this
    if (window.history.scrollRestoration)
        window.history.scrollRestoration = 'manual';

    // Update settings
    await updateSettings();

    // Initialize module routes
    Object.assign(McoInfoMod.route, McoInfoMod.parseURL(''));
    Object.assign(McoCasemixMod.route, McoCasemixMod.parseURL(''));
    Object.assign(SessionMod.route, SessionMod.parseURL(''));

    // Start interface
    updateMenu('');
    await go(window.location.href, {}, false);

    // Ready!
    document.body.classList.remove('th_loading');
}

function initNavigation() {
    window.addEventListener('popstate', e => go(window.location.href, {}, false));

    Util.interceptLocalAnchors((e, href) => {
        // Close all mobile menus (just in case)
        closeAllDeployedElements();

        go(href);
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

async function go(url, args = {}, push_history = true) {
    await mutex.run(async () => {
        let new_url = route(url, args, push_history);

        if (!new_url)
            return;

        // Update URL quickly, even though we'll do it again after module run because some
        // parts may depend on fetched resources. Same thing for session.
        updateHistory(new_url, push_history && new_url !== route_url);
        await updateSettings();

        // Run!
        await run();
        await updateSettings();

        // Update again, even though we probably got it right earlier... but maybe not?
        {
            let hash = new_url.substr(new_url.indexOf('#'));
            new_url = `${route_mod.makeURL()}${hash.startsWith('#') ? hash : ''}`;
        }

        updateHistory(new_url, false);
        updateScroll(route_url, new_url);
        updateMenu(new_url);

        route_url = new_url;
    });
}

function goFake(url, args = {}, push_history = true) {
    let new_url = route(url, args);

    if (!new_url)
        return;

    updateHistory(new_url, push_history);
    updateScroll(route_url, new_url);
    updateMenu(new_url);

    route_url = new_url;
}

function goBack() { go(prev_url || '/'); }

function route(url, args, push_history) {
    if (typeof url === 'string') {
        let new_url = new URL(url, window.location.href);

        let path_str = new_url.pathname.substr(ENV.base_url.length);
        let [mod_name, ...mod_path] = path_str.split('/');

        let params = {};
        for (let [key, value] of new_url.searchParams)
            params[key] = value;

        switch (mod_name || 'mco_info') {
            case 'mco_info': { route_mod = McoInfoMod; } break;
            case 'mco_casemix': { route_mod = McoCasemixMod; } break;
            case 'user': { route_mod = SessionMod; } break;

            default: {
                // Cannot make canonical URL (because it's invalid), but it's better than nothing
                updateHistory(url, push_history);
                updateScroll(route_url, url);
                updateMenu(url);

                Log.error('Aucun module disponible pour cette adresse');
                return null;
            } break;
        }

        Util.assignDeep(route_mod.route, route_mod.parseURL(mod_path, params));
        Util.assignDeep(route_mod.route, args);

        return `${route_mod.makeURL()}${new_url.hash || ''}`;
    } else {
        switch (url) {
            case McoInfoMod.route: { route_mod = McoInfoMod; } break;
            case McoCasemixMod.route: { route_mod = McoCasemixMod; } break;
            case SessionMod.route: { route_mod = SessionMod; } break;
        }

        Util.assignDeep(route_mod.route, args);

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
        Log.error(err);

        view_el.classList.add('error');
    }
    view_el.classList.remove('busy');
}

async function updateSettings() {
    if (ENV.has_users)
        SessionMod.readSessionCookies();
    if (settings_rnd === SessionMod.getSessionRnd())
        return;

    // Clear cache, which may contain user-specific and even sensitive data
    DataCache.clear();
    scroll_cache.clear();

    // Fetch new settings
    {
        // We'll parse it manually to revive dates. It's relatively small anyway.
        let url = Util.pasteURL(`${ENV.base_url}api/user/settings`, { rnd: SessionMod.getSessionRnd() });
        let new_settings = await Net.get(url);

        fixDates(new_settings);

        settings = new_settings;
        settings_rnd = SessionMod.getSessionRnd();
    }

    // Update session information
    if (ENV.has_users) {
        render(html`
            ${!SessionMod.isConnected() ?
                html`<a href=${SessionMod.makeURL({ mode: 'login' })}>Se connecter</a>` : ''}
            ${SessionMod.isConnected() ?
                html`${settings.username} (<a href=${SessionMod.makeURL({ mode: 'login' })}>changer</a>,
                                           <a @click=${handleLogoutClick}>déconnexion</a>)` : ''}
        `, document.querySelector('#th_session'));
    }
}

function fixDates(obj) {
    for (let key in obj) {
        let value = obj[key];

        if (typeof value == 'string' && value.match(/^[0-9]{4}-[0-9]{2}-[0-9]{2}$/)) {
           obj[key] = LocalDate.parse(value);
        } else if (Array.isArray(value)) {
            for (let item of value)
                fixDates(item);
        } else if (Util.isPodObject(value)) {
            fixDates(value);
        }
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
    let p = SessionMod.logout();

    p.then(success => {
        if (success)
            go();
    });
    p.catch(Log.error);
}

function updateMenu(current_url) {
    render(html`
        <a class="category">Informations MCO</a>
        ${makeMenuLink('Racines de GHM', McoInfoMod.makeURL({ mode: 'ghm_roots' }), current_url)}
        ${makeMenuLink('Tarifs GHS', McoInfoMod.makeURL({ mode: 'ghs' }), current_url)}
        ${makeMenuLink('Arbre de groupage', McoInfoMod.makeURL({ mode: 'tree' }), current_url)}
        ${makeMenuLink('GHM / GHS', McoInfoMod.makeURL({ mode: 'ghmghs' }), current_url)}
        ${makeMenuLink('Diagnostics', McoInfoMod.makeURL({ mode: 'diagnoses' }), current_url)}
        ${makeMenuLink('Actes', McoInfoMod.makeURL({ mode: 'procedures' }), current_url)}

        ${settings.permissions.mco_casemix ? html`
            <a class="category">Activité MCO</a>
            ${makeMenuLink('GHM', McoCasemixMod.makeURL({ mode: 'ghm' }), current_url)}
            ${makeMenuLink('Unités', McoCasemixMod.makeURL({ mode: 'units' }), current_url)}
            ${settings.permissions.mco_results ?
                makeMenuLink('Résumés (RSS)', McoCasemixMod.makeURL({ mode: 'rss' }), current_url) : ''}
            ${makeMenuLink('Valorisation', McoCasemixMod.makeURL({ mode: 'valorisation' }), current_url)}
        ` : ''}
    `, document.querySelector('#th_menu'));
}

function makeMenuLink(label, url, current_url) {
    let active = current_url.startsWith(url);
    return html`<a class=${active ? 'active': ''} href=${url}>${label}</a>`;
}

function notifyHandler(action, entry) {
    if (entry.type !== 'debug') {
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

    Log.defaultHandler(action, entry);
}

function renderLog() {
    let log_el = document.querySelector('#th_log');
    if (!log_el) {
        log_el = document.createElement('div');
        log_el.id = 'th_log';
        document.body.appendChild(log_el);
    }

    render(log_entries.map((entry, idx) => {
        let msg = (entry.msg instanceof Error) ? entry.msg.message : entry.msg;

        if (typeof msg === 'string')
            msg = msg.split('\n').map(line => [line, html`<br/>`]);

        if (entry.type === 'progress') {
            return html`<div class="progress">
                <div class="th_log_spin"></div>
                ${msg}
            </div>`;
        } else if (entry.type === 'error') {
            return html`<div class="error" @click=${e => entry.close()}>
                <button class="th_log_close">X</button>
                <b>Une erreur est survenue</b><br/>
                ${msg}
            </div>`;
        } else {
            return html`<div class=${entry.type} @click=${e => entry.close()}>
                <button class="th_log_close">X</button>
                ${msg}
            </div>`;
        }
    }), log_el);
}

export {
    settings,

    start,

    go,
    goFake,
    goBack
}
