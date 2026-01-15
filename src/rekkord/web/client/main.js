// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import { render, html, live, classMap, unsafeHTML } from 'vendor/lit-html/lit-html.bundle.js';
import dayjs from 'vendor/dayjs/dayjs.bundle.js';
import { Util, Log, Net, HttpError } from 'lib/web/base/base.js';
import { Base64 } from 'lib/web/base/mixer.js';
import * as UI from 'lib/web/ui/ui.js';
import { deploy } from 'lib/web/flat/static.js';
import * as UserMod from './user.js';
import * as PlanMod from './plan.js';
import * as RepositoryMod from './repository.js';
import { ASSETS } from '../assets/assets.js';

import en from '../../i18n/en.json';
import fr from '../../i18n/fr.json';

import '../assets/client.css';

const RUN_LOCK = 'run';

const MODES = {
    login: { run: UserMod.runLogin, session: false },
    oidc: { run: UserMod.runOidc, session: false, },
    register: { run: UserMod.runRegister, session: false },
    finalize: { run: UserMod.runFinalize, session: false },
    recover: { run: UserMod.runRecover, session: false },
    reset: { run: UserMod.runReset, session: false },
    link: { run: UserMod.runLink, session: false },
    account: { run: UserMod.runAccount, session: true },

    repositories: { run: RepositoryMod.runRepositories, session: true },
    repository: { run: RepositoryMod.runRepository, session: true, path: [{ key: 'repository', type: 'integer' }]},

    plans: { run: PlanMod.runPlans, session: true },
    plan: { run: PlanMod.runPlan, session: true, path: [{ key: 'plan', type: 'integer' }]},
};
const DEFAULT_MODE = 'repositories';

let languages = {};

let route = {
    mode: null,
    repository: null,
    plan: null
};
let route_url = null;
let poisoned = false;
let pending = -1;

let session = null;
let ping_timer = null;

let root_el = null;

let cache = {
    repositories: [],
    repository: null,
    plans: [],
    plan: null
};

// ------------------------------------------------------------------------
// Init
// ------------------------------------------------------------------------

async function start() {
    languages.en = en;
    languages.fr = fr;

    // Init locale
    {
        let lang = Util.getCookie('lang');
        Util.initLocales(languages, 'en', lang);

        dayjs.locale(document.documentElement.lang);
        Util.setCookie('lang', document.documentElement.lang, '/', 365 * 86400);
    }

    UI.init(run, renderApp);

    // Handle internal links
    Util.interceptLocalAnchors((e, href) => {
        let url = new URL(href, window.location.href);

        let func = UI.wrap(e => go(url));
        func(e);

        e.preventDefault();

        let top = document.querySelector('#top');
        top.classList.remove('active');
    });

    // Handle back navigation
    window.addEventListener('popstate', e => {
        UI.closeDialog();
        go(window.location.href, false);
    });

    // Prevent loss of data
    window.onbeforeunload = e => {
        if (UI.isDialogOpen())
            return T.confirm_or_close_dialog;
    };

    // Retrieve session if there seems to be one
    {
        let rnd = Util.getCookie('session_rnd');

        if (rnd != null)
            session = await Net.get('/api/session/info');
    }

    await go(window.location.href, false);

    UI.main();
    document.body.classList.remove('loading');
}

// ------------------------------------------------------------------------
// Run
// ------------------------------------------------------------------------

function go(url = null, push = true) {
    if (url == null)
        return run({}, push);

    let changes = Object.assign({}, route);

    if (!(url instanceof URL))
        url = new URL(url, window.location.href);

    let path = url.pathname.replace(/\/+$/, '');
    let parts = path.slice(1).split('/');
    let mode = parts.shift();

    switch (mode) {
        case 'login': {
            if (isLogged()) {
                changes.mode = DEFAULT_MODE;
            } else {
                changes.mode = 'login';
            }
        } break;

        case 'register':
        case 'finalize':
        case 'recover':
        case 'reset': { changes.mode = mode; } break;

        default: {
            let info = MODES[mode];

            if (info == null) {
                mode = DEFAULT_MODE;
                info = MODES[mode];
            }

            if (info.path != null) {
                for (let i = 0; i < info.path.length; i++) {
                    let arg = info.path[i];
                    let value = parts[i];

                    switch (arg.type) {
                        case 'string': {} break;
                        case 'integer': { value = parseInt(value, 10); } break;
                        case 'number': { value = parseFloat(value); } break;
                    }

                    if (value == null || Number.isNaN(value))
                        value = null;

                    changes[arg.key] = value;
                }
            }

            changes.mode = mode;
        } break;
    }

    return run(changes, push);
}

