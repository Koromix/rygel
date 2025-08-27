// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
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

import { render, html, live, unsafeHTML } from '../../../../vendor/lit-html/lit-html.bundle.js';
import { Chart } from '../../../../vendor/chartjs/chart.bundle.js';
import { Util, Log, Net, HttpError } from '../../../web/core/base.js';
import { Base64 } from '../../../web/core/mixer.js';
import * as UI from './ui.js';
import { deploy } from '../../../web/flat/static.js';
import { ASSETS } from '../assets/assets.js';
import { PictureCropper } from './picture.js';

import en from '../../i18n/en.json';
import fr from '../../i18n/fr.json';

import '../assets/client.css';

const DAYS = [T.day_monday, T.day_tuesday, T.day_wednesday, T.day_thursday, T.day_friday, T.day_saturday, T.day_sunday];

const RUN_LOCK = 'run';

let languages = {};

let route = {
    mode: null,
    repository: null,
    plan: null
};
let route_url = null;
let poisoned = false;

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

    Util.initLocales(languages, 'en');

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
            return T.confirm_or_close_dialog;
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
        case 'login': {
            if (session != null) {
                changes.mode = 'repositories';
            } else {
                changes.mode = 'login';
            }
        } break;

        case 'register':
        case 'finalize':
        case 'recover':
        case 'reset': { changes.mode = mode; } break;

        case 'repositories':
        case 'plans':
        case 'account': { changes.mode = mode; } break;

        case 'repository': {
            changes.repository = parseInt(parts[0], 10);
            if (Number.isNaN(changes.repository))
                changes.repository = null;
            changes.mode = mode;
        } break;

        case 'plan': {
            changes.plan = parseInt(parts[0], 10);
            if (Number.isNaN(changes.plan))
                changes.plan = null;
            changes.mode = mode;
        } break;

        default: { changes.mode = 'repositories'; } break;
    }

    return run(changes, push);
}

