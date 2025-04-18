// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see https://www.gnu.org/licenses/.

import { html, svg } from '../../../vendor/lit-html/lit-html.bundle.js';
import { Util, Log } from '../../web/core/base.js';
import { ApplicationInfo, ApplicationBuilder } from '../client/instance_app.js';
import * as Data from '../../web/core/data.js';
import { FormState, FormModel, FormBuilder } from '../client/form.js';

// Create fake globals
Object.defineProperties(globalThis, {
    console: {
        value: {
            error: () => {},
            log: () => {}
        },
        configurable: false,
        writable: false
    },

    html: { value: html, configurable: false, writable: false },
    svg: { value: svg, configurable: false, writable: false }
});

function MetaModel() {
    this.summary = null;
    this.constraints = {};
    this.counters = {};
}

function MetaInterface(thread, page, meta) {
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

    this.randomize = function(key, max = 2, secret = false) {
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

    this.contextualize = function(url, thread) { /* Dummy */ };
    this.status = function(item, thread) { /* Dummy */ };
    this.go = function(e, url = null) { /* Dummy */ };
}

function VmApi(native) {
    let self = this;

    this.mergeDataMeta = function(data, meta) {
        data = JSON.parse(data);
        meta = JSON.parse(meta);

        wrapDataMeta(data, meta, meta);

        return JSON.stringify(data);
    };

    function wrapDataMeta(data, meta, root) {
        let keys = Object.keys(data);
        let try_root = keys.length && keys.every(key => key.length && key.match(/^[0-9]+$/));

        for (let key of keys) {
            let value = data[key];

            if (Util.isPodObject(value))
                wrapDataMeta(value, meta.children[key] ?? {}, meta);

            let wrap = {
                $v: value,
                $n: {
                    errors: meta.notes?.errors?.[key] ?? [],
                    variable: (try_root ? root.notes?.variables?.[key] : null) ?? meta.notes?.variables?.[key] ?? {},
                    status: meta.notes?.status?.[key] ?? {}
                }
            };

            data[key] = wrap;
        }
    }

    this.buildApp = function(profile) {
        profile = JSON.parse(profile);

        let script = native.getFile('main.js');
        let main = buildScript(script, ['app', 'profile']);

        let app = new ApplicationInfo(profile);
        let builder = new ApplicationBuilder(app);

        try {
            main({
                app: builder,
                profile: profile
            });

            if (!app.pages.length)
                throw new Error('Main script does not define any page');
        } catch (err) {
            throw err;
        }

        return app;
    };

    this.runData = function(app, profile, store, data) {
        profile = JSON.parse(profile);
        data = JSON.parse(data);

        let changes = [];

        for (let page of app.pages) {
            if (page.store?.key != store)
                continue;
            if (typeof page.filename != 'string')
                continue;

            let script = native.getFile(page.filename);
            let func = buildScript(script, ['app', 'form', 'meta', 'page', 'thread', 'values']);

            let [raw, obj] = Data.wrap(data);

            let state = new FormState(raw, obj);
            let model = new FormModel;
            let builder = new FormBuilder(state, model, { annotate: app.annotate });
            let meta = new MetaModel;

            // XXX: Get whole thread
            let thread = {};

            func({
                app: app,
                form: builder,
                meta: new MetaInterface(thread, page, meta),
                page: page,
                thread: thread,
                values: state.values
            });

            let change = {
                store: store,
                data: state.raw
            };

            changes.push(change);
        }

        return JSON.stringify(changes);
    };

    function buildScript(code, variables) {
        try {
            let func = new Function(variables, code);

            return api => {
                try {
                    let values = variables.map(key => api[key]);
                    func(...values);
                } catch (err) {
                    throwScriptError(err);
                }
            };
        } catch (err) {
            throwScriptError(err);
        }
    }

    function throwScriptError(err) {
        let line = Util.locateEvalError(err)?.line;
        let msg = `Erreur de script\n${line != null ? `Ligne ${line} : ` : ''}${err.message}`;

        throw new Error(msg);
    }
}

export { VmApi }
