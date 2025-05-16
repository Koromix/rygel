// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
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

import { Util, Log, Net, LocalDate, LocalTime } from '../../web/core/base.js';
import * as Data from '../../web/core/data.js';

function DataRemote() {
    let self = this;

    this.load = async function(tid, anchor) {
        let url = Util.pasteURL(`${ENV.urls.instance}api/records/get`, {
            tid: tid,
            anchor: anchor
        });
        let thread = await Net.get(url);

        for (let store in thread.entries) {
            let entry = thread.entries[store];
            entry.data = restore(entry.data);
        }

        return thread;
    };

    this.save = async function(tid, entry, frag, fs, signup, claim) {
        await Net.post(ENV.urls.instance + 'api/records/save', {
            tid: tid,

            eid: entry.eid,
            store: entry.store,
            anchor: entry.anchor,
            fs: fs,

            summary: frag.summary,
            data: preserve(frag.data),
            tags: frag.tags,
            constraints: frag.constraints,
            counters: frag.counters,
            signup: signup,

            claim: claim
        });
    };

    this.delete = async function(tid) {
        await Net.post(ENV.urls.instance + 'api/records/delete', { tid: tid });
    };

    this.lock = async function(tid) {
        await Net.post(ENV.urls.instance + 'api/records/lock', { tid: tid });
    };

    this.unlock = async function(tid) {
        await Net.post(ENV.urls.instance + 'api/records/unlock', { tid: tid });
    };
}

function preserve(obj) {
    if (Util.isPodObject(obj)) {
        obj = Object.assign({}, obj);

        for (let key in obj) {
            let wrap = obj[key];

            wrap = {
                $n: Object.assign({}, wrap.$n),
                $v: preserve(wrap.$v)
            };

            if (wrap.$v instanceof LocalDate) {
                wrap.$n.special = 'LocalDate';
            } else if (wrap.$v instanceof LocalTime) {
                wrap.$n.special = 'LocalTime';
            } else {
                delete wrap.$n.special;
            }

            obj[key] = wrap;
        }

        return obj;
    } else if (obj == null) {
        return null;
    } else {
        return obj;
    }
}

function restore(obj) {
    if (Util.isPodObject(obj)) {
        obj = Object.assign({}, obj);

        for (let key in obj) {
            let wrap = obj[key];

            if (!Util.isPodObject(wrap) || !wrap.hasOwnProperty('$v'))
                continue;

            wrap = {
                $n: Object.assign({}, wrap.$n ?? {}),
                $v: preserve(wrap.$v)
            };

            if (wrap.$n.special == 'LocalDate') {
                wrap.$v = LocalDate.parse(wrap.$v);
            } else if (wrap.$n?.special == 'LocalTime') {
                wrap.$v = LocalTime.parse(wrap.$v);
            }
            delete wrap.$n.special;

            obj[key] = wrap;
        }

        return obj;
    } else if (obj == null) {
        return null;
    } else {
        return obj;
    }
}

export { DataRemote };
