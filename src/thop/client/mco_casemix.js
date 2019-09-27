// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let mco_casemix = (function() {
    let self = this;

    let route = {};
    this.route = route;

    this.run = async function() {
        if (!user.isConnected())
            throw new Error('Vous devez être connecté(e)');

        switch (route.mode) {
            case 'ghm': {} break;
            case 'units': {} break;
            case 'valorisation': {} break;
            case 'rss': {} break;

            default: {
                throw new Error(`Mode inconnu '${route.mode}'`);
            } break;
        }
    };

    this.parseURL = function(path, query) {
        let parts = path.split('/');

        // Common part
        let args = {
            mode: parts[0] || 'ghm'
        };

        return args;
    };

    this.makeURL = function(args = {}) {
        args = util.assignDeep({}, route, args);
        return `${env.base_url}mco_casemix/${args.mode}/`;
    };

    return this;
}).call({});
