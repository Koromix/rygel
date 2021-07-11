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

let ENV = {ENV_JSON};

self.addEventListener('install', e => {
    e.waitUntil(async function() {
        let [assets, list, cache] = await Promise.all([
            net.fetchJson(`${ENV.urls.base}api/files/static`),
            net.fetchJson(util.pasteURL(`${ENV.urls.base}api/files/list`, { version: ENV.version })),
            caches.open(ENV.buster)
        ]);

        await cache.addAll(assets.map(url => `${ENV.urls.base}${url}`));
        await cache.addAll(list.files.map(file => `${ENV.urls.files}${file.filename}`));

        await self.skipWaiting();
    }());
});

self.addEventListener('activate', e => {
    e.waitUntil(async function() {
        let keys = await caches.keys();

        for (let key of keys) {
            if (key !== ENV.buster)
                await caches.delete(key);
        }
    }());
});

self.addEventListener('fetch', e => {
    e.respondWith(async function() {
        let url = new URL(e.request.url);

        // Try the cache
        if (e.request.method === 'GET' && url.pathname.startsWith(ENV.urls.base)) {
            let path = url.pathname.substr(ENV.urls.base.length - 1);

            if (path.match(/^\/(?:[a-z0-9_]+\/)?$/) || path.match(/^\/(?:[a-z0-9_]+\/)?main\//)) {
                return await caches.match(ENV.urls.base) || await fetch(e.request);
            } else {
                return await caches.match(e.request) || await fetch(e.request);
            }
        }

        return await fetch(e.request);
    }());
});
