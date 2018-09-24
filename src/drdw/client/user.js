// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

var user = {};
(function() {
    var change_handlers = [];

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

    function runSession()
    {
        if (ShowUser) {
            refreshSession();

            if (!downloadJson.busy) {
                updateSessionBox();
                updateSessionMenu();
            }
        }

        toggleClass(__('#side_session_box, #side_session_menu'), 'hide', !ShowUser);
    }
    this.runSession = runSession;

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
        downloadJson('POST', url, randomizeUrlKey);
    }
    this.connect = connect;

    function disconnect()
    {
        let url = buildUrl(BaseUrl + 'api/disconnect.json');
        downloadJson('POST', url, function(json) {
            session = null;
            randomizeUrlKey();
            notifyChange();
        });
    }
    this.disconnect = disconnect;

    function login()
    {
        let username = _('#user_username').value;
        let password = _('#user_password').value;
        connect(username, password);
    }
    this.login = login;

    function refreshSession()
    {
        let url = buildUrl(BaseUrl + 'api/session.json', {key: url_key});
        downloadJson('GET', url, function(json) {
            let prev_username = session ? session.username : null;
            let new_username = json ? json.username : null;
            session = json;
            if (new_username !== prev_username)
                notifyChange();
        }, function(error) {
            session = null;
            notifyChange();
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

    function updateSessionMenu()
    {
        let menu_item;
        if (session) {
            menu_item = createElement('li', {},
                createElement('a', {href: '#',
                                    click: function(e) { user.disconnect(); e.preventDefault(); }},
                              'Se déconnecter (' + session.username + ')')
            );
        } else {
            menu_item = createElement('li', {},
                createElement('a', {href: user.routeToUrl()}, 'Se connecter')
            );
        }

        let old_menu_item = _('#side_session_menu');
        cloneAttributes(old_menu_item, menu_item);
        old_menu_item.parentNode.replaceChild(menu_item, old_menu_item);
    }

    function randomizeUrlKey()
    {
        url_key = generateRandomInt(1, 281474976710656);
    }

    function notifyChange()
    {
        for (let i = 0; i < change_handlers.length; i++)
            change_handlers[i]();
    }

    this.addChangeHandler = function(func) { change_handlers.push(func); }
    this.getSession = function() { return session; }
    this.getUrlKey = function() { return url_key; }

    randomizeUrlKey();
}).call(user);

registerUrl('login', user, user.runLogin);
