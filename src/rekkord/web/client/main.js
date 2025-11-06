// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import { render, html, live, unsafeHTML } from '../../../../vendor/lit-html/lit-html.bundle.js';
import { Util, Log, Net, HttpError } from '../../../core/web/base/base.js';
import { Base64 } from '../../../core/web/base/mixer.js';
import * as UI from '../../../core/web/base/ui.js';
import { deploy } from '../../../core/web/flat/static.js';
import { PictureCropper } from './picture.js';
import { runPlans, runPlan } from './plan.js';
import { runRepositories, runRepository } from './repository.js';
import dayjs from '../../../../vendor/dayjs/dayjs.bundle.js';
import { ASSETS } from '../assets/assets.js';

import en from '../../i18n/en.json';
import fr from '../../i18n/fr.json';

import '../assets/client.css';

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
            if (isLogged()) {
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
    if (isLogged() && ping_timer == null) {
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
                } else if (!session.confirmed) {
                    await runConfirm();
                }
            } break;
        }

        if (isLogged()) {
            cache.repositories = await Net.cache('repositories', '/api/repository/list');

            switch (route.mode) {
                case 'repositories': { await runRepositories(); } break;
                case 'repository': { await runRepository(); } break;
                case 'plans': { await runPlans(); } break;
                case 'plan': { await runPlan(); } break;
                case 'account': { await runAccount(); } break;
            }
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
                ${isLogged() ? html`
                    <li><a href=${!in_repositories && route.repository != null ? makeURL({ mode: 'repository' }) : '/repositories'}
                           class=${in_repositories ? 'active' : ''}>${T.repositories}</a></li>
                    <li><a href=${!in_plans && route.plan != null ? makeURL({ mode: 'plan' }) : '/plans'}
                           class=${in_plans ? 'active' : ''}>${T.plans}</a></li>
                ` : ''}
                <div style="flex: 1;"></div>
                ${isLogged() ? html`
                    <li><a href="/account" class=${route.mode == 'account' ? 'active' : ''}>${T.account}</a></li>
                    <img class="picture" src=${`/pictures/${session.userid}?v=${session.picture}`} alt="" />
                ` : ''}
                ${!isLogged() ? html`
                    <li><a href="/register" class=${route.mode == 'register' ? 'active' : ''}>${T.register}</a></li>
                    <li><a href="/repositories" class=${route.mode != 'register' ? 'active' : ''}>${T.login}</a></li>
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
            <a class="active">${T.new_account}</a>
        </div>

        <div class="tab" style="align-items: center;">
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

        <div class="tab" style="align-items: center;">
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

        <div class="tab" style="align-items: center;">
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

        <div class="tab" style="align-items: center;">
            <div class="header">${T.format(T.login_to_x, ENV.title)}</div>

            <form style="text-align: center;" @submit=${UI.wrap(submit)}>
                <p>${T.two_factor_authentication}</p>
                <label>
                    <input type="text" name="code" style="width: 20em;" placeholder=${T.totp_digits} />
                </label>
                <div class="actions">
                    <button type="submit">${T.confirm}</button>
                    <button type="button" class="secondary" @click=${UI.insist(logout)}>${T.cancel}</button>
                </div>
            </form>
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

        <div class="tab" style="align-items: center;">
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

        <div class="tab" style="align-items: center;">
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
    if (session == null)
        return false;
    if (!session.confirmed)
        return false;

    return true;
}

// ------------------------------------------------------------------------
// Account
// ------------------------------------------------------------------------

async function runAccount() {
    UI.main(html`
        <div class="tabbar">
            <a class="active">${T.account}</a>
        </div>

        <div class="tab" style="align-items: center;">
            <div class="header">${T.account}</div>
            <div class="sub">${session.username}</div>
            <img class="picture" src=${`/pictures/${session.userid}?v=${session.picture}`} alt="" />
            <div class="actions">
                <button type="button" class="secondary" @click=${UI.wrap(changePicture)}>${T.change_picture}</button>
                <button type="button" class="secondary" @click=${UI.wrap(configureTOTP)}>${T.configure_2fa}</button>
                <button type="button" class="secondary" @click=${UI.insist(logout)}>${T.logout}</button>
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
                <button type="button" class="secondary" @click=${UI.insist(close)}>✖\uFE0E</button>
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
                    <div style="text-align: center; margin-top: 2em;"><img src=${qrcode.image} alt="" /></div>
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
    cache,

    start,

    makeURL,
    go,
    run,

    isLogged
}
