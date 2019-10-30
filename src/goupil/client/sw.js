// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let env = {
    app_key: '{APP_KEY}',
    base_url: '{BASE_URL}',

    cache_key: '{CACHE_KEY}'
};

let cache_urls = [
    env.base_url,
    `${env.base_url}static/goupil.pk.css`,
    `${env.base_url}static/goupil.pk.js`,
    `${env.base_url}static/ace.js`,
    `${env.base_url}static/theme-monokai.js`,
    `${env.base_url}static/mode-javascript.js`,
    `${env.base_url}static/xlsx.core.min.js`,
    `${env.base_url}static/NotoSans-Regular.woff2`,

    `${env.base_url}manifest.json`,
    `${env.base_url}favicon.png`
];

self.addEventListener('install', e => {
    e.waitUntil(async function() {
        if (env.cache_key) {
            let cache = await caches.open(env.cache_key);
            await cache.addAll(cache_urls);
        }

        await self.skipWaiting();
    }());
});

self.addEventListener('activate', e => {
    e.waitUntil(async function() {
        let keys = await caches.keys();

        for (let key of keys) {
            if (key !== env.cache_key)
                await caches.delete(key);
        }
    }());
});

self.addEventListener('fetch', e => {
    e.respondWith(async function() {
        let url = new URL(e.request.url);

        if (e.request.method === 'GET' && url.pathname.startsWith(env.base_url)) {
            let short_path = url.pathname.substr(env.base_url.length);

            if (short_path.startsWith('app/')) {
                let db_name = `goupil_${env.app_key}`;
                let db = await idb.open(db_name);

                let file_path = url.pathname.substr(env.base_url.length);
                let data = await db.load('files', file_path);

                if (data) {
                    return new Response(data);
                } else {
                    return new Response('Error 404: Asset does not exist', {
                        status: 404,
                        statusText: 'Not found'
                    });
                }
            } else if (short_path.startsWith('dev/')) {
                return await caches.match(env.base_url) || await fetch(env.base_url);
            } else if (url.pathname.startsWith(env.base_url)) {
                return await caches.match(e.request) || await fetch(e.request);
           }
        }

        // Nothing matched, do the usual
        return await fetch(e.request);
    }());
});
