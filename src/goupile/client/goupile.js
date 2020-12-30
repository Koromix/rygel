// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

const goupile = new function() {
    let self = this;

    let session_profile = {};
    let session_rnd;

    let controller;

    this.start = async function() {
        if (ENV.base_url === '/admin/') {
            document.documentElement.classList.add('admin');
            controller = new AdminController;
        } else {
            document.documentElement.classList.add('instance');
            controller = new InstanceController;
        }

        await controller.init();
        controller.run();
    };

    this.runLogin = function() {
        ui.runScreen((d, resolve, reject) => {
            d.output(html`
                <img id="gp_logo" src="/admin/favicon.png" alt="" />
                <br/>
            `);

            let username = d.text('*username', 'Nom d\'utilisateur');
            let password = d.password('*password', 'Mot de passe');

            d.action('Se connecter', {disabled: !d.isValid()}, async () => {
                let success = await self.login(username.value, password.value);

                // Never reject because we want to keep the screen open
                if (success)
                    resolve(username.value);
            });
        });
    }

    this.login = async function(username, password) {
        let notification = log.progress('Connexion en cours');

        try {
            let query = new URLSearchParams;
            query.set('username', username.toLowerCase());
            query.set('password', password);

            let response = await net.fetch('/admin/api/user/login', {
                method: 'POST',
                body: query
            });

            if (response.ok) {
                session_profile = await response.json();
                session_rnd = util.getCookie('session_rnd');

                notification.success('Connexion réussie');
                controller.run();

                return true;
            } else {
                let err = (await response.text()).trim();
                notification.error(err);

                return false;
            }
        } catch (err) {
            notification.error(err);
            return false;
        }
    }

    this.logout = async function() {
        let notification = log.progress('Connexion en cours');

        try {
            let response = await net.fetch('/admin/api/user/logout', {method: 'POST'})

            if (response.ok) {
                session_profile = {};
                session_rnd = undefined;

                notification.success('Déconnexion réussie');
                controller.run();

                return true;
            } else {
                notification.error('Échec de la connexion');
                return false;
            }
        } catch (err) {
            notification.error(err);
            return false;
        }
    }

    this.syncProfile = async function() {
        let new_rnd = util.getCookie('session_rnd');

        if (new_rnd !== session_rnd) {
            let response = await net.fetch('/admin/api/user/profile');
            let profile = await response.json();

            session_profile = profile;
            session_rnd = util.getCookie('session_rnd');
        }
    };

    this.isAuthorized = function() { return !!session_profile.username; }
};
