// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

import { Util, Log, Net } from 'lib/web/base/base.js';

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
                    if (fragment == null)
                        return;

                    let url = Util.pasteURL('/api/drop/fragment', { kid: kid, fragment: fragment });
                    let expected = Math.min(info.size - fragment * info.chunk, info.chunk);

                    let buf = null;

                    for (let i = 0; i < 4; i++) {
                        try {
                            let response = await fetch(url);

                            if (response.ok)
                                buf = await response.bytes();
                        } catch (err) {
                            console.error(err);
                        }

                        if (buf?.length == expected)
                            break;

                        // Transient S3 errors will result in truncated output, but a 200 status because the server
                        // streams the response. Retry if buffer is shorter than expected!
                        await Util.waitFor(1000 + i * 2000);
                    }

                    if (buf?.length != expected) {
                        controller.error(new Error('Failed to download file fragment'));
                        controller.close();
                        fragment = null;

                        return;
                    }

                    downloaded += buf.length;
                    fragment++;

                    controller.enqueue(buf);

                    if (downloaded == info.size) {
                        controller.close();
                        fragment = null;
                    }
                }
            });

            return new Response(stream, options);
        }

        return await fetch(e.request);
    }());
});
