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

import { render, html, ref } from '../../../../vendor/lit-html/lit-html.bundle.js';
import { Chart } from '../../../../vendor/chartjs/chart.bundle.js';
import { Util, Log, Net, HttpError } from '../../../web/core/base.js';
import * as UI from './ui.js';
import { deploy } from '../../../web/flat/static.js';
import { ASSETS } from '../assets/assets.js';
import { PictureCropper } from './picture.js';

import '../assets/client.css';

const RUN_LOCK = 'run';

let route = {
    mode: null,
    repository: null
};
let route_url = null;
let poisoned = false;

let session = null;
let ping_timer = null;

let root_el = null;

let cache = {
    repositories: [],
    repository: null
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
        case 'login': {
            if (session != null) {
                changes.mode = 'dashboard';
            } else {
                changes.mode = 'login';
            }
        } break;

        case 'register':
        case 'confirm':
        case 'recover':
        case 'reset': { changes.mode = mode; } break;

        case 'dashboard':
        case 'account': { changes.mode = mode; } break;

        case 'repository': {
            changes.repository = parseInt(parts[0], 10);
            if (Number.isNaN(changes.repository))
                changes.repository = null;
            changes.mode = mode;
        } break;

        default: { changes.mode = 'dashboard'; } break;
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
            case 'confirm': { await runConfirm(); } break;

            case 'recover': { await runRecover(); } break;
            case 'reset':  { await runReset(); } break;

            default: {
                if (session == null) {
                    await runLogin();
                    return;
                }
            } break;
        }

        switch (route.mode) {
            case 'dashboard': { await runDashboard(); } break;
            case 'repository': { await runRepository(); } break;
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
    let hash = window.location.hash ?? '';

    let values = Object.assign({}, route, changes);
    let path = '/' + values.mode + hash;

    switch (values.mode) {
        case 'repository': { path += '/' + values.repository; } break;
    }

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
                ${session != null ? html`
                    <li><a href="/dashboard" class=${route.mode == 'dashboard' || route.mode == 'repository' ? 'active' : ''}>Repositories</a></li>
                    <div style="flex: 1;"></div>
                    <li><a href="/account" class=${route.mode == 'account' ? 'active' : ''}>Account</a></li>
                    <img class="picture" src=${`/pictures/${session.userid}?v=${session.picture}`} alt="" />
                ` : ''}
                ${session == null ? html`
                    <div style="flex: 1;"></div>
                    <li><a href="/register" class=${route.mode == 'register' ? 'active' : ''}>Register</a></li>
                    <li><a href="/dashboard" class=${route.mode != 'register' ? 'active' : ''}>Login</a></li>
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
        </footer>
    `, root_el);
}

async function login(mail, password) {
    session = await Net.post('/api/user/login', {
        mail: mail,
        password: password
    });

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
                <div class="header">Create your account</div>

                <form style="text-align: center;" @submit=${UI.wrap(submit)}>
                    <label>
                        <input type="text" name="mail" style="width: 20em;" placeholder="mail address" />
                    </label>
                    <div class="actions">
                        <button type="submit">Register</button>
                        <a href="/login">Already have an account?</a>
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
            throw new Error('Mail address is missing');

        await Net.post('/api/user/register', { mail: mail });

        form.innerHTML = '';
        render(html`<p>Consult the <b>mail that has been sent</b> and click the link inside to finalize your registration.</p>`, form);
    }
}

