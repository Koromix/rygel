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

import { render, html, ref } from '../../../../vendor/lit-html/lit-html.bundle.js';
import { Util, Log, Net } from '../../../web/core/base.js';
import * as UI from './ui.js';
import { deploy } from '../../../web/flat/static.js';
import { ASSETS } from '../assets/assets.js';
import * as app from './app.js';

import '../assets/client.css';

const RUN_LOCK = 'run';

let route = {
    mode: null
};
let route_url = null;
let poisoned = false;

let session = null;

let root_el = null;

let cache = {
    // Nothing yet
};

// ------------------------------------------------------------------------
// Init
// ------------------------------------------------------------------------

async function start() {
    UI.init(run, renderApp);

    // Handle internal links
    Util.interceptLocalAnchors((e, href) => {
        let url = new URL(href, window.location.href);

        let func = UI.wrap(e => go(url));
        func(e);

        e.preventDefault();
    });

    // Handle back navigation
    window.addEventListener('popstate', e => {
        UI.closeDialog();
        go(window.location.href, false);
    });

    // Prevent loss of data
    window.onbeforeunload = e => {
        if (UI.isDialogOpen())
            return 'Validez ou fermez la boîte de dialogue avant de continuer';
    };

    // Keep this around for gesture emulation on desktop
    if (false) {
        let script = document.createElement('script');

        script.onload = () => TouchEmulator();
        script.src = BUNDLES['touch-emulator.js'];

        document.head.appendChild(script);
    }

    // Retrieve session if there seems to be one
    {
        let rnd = Util.getCookie('session_rnd');

        if (rnd != null)
            session = await Net.get('/api/user/session');
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
        case 'register':
        case 'login':
        case 'confirm':
        case 'recover': { changes.mode = mode; } break;

        case 'dashboard': { changes.mode = mode; } break;

        default: { changes.mode = 'dashboard'; } break;
    }

    return run(changes, push);
}

async function run(changes = {}, push = false) {
    if (poisoned)
        return;

    // Go back to top app when the context changes
    let scroll = (changes.mode != null && changes.mode != route.mode);

    await navigator.locks.request(RUN_LOCK, async () => {
        Object.assign(route, changes);

        // Don't run stateful code while dialog is running to avoid concurrency issues
        if (UI.isDialogOpen()) {
            UI.runDialog();
            return;
        }

        // Run module
        switch (route.mode) {
            case 'register': { await runRegister(); } break;
            case 'login': { await runLogin(); } break;
            case 'confirm': { await runConfirm(); } break;
            case 'recover': { await runRecover(); } break;
            case 'dashboard': { await runDashboard(); } break;
        }

        // Update URL
        {
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
}

function makeURL(changes = {}) {
    let values = Object.assign({}, route, changes);
    let path = '/' + values.mode;

    return path;
}

function renderApp(el) {
    if (root_el == null) {
        root_el = document.createElement('div');
        root_el.id = 'root';

        document.body.appendChild(root_el);
    }

    render(html`
        <div class="deploy" @click=${deploy}></div>

        <nav id="top">
            <menu>
                <a id="logo" href="/"><img src=${ASSETS['main/logo']} alt=${'Logo ' + ENV.title} /></a>
                <div style="flex: 1;"></div>
                <a href="/dashboard"><img class="avatar anonymous" src=${ASSETS['ui/anonymous']} alt="" /></a>
            </menu>
        </nav>

        ${el}

        <footer>
            <div>${ENV.title} © 2025</div>
            <div style="font-size: 0.8em;">
                Niels Martignène (<a href="https://codeberg.org/Koromix/" target="_blank">Koromix</a>)<br>
                <a href="mailto:niels.martignene@protonmail.com" style="font-weight: bold; color: inherit;">niels.martignene@protonmail.com</a>
            </div>
        </footer>
    `, root_el);
}

// ------------------------------------------------------------------------
// User
// ------------------------------------------------------------------------

async function runRegister() {
    UI.main(html`
        <div class="tabbar">
            <a class="active">Enregistrez-vous</a>
        </div>

        <div class="tab">
            <div class="box" style="align-items: center;">
                <div class="header">Register to continue</div>

                <form style="text-align: center;" @submit=${UI.wrap(register)}>
                    <input type="text" name="mail" style="width: 20em;" placeholder="mail address" />
                    <div class="actions">
                        <button type="submit">Create account</button>
                    </div>
                </form>
            </div>
        </div>
    `);
}

async function register(e) {
    let form = e.currentTarget;
    let elements = form.elements;

    let mail = elements.mail.value.trim();

    if (!mail)
        throw new Error('Mail address is missing');

    await Net.post('/api/user/register', { mail: mail });

    form.innerHTML = '';
    render(html`<p>Consult the <b>mail that has been sent</b> and click the link inside to finalize your registration.</p>`, form);
}

async function runConfirm() {
    UI.main('');
}

async function runLogin() {
    UI.main('');
}

async function runRecover() {
    UI.main('');
}

function isLogged() {
    let logged = (session != null);
    return logged;
}

// ------------------------------------------------------------------------
// Repositories
// ------------------------------------------------------------------------

async function runDashboard() {
    UI.main('');
}

export {
    route,

    start,
    go,
    run,

    isLogged
}
