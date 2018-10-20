// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let user = {};
(function() {
    'use strict';

    let change_handlers = [];

    let url_key = 0;
    let username = null;

    function runLogin(route, path, parameters, hash, errors)
    {
        query('#user button').disabled = false;
        query('#user').removeClass('hide');
    }

    function runSession()
    {
        if (ShowUser) {
            updateSession();

            if (!data.isBusy()) {
                refreshSessionBox();
                refreshSessionMenu();
            }
        }

        queryAll('#side_session_box, #side_session_menu').toggleClass('hide', !ShowUser);
    }
    this.runSession = runSession;

    function routeToUrl(args)
    {
        return thop.baseUrl('login');
    }
    this.routeToUrl = routeToUrl;

    function go(args, delay)
    {
        thop.route(routeToUrl(args), delay);
    }
    this.go = go;

    function connect(username, password, func)
    {
        let url = buildUrl(thop.baseUrl('api/connect.json'));
        data.post(url, {username: username, password: password}, function() {
            updateSession();
            if (func)
                func();
        });
    }
    this.connect = connect;

    function disconnect(func)
    {
        let url = buildUrl(thop.baseUrl('api/disconnect.json'));
        data.post(url, {}, function(json) {
            updateSession();
            if (func)
                func();
        });
    }
    this.disconnect = disconnect;

    function login()
    {
        let username = query('#user_username').value;
        let password = query('#user_password').value;
        query('#user_username').value = '';
        query('#user_password').value = '';
        query('#user button').disabled = true;

        connect(username, password, thop.goBackOrHome);
    }
    this.login = login;

    function logout()
    {
        disconnect(thop.goHome);
    }
    this.logout = logout;

    function updateSession()
    {
        let prev_url_key = url_key;

        url_key = getCookie('url_key');
        username = getCookie('username');

        if (url_key !== prev_url_key) {
            for (let handler of change_handlers)
                handler();
        }
    }

    function refreshSessionBox()
    {
        let div = null;
        if (url_key) {
            div = html('div',
                username + ' (',
                html('a', {href: routeToUrl()}, 'changer'),
                ', ',
                html('a', {href: '#', click: function(e) { logout(); e.preventDefault(); }},
                     'déconnexion'),
                ')'
            );
        } else {
            div = html('div',
                html('a', {href: routeToUrl('login')}, 'Se connecter')
            );
        }

        let old_div = query('#side_session_box');
        div.copyAttributesFrom(old_div);
        old_div.replaceWith(div);
    }

    function refreshSessionMenu()
    {
        let menu_item;
        if (url_key) {
            menu_item = html('li',
                html('a', {href: '#', click: function(e) { logout(); e.preventDefault(); }},
                     'Se déconnecter (' + username + ')')
            );
        } else {
            menu_item = html('li',
                html('a', {href: user.routeToUrl()}, 'Se connecter')
            );
        }

        let old_menu_item = query('#side_session_menu');
        menu_item.copyAttributesFrom(old_menu_item);
        old_menu_item.replaceWith(menu_item);
    }

    this.addChangeHandler = function(func) { change_handlers.push(func); }

    this.isConnected = function() { return !!url_key; }
    this.getUrlKey = function() { return url_key; }
    this.getUsername = function() { return username; }

    thop.registerUrl('login', this, runLogin);
}).call(user);