async function runConfirm() {
    let error = 'Missing token';
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
            <a class="active">Finalize account</a>
        </div>

        <div class="tab">
            <div class="box" style="align-items: center;">
                <div class="header">Create password</div>

                <form style="text-align: center;" @submit=${UI.wrap(submit)}>
                    ${error == null ? html`
                        <label>
                            <span>Choose password</span>
                            <input type="password" name="password1" style="width: 20em;" placeholder="password" />
                        </label>
                        <label>
                            <input type="password" name="password2" style="width: 20em;" placeholder="confirm" />
                        </label>
                        <div class="actions">
                            <button type="submit">Set password</button>
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
            throw new Error('Password is missing');
        if (password2 != password1)
            throw new Error('Passwords do not match');

        await Net.post('/api/user/reset', {
            token: token,
            password: password1
        });

        form.innerHTML = '';
        render(html`<p>Your account is now ready, please login to continue.</p>`, form);
    }
}

async function runLogin() {
    UI.main(html`
        <div class="tabbar">
            <a class="active">Login</a>
        </div>

        <div class="tab">
            <div class="box" style="align-items: center;">
                <div class="header">Login to ${ENV.title}</div>

                <form style="text-align: center;" @submit=${UI.wrap(submit)}>
                    <label>
                        <input type="text" name="mail" style="width: 20em;" placeholder="mail address" />
                    </label>
                    <label>
                        <input type="password" name="password" style="width: 20em;" placeholder="password" />
                    </label>
                    <div class="actions">
                        <button type="submit">Login</button>
                        <a href="/register">Create a new account?</a>
                        <a href="/recover">Lost your password?</a>
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
            throw new Error('Mail address is missing');
        if (!password)
            throw new Error('Password is missing');

        await login(mail, password);
    }
}

async function runRecover() {
    UI.main(html`
        <div class="tabbar">
            <a class="active">Recover account</a>
        </div>

        <div class="tab">
            <div class="box" style="align-items: center;">
                <div class="header">Recover account</div>

                <form style="text-align: center;" @submit=${UI.wrap(submit)}>
                    <label>
                        <input type="text" name="mail" style="width: 20em;" placeholder="mail address" />
                    </label>
                    <div class="actions">
                        <button type="submit">Reset password</button>
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
            throw new Error('Mail address is missing');

        await Net.post('/api/user/recover', { mail: mail });

        form.innerHTML = '';
        render(html`<p>Consult the <b>mail that has been sent</b> and click the link inside to reset your password.</p>`, form);
    }
}

async function runReset() {
    let error = 'Missing token';
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
            <a class="active">Recover account</a>
        </div>

        <div class="tab">
            <div class="box" style="align-items: center;">
                <div class="header">Recover account</div>

                <form style="text-align: center;" @submit=${UI.wrap(submit)}>
                    ${error == null ? html`
                        <label>
                            <span>New password</span>
                            <input type="password" name="password1" style="width: 20em;" placeholder="password" />
                        </label>
                        <label>
                            <input type="password" name="password2" style="width: 20em;" placeholder="confirm" />
                        </label>
                        <div class="actions">
                            <button type="submit">Confirm password</button>
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
            throw new Error('Password is missing');
        if (password2 != password1)
            throw new Error('Passwords do not match');

        await Net.post('/api/user/reset', {
            token: token,
            password: password1
        });

        form.innerHTML = '';
        render(html`<p>Your password has been changed, please login to continue.</p>`, form);
    }
}

function isLogged() {
    let logged = (session != null);
    return logged;
}

// ------------------------------------------------------------------------
// Repositories
// ------------------------------------------------------------------------

