// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import { render, html, live, unsafeHTML } from 'vendor/lit-html/lit-html.bundle.js';
import { Util, Log, Net } from 'lib/web/base/base.js';
import * as UI from 'lib/web/ui/ui.js';
import * as App from './main.js';
import { route, cache, session } from './main.js';
import { PictureCropper } from './picture.js';
import { ASSETS } from '../assets/assets.js';

async function runRegister() {
    if (!ENV.auth.internal) {
        App.go('/login');
        return;
    }

    UI.main(html`
        <div class="header">${T.create_your_account}</div>

        <div class="block" style="align-items: center;">
            <div class="columns">
                <form style="text-align: center;" @submit=${UI.wrap(submit)}>
                    <label>
                        <input type="text" name="mail" style="width: 20em;" placeholder=${T.mail_address.toLowerCase()} />
                    </label>
                    <div class="actions">
                        <button type="submit">${T.register}</button>
                        <a href="/login">${T.already_have_account}</a>
                    </div>
                </form>

                ${ENV.auth.providers.length ? html`
                    <div>
                        <p class="sub" style="text-align: center;">${T.you_can_login_with}</p>

                        <div class="actions">
                            ${ENV.auth.providers.map(provider =>
                                html`<button type="button" @click=${UI.wrap(e => App.sso(provider.issuer, '/'))}>${T.format(T.login_with_x, provider.title)}</button>`)}
                        </div>
                    </div>
                ` : ''}
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

        let div = form.parentNode;

        div.innerHTML = '';
        render(html`<p>${unsafeHTML(T.consult_registration_mail)}</p>`, div);
    }
}

async function runFinalize() {
    if (!ENV.auth.internal) {
        App.go('/login');
        return;
    }

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
        <div class="header">${T.create_password}</div>

        <div class="block" style="align-items: center;">
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
    if (session == null) {
        UI.main(html`
            <div class="header">${T.format(T.login_to_x, ENV.title)}</div>

            <div class="block" style="align-items: center;">
                <div class="columns">
                    ${ENV.auth.internal ? html`
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
                    ` : ''}

                    ${ENV.auth.providers.length ? html`
                        <div>
                            ${ENV.auth.internal ? html`<p class="sub" style="text-align: center;">${T.you_can_login_with}</p>` : ''}

                            <div class="actions">
                                ${ENV.auth.providers.map(provider =>
                                    html`<button type="button" @click=${UI.wrap(e => App.sso(provider.issuer, window.location.pathname))}>${T.format(T.login_with_x, provider.title)}</button>`)}
                            </div>
                        </div>
                    ` : ''}
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

            await App.login(mail, password);
        }
    } else if (!session.authorized) {
        UI.main(html`
            <div class="header">${T.format(T.login_to_x, ENV.title)}</div>

            <div class="block" style="align-items: center;">
                <form style="text-align: center;" @submit=${UI.wrap(submit)}>
                    <p>${T.two_factor_authentication}</p>
                    <label  style="text-align: center;">
                        <input type="text" name="code" style="width: 20em;" placeholder=${T.totp_digits} />
                    </label>
                    <div class="actions">
                        <button type="submit">${T.confirm}</button>
                        <button type="button" class="secondary" @click=${UI.insist(App.logout)}>${T.cancel}</button>
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

            await App.confirm(code);
        }
    } else {
        App.go('/');
    }
}

async function runOidc() {
    let query = new URLSearchParams(window.location.search);

    let code = query.get('code');
    let state = query.get('state');

    if (code == null || state == null) {
        console.error('Missing OIDC callback parameters');

        App.go('/');
        return;
    }

    try {
        await App.oidc(code, state);
    } catch (err) {
        Log.error(err);
        App.go('/login');
    }
}

async function runRecover() {
    if (!ENV.auth.internal) {
        App.go('/login');
        return;
    }

    UI.main(html`
        <div class="header">${T.recover_account}</div>

        <div class="block" style="align-items: center;">
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
    if (!ENV.auth.internal) {
        App.go('/login');
        return;
    }

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
        <div class="header">${T.recover_account}</div>

        <div class="block" style="align-items: center;">
            <form @submit=${UI.wrap(submit)}>
                ${error == null ? html`
                    <label>
                        <span>${T.new_password}</span>
                        <input type="password" name="password1" style="width: 20em;" placeholder=${T.password.toLowerCase()} />
                    </label>
                    <label>
                        <span>${T.confirmation}</span>
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
        render(html`<p>${unsafeHTML(T.password_reset_successful)}</p>`, form);

        let url = window.location.pathname + window.location.search;
        history.replaceState(null, null, url);
    }
}

async function runLink() {
    let token = null;

    if (window.location.hash) {
        let hash = window.location.hash.substr(1);
        let query = new URLSearchParams(hash);

        token = query.get('token') || null;
    }
    if (token == null)
        throw new Error(T.message(`Missing token`));

    let link = await Net.post('/api/sso/link', { token: token });
    let provider = ENV.auth.providers.find(provider => provider.issuer == link.issuer);

    UI.main(html`
        <div class="header">${T.account_link}</div>

        <div class="block" style="align-items: center;">
            <div>
                <p>${unsafeHTML(T.format(T.account_linked_with_x, provider.title))}</p>
            </div>
        </div>
    `);
}

async function runAccount() {
    UI.main(html`
        <div class="header">${T.account}</div>

        <div class="block" style="align-items: center;">
            <div class="sub">${session.username}</div>
            <img class="picture" src=${`/pictures/${session.userid}?v=${session.picture}`} alt="" />
            <div class="actions horizontal">
                <button type="button" class="secondary" @click=${UI.wrap(changePicture)}>${T.change_picture}</button>
                <button type="button" class="secondary" @click=${UI.wrap(configureSecurity)}>${T.account_security}</button>
            </div>
        </div>

        <div class="actions">
            <button type="button" class="secondary" @click=${UI.insist(App.logout)}>${T.logout}</button>
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
}

async function configureSecurity() {
    let security = await Net.get('/api/user/security');

    let tab = security.password ? 'password' : 'identities';

    let enable_totp = !security.totp;
    let totp = ENV.auth.internal ? await Net.get('/api/totp/secret') : null;

    await UI.dialog({
        run: (render, close) => {
            let can_dissociate = security.password || (security.identities.reduce((acc, identity) => acc + identity.allowed, 0) > 1);

            return html`
                <div class="title">
                    ${T.account_security}
                    <div style="flex: 1;"></div>
                    <button type="button" class="secondary" @click=${UI.insist(close)}>✖\uFE0E</button>
                </div>

                <div class="main">
                    <div class="tabbar">
                        ${security.password ? html`
                            <a class=${tab == 'password' ? 'active' : '' } @click=${e => { tab = 'password'; render(); }}>${T.password}</a>
                            <a class=${tab == 'totp' ? 'active' : '' } @click=${e => { tab = 'totp'; render(); }}>${T.two_factor_authentication}</a>
                            ${ENV.auth.providers.length ? html`<a class=${tab == 'identities' ? 'active' : '' } @click=${e => { tab = 'identities'; render(); }}>${T.external_identities}</a>` : ''}
                        ` : ''}
                        ${!security.password ? html`
                            <a class=${tab == 'identities' ? 'active' : '' } @click=${e => { tab = 'identities'; render(); }}>${T.external_identities}</a>
                            ${ENV.auth.internal ? html`<a class=${tab == 'password' ? 'active' : '' } @click=${e => { tab = 'password'; render(); }}>${T.password}</a>` : ''}
                        ` : ''}
                    </div>

                    <div class="block">
                        ${tab == 'password' && security.password ? html`
                            <label>
                                <span>${T.new_password}</span>
                                <input type="password" name="password1" style="width: 20em;" placeholder=${T.new_password.toLowerCase()} />
                            </label>
                            <label>
                                <span>${T.confirmation}</span>
                                <input type="password" name="password2" style="width: 20em;" placeholder=${T.confirmation.toLowerCase()} />
                            </label>
                        ` : ''}
                        ${tab == 'password' && !security.password ? html`
                            <p>${T.this_account_has_no_password_create_one}</p>

                            <label>
                                <input type="checkbox" name="recover" />
                                <span>${T.enable_direct_login_with_password}</span>
                            </label>
                        ` : ''}

                        ${tab == 'totp' ? html`
                            ${security.totp ? html`
                                <label>
                                    <input type="checkbox" checked @change=${UI.wrap(e => { enable_totp = !e.target.checked; render(); })} />
                                    <span>${T.disable_2fa}</span>
                                </label>
                            ` : ''}

                            ${enable_totp ? html`
                                <div style="text-align: center;"><img src=${totp.image} alt="" /></div>
                                <p style="text-align: center; font-size: 0.8em; margin-top: 0;">${totp.secret}</p>

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
                        ` : ''}

                        ${tab == 'identities' ? html`
                            <p>${T.list_of_external_identities}</p>

                            <table style="table-layout: fixed;">
                                <colgroup>
                                    <col style="width: 200px;"/>
                                    <col style="width: 100px;"/>
                                </colgroup>
                                <thead>
                                    <tr>
                                        <th>${T.provider}</th>
                                        <th>${T.status}</th>
                                    </tr>
                                </thead>
                                <tbody>
                                    ${ENV.auth.providers.map(provider => {
                                        let identity = security.identities.find(identity => identity.issuer == provider.issuer);

                                        return html`
                                            <tr>
                                                <td>
                                                    ${provider.title}
                                                    ${identity?.allowed === false ? html`<span class="sub">(${T.pending.toLowerCase()})</span>` : ''}
                                                </td>
                                                <td class="center">
                                                    ${identity != null && !can_dissociate ? T.allowed : ''}
                                                    ${identity != null && can_dissociate ? html`
                                                        <button type="button" class="danger small"
                                                                @click=${UI.wrap(e => delete_identity(identity.id))}>${T.dissociate}</button>
                                                    ` : ''}
                                                    ${identity == null ? html`<span class="sub">${T.not_used}</span>` : ''}
                                                </td>
                                            </tr>
                                        `;
                                    })}
                                </tbody>
                            </table>
                        ` : ''}
                    </div>
                </div>

                ${tab != 'identities' ? html`
                    <div class="footer">
                        <button type="button" class="secondary" @click=${UI.insist(close)}>${T.cancel}</button>
                        <button type="submit">${T.confirm}</button>
                    </div>
                ` : ''}
            `;

            async function delete_identity(id) {
                await UI.dialog((render, close) => html`
                    <div class="title">${T.dissociate}</div>
                    <div class="main">${T.confirm_not_reversible}</div>
                    <div class="footer">
                        <button type="button" class="secondary" @click=${UI.wrap(close)}>${T.cancel}</button>
                        <button type="submit">${T.confirm}</button>
                    </div>
                `);

                await Net.post('/api/sso/unlink', { identity: id });

                security = await Net.get('/api/user/security');
                render();
            }
        },

        submit: async (elements) => {
            switch (tab) {
                case 'password': {
                    if (security.password) {
                        let password1 = elements.password1.value.trim();
                        let password2 = elements.password2.value.trim();

                        if (!password1 || !password2)
                            throw new Error(T.message(`New password is missing`));
                        if (password2 != password1)
                            throw new Error(T.message(`Passwords do not match`));

                        let old_password = security.password ? await confirmPassword() : null;

                        await Net.post('/api/user/password', {
                            old_password: old_password,
                            new_password: password1
                        });
                    } else {
                        if (!elements.recover.checked)
                            throw new Error(T.message(`Please check to continue`));

                        await Net.post('/api/user/recover', { mail: security.mail });

                        Log.info(unsafeHTML(T.consult_reset_mail));
                    }
                } break;

                case 'totp': {
                    let password = security.password ? await confirmPassword() : null;

                    if (enable_totp) {
                        await Net.post('/api/totp/change', {
                            token: totp.token,
                            password: password,
                            code: elements.code.value
                        });
                    } else {
                        await Net.post('/api/totp/disable', { password: password });
                    }
                } break;
            }
        }
    });
}

async function confirmPassword() {
    let password = await UI.dialog({
        run: (render, close) => html`
            <div class="title">${T.confirm_identity}</div>
            <div class="main">
                <p>${T.confirm_identity_with_password}</p>
                <label>
                    <span>${T.current_password}</span>
                    <input type="password" name="password" style="width: 20em;" placeholder=${T.current_password.toLowerCase()} />
                </label>
            </div>
            <div class="footer">
                <button type="button" class="secondary" @click=${UI.wrap(close)}>${T.cancel}</button>
                <button type="submit">${T.confirm}</button>
            </div>
        `,

        submit: async (elements) => {
            let password = elements.password.value.trim();

            if (!password)
                throw new Error(T.message(`Password is missing`));

            return password;
        }
    });

    return password;
}

export {
    runLogin,
    runOidc,
    runRegister,
    runFinalize,
    runRecover,
    runReset,
    runLink,
    runAccount
}
