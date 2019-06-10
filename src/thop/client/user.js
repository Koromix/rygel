// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let user = {};
(function() {
    let change_handlers = [];

    let url_key = 0;
    let username = null;

    function runModule(route)
    {
        if (!thop.isBusy()) {
            let view_el = query('#view');

            render(html`
                <form>
                    <label>Utilisateur : <input id="usr_username" type="text"/></label>
                    <label>Mot de passe : <input id="usr_password" type="password"/></label>

                    <button id="usr_button" @click=${e => {user.login(); e.preventDefault(); }}>Se connecter</button>
                </form>
            `, view_el);

            if (!document.querySelector('#usr_password:focus'))
                query('#usr_username').focus();
            query('#usr_button').disabled = false;
        }
    }
    this.runModule = runModule;

    function routeToUrl(args)
    {
        return {
            url: thop.baseUrl('login'),
            allowed: true
        };
    }
    this.routeToUrl = routeToUrl;

    function go(args, delay)
    {
        thop.route(routeToUrl(args).url, delay);
    }
    this.go = go;

    function connect(username, password, func)
    {
        let url = util.buildUrl(thop.baseUrl('api/connect.json'));
        let params = {
            username: username,
            password: password
        };

        data.post(url, 'json', params, function() {
            updateSession();
            if (func)
                func();
        });
    }
    this.connect = connect;

    function disconnect(func)
    {
        let url = util.buildUrl(thop.baseUrl('api/disconnect.json'));
        let params = {};

        data.post(url, 'json', params, function(json) {
            updateSession();
            if (func)
                func();
        });
    }
    this.disconnect = disconnect;

    function login()
    {
        let username = query('#usr_username').value;
        let password = query('#usr_password').value;
        query('#usr_username').value = '';
        query('#usr_password').value = '';
        query('#usr_button').disabled = true;

        connect(username, password, thop.goBackOrHome);
    }
    this.login = login;

    function logout()
    {
        disconnect();
    }
    this.logout = logout;

    function updateSession()
    {
        let prev_url_key = url_key;

        url_key = util.getCookie('url_key') || 0;
        username = util.getCookie('username');

        if (url_key !== prev_url_key) {
            for (let handler of change_handlers)
                handler();
        }
    }
    this.updateSession = updateSession;

    this.addChangeHandler = function(func) { change_handlers.push(func); }

    this.isConnected = function() { return !!url_key; }
    this.getUrlKey = function() { return url_key; }
    this.getUsername = function() { return username; }
}).call(user);