async function runDashboard() {
    cache.repositories = await Net.cache('repositories', '/api/repository/list');

    let repositories = UI.tableValues('repositories', cache.repositories, 'name');

    UI.main(html`
        <div class="tabbar">
            <a class="active">Overview</a>
            ${cache.repository != null ? html`<a href=${makeURL({ mode: 'repository' })}>${cache.repository.name}</a>` : ''}
        </div>

        <div class="tab">
            <div class="box">
                <div class="header">Repositories</div>
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
                                        ${!repo.checked ? 'Valid' : ''}
                                        ${repo.checked && !repo.errors ? 'Success' : ''}
                                        ${repo.errors ? html`<span style="color: red;">${repo.failed || 'Unknown error'}</span>` : ''}
                                        <br><span class="sub">${repo.checked ? (new Date(repo.checked)).toLocaleString() : 'Check pending'}</span>
                                    </td>
                                </tr>
                            `;
                        })}
                        ${!repositories.length ? html`<tr><td colspan="3" style="text-align: center;">No repository</td></tr>` : ''}
                    </tbody>
                </table>
                <div class="actions">
                    <button type="button" @click=${UI.wrap(e => configureRepository(null))}>Add repository</button>
                </div>
            </div>
        </div>
    `);
}

async function runRepository() {
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

    route.repository = cache.repository?.id;

    if (cache.repository == null) {
        go('/dashboard');
        return;
    }

    let channels = UI.tableValues('channels', cache.repository.channels, 'name');

    UI.main(html`
        <div class="tabbar">
            <a href="/dashboard">Overview</a>
            <a class="active">${cache.repository.name}</a>
        </div>

        <div class="tab">
            <div class="row">
                <div class="box" style="min-width: 200px;">
                    <div class="header">Repository</div>
                    <div class="info">
                        ${cache.repository.name}
                        <div class="sub">${cache.repository.url}</div>
                    </div>
                    <button type="button" @click=${UI.wrap(e => configureRepository(cache.repository))}>Configure</button>
                </div>

                <div class="box">
                    <div class="header">Channels</div>
                    <table style="table-layout: fixed; width: 100%;">
                        <colgroup>
                            <col></col>
                            <col></col>
                            <col></col>
                        </colgroup>
                        <thead>
                            <tr>
                                ${UI.tableHeader('channels', 'name', 'Name')}
                                ${UI.tableHeader('channels', 'size', 'Size')}
                                ${UI.tableHeader('channels', 'time', 'Timestamp')}
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
                            ${!channels.length ? html`<tr><td colspan="3" style="text-align: center;">No channel</td></tr>` : ''}
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
            url: '',
            user: 'log',
            password: '',
            variables: {}
        };
    }

    let url = repo.url;

    await UI.dialog({
        run: (render, close) => {
            let type = detectType(url);

            return html`
                <div class="title">
                    ${ptr != null ? 'Edit repository' : 'Create repository'}
                    <div style="flex: 1;"></div>
                    <button type="button" class="secondary" @click=${UI.wrap(close)}>✖\uFE0E</button>
                </div>

                <div class="main">
                    <label>
                        <span>Name</span>
                        <input name="name" required value=${repo.name} />
                    </label>
                    <div class="section">Repository</div>
                    <label>
                        <span>URL</span>
                        <input name="url" required value=${url}
                               @input=${e => { url = e.target.value; render(); }} />
                    </label>
                    <label>
                        <span>User</span>
                        <input name="user" required value=${repo.user} />
                    </label>
                    <label>
                        <span>Password</span>
                        <input name="password" required value=${repo.password} />
                    </label>
                    ${type == 's3' ? html`
                        <div class="section">S3</div>
                        <label>
                            <span>Key ID</span>
                            <input name="AWS_ACCESS_KEY_ID" required value=${repo.variables.AWS_ACCESS_KEY_ID ?? ''}>
                        </label>
                        <label>
                            <span>Secret access key</span>
                            <input name="AWS_SECRET_ACCESS_KEY" required value=${repo.variables.AWS_SECRET_ACCESS_KEY ?? ''}>
                        </label>
                    ` : ''}
                    ${type == 'ssh' ? html`
                        <div class="section">SFTP</div>
                        <label>
                            <span>Password</span>
                            <input name="SSH_PASSWORD" value=${repo.variables.SSH_PASSWORD ?? ''}>
                        </label>
                        <label>
                            <span>Key</span>
                            <input name="SSH_KEY" value=${repo.variables.SSH_KEY ?? ''}>
                        </label>
                        <label>
                            <span>Fingerprint</span>
                            <input name="SSH_FINGERPRINT" value=${repo.variables.SSH_FINGERPRINT ?? ''}>
                        </label>
                    ` : ''}
                </div>

                <div class="footer">
                    ${ptr != null ? html`
                        <button type="button" class="danger"
                                @click=${UI.confirm('Delete repository', e => deleteRepository(repo.id).then(close))}>Delete</button>
                        <div style="flex: 1;"></div>
                    ` : ''}
                    <button type="button" class="secondary" @click=${UI.insist(close)}>${T.cancel}</button>
                    <button type="submit">${ptr != null ? 'Save' : 'Create'}</button>
                </div>
            `
        },

        submit: async (elements) => {
            let obj = {
                id: repo.id,
                name: elements.name.value.trim() || null,
                url: elements.url.value.trim() || null,
                user: elements.user.value.trim() || null,
                password: elements.password.value.trim() || null,
                variables: {}
            };

            for (let el of elements) {
                let name = el.getAttribute('name');

                if (!name)
                    continue;
                if (obj.hasOwnProperty(name))
                    continue;
                if (!el.value)
                    continue;

                let value = el.value.toString();
                obj.variables[name] = value.trim() || null;
            }

            await Net.post('/api/repository/save', obj);

            Net.invalidate('repositories');
            Net.invalidate('repository');
        }
    });
}

