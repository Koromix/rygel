// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let ENV = {
    base_url: '{BASE_URL}',
    version: '{VERSION}'
};

self.addEventListener('install', e => {
    e.waitUntil(async function() {
        let files = await net.fetch(`${ENV.base_url}api/files/static`).then(response => response.json());
        let cache = await caches.open(ENV.version);
        await cache.addAll(files);

        await self.skipWaiting();
    }());
});

self.addEventListener('activate', e => {
    e.waitUntil(async function() {
        let keys = await caches.keys();

        for (let key of keys) {
            if (key !== ENV.version)
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

        return await net.fetch(e.request);
    }());
});
