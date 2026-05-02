// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import { Util, Log, Net } from 'lib/web/base/base.js';

self.addEventListener('install', e => {
    self.skipWaiting();
});

self.addEventListener('fetch', e => {
    e.respondWith(async function() {
        let url = new URL(e.request.url);

        if (e.request.method == 'GET' && url.pathname.startsWith('/api/drop/download/')) {
            let kid = url.pathname.substr(19);

            let info;
            {
                let url = Util.pasteURL('/api/drop/info', { kid: kid });
                let response = await fetch(url);

                if (!response.ok)
                    return response;

                info = await response.json();
            }

            let fragment = 0;
            let downloaded = 0;

            let options = {
                status: 200,
                headers: {
                    'Content-Type': 'application/octet-stream',
                    'Content-Disposition': `attachment; filename="${info.name}"`,
                    'Content-Length': info.size
                }
            };

            let stream = new ReadableStream({
                pull: async (controller) => {
                    let url = Util.pasteURL('/api/drop/fragment', { kid: kid, fragment: fragment });

                    try {
                        let response = await fetch(url);

                        if (!response.ok) {
                            controller.error(new Error('Failed to download fragment with status ' + response.status));
                            controller.close();
                            return;
                        }

                        let buf = await response.bytes();

                        downloaded += buf.length;
                        fragment++;

                        controller.enqueue(buf);

                        if (downloaded == info.size)
                            controller.close();
                    } catch (err) {
                        controller.error(err);
                        controller.close();
                    }
                }
            });

            return new Response(stream, options);
        }

        return await fetch(e.request);
    }());
});
