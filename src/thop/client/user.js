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

const user = new function() {
    let self = this;

    let route = {};
    this.route = route;

    this.run = async function() {
        if (!ENV.has_users)
            throw new Error('Le module utilisateur est désactivé');

        switch (route.mode) {
            case 'login': { runLogin(); } break;

            default: {
                throw new Error(`Mode inconnu '${route.mode}'`);
            } break;
        }
    };

    this.parseURL = function(path, query) {
        let args = {
            mode: path[0] || 'login'
        };

        return args;
    };

    this.makeURL = function(args = {}) {
        args = util.assignDeep({}, route, args);
        return `${ENV.base_url}user/${args.mode}/`;
    };

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
        let p = self.login(username, password);

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
            log.error(err);
        });

        e.preventDefault();
    }

    this.login = async function(username, password) {
        let query = new URLSearchParams;
        query.set('username', username.toLowerCase());
        query.set('password', password);

        let response = await net.fetch(`${ENV.base_url}api/user/login`, {
            method: 'POST',
            body: query
        });

        if (response.ok) {
            log.success('Vous êtes connecté(e)');
            self.readSessionCookies(false);
        } else {
            log.error('Connexion refusée : utilisateur ou mot de passe incorrect');
        }

        return response.ok;
    };

    this.logout = async function() {
        let response = await net.fetch(`${ENV.base_url}api/user/logout`, {method: 'POST'});

        if (response.ok) {
            log.info('Vous êtes déconnecté(e)');
            self.readSessionCookies(false);
        } else {
            // Should never happen, but just in case...
            log.error('Déconnexion refusée');
        }

        return response.ok;
    };

    // ------------------------------------------------------------------------
    // Session
    // ------------------------------------------------------------------------

    let session_rnd = 0;

    this.readSessionCookies = function(warn = true) {
        let new_session_rnd = util.getCookie('session_rnd') || 0;

        if (new_session_rnd !== session_rnd && !new_session_rnd && warn)
            log.info('Votre session a expiré');

        session_rnd = new_session_rnd;
    };

    this.isConnected = function() { return !!session_rnd; };
    this.getSessionRnd = function() { return session_rnd; };
};
