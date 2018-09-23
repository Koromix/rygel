// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

var user = {};
(function() {
    var session = null;
    var url_key = 0;

    function runLogin(route, url, parameters, hash)
    {
        let errors = new Set(downloadJson.errors);

        if (session)
            errors.add('Vous êtes déjà connecté(e)');

        refreshErrors(Array.from(errors));
        if (!downloadJson.busy)
            downloadJson.errors = [];
        _('#user').classList.toggle('hide', !!session);
    }
    this.runLogin = runLogin;

    function runSessionBox()
    {
        refreshSession();

        if (!downloadJson.busy)
            updateSessionBox();
    }
    this.runSessionBox = runSessionBox;

    function routeToUrl(args)
    {
        return buildModuleUrl('login');
    }
    this.routeToUrl = routeToUrl;

    function route(args, delay)
    {
        go(routeToUrl(args), true, delay);
    }
    this.route = route;

    function connect(username, password, func)
    {
        let url = buildUrl(BaseUrl + 'api/connect.json', {username: username, password: password});
        downloadJson('POST', url, function(json) {
            session = json;
            url_key = generateRandomInt(1, 281474976710656);

            func();
        });
    }
    this.connect = connect;

    function disconnect()
    {
        let url = buildUrl(BaseUrl + 'api/disconnect.json');
        downloadJson('POST', url, function(json) {
            url_key = 0;
        });
    }
    this.disconnect = disconnect;

    function login()
    {
        let username = _('#user_username').value;
        let password = _('#user_password').value;

        connect(username, password, function() {
            window.history.back();
        });
    }
    this.login = login;

    function refreshSession()
    {
        let url = buildUrl(BaseUrl + 'api/session.json', {key: url_key});
        downloadJson('GET', url, function(json) {
            if (json.username) {
                if (!url_key)
                    url_key = generateRandomInt(1, 281474976710656);
                session = json;
            } else {
                session = null;
                url_key = 0;
            }
        }, function(error) {
            session = null;
            url_key = 0;
        });
    }

    function updateSessionBox()
    {
        let div = null;
        if (session) {
            div = createElement('div', {},
                'Connecté : ' + session.username + ' ',
                createElement('a', {href: '#',
                                    click: function(e) { disconnect(); e.preventDefault(); }}, '(déconnexion)')
            );
        } else {
            div = createElement('div', {},
                createElement('a', {href: routeToUrl('login')}, 'Se connecter')
            );
        }

        let old_div = document.querySelector('#side_session_box');
        cloneAttributes(old_div, div);
        old_div.parentNode.replaceChild(div, old_div);
    }

    function getSession() { return session; }
    function getUrlKey() { return url_key; }
    this.getSession = getSession;
    this.getUrlKey = getUrlKey;

    if (document.cookie.indexOf('session_key='))
        url_key = generateRandomInt(1, 281474976710656);
}).call(user);

registerUrl('login', user, user.runLogin);
