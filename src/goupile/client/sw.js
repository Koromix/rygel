// Copyright (C) 2024  Niels Martignène <niels.martignene@protonmail.com>
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

import { Util, Log, Net } from '../../web/core/common.js';

// Hack around esbuild dropping simple template literals
let raw = (strings) => strings[0];
let ENV = JSON.parse(raw`{{ ENV_JSON }}`);

self.addEventListener('install', e => {
    e.waitUntil(async function() {
        let [assets, list, cache] = await Promise.all([
            Net.get(`${ENV.urls.base}api/files/static`),
            Net.get(Util.pasteURL(`${ENV.urls.base}api/files/list`, { version: ENV.version })),
            caches.open(ENV.buster)
        ]);

        await cache.addAll(assets);
        await cache.addAll(list.files.map(file => `${ENV.urls.files}${file.filename}`));

        // We need to cache application files with two URLs:
        // the URL with the version qualifier, and the one without.
        for (let file of list.files) {
            let url1 = `${ENV.urls.files}${file.filename}`;
            let url2 = `${ENV.urls.base}files/${file.filename}`;

            let response = await cache.match(url1);
            await cache.put(url2, response);
        }

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

        if (e.request.method === 'GET' && url.pathname.startsWith(ENV.urls.base)) {
            // Try directly
            {
                let response = await caches.match(e.request);

                if (response != null)
                    return response;
            }

            // Try ancestor URLs
            {
                let parts = url.pathname.split('/');

                if (parts.find(part => part == 'api' || part == 'files' ||
                                       part == 'static' || parts.includes('.')))
                    parts.length = 0;

                for (let i = parts.length - 1; i > 1; i--) {
                    let path = parts.slice(0, i).join('/') + '/';
                    let response = await caches.match(path);

                    if (response != null)
                        return response;
                }
            }
        }

        let response = await fetch(e.request);
        return response;
    }());
});
