// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let user = (function() {
    let self = this;

    let change_handlers = [];

    let url_key = 0;
    let username = null;

    this.runModule = function(route) {
        if (!thop.isBusy()) {
            let view_el = query('#view');

            render(html`
                <form>
                    <label>Utilisateur : <input id="usr_username" type="text"/></label>
                    <label>Mot de passe : <input id="usr_password" type="password"/></label>

                    <button id="usr_button" @click=${e => {self.login(); e.preventDefault(); }}>Se connecter</button>
                </form>
            `, view_el);

            if (!document.querySelector('#usr_password:focus'))
                query('#usr_username').focus();
            query('#usr_button').disabled = false;
        }
    };

    this.routeToUrl = function(args) {
        return {
            url: thop.baseUrl('login'),
            allowed: true
        };
    };

    this.go = function(args, delay) {
        thop.route(self.routeToUrl(args).url, delay);
    };

    this.connect = function(username, password, func) {
        let url = util.buildUrl(thop.baseUrl('api/connect.json'));
        let params = {
            username: username,
            password: password
        };

        data.post(url, 'json', params, function() {
            self.updateSession();
            if (func)
                func();
        });
    };

    this.disconnect = function(func) {
        let url = util.buildUrl(thop.baseUrl('api/disconnect.json'));
        let params = {};

        data.post(url, 'json', params, function(json) {
            self.updateSession();
            if (func)
                func();
        });
    };

    this.login = function() {
        let username = query('#usr_username').value;
        let password = query('#usr_password').value;
        query('#usr_username').value = '';
        query('#usr_password').value = '';
        query('#usr_button').disabled = true;

        self.connect(username, password, thop.goBackOrHome);
    };

    this.logout = function() {
        self.disconnect();
    };

    this.updateSession = function() {
        let prev_url_key = url_key;

        url_key = util.getCookie('url_key') || 0;
        username = util.getCookie('username');

        if (url_key !== prev_url_key) {
            for (let handler of change_handlers)
                handler();
        }
    };

    this.addChangeHandler = function(func) { change_handlers.push(func); }

    this.isConnected = function() { return !!url_key; }
    this.getUrlKey = function() { return url_key; }
    this.getUsername = function() { return username; }

    return this;
}).call({});
