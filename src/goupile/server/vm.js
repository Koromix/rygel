// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import { html, svg } from 'vendor/lit-html/lit-html.bundle.js';
import { Util, Log } from 'lib/web/base/base.js';
import * as Data from 'lib/web/ui/data.js';

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

function VmApi(native) {
    let self = this;

    this.mergeData = function(data, meta) {
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

    this.expandData = function(data) {
        data = JSON.parse(data);

        let [raw, values] = Data.wrap(data);

        return JSON.stringify(raw);
    };
}

export { VmApi }