async function run(changes = {}, push = false) {
    if (poisoned)
        return;

    // Keep session alive
    if (session != null && ping_timer == null) {
        ping_timer = setInterval(() => {
            Net.get('/api/user/ping');
        }, 360 * 1000);
    }

    // Go back to top app when the context changes
    let scroll = (changes.mode != null && changes.mode != route.mode);

    await navigator.locks.request(RUN_LOCK, async () => {
        Object.assign(route, changes);

        // Don't run stateful code while dialog is running to avoid concurrency issues
        if (UI.isDialogOpen()) {
            UI.runDialog();
            return;
        }

        switch (route.mode) {
            case 'login': { await runLogin(); } break;
            case 'register': { await runRegister(); } break;
            case 'finalize': { await runFinalize(); } break;

            case 'recover': { await runRecover(); } break;
            case 'reset':  { await runReset(); } break;

            default: {
                if (session == null) {
                    await runLogin();
                    return;
                } else if (!session.confirmed) {
                    await runConfirm();
                    return;
                }
            } break;
        }

        if (session != null)
            cache.repositories = await Net.cache('repositories', '/api/repository/list');

        switch (route.mode) {
            case 'repositories': { await runRepositories(); } break;
            case 'repository': { await runRepository(); } break;
            case 'plans': { await runPlans(); } break;
            case 'plan': { await runPlan(); } break;
            case 'account': { await runAccount(); } break;
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

    switch (values.mode) {
        case 'repository': { path += '/' + values.repository; } break;
        case 'plan': { path += '/' + values.plan; } break;
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
        <div class="deploy" @click=${deploy}></div>

        <nav id="top">
            <menu>
                <a id="logo" href="/"><img src=${ASSETS['main/logo']} alt=${'Logo ' + ENV.title} /></a>
                ${session != null ? html`
                    <li><a href=${!in_repositories && route.repository != null ? makeURL({ mode: 'repository' }) : '/repositories'}
                           class=${in_repositories ? 'active' : ''}>${T.repositories}</a></li>
                    <li><a href=${!in_plans && route.plan != null ? makeURL({ mode: 'plan' }) : '/plans'}
                           class=${in_plans ? 'active' : ''}>${T.plans}</a></li>
                ` : ''}
                <div style="flex: 1;"></div>
                ${session != null ? html`
                    <li><a href="/account" class=${route.mode == 'account' ? 'active' : ''}>${T.account}</a></li>
                    <img class="picture" src=${`/pictures/${session.userid}?v=${session.picture}`} alt="" />
                ` : ''}
                ${session == null ? html`
                    <li><a href="/register" class=${route.mode == 'register' ? 'active' : ''}>${T.register}</a></li>
                    <li><a href="/repositories" class=${route.mode != 'register' ? 'active' : ''}>${T.login}</a></li>
                ` : ''}
            </menu>
        </nav>

        ${el}

        <footer>
            <div>${ENV.title} © 2025</div>
            <div style="font-size: 0.8em;">
                Niels Martignène (<a href="https://codeberg.org/Koromix/" target="_blank">Koromix</a>)<br>
                <a href="mailto:niels.martignene@protonmail.com" style="font-weight: bold; color: inherit;">niels.martignene@protonmail.com</a>
            </div>
            <select id="language" aria-label=${T.language} @change=${UI.wrap(e => switchLanguage(e.target.value))}>
                ${Object.keys(languages).map(lang =>
                    html`<option value=${lang} .selected=${lang == T.lang}>${lang.toUpperCase()}</option>`)}
            </select>
        </footer>
    `, root_el);
}

async function switchLanguage(lang) {
    Util.setLocale(lang);
    Util.setCookie('language', lang, '/', 365 * 86400);

    await run({}, false);
}

async function login(mail, password) {
    session = await Net.post('/api/user/login', {
        mail: mail,
        password: password
    });

    await run({}, false);
}

async function totp(code) {
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

// ------------------------------------------------------------------------
// User
// ------------------------------------------------------------------------

async function runRegister() {
    UI.main(html`
        <div class="tabbar">
            <a class="active">Register</a>
        </div>

        <div class="tab">
            <div class="box" style="align-items: center;">
                <div class="header">${T.create_your_account}</div>

                <form style="text-align: center;" @submit=${UI.wrap(submit)}>
                    <label>
                        <input type="text" name="mail" style="width: 20em;" placeholder=${T.mail_address.toLowerCase()} />
                    </label>
                    <div class="actions">
                        <button type="submit">${T.register}</button>
                        <a href="/login">${T.already_have_account}</a>
                    </div>
                </form>
            </div>
        </div>
    `);

    async function submit(e) {
        let form = e.currentTarget;
        let elements = form.elements;

        let mail = elements.mail.value.trim();

        if (!mail)
            throw new Error(T.message(`Mail address is missing`));

        await Net.post('/api/user/register', { mail: mail });

        form.innerHTML = '';
        render(html`<p>${unsafeHTML(T.consult_registration_mail)}</p>`, form);
    }
}

async function runFinalize() {
    let error = T.message(`Missing token`);
    let token = null;

    if (window.location.hash) {
        let hash = window.location.hash.substr(1);
        let query = new URLSearchParams(hash);

        token = query.get('token') || null;

        if (token != null) {
            try {
                await Net.post('/api/user/reset', { token: token });
                error = null;
            } catch (err) {
                if (!(error instanceof HttpError))
                    throw err;

                error = err.message;
            }
        }
    }

    UI.main(html`
        <div class="tabbar">
            <a class="active">${T.finalize_account}</a>
        </div>

        <div class="tab">
            <div class="box" style="align-items: center;">
                <div class="header">${T.create_password}</div>

                <form @submit=${UI.wrap(submit)}>
                    ${error == null ? html`
                        <label>
                            <span>${T.choose_password}</span>
                            <input type="password" name="password1" style="width: 20em;" placeholder=${T.password.toLowerCase()} />
                        </label>
                        <label>
                            <span>${T.confirmation}</span>
                            <input type="password" name="password2" style="width: 20em;" placeholder=${T.confirmation.toLowerCase()} />
                        </label>
                        <div class="actions">
                            <button type="submit">${T.set_password}</button>
                        </div>
                    ` : ''}
                    ${error != null ? html`<p>${error}</p>` : ''}
                </form>
            </div>
        </div>
    `);

    async function submit(e) {
        let form = e.currentTarget;
        let elements = form.elements;

        let password1 = elements.password1.value.trim();
        let password2 = elements.password2.value.trim();

        if (!password1 || !password2)
            throw new Error(T.message(`Password is missing`));
        if (password2 != password1)
            throw new Error(T.message(`Passwords do not match`));

        await Net.post('/api/user/reset', {
            token: token,
            password: password1
        });

        form.innerHTML = '';
        render(html`<p>${T.account_is_ready}</p>`, form);

        let url = window.location.pathname + window.location.search;
        history.replaceState(null, null, url);
    }
}

async function runLogin() {
    UI.main(html`
        <div class="tabbar">
            <a class="active">${T.login}</a>
        </div>

        <div class="tab">
            <div class="box" style="align-items: center;">
                <div class="header">${T.format(T.login_to_x, ENV.title)}</div>

                <form style="text-align: center;" @submit=${UI.wrap(submit)}>
                    <label>
                        <input type="text" name="mail" style="width: 20em;" placeholder=${T.mail_address.toLowerCase()} />
                    </label>
                    <label>
                        <input type="password" name="password" style="width: 20em;" placeholder=${T.password.toLowerCase()} />
                    </label>
                    <div class="actions">
                        <button type="submit">${T.login}</button>
                        <a href="/register">${T.maybe_create_account}</a>
                        <a href="/recover">${T.maybe_lost_password}</a>
                    </div>
                </form>
            </div>
        </div>
    `);

    async function submit(e) {
        let form = e.currentTarget;
        let elements = form.elements;

        let mail = elements.mail.value.trim();
        let password = elements.password.value.trim();

        if (!mail)
            throw new Error(T.message(`Mail address is missing`));
        if (!password)
            throw new Error(T.message(`Password is missing`));

        await login(mail, password);
    }
}

async function runConfirm() {
    UI.main(html`
        <div class="tabbar">
            <a class="active">${T.login}</a>
        </div>

        <div class="tab">
            <div class="box" style="align-items: center;">
                <div class="header">${T.format(T.login_to_x, ENV.title)}</div>

                <form style="text-align: center;" @submit=${UI.wrap(submit)}>
                    <p>${T.two_factor_authentication}</p>
                    <label>
                        <input type="text" name="code" style="width: 20em;" placeholder=${T.totp_digits} />
                    </label>
                    <div class="actions">
                        <button type="submit">${T.confirm}</button>
                    </div>
                </form>
            </div>
        </div>
    `);

    async function submit(e) {
        let form = e.currentTarget;
        let elements = form.elements;

        let code = elements.code.value.trim();

        if (!code)
            throw new Error(T.message(`TOTP code is missing`));

        await totp(code);
    }
}

async function runRecover() {
    UI.main(html`
        <div class="tabbar">
            <a class="active">${T.recover_account}</a>
        </div>

        <div class="tab">
            <div class="box" style="align-items: center;">
                <div class="header">${T.recover_account}</div>

                <form style="text-align: center;" @submit=${UI.wrap(submit)}>
                    <label>
                        <input type="text" name="mail" style="width: 20em;" placeholder=${T.mail_address.toLowerCase()} />
                    </label>
                    <div class="actions">
                        <button type="submit">${T.reset_password}</button>
                    </div>
                </form>
            </div>
        </div>
    `);

    async function submit(e) {
        let form = e.currentTarget;
        let elements = form.elements;

        let mail = elements.mail.value.trim();

        if (!mail)
            throw new Error(T.message(`Mail address is missing`));

        await Net.post('/api/user/recover', { mail: mail });

        form.innerHTML = '';
        render(html`<p>${unsafeHTML(T.consult_reset_mail)}</p>`, form);
    }
}

async function runReset() {
    let error = T.message(`Missing token`);
    let token = null;

    if (window.location.hash) {
        let hash = window.location.hash.substr(1);
        let query = new URLSearchParams(hash);

        token = query.get('token') || null;

        if (token != null) {
            try {
                await Net.post('/api/user/reset', { token: token });
                error = null;
            } catch (err) {
                if (!(error instanceof HttpError))
                    throw err;

                error = err.message;
            }
        }
    }

    UI.main(html`
        <div class="tabbar">
            <a class="active">${T.recover_account}</a>
        </div>

        <div class="tab">
            <div class="box" style="align-items: center;">
                <div class="header">${T.recover_account}</div>

                <form @submit=${UI.wrap(submit)}>
                    ${error == null ? html`
                        <label>
                            <span>New password</span>
                            <input type="password" name="password1" style="width: 20em;" placeholder=${T.password.toLowerCase()} />
                        </label>
                        <label>
                            <span>Confirm</span>
                            <input type="password" name="password2" style="width: 20em;" placeholder=${T.confirmation.toLowerCase()} />
                        </label>
                        <div class="actions">
                            <button type="submit">${T.confirm_password}</button>
                        </div>
                    ` : ''}
                    ${error != null ? html`<p>${error}</p>` : ''}
                </form>
            </div>
        </div>
    `);

    async function submit(e) {
        let form = e.currentTarget;
        let elements = form.elements;

        let password1 = elements.password1.value.trim();
        let password2 = elements.password2.value.trim();

        if (!password1 || !password2)
            throw new Error(T.message(`Password is missing`));
        if (password2 != password1)
            throw new Error(T.message(`Passwords do not match`));

        await Net.post('/api/user/reset', {
            token: token,
            password: password1
        });

        form.innerHTML = '';
        render(html`<p>Your password has been changed, please login to continue.</p>`, form);

        let url = window.location.pathname + window.location.search;
        history.replaceState(null, null, url);
    }
}

function isLogged() {
    let logged = (session != null);
    return logged;
}

// ------------------------------------------------------------------------
// Repositories
// ------------------------------------------------------------------------

async function runRepositories() {
    let repositories = UI.tableValues('repositories', cache.repositories, 'name');

    UI.main(html`
        <div class="tabbar">
            <a class="active">${T.repositories}</a>
            ${cache.repository != null ? html`<a href=${makeURL({ mode: 'repository' })}>${cache.repository.name}</a>` : ''}
        </div>

        <div class="tab">
            <div class="box">
                <div class="header">${T.repositories}</div>
                <table style="table-layout: fixed; width: 100%;">
                    <colgroup>
                        <col style="width: 30%;"></col>
                        <col></col>
                        <col style="width: 140px;"></col>
                    </colgroup>
                    <thead>
                        <tr>
                            ${UI.tableHeader('repositories', 'name', 'Name')}
                            ${UI.tableHeader('repositories', 'url', 'URL')}
                            ${UI.tableHeader('repositories', repo => {
                                if (repo.errors) {
                                    return 1;
                                } else {
                                    return repo.checked ?? 0;
                                }
                            }, 'Status')}
                        </tr>
                    </thead>
                    <tbody>
                        ${repositories.map(repo => {
                            let url = makeURL({ mode: 'repository', repository: repo.id });

                            return html`
                                <tr style="cursor: pointer;" @click=${UI.wrap(e => go(url))}>
                                    <td><a href=${url}>${repo.name}</a></td>
                                    <td>${repo.url}</td>
                                    <td style="text-align: right;">
                                        ${!repo.checked ? T.valid : ''}
                                        ${repo.checked && !repo.errors ? T.success : ''}
                                        ${repo.errors ? html`<span style="color: red;">${repo.failed || T.unknown_error}</span>` : ''}
                                        <br><span class="sub">${repo.checked ? (new Date(repo.checked)).toLocaleString() : T.check_pending}</span>
                                    </td>
                                </tr>
                            `;
                        })}
                        ${!repositories.length ? html`<tr><td colspan="3" style="text-align: center;">${T.no_repository}</td></tr>` : ''}
                    </tbody>
                </table>
                <div class="actions">
                    <button type="button" @click=${UI.wrap(e => configureRepository(null))}>${T.add_repository}</button>
                </div>
            </div>
        </div>
    `);
}

async function runRepository() {
    if (route.repository != null) {
        try {
            let url = Util.pasteURL('/api/repository/get', { id: route.repository });
            cache.repository = await Net.cache('repository', url);
        } catch (err) {
            if (!(err instanceof HttpError))
                throw err;
            if (err.status != 404 && err.status != 422)
                throw err;

            cache.repository = null;
        }
    } else {
        cache.repository = null;
    }

    route.repository = cache.repository?.id;

    if (cache.repository == null) {
        go('/repositories');
        return;
    }

    let channels = UI.tableValues('channels', cache.repository.channels, 'name');

    UI.main(html`
        <div class="tabbar">
            <a href="/repositories">${T.repositories}</a>
            <a class="active">${cache.repository.name}</a>
        </div>

        <div class="tab">
            <div class="row">
                <div class="box" style="min-width: 250px;">
                    <div class="header">${T.repository}</div>
                    <div class="info">
                        ${cache.repository.name}
                        <div class="sub">${cache.repository.url}</div>
                    </div>
                    <button type="button" @click=${UI.wrap(e => configureRepository(cache.repository))}>${T.configure}</button>
                </div>

                <div class="box">
                    <div class="header">${T.channels}</div>
                    <table style="table-layout: fixed; width: 100%;">
                        <colgroup>
                            <col></col>
                            <col></col>
                            <col></col>
                        </colgroup>
                        <thead>
                            <tr>
                                ${UI.tableHeader('channels', 'name', T.name)}
                                ${UI.tableHeader('channels', 'size', T.size)}
                                ${UI.tableHeader('channels', 'time', T.timestamp)}
                            </tr>
                        </thead>
                        <tbody>
                            ${channels.map(channel => html`
                                <tr style="cursor: pointer;" @click=${UI.wrap(e => runChannel(cache.repository, channel.name))}>
                                    <td>${channel.name} <span class="sub">(${channel.count})</span></td>
                                    <td style="text-align: right;">${formatSize(channel.size)}</td>
                                    <td style="text-align: right;">${(new Date(channel.time)).toLocaleString()}</td>
                                </tr>
                            `)}
                            ${!channels.length ? html`<tr><td colspan="3" style="text-align: center;">${T.no_channel}</td></tr>` : ''}
                        </tbody>
                    </table>
                </div>
            </div>
        </div>
    `);
}

async function configureRepository(repo) {
    let ptr = repo;

    if (repo == null) {
        repo = {
            name: '',
            url: ''
        };
    } else {
        repo = Object.assign({}, repo);
    }

    let url = repo.url;

    await UI.dialog({
        run: (render, close) => {
            return html`
                <div class="title">
                    ${ptr != null ? T.edit_repository : T.create_repository}
                    <div style="flex: 1;"></div>
                    <button type="button" class="secondary" @click=${UI.wrap(close)}>✖\uFE0E</button>
                </div>

                <div class="main">
                    <label>
                        <span>${T.name}</span>
                        <input type="text" name="name" required value=${repo.name} />
                    </label>
                    <label>
                        <span>${T.url}</span>
                        <input type="text" name="url" required value=${url}
                               @input=${e => { url = e.target.value; render(); }} />
                    </label>
                </div>

                <div class="footer">
                    ${ptr != null ? html`
                        <button type="button" class="danger"
                                @click=${UI.confirm(T.delete_repository, e => deleteRepository(repo.id).then(close))}>${T.delete}</button>
                        <div style="flex: 1;"></div>
                    ` : ''}
                    <button type="button" class="secondary" @click=${UI.insist(close)}>${T.cancel}</button>
                    <button type="submit">${ptr != null ? T.save : T.create}</button>
                </div>
            `
        },

        submit: async (elements) => {
            let obj = {
                id: repo.id,
                name: elements.name.value.trim() || null,
                url: elements.url.value.trim() || null
            };

            let json = await Net.post('/api/repository/save', obj);

            Net.invalidate('repositories');
            Net.invalidate('repository');

            let url = makeURL({ mode: 'repository', repository: json.id });
            await go(url);
        }
    });
}

async function deleteRepository(id) {
    await Net.post('/api/repository/delete', { id: id });
    Net.invalidate('repositories');

    if (route.repository == id)
        route.repository = null;
}

async function runChannel(repo, channel) {
    let url = Util.pasteURL('/api/repository/snapshots', { id: repo.id, channel: channel });
    let snapshots = await Net.cache('snapshots', url);

    // Make sure it is sorted by time
    snapshots.sort((snapshot1, snapshot2) => snapshot1.time - snapshot2.time);

    let canvas = null;

    if (snapshots.length >= 2) {
        canvas = document.createElement('canvas');

        let sets = {
            size: { label: T.size, data: [], fill: false },
            stored: { label: T.stored, data: [], fill: false },
            added: { label: T.added, data: [], fill: false }
        };

        for (let snapshot of snapshots) {
            sets.size.data.push({ x: snapshot.time, y: snapshot.size });
            sets.stored.data.push({ x: snapshot.time, y: snapshot.stored });
            if (snapshot.added != null)
                sets.added.data.push({ x: snapshot.time, y: snapshot.added });
        }

        let ctx = canvas.getContext('2d');

        new Chart(ctx, {
            type: 'line',
            data: {
                datasets: Object.values(sets)
            },
            options: {
                responsive: true,
                plugins: {
                    legend: {
                        position: 'right',
                    },
                    tooltip: {
                        callbacks: {
                            title: items => (new Date(items[0].parsed.x)).toLocaleString(),
                            label: item => `${item.dataset.label}: ${formatSize(item.parsed.y)}`
                        }
                    }
                },
                scales: {
                    x: {
                        type: 'linear',
                        min: snapshots[0].time,
                        max: snapshots[snapshots.length - 1].time,
                        ticks: {
                            callback: value => (new Date(value)).toLocaleDateString()
                        }
                    },
                    y: {
                        type: 'linear',
                        min: 0,
                        ticks: {
                            callback: formatSize
                        }
                    }
                }
            }
        });
    }

    await UI.dialog({
        run: (render, close) => {
            snapshots = UI.tableValues('snapshots', snapshots, 'time', false);

            return html`
                <div class="title">
                    ${T.format(T.x_snapshots, channel)}
                    <div style="flex: 1;"></div>
                    <button type="button" class="secondary" @click=${UI.wrap(close)}>✖\uFE0E</button>
                </div>

                <div class="main">
                    ${canvas != null ? html`<div style="width: 80%; margin: 0 auto;">${canvas}</div>` : ''}

                    <table style="table-layout: fixed;">
                        <colgroup>
                            <col></col>
                            <col></col>
                            <col></col>
                            <col></col>
                            <col></col>
                        </colgroup>
                        <thead>
                            <tr>
                                ${UI.tableHeader('snapshots', 'time', T.timestamp)}
                                ${UI.tableHeader('snapshots', 'oid', T.object)}
                                ${UI.tableHeader('snapshots', 'size', T.size)}
                                ${UI.tableHeader('snapshots', 'stored', T.stored)}
                                ${UI.tableHeader('snapshots', 'added', T.added)}
                            </tr>
                        </thead>
                        <tbody>
                            ${snapshots.map(snapshot => html`
                                <tr>
                                    <td>${(new Date(snapshot.time)).toLocaleString()}</td>
                                    <td><span class="sub">${snapshot.oid}</span></td>
                                    <td style="text-align: right;">${formatSize(snapshot.size)}</td>
                                    <td style="text-align: right;">${formatSize(snapshot.stored)}</td>
                                    <td style="text-align: right;">
                                        ${snapshot.added != null ? formatSize(snapshot.added) : ''}
                                        ${snapshot.added == null ? html`<span class="sub">(${T.unknown.toLowerCase()})</span>` : ''}
                                    </td>
                                </tr>
                            `)}
                            ${!snapshots.length ? html`<tr><td colspan="5" style="text-align: center;">${T.no_snapshot}</td></tr>` : ''}
                        </tbody>
                    </table>
                </div>
            `;
        }
    });
}

function formatSize(size) {
    if (size >= 999950000) {
        let value = size / 1000000000;
        let prec = 1 + (value < 9.9995) + (value < 99.995);

        return value.toFixed(prec) + ' ' + T.unit_gb;
    } else if (size >= 999950) {
        let value = size / 1000000;
        let prec = 1 + (value < 9.9995) + (value < 99.995);

        return value.toFixed(prec) + ' ' + T.unit_mb;
    } else if (size >= 999.95) {
        let value = size / 1000;
        let prec = 1 + (value < 9.9995) + (value < 99.995);

        return value.toFixed(prec) + ' ' + T.unit_kb;
    } else {
        return size + ' ' + T.unit_b;
    }
}

// ------------------------------------------------------------------------
// Plans
// ------------------------------------------------------------------------

async function runPlans() {
    cache.plans = await Net.cache('plans', '/api/plan/list');

    let plans = UI.tableValues('plans', cache.plans, 'name');

    UI.main(html`
        <div class="tabbar">
            <a class="active">${T.plans}</a>
            ${cache.plan != null ? html`<a href=${makeURL({ mode: 'plan' })}>${cache.plan.name}</a>` : ''}
        </div>

        <div class="tab">
            <div class="box">
                <div class="header">${T.plans}</div>
                <table style="table-layout: fixed; width: 100%;">
                    <colgroup>
                        <col/>
                        <col/>
                        <col/>
                        <col/>
                    </colgroup>
                    <thead>
                        <tr>
                            ${UI.tableHeader('plans', 'name', T.name)}
                            ${UI.tableHeader('plans', 'repository', T.repository)}
                            <th>${T.key_prefix}</th>
                            ${UI.tableHeader('plans', makePlanTasks, T.tasks)}
                        </tr>
                    </thead>
                    <tbody>
                        ${plans.map(plan => {
                            let url = makeURL({ mode: 'plan', plan: plan.id });
                            let repo = cache.repositories.find(repo => repo.id == plan.repository);

                            return html`
                                <tr style="cursor: pointer;" @click=${UI.wrap(e => go(url))}>
                                    <td><a href=${url}>${plan.name}</a></td>
                                    <td><a href=${makeURL({ mode: 'repository', repository: plan.repository })}>${repo?.name}</a></td>
                                    <td><span class="sub">${plan.key}</sub></td>
                                    <td>${makePlanTasks(plan)}</td>
                                </tr>
                            `;
                        })}
                        ${!plans.length ? html`<tr><td colspan="4" style="text-align: center;">${T.no_plan}</td></tr>` : ''}
                    </tbody>
                </table>
                <div class="actions">
                    <button type="button" @click=${UI.wrap(e => configurePlan(null))}>${T.add_plan}</button>
                </div>
            </div>
        </div>
    `);
}

function makePlanTasks(plan) {
    let parts = [];

    if (plan.items) {
        let text = (plan.items > 1) ? T.format(T.run_x_snapshot_items, plan.items) : T.run_snapshot_item;
        parts.push(text);
    }
    if (plan.scan != null) {
        let text = T.format(T.scan_repository_at_x, formatClock(plan.scan));
        parts.push(text);
    }

    if (!parts.length) {
        let empty = html`<span class="sub">(${T.nothing_to_do.toLowerCase()})</span>`;
        parts.push(empty);
    }

    return parts.map((part, idx) => html`${idx ? html`<br>` : ''}${part}`);
}

async function runPlan() {
    if (route.plan != null) {
        try {
            let url = Util.pasteURL('/api/plan/get', { id: route.plan });
            cache.plan = await Net.cache('plan', url);
        } catch (err) {
            if (!(err instanceof HttpError))
                throw err;
            if (err.status != 404 && err.status != 422)
                throw err;

            cache.plan = null;
        }
    } else {
        cache.plan = null;
    }

    route.plan = cache.plan?.id;

    if (cache.plan == null) {
        go('/plans');
        return;
    }

    let repo = cache.repositories.find(repo => repo.id == cache.plan.repository);

    UI.main(html`
        <div class="tabbar">
            <a href="/plans">${T.plans}</a>
            <a class="active">${cache.plan.name}</a>
        </div>

        <div class="tab">
            <div class="row">
                <div class="box" style="min-width: 250px;">
                    <div class="header">${T.plan}</div>
                    <div class="info">
                        ${cache.plan.name} (${repo?.name ?? T.unassigned.toLowerCase()})
                        <div class="sub">${cache.plan.key}</div>
                    </div>
                    <button type="button" @click=${UI.wrap(e => configurePlan(cache.plan))}>${T.configure}</button>
                    ${cache.plan.scan != null ?
                        html`<div style="text-align: center;">${T.format(T.scan_repository_at_x, formatClock(cache.plan.scan))}</div>` : ''}
                </div>

                <div class="box">
                    <div class="header">${T.snapshot_items}</div>
                    <table style="table-layout: fixed; width: 100%;">
                        <colgroup>
                            <col style="width: 150px;"></col>
                            <col></col>
                            <col style="width: 100px;"></col>
                            <col></col>
                            <col></col>
                        </colgroup>
                        <thead>
                            <tr>
                                <th>Channel</th>
                                <th>Days</th>
                                <th>Clock time</th>
                                <th>Paths</th>
                                <th>Last run</th>
                            </tr>
                        </thead>
                        <tbody>
                            ${cache.plan.items.map(item => html`
                                <tr>
                                    <td>${item.channel}</td>
                                    <td>${formatDays(item.days)}</td>
                                    <td style="text-align: center;">${formatClock(item.clock)}</td>
                                    <td>${item.paths.map(path => html`${path}<br>`)}</td>
                                    <td style=${'text-align: right;' + (item.error != null ? ' color: var(--color, red);' : '')}>
                                        ${item.timestamp == null ? 'Never' : ''}
                                        ${item.timestamp != null ? new Date(item.timestamp).toLocaleString() : ''}
                                        <br>${item.error || ''}
                                    </td>
                                </tr>
                            `)}
                            ${!cache.plan.items.length ? html`<tr><td colspan="5" style="text-align: center;">${T.no_snapshot_item}</td></tr>` : ''}
                        </tbody>
                    </table>
                </div>
            </div>
        </div>
    `);
}

async function configurePlan(plan) {
    let ptr = plan;

    if (plan == null) {
        if (!cache.repositories.length)
            throw new Error(T.message(`Create a repository before you create a plan`));

        plan = {
            name: '',
            repository: route.repository ?? cache.repositories[0].id,
            scan: null,
            items: []
        };
    } else {
        plan = Object.assign({}, plan);
        plan.items = plan.items.slice();
    }

    let scan = true;

    if (plan.scan == null) {
        scan = false;
        plan.scan = 1200;
    }

    await UI.dialog({
        run: (render, close) => {
            return html`
                <div class="title">
                    ${ptr != null ? T.edit_plan : T.create_plan}
                    <div style="flex: 1;"></div>
                    <button type="button" class="secondary" @click=${UI.wrap(close)}>✖\uFE0E</button>
                </div>

                <div class="main">
                    <label>
                        <span>Name</span>
                        <input type="text" name="name" required .value=${live(plan.name)}
                               @change=${UI.wrap(e => { plan.name = e.target.value; render(); })} />
                    </label>
                    <label>
                        <span>Repository</span>
                        <select ?disabled=${ptr != null}
                                @change=${UI.wrap(e => { plan.repository = parseInt(e.target.value, 10); render(); })}>
                            ${cache.repositories.map(repo =>
                                html`<option value=${repo.id} ?selected=${repo.id == plan.repository}>${repo.name}</option>`)}
                        </select>
                    </label>

                    <div class="section">${T.periodic_scans}</div>
                    <label>
                        <input type="checkbox" ?checked=${scan} @change=${UI.wrap(e => { scan = e.target.checked; render(); })} />
                        <span>${T.run_daily_scans}</span>
                    </label>
                    ${scan ? html`
                        <label>
                            <span>${T.scan_time}</span>
                            <td><input type="time" .value=${live(formatClock(plan.scan))}
                                       @change=${UI.wrap(e => { plan.scan = parseClock(e.target.value); render(); })} /></td>
                        </label>
                        <div style="color: red; font-style: italic;">${T.scan_safety_warning}</div>
                    ` : ''}

                    <div class="section">
                        Snapshot items
                        <div style="flex: 1;"></div>
                        <button type="button" class="small" @click=${UI.wrap(add_item)}>${T.add_item}</button>
                    </div>
                    <table style="table-layout: fixed;">
                        <colgroup>
                            <col class="check"/>
                            <col style="width: 200px;"/>
                            <col style="width: 200px;"/>
                            <col style="width: 150px;"/>
                            <col style="width: 200px;"/>
                            <col style="width: 60px;"/>
                        </colgroup>

                        <thead>
                            <tr>
                                <th></th>
                                <th>${T.channel}</th>
                                <th>${T.days}</th>
                                <th>${T.clock_time}</th>
                                <th>${T.paths}</th>
                                <th></th>
                            </tr>
                        </thead>

                        <tbody>
                            ${plan.items.map(item => {
                                let paths = [...item.paths, ''];

                                return html`
                                    <tr ${UI.reorderItems(plan.items, item)}>
                                        <td class="grab"><img src=${ASSETS['ui/move']} width="16" height="16" alt=${T.move} /></td>
                                        <td><input type="text" .value=${live(item.channel)} @change=${UI.wrap(e => { item.channel = e.target.value; render(); })} /></td>
                                        <td>
                                            ${Util.mapRange(0, DAYS.length, idx => {
                                                let active = !!(item.days & (1 << idx));

                                                return html`
                                                    <label>
                                                        <input type="checkbox" .checked=${active} @change=${UI.wrap(e => toggle_day(item, idx))} />
                                                        ${DAYS[idx]}
                                                    </label>
                                                `;
                                            })}
                                        </td>
                                        <td><input type="time" .value=${live(formatClock(item.clock))}
                                                   @change=${UI.wrap(e => { item.clock = parseClock(e.target.value); render(); })} /></td>
                                        <td>${paths.map((path, idx) =>
                                            html`<input type="text" style=${idx ? 'margin-top: 3px;' : ''} .value=${live(path)}
                                                        @input=${UI.wrap(e => edit_path(item, idx, e.target.value))} />`)}</td>
                                        <td class="right">
                                            <button type="button" class="small"
                                                    @click=${UI.insist(e => delete_item(item))}><img src=${ASSETS['ui/delete']} alt=${T.delete} /></button>
                                        </td>
                                    </tr>
                                `;
                            })}
                            ${!plan.items.length ?
                                html`<tr><td colspan="6" style="text-align: center;">${T.no_item}</td></tr>` : ''}
                        </tbody>
                    </table>

                    ${ptr != null ? html`
                        <div class="section">${T.api_key}</div>
                        <label>
                            <span>${T.key_prefix}</span>
                            <div class="sub">${plan.key}</div>
                            <button type="button" class="small secondary" @click=${UI.insist(renew_key)}>${T.renew_key}</button>
                        </label>
                    ` : ''}
                </div>

                <div class="footer">
                    ${ptr != null ? html`
                        <button type="button" class="danger"
                                @click=${UI.confirm('Delete plan', e => deletePlan(plan.id).then(close))}>${T.delete}</button>
                        <div style="flex: 1;"></div>
                    ` : ''}
                    <button type="button" class="secondary" @click=${UI.insist(close)}>${T.cancel}</button>
                    <button type="submit">${ptr != null ? T.save : T.create}</button>
                </div>
            `;

            function add_item() {
               let item = {
                    channel: '',
                    days: 0b0011111,
                    clock: 0,
                    paths: []
                };
                plan.items.push(item);

                render();
            }

            function delete_item(item) {
                plan.items = plan.items.filter(it => it !== item);
                render();
            }

            function toggle_day(item, idx) {
                item.days = (item.days ^ (1 << idx)) || item.days;
                render();
            }

            function edit_path(item, idx, path) {
                if (idx >= item.paths.length) {
                    if (path)
                        item.paths.push(path);
                } else if (path) {
                    item.paths[idx] = path;
                } else {
                    item.paths.splice(idx, 1);
                }

                render();
            }

            async function renew_key() {
                let json = await Net.post('/api/plan/key', { id: plan.id });

                Net.invalidate('plans');
                Net.invalidate('plan');

                await showKey(plan, json.key, json.secret);
                plan.key = json.key;

                render();
            }
        },

        submit: async (elements) => {
            let obj = {
                id: plan.id,
                name: plan.name,
                repository: plan.repository,
                scan: scan ? plan.scan : null,
                items: plan.items.map(item => ({
                    channel: item.channel,
                    days: item.days,
                    clock: item.clock,
                    paths: item.paths
                }))
            };

            let json = await Net.post('/api/plan/save', obj);

            Net.invalidate('plans');
            Net.invalidate('plan');

            let url = makeURL({ mode: 'plan', plan: json.id });
            await go(url);

            if (ptr == null)
                await showKey(plan, json.key, json.secret);
        }
    });
}

async function showKey(plan, key, secret) {
    let full = `${key}/${secret}`;

    try {
        await UI.dialog({
            run: (render, close) => html`
                <div class="title">
                    ${T.api_key}
                    <div style="flex: 1;"></div>
                    <button type="button" class="secondary" @click=${UI.wrap(close)}>✖\uFE0E</button>
                </div>

                <div class="main">
                    <label>
                        <span>API key</span>
                        <input type="text" style="width: 40em;" readonly value=${full} />
                        <button type="button" class="small" @click=${UI.wrap(e => writeClipboard('API key', full))}>${T.copy}</button>
                    </label>

                    <div style="color: red; font-style: italic; text-align: center">${T.please_copy_api_key}</div>
                </div>
            `
        });
    } catch (err) {
        if (err != null)
            throw err;
    }
}

async function deletePlan(id) {
    await Net.post('/api/plan/delete', { id: id });
    Net.invalidate('plans');

    if (route.plan == id)
        route.plan = null;
}

function formatDays(days) {
    let parts = [];

    for (let i = 0; i < DAYS.length; i++) {
        if (days & (1 << i)) {
            let prefix = DAYS[i].substr(0, 3);
            parts.push(prefix);
        }
    }

    return parts.join(', ');
}

function formatClock(clock) {
    let hh = Math.floor(clock / 100).toString().padStart(2, '0');
    let mm = Math.floor(clock % 100).toString().padStart(2, '0');

    return `${hh}:${mm}`;
}

function parseClock(clock) {
    let [hh, mm] = (clock ?? '').split(':').map(value => parseInt(value, 10));
    return hh * 100 + mm;
}

// ------------------------------------------------------------------------
// Account
// ------------------------------------------------------------------------

async function runAccount() {
    UI.main(html`
        <div class="tabbar">
            <a class="active">${T.account}</a>
        </div>

        <div class="tab">
            <div class="box" style="align-items: center;">
                <div class="header">${T.account}</div>
                <div class="sub">${session.username}</div>
                <img class="picture" src=${`/pictures/${session.userid}?v=${session.picture}`} alt="" />
                <div class="actions">
                    <button type="button" class="secondary" @click=${UI.wrap(changePicture)}>${T.change_picture}</button>
                    <button type="button" class="secondary" @click=${UI.wrap(configureTOTP)}>${T.configure_2fa}</button>
                    <button type="button" @click=${UI.insist(logout)}>${T.logout}</button>
                </div>
            </div>
        </div>
    `);
}

async function changePicture() {
    let current = null;

    // Fetch existing picture
    {
        let response = await Net.fetch('/api/picture/get');

        if (response.ok) {
            current = await response.blob();
        } else if (response.status == 404) {
            current = null;
        } else {
            let err = await Net.readError(response);
            throw new Error(err);
        }
    }

    let cropper = new PictureCropper(T.change_picture, 256);

    cropper.defaultURL = ASSETS['ui/anonymous'];
    cropper.imageFormat = 'image/png';

    await cropper.run(current, async blob => {
        if (blob != null) {
            let json = await Net.post('/api/picture/save', blob);
            session.picture = json.picture;
        } else {
            let json = await Net.post('/api/picture/delete');
            session.picture = json.picture;
        }
    });

    await run({}, false);
}

async function configureTOTP(e) {
    let enable = !session.totp;

    let qrcode;
    {
        let response = await Net.fetch('/api/totp/secret');

        if (!response.ok) {
            let err = await Net.readError(response);
            throw new Error(err);
        }

        let secret = response.headers.get('X-TOTP-SecretKey');
        let buf = await response.arrayBuffer();

        qrcode = {
            secret: secret,
            image: 'data:image/png;base64,' + Base64.toBase64(buf)
        };
    }

    await UI.dialog({
        run: (render, close) => html`
            <div class="title">
                Configure TOTP
                <div style="flex: 1;"></div>
                <button type="button" class="secondary" @click=${UI.wrap(close)}>✖\uFE0E</button>
            </div>

            <div class="main">
                <p>
                    ${!session.totp ? T.confirm_to_enable_2fa : ''}
                    ${session.totp ? T.confirm_to_change_2fa : ''}
                </p>

                <label>
                    <span>${T.password}</span>
                    <input type="password" name="password" style="width: 20em;" placeholder=${T.password.toLowerCase()} />
                </label>

                ${session.totp ? html`
                    <label>
                        <input type="checkbox" checked @change=${UI.wrap(e => { enable = !e.target.checked; render(); })} />
                        <span>${T.disable_2fa}</span>
                    </label>
                ` : ''}

                ${enable ? html`
                    <div style="text-align: center; margin-top: 2em;"><img src="${qrcode.image}" alt="" /></div>
                    <p style="text-align: center; font-size: 0.8em; margin-top: 0;">${qrcode.secret}</p>

                    <p>
                        ${T.totp_scan1}<br>
                        ${T.totp_scan2}
                    </p>

                    <label>
                        <span>Code</span>
                        <input type="text" name="code" style="width: 20em;" placeholder=${T.totp_digits} />
                    </label>

                    <p><i>${T.totp_applications}</i></p>
                ` : ''}
            </div>

            <div class="footer">
                <button type="button" class="secondary" @click=${UI.insist(close)}>${T.cancel}</button>
                <button type="submit">${T.confirm}</button>
            </div>
        `,

        submit: async (elements) => {
            if (enable) {
                await Net.post('/api/totp/change', {
                    password: elements.password.value,
                    code: elements.code.value
                });
            } else {
                await Net.post('/api/totp/disable', { password: elements.password.value });
            }

            session.totp = enable;
        }
    });
}

async function writeClipboard(label, text) {
    await navigator.clipboard.writeText(text);

    let msg = `${label} copied to clipboard`;
    Log.info(msg);
}

export {
    route,

    start,
    go,
    run,

    isLogged
}
