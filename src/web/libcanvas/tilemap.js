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

import { util, net, LruMap } from '../libjs/util.js';

function TileMap(runner) {
    let self = this;

    // Shortcuts
    let canvas = runner.canvas;
    let ctx = runner.ctx;
    let mouse_state = runner.mouseState;
    let pressed_keys = runner.pressedKeys;

    let tiles = null;

    let marker_groups = {};

    const DEFAULT_ZOOM = 7;
    const MAX_FETCHERS = 8;

    let state = null;

    let zoom_animation = null;

    let missing_assets = 0;
    let active_fetchers = 0;
    let fetch_queue = [];
    let fetch_handles = new Map;

    let known_tiles = new LruMap(256);
    let marker_textures = new LruMap(32);

    Object.defineProperties(this, {
        width: { get: () => canvas.width, enumerable: true },
        height: { get: () => canvas.height, enumerable: true },

        coordinates: { get: () => {
            let center = { x: canvas.width / 2, y: canvas.height / 2 };
            return self.screenToCoord(center);
        }, enumerable: true },

        zoom: { get: () => state.zoom, enumerable: true }
    });

    this.init = async function(config) {
        tiles = Object.assign({
            min_zoom: 1
        }, config);

        ctx.imageSmoothingEnabled = true;
        ctx.imageSmoothingQuality = 'high';

        state = {
            zoom: DEFAULT_ZOOM,
            pos: latLongToXY(48.866667, 2.333333, DEFAULT_ZOOM),
            grab: null
        };

        known_tiles.clear();
        marker_textures.clear();
    };

    this.move = function(lat, lng, zoom = null) {
        if (zoom != null)
            state.zoom = zoom;
        state.pos = latLongToXY(lat, lng, state.zoom);

        zoom_animation = null;

        runner.busy();
    };

    this.setMarkers = function(key, markers) {
        if (!Array.isArray(markers))
            throw new Error('Not an array of markers');

        marker_groups[key] = markers;
    };

    this.clearMarkers = function(key) {
        delete marker_groups[key];
    };

    this.update = function() {
        if (ctx == null)
            return;

        // Animate zoom
        if (zoom_animation != null) {
            let t = (runner.updateCounter - zoom_animation.start) / 12;

            if (t < 1) {
                zoom_animation.value = zoom_animation.from + t * (zoom_animation.to - zoom_animation.from);
                runner.busy();
            } else {
                zoom_animation = null;
            }
        }

        // Detect what we're pointing at (if anything)
        let target = null;
        {
            let groups = Object.values(marker_groups);

            for (let i = groups.length - 1; i >= 0; i--) {
                let markers = groups[i];

                for (let j = markers.length - 1; j >= 0; j--) {
                    let marker = markers[j];

                    if (marker.click == null)
                        continue;

                    let pos = self.coordToScreen(marker.latitude, marker.longitude);

                    if (distance(pos, mouse_state) < adaptMarkerSize(marker.size, state.zoom) / 2) {
                        target = marker;
                        break;
                    }
                }
            }
        }

        // Handle actions
        if (mouse_state.left >= 1 && (target == null || state.grab != null)) {
            if (state.grab != null) {
                state.pos.x += state.grab.x - mouse_state.x;
                state.pos.y += state.grab.y - mouse_state.y;
            }

            state.grab = {
                x: mouse_state.x,
                y: mouse_state.y
            };
        } else if (!mouse_state.left) {
            state.grab = null;
        }
        if (mouse_state.left == -1 && target != null && state.grab == null)
            target.click();

        // Adjust cursor style
        if (state.grab != null) {
            runner.cursor = 'grabbing';
        } else if (target != null) {
            runner.cursor = 'pointer';
        } else {
            runner.cursor = 'grab';
        }

        // Handle zooming
        if (mouse_state.wheel < 0) {
            zoom(1, mouse_state);
        } else if (mouse_state.wheel > 0) {
            zoom(-1, mouse_state);
        }

        // Make sure we stay in the box
        {
            let size = mapSize(state.zoom);

            if (size >= canvas.width)
                state.pos.x = util.clamp(state.pos.x, canvas.width / 2, size - canvas.width / 2);
            if (size >= canvas.height)
                state.pos.y = util.clamp(state.pos.y, canvas.height / 2, size - canvas.height / 2);
        }

        // Fix rounding issues
        state.pos.x = Math.floor(state.pos.x);
        state.pos.y = Math.floor(state.pos.y);
    };

    function zoom(delta, at) {
        if (state.zoom + delta < tiles.min_zoom || state.zoom + delta > tiles.max_zoom)
            return;

        if (delta > 0) {
            for (let i = 0; i < delta; i++) {
                state.pos.x = (state.pos.x * 2) + (at.x - canvas.width / 2);
                state.pos.y = (state.pos.y * 2) + (at.y - canvas.height / 2);
            }
        } else {
            for (let i = 0; i < -delta; i++) {
                state.pos.x = (state.pos.x * 0.5) - 0.5 * (at.x - canvas.width / 2);
                state.pos.y = (state.pos.y * 0.5) - 0.5 * (at.y - canvas.height / 2);
            }
        }

        if (zoom_animation == null) {
            zoom_animation = {
                start: null,
                from: state.zoom,
                to: null,
                value: state.zoom,
                at: null
            };
        } else {
            zoom_animation.from = zoom_animation.value;
        }
        zoom_animation.start = runner.updateCounter;
        zoom_animation.to = state.zoom + delta;
        zoom_animation.at = { x: at.x, y: at.y };

        state.zoom += delta;

        stopFetches();
    }

    function getViewport() {
        let viewport = {
            x1: Math.floor(state.pos.x - canvas.width / 2),
            y1: Math.floor(state.pos.y - canvas.height / 2),
            x2: Math.ceil(state.pos.x + canvas.width / 2),
            y2: Math.floor(state.pos.y + canvas.height / 2)
        };

        return viewport;
    }

    this.draw = function() {
        if (ctx == null)
            return;

        let viewport = getViewport();

        fetch_queue.length = 0;
        missing_assets = 0;

        let adjust = { x: 0, y: 0 };
        let scale = 1;

        if (zoom_animation != null) {
            let delta = Math.pow(2, state.zoom - zoom_animation.value) - 1;

            adjust.x = delta * (zoom_animation.at.x - canvas.width / 2);
            adjust.y = delta * (zoom_animation.at.y - canvas.height / 2);

            scale = Math.pow(2, zoom_animation.value - state.zoom);
        }

        // Draw tiles
        {
            ctx.save();

            ctx.translate(Math.floor(canvas.width / 2), Math.floor(canvas.height / 2));
            ctx.scale(scale, scale);
            ctx.translate(-state.pos.x + adjust.x, -state.pos.y + adjust.y);

            let i1 = Math.floor(viewport.x1 / tiles.tilesize);
            let j1 = Math.floor(viewport.y1 / tiles.tilesize);
            let i2 = Math.ceil(viewport.x2 / tiles.tilesize);
            let j2 = Math.ceil(viewport.y2 / tiles.tilesize);

            for (let i = i1; i <= i2; i++) {
                for (let j = j1; j <= j2; j++)
                    drawTile(origin, i, j);
            }

            if (zoom_animation != null) {
                for (let j = j1 - 1; j <= j2 + 1; j++) {
                    drawTile(origin, i1 - 1, j, false);
                    drawTile(origin, i2 + 1, j, false);
                }
                for (let i = i1; i <= i2; i++) {
                    drawTile(origin, i, j1 - 1, false);
                    drawTile(origin, i, j2 + 1, false);
                }
            }

            ctx.restore();
        }

        // Draw markers
        {
            ctx.save();
            ctx.translate(0.5, 0.5);

            let current_filter = null;

            ctx.filter = null || 'none';

            for (let markers of Object.values(marker_groups)) {
                for (let marker of markers) {
                    let pos = self.coordToScreen(marker.latitude, marker.longitude);

                    if (zoom_animation != null) {
                        let centered = {
                            x: pos.x - canvas.width / 2,
                            y: pos.y - canvas.height / 2
                        };

                        pos.x = Math.round(canvas.width / 2 + scale * (centered.x + adjust.x));
                        pos.y = Math.round(canvas.height / 2 + scale * (centered.y + adjust.y));
                    }

                    if (pos.x < -marker.size || pos.x > canvas.width + marker.size)
                        continue;
                    if (pos.y < -marker.size || pos.y > canvas.height + marker.size)
                        continue;

                    if (marker.filter != current_filter) {
                        ctx.filter = marker.filter || 'none';
                        current_filter = marker.filter;
                    }

                    if (marker.icon != null) {
                        let img = getImage(marker_textures, marker.icon);

                        let width = adaptMarkerSize(marker.size, state.zoom);
                        let height = adaptMarkerSize(marker.size, state.zoom);

                        if (img != null)
                            ctx.drawImage(img, pos.x - width / 2, pos.y - height / 2, width, height);
                    } else if (marker.circle != null) {
                        let radius = adaptMarkerSize(marker.size / 2, state.zoom);

                        ctx.beginPath();
                        ctx.arc(pos.x, pos.y, radius, 0, 2 * Math.PI, false);

                        ctx.fillStyle = marker.circle || 'red';
                        ctx.fill();
                    } else {
                        console.error('Unknown marker type');
                    }
                }
            }

            ctx.restore();
        }

        if (!missing_assets)
            stopFetches();
    };

    function adaptMarkerSize(size, zoom) {
        if (zoom >= 7) {
            return size;
        } else {
            return size * ((zoom + 3) / 10);
        }
    }

    function drawTile(origin, i, j, fetch = true) {
        let x = Math.floor(i * tiles.tilesize);
        let y = Math.floor(j * tiles.tilesize);

        // Start with appropriate tile (if any)
        {
            let tile = getTile(state.zoom, i, j, fetch);

            if (tile != null) {
                ctx.drawImage(tile, x, y, tiles.tilesize, tiles.tilesize);
                return;
            }
        }

        // Try zoomed out tiles if we have any
        for (let out = 1; state.zoom - out >= tiles.min_zoom; out++) {
            let factor = Math.pow(2, out);

            let i2 = Math.floor(i / factor);
            let j2 = Math.floor(j / factor);
            let tile = getTile(state.zoom - out, i2, j2, false);

            if (tile != null) {
                ctx.drawImage(tile, tiles.tilesize / factor * (i % factor), tiles.tilesize / factor * (j % factor),
                                    tiles.tilesize / factor, tiles.tilesize / factor,
                                    x, y, tiles.tilesize, tiles.tilesize);
                break;
            }
        }

        // Also put in zoomed in tiles (could be partial)
        for (let out = 1; out < 5; out++) {
            if (state.zoom + out > tiles.max_zoom)
                break;

            let factor = Math.pow(2, out);

            let i2 = Math.floor(i * factor);
            let j2 = Math.floor(j * factor);

            for (let di = 0; di < factor; di++) {
                for (let dj = 0; dj < factor; dj++) {
                    let tile = getTile(state.zoom + out, i2 + di, j2 + dj, false);

                    if (tile != null) {
                        ctx.drawImage(tile, 0, 0, tiles.tilesize, tiles.tilesize,
                                      x + (di * tiles.tilesize / factor),
                                      y + (dj * tiles.tilesize / factor),
                                      tiles.tilesize / factor, tiles.tilesize / factor);
                    }
                }
            }
        }
    }

    function getTile(zoom, i, j, fetch = true) {
        if (i < 0 || i >= Math.pow(2, zoom))
            return null;
        if (j < 0 || j >= Math.pow(2, zoom))
            return null;

        let url = parseURL(tiles.url, zoom, i, j);
        let tile = getImage(known_tiles, url, fetch);

        return tile;
    }

    function getImage(cache, url, fetch = true) {
        if (typeof url != 'string')
            return url;

        let img = cache.get(url);

        missing_assets += (img == null && fetch);

        if (img == null && fetch) {
            if (fetch_queue.includes(url))
                return null;
            if (fetch_handles.has(url))
                return null;

            fetch_queue.push(url);

            if (active_fetchers < MAX_FETCHERS) {
                let run_fetcher = async function() {
                    active_fetchers++;

                    while (fetch_queue.length) {
                        let url = fetch_queue.pop();

                        let handle = {
                            url: url,
                            img: null
                        };

                        fetch_handles.set(url, handle);

                        try {
                            let img = await fetchImage(handle);

                            cache.set(url, img);
                            runner.busy();
                        } catch (err) {
                            if (err != null)
                                console.error(err);
                        } finally {
                            fetch_handles.delete(url);
                        }
                    }

                    active_fetchers--;
                };

                run_fetcher();
            }
        }

        return img;
    }

    function parseURL(url, zoom, i, j) {
        let ret = url.replace(/{[a-z]+}/g, m => {
            switch (m) {
                case '{s}': return 'a';
                case '{z}': return zoom;
                case '{x}': return i;
                case '{y}': return j;
                case '{r}': return '';
                case '{ext}': return 'png';
            }
        });

        return ret;
    }

    async function fetchImage(handle) {
        let img = await new Promise((resolve, reject) => {
            let img = new Image();

            if (handle.url == null) {
                reject(null);
                return;
            }

            img.onload = () => resolve(img);
            img.onerror = () => {
                if (handle.url == null)
                    reject(null);

                reject(new Error(`Failed to load texture '${handle.url}'`));
            };

            img.src = handle.url;
            img.crossOrigin = 'anonymous';

            handle.img = img;
        });

        // Fix latency spikes caused by image decoding
        if (typeof createImageBitmap != 'undefined')
            img = await createImageBitmap(img);

        return img;
    }

    function stopFetches() {
        fetch_queue.length = 0;

        for (let handle of fetch_handles.values()) {
            handle.url = null;

            if (handle.img != null)
                handle.img.setAttribute('src', '');
        }
    }

    function latLongToXY(latitude, longitude, zoom) {
        const MinLatitude = -85.05112878;
        const MaxLatitude = 85.05112878;
        const MinLongitude = -180;
        const MaxLongitude = 180;

        latitude = util.clamp(latitude, MinLatitude, MaxLatitude);
        longitude = util.clamp(longitude, MinLongitude, MaxLongitude);

        let x = (longitude + 180) / 360;
        let sinLatitude = Math.sin(latitude * Math.PI / 180);
        let y = 0.5 - Math.log((1 + sinLatitude) / (1 - sinLatitude)) / (4 * Math.PI);

        let size = mapSize(zoom);
        let px = util.clamp(x * size + 0.5, 0, ((size - 1) >>> 0));
        let py = util.clamp(y * size + 0.5, 0, ((size - 1) >>> 0));

        return { x: px, y: py };
    }

    this.coordToScreen = function(latitude, longitude) {
        let viewport = getViewport();
        let pos = latLongToXY(latitude, longitude, state.zoom);

        pos.x -= viewport.x1;
        pos.y -= viewport.y1;

        return pos;
    };

    this.screenToCoord = function(pos) {
        let size = mapSize(state.zoom);
        let px = util.clamp(pos.x + state.pos.x - canvas.width / 2, 0, size);
        let py = size - util.clamp(pos.y + state.pos.y - canvas.height / 2, 0, size);

        let x = (util.clamp(px, 0, ((size - 1) >>> 0)) - 0.5) / size;
        let y = (util.clamp(py, 0, ((size - 1) >>> 0)) - 0.5) / size;

        let longitude = (x * 360) - 180;
        let latitude = Math.atan(Math.sinh(2 * (y - 0.5) * Math.PI)) * (180 / Math.PI);

        return [latitude, longitude];
    };

    function mapSize(zoom) {
        return tiles.tilesize * Math.pow(2, zoom);
    }

    function distance(p1, p2) {
        let dx = p1.x - p2.x;
        let dy = p1.y - p2.y;

        return Math.sqrt(dx * dx + dy * dy);
    }
}

module.exports = {
    TileMap
};
