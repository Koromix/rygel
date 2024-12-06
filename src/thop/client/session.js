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
import { Util, Log, Net } from '../../web/core/common.js';
import * as thop from './thop.js';
import { settings } from './thop.js';

let route = {};

async function run() {
    if (!ENV.has_users)
        throw new Error('Le module utilisateur est désactivé');

    switch (route.mode) {
        case 'login': { runLogin(); } break;

        default: {
            throw new Error(`Mode inconnu '${route.mode}'`);
        } break;
    }
}

function parseURL(path, query) {
    let args = {
        mode: path[0] || 'login'
    };

    return args;
}

function makeURL(args = {}) {
    args = Util.assignDeep({}, route, args);
    return `${ENV.base_url}user/${args.mode}/`;
}

// ------------------------------------------------------------------------
// Login
// ------------------------------------------------------------------------

function runLogin() {
    // Options
    render('', document.querySelector('#th_options'));

    document.title = 'THOP – Connexion';

    // View
    render(html`
        <form class="th_form" @submit=${handleLoginSubmit}>
            <fieldset id="usr_fieldset" style="margin: 0; padding: 0; border: 0;" ?disabled=${false}>
                <label>Utilisateur : <input id="usr_username" type="text"/></label>
                <label>Mot de passe : <input id="usr_password" type="password"/></label>

                <button>Se connecter</button>
            </fieldset>
        </form>
    `, document.querySelector('#th_view'));

    document.querySelector('#usr_username').focus();
}

function handleLoginSubmit(e) {
    let fieldset_el = document.querySelector('#usr_fieldset');
    let username_el = document.querySelector('#usr_username');
    let password_el = document.querySelector('#usr_password');

    let username = (username_el.value || '').trim();
    let password = (password_el.value || '').trim();

    fieldset_el.disabled = true;
    let p = login(username, password);

    p.then(success => {
        if (success) {
            thop.goBack();
        } else {
            fieldset_el.disabled = false;
            password_el.focus();
        }
    });
    p.catch(err => {
        fieldset_el.disabled = false;
        Log.error(err);
    });

    e.preventDefault();
}

async function login(username, password) {
    try {
        await Net.post(`${ENV.base_url}api/user/login`, {
            username: username.toLowerCase(),
            password: password
        });

        Log.success('Vous êtes connecté(e)');
        readSessionCookies(false);

        return true;
    } catch (err) {
        Log.error('Connexion refusée : utilisateur ou mot de passe incorrect');
        return false;
    }
}

async function logout() {
    try {
        await Net.post(`${ENV.base_url}api/user/logout`);

        Log.info('Vous êtes déconnecté(e)');
        readSessionCookies(false);

        return true;
    } catch (err) {
        // Should never happen, but just in case...
        Log.error('Déconnexion refusée');

        return false;
    }
}

// ------------------------------------------------------------------------
// Session
// ------------------------------------------------------------------------

let session_rnd = 0;

function readSessionCookies(warn = true) {
    let new_session_rnd = Util.getCookie('session_rnd') || 0;

    if (new_session_rnd !== session_rnd && !new_session_rnd && warn)
        Log.info('Votre session a expiré');

    session_rnd = new_session_rnd;
}

function isConnected() { return !!session_rnd; }
function getSessionRnd() { return session_rnd; }

export {
    route,

    run,
    parseURL,
    makeURL,

    login,
    logout,

    readSessionCookies,
    isConnected,
    getSessionRnd
}
