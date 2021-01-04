// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

const goupile = new function() {
    let self = this;

    let session_profile = {};
    let session_rnd;
    let passport;

    let controller;
    let current_url;

    this.start = async function() {
        ui.init();

        if (ENV.base_url === '/admin/') {
            controller = new AdminController;
        } else {
            controller = new InstanceController;
        }

        await controller.start();
    };

    this.runLogin = function() {
        return ui.runScreen((d, resolve, reject) => {
            d.output(html`
                <img id="gp_logo" src=${ENV.base_url + 'favicon.png'} alt="" />
                <br/>
            `);

            let username = d.text('*username', 'Nom d\'utilisateur');
            let password = d.password('*password', 'Mot de passe');

            d.action('Se connecter', {disabled: !d.isValid()}, async () => {
                try {
                    await self.login(username.value, password.value);
                    resolve(username.value);
                } catch (err) {
                    // Never reject because we want to keep the screen open
                    log.error(err);
                }
            });
        });
    };

    this.login = async function(username, password) {
        let progress = log.progress('Connexion en cours');

        try {
            let query = new URLSearchParams;
            query.set('username', username.toLowerCase());
            query.set('password', password);

            let response = await net.fetch(`${ENV.base_url}api/user/login`, {
                method: 'POST',
                body: query
            });

            if (response.ok) {
                session_profile = await response.json();
                session_rnd = util.getCookie('session_rnd');
                passport = (session_profile.passport != null) ? util.base64ToBytes(session_profile.passport) : null;

                progress.success('Connexion réussie');
            } else {
                let err = (await response.text()).trim();
                throw new Error(err);
            }
        } catch (err) {
            progress.close();
            throw err;
        }
    }

    this.logout = async function() {
        let progress = log.progress('Déconnexion en cours');

        try {
            let response = await net.fetch(`${ENV.base_url}api/user/logout`, {method: 'POST'})

            if (response.ok) {
                session_profile = {};
                session_rnd = undefined;
                passport = undefined;

                progress.success('Déconnexion réussie');
            } else {
                let err = (await response.text()).trim();
                throw new Error(err);
            }
        } catch (err) {
            progress.close();
            throw err;
        }
    }

    this.syncProfile = async function() {
        let new_rnd = util.getCookie('session_rnd');

        if (new_rnd !== session_rnd) {
            let response = await net.fetch(`${ENV.base_url}api/user/profile`);
            let profile = await response.json();

            session_profile = profile;
            session_rnd = util.getCookie('session_rnd');
            passport = (session_profile.passport != null) ? util.base64ToBytes(session_profile.passport) : null;
        }
    };

    this.isAuthorized = function() { return !!session_profile.username; };
    this.getUserName = function() { return session_profile.username; };

    this.syncHistory = function(url, push = true) {
        if (push && current_url != null && url !== current_url) {
            window.history.pushState(null, null, url);
        } else {
            window.history.replaceState(null, null, url);
        }

        current_url = url;
    };

    this.encryptWithPassport = function(obj) {
        if (passport == null)
            throw new Error('Cannot encrypt without passport');

        return encrypt(obj, passport);
    };

    this.decryptWithPassport = function(enc) {
        if (passport == null)
            throw new Error('Cannot decrypt without passport');

        return decrypt(enc, passport);
    };

    async function encrypt(obj, key) {
        let nonce = new Uint8Array(24);
        crypto.getRandomValues(nonce);

        let json = JSON.stringify(obj);
        let message = util.stringToBytes(json);
        let box = nacl.secretbox(message, nonce, key);

        let enc = {
            nonce: util.bytesToBase64(nonce),
            box: util.bytesToBase64(box)
        };
        return enc;
    }

    async function decrypt(enc, key) {
        let nonce = util.base64ToBytes(enc.nonce);
        let box = util.base64ToBytes(enc.box);

        let message = nacl.secretbox.open(box, nonce, key);
        if (message == null)
            throw new Error('Failed to decrypt message: wrong key?');

        let json = util.bytesToString(message);
        let obj = JSON.parse(json);

        return obj;
    }
};
