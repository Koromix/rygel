var user = {};
(function() {
    var user = null;

    function run(route, url, parameters, hash)
    {
        let errors = new Set(downloadJson.errors);

        if (!downloadJson.busy) {
            refreshErrors(Array.from(errors));
            downloadJson.errors = [];

            _('#user').classList.remove('hide');
        }
    }
    this.run = run;

    function runUserBox()
    {
        refreshUser();

        if (!downloadJson.busy)
            updateUserBox();
    }
    this.runUserBox = runUserBox;

    function routeToUrl(args)
    {
        return buildModuleUrl('user');
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
            func();
        });
    }
    this.connect = connect;

    function disconnect()
    {
        let url = buildUrl(BaseUrl + 'api/disconnect.json');
        downloadJson(url, 'post', function(json) {});
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
        let url = buildUrl(BaseUrl + 'api/user.json');
        downloadJson(url, 'get', function(json) {
            user = json.username ? json : null;
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
                createElement('a', {href: routeToUrl('user')}, 'Se connecter')
            );
        }

        let old_div = document.querySelector('#side_user_box');
        cloneAttributes(old_div, div);
        old_div.parentNode.replaceChild(div, old_div);
    }

    function getCurrentUser() { return user; }
    this.getCurrentUser = getCurrentUser;
}).call(user);
