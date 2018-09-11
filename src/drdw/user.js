// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

var user = {};
(function() {
    var user = null;
    var url_key = 0;

    function runLogin(route, url, parameters, hash)
    {
        let errors = new Set(downloadJson.errors);

        if (!downloadJson.busy) {
            refreshErrors(Array.from(errors));
            downloadJson.errors = [];

            _('#user').classList.remove('hide');
        }
    }
    this.runLogin = runLogin;

    function runUserBox()
    {
        refreshUser();

        if (!downloadJson.busy)
            updateUserBox();
    }
    this.runUserBox = runUserBox;

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
        downloadJson(url, 'post', function(json) {
            user = json;
            url_key = generateRandomInt(1, 281474976710656);

            func();
        });
    }
    this.connect = connect;

    function disconnect()
    {
        let url = buildUrl(BaseUrl + 'api/disconnect.json');
        downloadJson(url, 'post', function(json) {
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

    function refreshUser()
    {
        let url = buildUrl(BaseUrl + 'api/user.json', {key: url_key});
        downloadJson(url, 'get', function(json) {
            if (json.username) {
                if (!url_key)
                    url_key = generateRandomInt(1, 281474976710656);
                user = json;
            } else {
                user = null;
                url_key = 0;
            }
        });
    }

    function refreshErrors(errors)
    {
        var log = _('#log');

        log.innerHTML = errors.join('<br/>');
        log.classList.toggle('hide', !errors.length);
    }

    function updateUserBox()
    {
        let div = null;
        if (user) {
            div = createElement('div', {},
                'Connecté : ' + user.username + ' ',
                createElement('a', {href: '#',
                                    click: function(e) { disconnect(); e.preventDefault(); }}, '(déconnexion)')
            );
        } else {
            div = createElement('div', {},
                createElement('a', {href: routeToUrl('login')}, 'Se connecter')
            );
        }

        let old_div = document.querySelector('#side_user_box');
        cloneAttributes(old_div, div);
        old_div.parentNode.replaceChild(div, old_div);
    }

    function getCurrentUser() { return user; }
    function getUrlKey() { return url_key; }
    this.getCurrentUser = getCurrentUser;
    this.getUrlKey = getUrlKey;

    if (document.cookie.indexOf('session_key='))
        url_key = generateRandomInt(1, 281474976710656);
}).call(user);

registerUrl('login', user, user.runLogin);
