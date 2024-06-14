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

import { Util, Log, Net } from '../../web/libjs/common.js';

function DataRemote() {
    let self = this;

    this.load = async function(tid, anchor) {
        let url = Util.pasteURL(`${ENV.urls.instance}api/records/get`, {
            tid: tid,
            anchor: anchor
        });
        let thread = await Net.get(url);

        return thread;
    };

    this.save = async function(tid, entry, fs, constraints) {
        await Net.post(ENV.urls.instance + 'api/records/save', {
            tid: tid,
            fragment: {
                fs: fs,
                eid: entry.eid,
                store: entry.store,
                anchor: entry.anchor,
                data: entry.data,
                meta: entry.meta,
                tags: entry.tags,
                constraints: constraints
            }
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

export { DataRemote };
