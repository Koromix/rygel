// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import { render, html, svg } from '../../../vendor/lit-html/lit-html.bundle.js';
import { contextualizeURL, computeStatus, go } from './instance.js';

function MetaModel() {
    this.hid = null;
    this.summary = null;

    this.constraints = {};
    this.counters = {};
    this.signup = null;
}

function MetaInterface(app, page, thread, meta) {
    Object.defineProperties(this, {
        hid: { get: () => meta.hid, set: hid => { meta.hid = hid; }, enumerable: true },
        summary: { get: () => meta.summary, set: summary => { meta.summary = summary; }, enumerable: true }
    });

    this.constrain = function(key, types) {
        if (!key)
            throw new Error(T.message(`Constraint keys must not be empty`));
        if (key.startsWith('__'))
            throw new Error(T.message(`Keys must not start with '__'`));

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

                default: throw new Error(T.message(`Unknown constraint type '{1}'`, type));
            }
        }
        if (!Object.values(constraint).some(value => value))
            throw new Error(T.message(`Empty constraint`));

        meta.constraints[key] = constraint;
    };

    this.count = function(key, secret = false) {
        if (!key)
            throw new Error(T.message(`Counter keys must not be empty`));
        if (key.startsWith('__'))
            throw new Error(T.message(`Keys must not start with '__'`));

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

    this.randomize = function(key, max = 2, secret = false) {
        if (!key)
            throw new Error(T.message(`Randomization keys must not be empty`));
        if (key.startsWith('__'))
            throw new Error(T.message(`Keys must not start with '__'`));
        if (max < 2 || max > 32)
            throw new Error(T.message(`The number of randomization groups must be between 2 and 32`));

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

    this.signup = function(key, to, subject, content, text = null) {
        page = app.pages.find(page => page.key == key);

        if (page == null)
            throw new Error(T.message(`Unknown page '{1}'`, key));

        let div = document.createElement('div');
        render(content, div);

        content = div.innerHTML;

        if (text == null)
            text = content.innerText;

        meta.signup = {
            url: makeCompleteURL(page.url),
            to: to,
            subject: subject,
            html: content,
            text: text
        };
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
