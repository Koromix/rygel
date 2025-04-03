// Copyright (C) 2024  Niels Martignène <niels.martignene@protonmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

import { render, html, svg } from '../../../vendor/lit-html/lit-html.bundle.js';
import { contextualizeURL, computeStatus, go } from './instance.js';

function MetaModel() {
    this.summary = null;
    this.constraints = {};
    this.counters = {};
}

function MetaInterface(thread, page, meta) {
    let url = makeCompleteURL(page.url);

    Object.defineProperties(this, {
        summary: { get: () => meta.summary, set: summary => { meta.summary = summary; }, enumerable: true }
    });

    this.constrain = function(key, types) {
        if (!key)
            throw new Error('Constraint keys cannot be empty');
        if (key.startsWith('__'))
            throw new Error('Keys must not start with \'__\'');

        if (!Array.isArray(types))
            types = [types];

        let constraint = {
            exists: false,
            unique: false
        };

        for (let type of types) {
            switch (type) {
                case 'exists': { constraint.exists = true; } break;
                case 'unique': { constraint.unique = true; } break;

                default: throw new Error(`Invalid constraint type '${type}'`);
            }
        }
        if (!Object.values(constraint).some(value => value))
            throw new Error('Ignoring empty constraint');

        meta.constraints[key] = constraint;
    };

    this.count = function(key, secret = false) {
        if (!key)
            throw new Error('Counter keys cannot be empty');
        if (key.startsWith('__'))
            throw new Error('Keys must not start with \'__\'');

        secret = !!secret;

        meta.counters[key] = {
            max: null,
            randomize: false,
            secret: secret
        };

        if (secret)
            return null;

        let value = thread.counters[key];
        return value;
    };

    this.randomize = function(key, max = 2, secret = true) {
        if (!key)
            throw new Error('')
        if (max < 2 || max > 32)
            throw new Error('Number of randomization groups must be between 2 and 32');

        secret = !!secret;

        meta.counters[key] = {
            max: max,
            randomize: true,
            secret: secret
        };

        if (secret)
            return null;

        let value = thread.counters[key];
        return value;
    };

    function makeCompleteURL(url) {
        if (!(url instanceof URL))
            url = new URL(url, window.location.href);

        url = url.protocol + '//' + url.host + url.pathname;
        return url;
    }

    this.contextualize = function(url, thread) {
        if (typeof url != 'string')
            url = url?.url;

        return contextualizeURL(url, thread);
    };

    this.status = function(item, thread) {
        let status = computeStatus(item, thread);
        return status;
    };

    this.go = function(e, url = null) {
        return go(e, url);
    };
}

export {
    MetaModel,
    MetaInterface
}
