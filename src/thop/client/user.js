// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let user = {};
(function() {
    'use strict';

    let change_handlers = [];

    let session_init = false;
    let session = null;

    function runLogin(route, url, parameters, hash)
    {
        let errors = new Set(data.getErrors());

        thop.refreshErrors(Array.from(errors));
        if (!data.isBusy())
            data.clearErrors();
        query('#user button').disabled = false;
        query('#user').removeClass('hide');
    }

    function runSession()
    {
        if (ShowUser) {
            refreshSession();

            if (!data.isBusy()) {
                updateSessionBox();
                updateSessionMenu();
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
            refreshSession(true);

            if (func)
                func();
        });
    }
    this.connect = connect;

    function disconnect(func)
    {
        let url = buildUrl(thop.baseUrl('api/disconnect.json'));
        data.post(url, {}, function(json) {
            session = null;
            notifyChange();

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

        connect(username, password, function() {
            setTimeout(thop.goBackOrHome, 0);
        });
    }
    this.login = login;

    function refreshSession(force)
    {
        if (!!session !== (document.cookie.indexOf('connected=1') >= 0)) {
            if (session) {
                session = null;
                notifyChange();
            }

            force = true;
        }

        if (!session_init || force) {
            let url = thop.baseUrl('api/session.json');
            data.get(url, function(json) {
                let prev_username = session ? session.username : null;
                let new_username = json ? json.username : null;
                session = json;
                if (new_username !== prev_username)
                    notifyChange();
            }, function(error) {
                session = null;
                notifyChange();
            });

            session_init = true;
        }
    }

    function updateSessionBox()
    {
        let div = null;
        if (session) {
            div = html('div',
                session.username + ' (',
                html('a', {href: routeToUrl()}, 'changer'),
                ', ',
                html('a', {href: '#', click: function(e) { disconnect(); e.preventDefault(); }},
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

    function updateSessionMenu()
    {
        let menu_item;
        if (session) {
            menu_item = html('li',
                html('a', {href: '#', click: function(e) { user.disconnect(); e.preventDefault(); }},
                     'Se déconnecter (' + session.username + ')')
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

    function notifyChange()
    {
        for (let handler of change_handlers)
            handler();
    }

    this.addChangeHandler = function(func) { change_handlers.push(func); }
    this.getSession = function() { return session; }
    this.getUrlKey = function() { return session ? session.url_key : 0; }

    thop.registerUrl('login', this, runLogin);
}).call(user);
