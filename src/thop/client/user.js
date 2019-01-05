// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let user = {};
(function() {
    let change_handlers = [];

    let url_key = 0;
    let username = null;

    function runLogin(route, path, parameters, hash, errors)
    {
        query('#usr button').disabled = false;

        let focus = query('#usr').hasClass('hide');
        query('#usr').removeClass('hide');
        if (focus)
            query('#usr_username').focus();
    }

    function runSession()
    {
        if (ShowUser) {
            updateSession();

            refreshSessionBox();
            refreshSessionMenu();
        }

        queryAll('#side_session_box, #side_session_menu').toggleClass('hide', !ShowUser);
    }
    this.runSession = runSession;

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
        let url = buildUrl(thop.baseUrl('api/connect.json'));
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
        let url = buildUrl(thop.baseUrl('api/disconnect.json'));
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
        query('#usr button').disabled = true;

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

        url_key = getCookie('url_key') || 0;
        username = getCookie('username');

        if (url_key !== prev_url_key) {
            for (let handler of change_handlers)
                handler();
        }
    }

    function refreshSessionBox()
    {
        if (url_key) {
            query('#side_session_box').replaceContent(
                username + ' (',
                html('a', {href: routeToUrl().url}, 'changer'),
                ', ',
                html('a', {href: '#', click: function(e) { logout(); e.preventDefault(); }},
                     'déconnexion'),
                ')'
            );
        } else {
            query('#side_session_box').replaceContent(
                html('a', {href: routeToUrl().url}, 'Se connecter')
            );
        }
    }

    function refreshSessionMenu()
    {
        if (url_key) {
            query('#side_session_menu').replaceContent(
                html('a', {href: '#', click: function(e) { logout(); e.preventDefault(); }},
                     'Se déconnecter (' + username + ')')
            );
        } else {
            query('#side_session_menu').replaceContent(
                html('a', {href: routeToUrl().url}, 'Se connecter')
            );
        }
    }

    this.addChangeHandler = function(func) { change_handlers.push(func); }

    this.isConnected = function() { return !!url_key; }
    this.getUrlKey = function() { return url_key; }
    this.getUsername = function() { return username; }

    thop.registerUrl('login', this, runLogin);
}).call(user);
