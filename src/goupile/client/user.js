// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let user = new function() {
    let self = this;

    this.runLogin = function() {
        let state = new PageState;

        let update = () => {
            let page = new Page('@login');

            let builder = new PageBuilder(state, page);
            builder.changeHandler = update;
            builder.pushOptions({
                missingMode: 'disable',
                wide: true
            });

            makeLoginForm(builder);
            builder.actions([['Connexion', builder.isValid() ? builder.submit : null]]);

            let focus = !document.querySelector('#usr_login');

            render(html`
                <div style="flex: 1;"></div>
                <form id="usr_login" @submit=${e => e.preventDefault()}>
                    <img id="usr_logo" src=${`${env.base_url}favicon.png`} alt="" />
                    ${page.render()}
                </form>
                <div style="flex: 1;"></div>
            `, document.querySelector('#gp_all'));

            if (focus) {
                let el = document.querySelector('#usr_login input');
                setTimeout(() => el.focus(), 0);
            }
        };
        update();
    };

    this.showLoginDialog = function(e) {
        goupile.popup(e, 'Connexion', makeLoginForm);
    };

    function makeLoginForm(page, close = null) {
        let username = page.text('*username', 'Nom d\'utilisateur');
        let password = page.password('*password', 'Mot de passe');

        page.submitHandler = async () => {
            let entry = new log.Entry;

            entry.progress('Connexion en cours');
            try {
                let body = new URLSearchParams({
                    username: username.value.toLowerCase(),
                    password: password.value
                });

                let response = await net.fetch(`${env.base_url}api/login.json`, {method: 'POST', body: body});

                if (response.ok) {
                    if (close)
                        close();

                    // Emergency unlocking
                    deleteLock();

                    entry.success('Connexion réussie');
                    await goupile.initApplication();
                } else {
                    let msg = await response.text();
                    entry.error(msg);
                }
            } catch (err) {
                entry.error(err);
            }
        };
    }

    this.logout = async function() {
        let entry = new log.Entry;

        entry.progress('Déconnexion en cours');
        try {
            let response = await net.fetch(`${env.base_url}api/logout.json`, {method: 'POST'});

            if (response.ok) {
                entry.success('Déconnexion réussie');
                await goupile.initApplication();
            } else {
                let msg = await response.text();
                entry.error(msg);
            }
        } catch (err) {
            entry.error(err);
        }
    };

    this.getLockURL = function() {
        let url = localStorage.getItem('lock_url');
        return url;
    };

    this.showLockDialog = function(e, url) {
        goupile.popup(e, 'Verrouiller', (page, close) => {
            page.output('Entrez le code de verrouillage');
            let pin = page.pin('*code');

            if (pin.value && pin.value.length < 4)
                pin.error('Le code doit comporter au moins 4 chiffres', true);

            page.submitHandler = () => {
                close();

                localStorage.setItem('lock_url', url);
                localStorage.setItem('lock_pin', pin.value);

                log.success('Application verrouillée !');
                goupile.go();
            };
        });
    };

    this.showUnlockDialog = function(e) {
        goupile.popup(e, null, (page, close) => {
            page.output('Entrez le code de déverrouillage');
            let pin = page.pin('code');

            if (pin.value && pin.value.length >= 4) {
                let code = localStorage.getItem('lock_pin');

                if (pin.value === code) {
                    setTimeout(close, 0);

                    deleteLock();

                    log.success('Application déverrouillée !');
                    goupile.go();
                } else if (pin.value.length >= code.length) {
                    pin.error('Code erroné');
                }
            }
        });
    };

    function deleteLock() {
        localStorage.removeItem('lock_url');
        localStorage.removeItem('lock_pin');
    }
};