async function run(changes = {}, push = false) {
    if (poisoned)
        return;

    // Keep session alive
    if (isLogged() && ping_timer == null) {
        ping_timer = setInterval(() => {
            Net.get('/api/session/ping');
        }, 180 * 1000);
    }

    // Go back to top app when the context changes
    let scroll = (changes.mode != null && changes.mode != route.mode);

    try {
        pending++;

        await navigator.locks.request(RUN_LOCK, async () => {
            Object.assign(route, changes);

            // Don't run stateful code while dialog is running to avoid concurrency issues
            if (UI.isDialogOpen()) {
                UI.runDialog();
                return;
            }

            let info = MODES[route.mode] ?? MODES[DEFAULT_MODE];

            if (info.session && !isLogged()) {
                await UserMod.runLogin();
            } else {
                await info.run();
            }

            // Update URL
            if (!pending) {
                let url = makeURL();

                if (url != route_url) {
                    if (push) {
                        window.history.pushState(null, null, url);
                    } else if (url != route_url) {
                        window.history.replaceState(null, null, url);
                    }

                    route_url = url;
                }
            }
        });

        if (scroll)
            window.scrollTo(0, 0);
    } finally {
        pending--;
    }
}

function makeURL(changes = {}) {
    let values = Object.assign({}, route, changes);
    let path = '/' + values.mode;

    let info = MODES[values.mode];

    if (info?.path != null) {
        for (let i = 0; i < info.path.length; i++) {
            let arg = info.path[i];
            path += '/' + values[arg.key];
        }
    }

    if (path == window.location.pathname && window.location.hash)
        path += window.location.hash;

    return path;
}

function renderApp(el) {
    if (root_el == null) {
        root_el = document.createElement('div');
        root_el.id = 'root';

        document.body.appendChild(root_el);
    }

    let in_repositories = ['repositories', 'repository'].includes(route.mode);
    let in_plans = ['plans', 'plan'].includes(route.mode);

    render(html`
        <nav id="top">
            <div @click=${deploy}></div>
            <menu>
                <a id="logo" href="/"><img src=${ASSETS['main/logo']} alt=${'Logo ' + ENV.title} /></a>
                ${isLogged() ? html`
                    <li><a href=${!in_repositories && route.repository != null ? makeURL({ mode: 'repository' }) : '/repositories'}
                           class=${in_repositories ? 'active' : ''}>${T.repositories}</a></li>
                    <li><a href=${!in_plans && route.plan != null ? makeURL({ mode: 'plan' }) : '/plans'}
                           class=${in_plans ? 'active' : ''}>${T.machines}</a></li>
                ` : ''}
                <div style="flex: 1;"></div>
                ${isLogged() ? html`
                    <li><a href="/account" class=${route.mode == 'account' ? 'active' : ''}>${T.account}</a></li>
                    <img class="picture" src=${`/pictures/${session.userid}?v=${session.picture}`} alt="" />
                ` : ''}
                ${!isLogged() ? html`
                    ${ENV.auth.internal ? html`<li><a href="/register" class=${route.mode == 'register' ? 'active' : ''}>${T.register}</a></li>` : ''}
                    <li><a href="/" class=${route.mode != 'register' ? 'active' : ''}>${T.login}</a></li>
                ` : ''}
            </menu>
        </nav>

        ${el}

        <footer>
            <div>${ENV.title} © 2025</div>
            <div style="font-size: 0.8em;">
                Niels Martignène (<a href="https://koromix.dev/" target="_blank">Koromix</a>)<br>
                <a href="mailto:niels.martignene@protonmail.com" style="font-weight: bold; color: inherit;">niels.martignene@protonmail.com</a>
            </div>
            <select id="language" aria-label=${T.language} @change=${UI.wrap(e => switchLanguage(e.target.value))}>
                ${Object.keys(languages).map(lang =>
                    html`<option value=${lang} .selected=${lang == document.documentElement.lang}>${lang.toUpperCase()}</option>`)}
            </select>
        </footer>
    `, root_el);
}

async function switchLanguage(lang) {
    Util.setLocale(lang);

    dayjs.locale(document.documentElement.lang);
    Util.setCookie('lang', document.documentElement.lang, '/', 365 * 86400);

    await run({}, false);
}

// ------------------------------------------------------------------------
// Session
// ------------------------------------------------------------------------

function isLogged() {
    if (session == null)
        return false;
    if (!session.authorized)
        return false;

    return true;
}

async function login(mail, password) {
    session = await Net.post('/api/user/login', {
        mail: mail,
        password: password
    });

    await run({}, false);
}

async function confirm(code) {
    session = await Net.post('/api/totp/confirm', { code: code });

    await run({}, false);
}

async function logout() {
    await Net.post('/api/user/logout');

    session = null;

    window.onbeforeunload = null;
    window.location.href = '/';

    poisoned = true;
}

async function sso(provider, redirect) {
    let info = await Net.post('/api/sso/login', {
        provider: provider,
        redirect: redirect
    });

    window.location.href = info.url;
}

async function oidc(code, state) {
    let login = await Net.post('/api/sso/oidc', {
        code: code,
        state: state
    });

    session = login.session;
    go(login.redirect, false);
}

export {
    route,
    cache,

    start,

    makeURL,
    go,
    run,

    session,
    isLogged,

    login,
    confirm,
    logout,
    sso,
    oidc,
}
