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
                    variable: (try_root ? root.notes?.variables[key] : null) ?? meta.notes?.variables[key] ?? {},
                    status: meta.notes?.status?.[key] ?? {}
                }
            };

            data[key] = wrap;
        }
    }
}

export { VmApi }
