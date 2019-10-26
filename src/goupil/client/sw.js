// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let base_url = (new URL(self.registration.scope)).pathname;

let cache_key = 'v1';
let cache_urls = [
    base_url,
    `${base_url}static/goupil.pk.css`,
    `${base_url}static/goupil.pk.js`,
    `${base_url}static/ace.js`,
    `${base_url}static/theme-monokai.js`,
    `${base_url}static/mode-javascript.js`,
    `${base_url}static/xlsx.core.min.js`,
    `${base_url}static/NotoSans-Regular.woff2`,
    `${base_url}manifest.json`,
    `${base_url}static/fox192.png`,
    `${base_url}static/fox512.png`
];

self.addEventListener('install', e => {
    e.waitUntil(async function() {
        let cache = await caches.open(cache_key);

        await cache.addAll(cache_urls);
        await self.skipWaiting();
    }());
});

self.addEventListener('activate', e => {
    e.waitUntil(async function() {
        let keys = await caches.keys();

        for (let key of keys) {
            if (key !== cache_key)
                await caches.delete(key);
        }
    }());
});

self.addEventListener('fetch', e => {
    if (e.request.method === 'GET') {
        e.respondWith(async function() {
            let response;
            {
                let url = e.request.url;
                let name = url.substr(url.lastIndexOf('/') + 1);

                if (name.lastIndexOf('.') > 0) {
                    response = await caches.match(e.request);
                } else {
                    // Serve extension-less paths with goupil.html, just like the server does
                    response = await caches.match('/');
                }
            }

            // Should we fail directly when the asset is supposed to in cache?
            if (!response)
                response = await fetch(e.request);

            return response;
        }());
    }
});
