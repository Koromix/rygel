// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let user = new function() {
    let self = this;

    let route = {};
    this.route = route;

    this.run = async function() {
        if (!env.has_users)
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
        return `${env.base_url}user/${args.mode}/`;
    };

    // ------------------------------------------------------------------------
    // Login
    // ------------------------------------------------------------------------

    function runLogin() {
        // Options
        render('', document.querySelector('#th_options'));

        // View
        render(html`
            <form class="th_form" @submit=${handleLoginSubmit}>
                <fieldset id="usr_fieldset" style="margin: 0; padding: 0; border: 0;">
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

        fieldset_el.disabled = true;
        let p = self.login(username_el.value, password_el.value);

        p.then(success => {
            fieldset_el.disabled = false;

            if (success) {
                username_el.value = '';
                password_el.value = '';

                thop.go();
            } else {
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
        let response = await fetch(`${env.base_url}api/login.json`, {
            method: 'POST',
            body: new URLSearchParams({
                username: username,
                password: password
            })
        });

        if (response.ok) {
            log.success('Vous êtes connecté(e) !');
            self.readSessionCookies(false);
        } else {
            log.error('Connexion refusée : utilisateur ou mot de passe incorrect');
        }

        return response.ok;
    };

    this.logout = async function() {
        let response = await fetch(`${env.base_url}api/logout.json`, {method: 'POST'});

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

    let username;
    let url_key = 0;

    this.readSessionCookies = function(warn = true) {
        let prev_url_key = url_key;

        username = util.getCookie('username') || null;
        url_key = util.getCookie('url_key') || 0;

        if (url_key !== prev_url_key && !username && warn)
            log.info('Votre session a expiré');
    };

    this.isConnected = function() { return !!url_key; }
    this.getUrlKey = function() { return url_key; }
    this.getUserName = function() { return username; }
};
