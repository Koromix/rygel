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
        let [assets, files, cache] = await Promise.all([
            net.fetchJson(`${ENV.base_url}api/files/static`),
            net.fetchJson(`${ENV.base_url}api/files/list`),
            caches.open(ENV.cache_key)
        ]);

        await cache.addAll(assets.map(url => `${ENV.base_url}${url}`));
        await cache.addAll(files.map(file => `${ENV.base_url}files/${file.filename}`));

        await self.skipWaiting();
    }());
});

self.addEventListener('activate', e => {
    e.waitUntil(async function() {
        let keys = await caches.keys();

        for (let key of keys) {
            if (key !== ENV.cache_key)
                await caches.delete(key);
        }
    }());
});

self.addEventListener('fetch', e => {
    e.respondWith(async function() {
        let url = new URL(e.request.url);

        if (e.request.method === 'GET' && url.pathname.startsWith(ENV.base_url)) {
            let path = url.pathname.substr(ENV.base_url.length - 1);

            if (path.startsWith('/main/')) {
                return await caches.match(ENV.base_url) || await net.fetch(ENV.base_url);
            } else {
                return await caches.match(e.request) || await net.fetch(e.request);
            }
        }

        // Update cached files after deploy
        if (e.request.method === 'PUT' || e.request.method === 'DELETE') {
            let prefix = `${ENV.base_url}files/`;

            if (url.pathname.startsWith(prefix)) {
                let response = await net.fetch(e.request);

                if (response.ok) {
                    let cache = await caches.open(ENV.cache_key);

                    await cache.delete(url.pathname);
                    if (e.request.method === 'PUT')
                        await cache.add(url.pathname);
                }

                return response;
            }
        }

        return await net.fetch(e.request);
    }());
});
