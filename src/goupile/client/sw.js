// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let ENV = {ENV_JSON};

self.addEventListener('install', e => {
    e.waitUntil(async function() {
        let [assets, files, cache] = await Promise.all([
            fetch(`${ENV.base_url}api/files/static`).then(response => response.json()),
            fetch(`${ENV.base_url}api/files/list`).then(response => response.json()),
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
                return await caches.match(ENV.base_url) || await fetch(ENV.base_url);
            } else {
                return await caches.match(e.request) || await fetch(e.request);
            }
        }

        // Update cached files after deploy
        if (e.request.method === 'PUT' || e.request.method === 'DELETE') {
            let prefix = `${ENV.base_url}files/`;

            if (url.pathname.startsWith(prefix)) {
                let response = await fetch(e.request);

                if (response.ok) {
                    let cache = await caches.open(ENV.cache_key);

                    if (e.request.method === 'PUT') {
                        await cache.add(url.pathname);
                    } else {
                        await cache.delete(url.pathname);
                    }
                }

                return response;
            }
        }

        return await fetch(e.request);
    }());
});