async function deleteRepository(id) {
    await Net.post('/api/repository/delete', { id: id });
    Net.invalidate('repositories');
}

function detectType(url) {
    if (url.startsWith("http://"))
        return 's3';
    if (url.startsWith("https://"))
        return 's3';
    if (url.startsWith("s3://"))
        return 's3';

    if (url.startsWith("ssh://"))
        return 'ssh';
    if (url.startsWith("sftp://"))
        return 'ssh';
    if (url.match(/^[a-zA-Z0-9_\-\.]+@.+:$/))
        return 'ssh';

    return null;
}

async function runChannel(repo, channel) {
    let url = Util.pasteURL('/api/repository/snapshots', { id: repo.id, channel: channel });
    let snapshots = await Net.cache('snapshots', url);

    // Make sure it is sorted by time
    snapshots.sort((snapshot1, snapshot2) => snapshot1.time - snapshot2.time);

    let canvas = document.createElement('canvas');

    if (snapshots.length >= 2) {
        let size = {
            label: 'Size',
            data: [],
            fill: false
        };
        let storage = {
            label: 'Storage',
            data: [],
            fill: false
        };

        for (let snapshot of snapshots) {
            size.data.push({ x: snapshot.time, y: snapshot.size });
            storage.data.push({ x: snapshot.time, y: snapshot.storage });
        }

        let ctx = canvas.getContext('2d');

        new Chart(ctx, {
            type: 'line',
            data: {
                datasets: [size, storage]
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
                    ${channel} snapshots
                    <div style="flex: 1;"></div>
                    <button type="button" class="secondary" @click=${UI.wrap(close)}>✖\uFE0E</button>
                </div>

                <div class="main">
                    <div style="width: 80%; margin: 0 auto;">${canvas}</div>
                    <table style="table-layout: fixed;">
                        <colgroup>
                            <col></col>
                            <col></col>
                            <col></col>
                            <col></col>
                        </colgroup>
                        <thead>
                            <tr>
                                ${UI.tableHeader('snapshots', 'time', 'Timestamp')}
                                ${UI.tableHeader('snapshots', 'hash', 'Hash')}
                                ${UI.tableHeader('snapshots', 'size', 'Size')}
                                ${UI.tableHeader('snapshots', 'storage', 'Storage')}
                            </tr>
                        </thead>
                        <tbody>
                            ${snapshots.map(snapshot => html`
                                <tr>
                                    <td>${(new Date(snapshot.time)).toLocaleString()}</td>
                                    <td><span class="sub">${snapshot.hash}</span></td>
                                    <td style="text-align: right;">${formatSize(snapshot.size)}</td>
                                    <td style="text-align: right;">${formatSize(snapshot.storage)}</td>
                                </tr>
                            `)}
                            ${!snapshots.length ? html`<tr><td colspan="4" style="text-align: center;">No snapshot</td></tr>` : ''}
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

        return value.toFixed(prec) + ' GB';
    } else if (size >= 999950) {
        let value = size / 1000000;
        let prec = 1 + (value < 9.9995) + (value < 99.995);

        return value.toFixed(prec) + ' MB';
    } else if (size >= 999.95) {
        let value = size / 1000;
        let prec = 1 + (value < 9.9995) + (value < 99.995);

        return value.toFixed(prec) + ' kB';
    } else {
        return size + ' B';
    }
}

// ------------------------------------------------------------------------
// Account
// ------------------------------------------------------------------------

async function runAccount() {
    UI.main(html`
        <div class="tabbar">
            <a class="active">Account</a>
        </div>

        <div class="tab">
            <div class="box" style="align-items: center;">
                <div class="header">Account</div>
                <div class="sub">${session.username}</div>
                <img class="picture" src=${`/pictures/${session.userid}?v=${session.picture}`} alt="" />
                <div class="actions">
                    <button type="button" class="secondary" @click=${UI.wrap(changePicture)}>Change picture</button>
                    <button type="button" @click=${UI.insist(logout)}>Logout</button>
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

    let cropper = new PictureCropper('Change picture', 256);

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

export {
    route,

    start,
    go,
    run,

    isLogged
}
