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

import { Util, Net, LruMap } from '../../../web/core/common.js';

function TileMap(runner) {
    let self = this;

    // Shortcuts
    let canvas = runner.canvas;
    let ctx = canvas.getContext('2d');
    let mouse_state = runner.mouseState;
    let pressed_keys = runner.pressedKeys;

    let tiles = null;

    let marker_groups = {};

    let handle_click = (markers) => {};
    let style_cluster = (element) => {};
    let cluster_treshold = 1.8;

    const DEFAULT_ZOOM = 7;
    const MAX_FETCHERS = 8;

    let state = null;

    let last_wheel_time = 0;
    let zoom_animation = null;

    let render_elements = [];
    let render_tooltip = null;

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

        zoomLevel: { get: () => state.zoom, enumerable: true },

        onClick: { get: () => handle_click, set: func => { handle_click = func; }, enumerable: true },

        clusterTreshold: { get:() => cluster_treshold, set: treshold => { cluster_treshold = treshold; }, enumerable: true },
        styleCluster: { get: () => style_cluster, set: func => { style_cluster = func; }, enumerable: true }
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
        if (zoom != null) {
            state.zoom = zoom;
            self.refresh();
        }
        zoom_animation = null;

        state.pos = latLongToXY(lat, lng, state.zoom);

        runner.busy();
    };

    this.reveal = function(markers, minimize = false) {
        let prev_zoom = state.zoom;

        let zoom = tiles.max_zoom + 1;
        let pos1 = null;
        let pos2 = null;

        let items0 = markers.map(marker => markerToItem(marker, state.zoom));
        let cluster0 = clusterize(items0, cluster_treshold);

        // Looping is a bit dumb but it's easy and it works
        while (zoom > tiles.min_zoom) {
            let items = markers.map(marker => markerToItem(marker, zoom - 1));
            let cluster = clusterize(items, cluster_treshold);

            let min = {
                x: Math.min(...items.map(item => item.x - item.size)),
                y: Math.min(...items.map(item => item.y - item.size))
            };
            let max = {
                x: Math.max(...items.map(item => item.x + item.size)),
                y: Math.max(...items.map(item => item.y + item.size))
            };

            let valid = Math.abs(max.x - min.x) <= canvas.width * 0.9 &&
                        Math.abs(max.y - min.y) <= canvas.height * 0.9;

            zoom--;
            pos1 = min;
            pos2 = max;

            if (valid && minimize && (zoom <= state.zoom || cluster.length > cluster0.length))
                break;
            if (valid && !minimize)
                break;
        }

        let zoomed = (zoom != prev_zoom);

        if (minimize && !zoomed)
            return false;

        state.pos = {
            x: (pos1.x + pos2.x) / 2,
            y: (pos1.y + pos2.y) / 2
        };
        state.zoom = zoom;

        zoom_animation = null;

        self.refresh();

        runner.busy();

        return zoomed;
    };

    this.setMarkers = function(key, markers) {
        if (!Array.isArray(markers))
            throw new Error('Not an array of markers');

        marker_groups[key] = markers;

        self.refresh();
    };

    this.clearMarkers = function(key) {
        delete marker_groups[key];
        self.refresh();
    };

    this.refresh = function() {
        render_elements.length = 0;

        let items = [];
        for (let group of Object.values(marker_groups))
            items.push(...group.map(marker => markerToItem(marker, state.zoom)));
        let clusters = clusterize(items, cluster_treshold);

        let viewport = getViewport();

        for (let cluster of clusters) {
            if (cluster.length == 1) {
                let item = cluster[0];
                let marker = item.marker;

                let element = {
                    type: 'marker',

                    x: item.x,
                    y: item.y,
                    size: marker.size,
                    icon: marker.icon,
                    circle: marker.circle,
                    text: marker.text,

                    clickable: marker.clickable,
                    priority: marker.priority ?? 1,
                    filter: marker.filter,

                    markers: [marker]
                };

                render_elements.push(element);
            } else {
                let rect = {
                    x1: Math.min(...cluster.map(item => item.x - item.size / 2)),
                    y1: Math.min(...cluster.map(item => item.y - item.size / 2)),
                    x2: Math.max(...cluster.map(item => item.x + item.size / 2)),
                    y2: Math.max(...cluster.map(item => item.y + item.size / 2))
                };

                let center = {
                    x: (rect.x1 + rect.x2) / 2,
                    y: (rect.y1 + rect.y2) / 2
                };
                let radius = Math.max(rect.x2 - rect.x1, rect.y2 - rect.y1) / 2;

                let element = {
                    type: 'cluster',
                    cluster: cluster[0].cluster,

                    x: center.x,
                    y: center.y,
                    size: 2 * radius,
                    circle: cluster[0].cluster,
                    text: '' + cluster.length,

                    clickable: true,
                    priority: 0,
                    filter: null,

                    markers: cluster.map(item => item.marker)
                };

                style_cluster(element);

                render_elements.push(element);
            }
        }

        // Show individual markers on top, sorted by priority (if any)
        // and size, and then show clusters by size.
        render_elements.sort((element0, element1) => {
            if (element0.priority != element1.priority) {
                return element0.priority - element1.priority;
            } else {
                return element1.size - element0.size;
            }
        });
    };

    this.update = function() {
        if (ctx == null)
            return;

        // Animate zoom
        if (zoom_animation != null) {
            let t = easeInOutSine((runner.updateCounter - zoom_animation.start) / 60);

            if (t < 1) {
                zoom_animation.value = zoom_animation.from + t * (zoom_animation.to - zoom_animation.from);
                runner.busy();
            } else {
                zoom_animation = null;
            }
        }

        // Detect user targets
        let target = null;
        {
            let viewport = getViewport();

            for (let i = render_elements.length - 1; i >= 0; i--) {
                let element = render_elements[i];

                let pos = {
                    x: element.x - viewport.x1,
                    y: element.y - viewport.y1,
                };

                if (distance(pos, mouse_state) < adaptMarkerSize(element.size, state.zoom) / 2) {
                    target = element;
                    break;
                }
            }
        }

        // Handle tooltip
        if (target != null) {
            let tooltips = target.markers.filter(marker => marker.tooltip).map(marker => marker.tooltip);

            if (tooltips.length) {
                render_tooltip = {
                    target: target,
                    text: Array.from(new Set(tooltips)).sort().join('\n')
                };
            } else {
                render_tooltip = null;
            }
        } else {
            render_tooltip = null;
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
            handle_click(target.markers, target.clickable);

        // Adjust cursor style
        if (state.grab != null) {
            runner.cursor = 'grabbing';
        } else if (target != null) {
            runner.cursor = 'pointer';
        } else {
            runner.cursor = 'grab';
        }

        // Handle zooming
        if (mouse_state.wheel) {
            let now = performance.now();

            if (now - last_wheel_time >= 200) {
                if (mouse_state.wheel < 0) {
                    self.zoom(1, mouse_state);
                } else if (mouse_state.wheel > 0) {
                    self.zoom(-1, mouse_state);
                }

                last_wheel_time = now;
            }
        }

        // Make sure we stay in the box
        {
            let size = mapSize(state.zoom);

            if (size >= canvas.width)
                state.pos.x = Util.clamp(state.pos.x, canvas.width / 2, size - canvas.width / 2);
            if (size >= canvas.height)
                state.pos.y = Util.clamp(state.pos.y, canvas.height / 2, size - canvas.height / 2);
        }

        // Fix rounding issues
        state.pos.x = Math.floor(state.pos.x);
        state.pos.y = Math.floor(state.pos.y);
    };

    function easeInOutSine(t) {
        return -(Math.cos(Math.PI * t) - 1) / 2;
    }

    this.zoom = function(delta, at = null) {
        if (state.zoom + delta < tiles.min_zoom || state.zoom + delta > tiles.max_zoom)
            return;
        if (at == null)
            at = { x: canvas.width / 2, y: canvas.height / 2 };

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

        self.refresh();
        stopFetchers();
    };

    function getViewport() {
        let viewport = {
            x1: Math.floor(state.pos.x - canvas.width / 2),
            y1: Math.floor(state.pos.y - canvas.height / 2),
            x2: Math.ceil(state.pos.x + canvas.width / 2),
            y2: Math.ceil(state.pos.y + canvas.height / 2)
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
        let anim_zoom = state.zoom;
        let anim_scale = 1;

        if (zoom_animation != null) {
            anim_zoom = zoom_animation.value;

            let delta = Math.pow(2, state.zoom - anim_zoom) - 1;

            adjust.x = delta * (zoom_animation.at.x - canvas.width / 2);
            adjust.y = delta * (zoom_animation.at.y - canvas.height / 2);

            anim_scale = Math.pow(2, anim_zoom - state.zoom);
        }

        // Draw tiles
        {
            ctx.save();

            ctx.translate(Math.floor(canvas.width / 2), Math.floor(canvas.height / 2));
            ctx.scale(anim_scale, anim_scale);
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
                for (let j = j1 - 2; j <= j2 + 2; j++) {
                    drawTile(origin, i1 - 2, j, false);
                    drawTile(origin, i1 - 1, j, false);
                    drawTile(origin, i2 + 1, j, false);
                    drawTile(origin, i2 + 2, j, false);
                }
                for (let i = i1; i <= i2; i++) {
                    drawTile(origin, i, j1 - 2, false);
                    drawTile(origin, i, j1 - 1, false);
                    drawTile(origin, i, j2 + 1, false);
                    drawTile(origin, i, j2 + 2, false);
                }
            }

            ctx.restore();
        }

        // Draw markers
        {
            ctx.save();
            ctx.translate(0.5, 0.5);

            let current_filter = null;

            ctx.filter = 'none';

            for (let element of render_elements) {
                let pos = {
                    x: element.x - viewport.x1,
                    y: element.y - viewport.y1
                };

                if (zoom_animation != null) {
                    let centered = {
                        x: pos.x - canvas.width / 2,
                        y: pos.y - canvas.height / 2
                    };

                    pos.x = Math.round(canvas.width / 2 + anim_scale * (centered.x + adjust.x));
                    pos.y = Math.round(canvas.height / 2 + anim_scale * (centered.y + adjust.y));
                }

                if (pos.x < -element.size || pos.x > canvas.width + pos.size)
                    continue;
                if (pos.y < -element.size || pos.y > canvas.height + pos.size)
                    continue;

                if (element.filter != current_filter) {
                    ctx.filter = element.filter ?? 'none';
                    current_filter = element.filter;
                }

                if (element.icon != null) {
                    let img = getImage(marker_textures, element.icon);

                    let width = adaptMarkerSize(element.size, anim_zoom);
                    let height = adaptMarkerSize(element.size, anim_zoom);

                    if (img != null)
                        ctx.drawImage(img, pos.x - width / 2, pos.y - height / 2, width, height);
                } else if (element.circle != null) {
                    let radius = adaptMarkerSize(element.size / 2, anim_zoom);

                    ctx.beginPath();
                    ctx.arc(pos.x, pos.y, radius, 0, 2 * Math.PI, false);

                    ctx.fillStyle = element.circle;
                    ctx.fill();
                }

                if (element.text) {
                    let target = 0.75 * adaptMarkerSize(element.size, anim_zoom);
                    let size = Math.floor(element.size / 2) + 1;
                    let width = null;

                    // Find appropriate font size
                    do {
                        ctx.font = (--size) + 'px Open Sans';
                        width = ctx.measureText(element.text).width;
                    } while (width > target);

                    ctx.fillStyle = 'white';
                    ctx.fillText(element.text, pos.x - width / 2, pos.y + size / 3);
                }
            }

            // Draw tooltip
            if (render_tooltip != null) {
                ctx.font = '12px Open Sans';

                let target = render_tooltip.target;

                let lines = render_tooltip.text.split('\n');
                let width = Math.max(...lines.map(line => ctx.measureText(line).width));
                let height = 16 * lines.length;

                let pos = {
                    x: target.x + (target.size / 2) - viewport.x1 + 5,
                    y: target.y - viewport.y1 - height / 2 - 5
                };

                if (pos.x + width > viewport.x2 - viewport.x1 - 10)
                    pos.x = target.x - (target.size / 2) - width - viewport.x1 - 15;
                if (pos.y < 10) {
                    pos.y = 10;
                } else if (pos.y + height > viewport.y2 - viewport.y1 - 10) {
                    pos.y = viewport.y2 - height - viewport.y1 - 10;
                }

                ctx.fillStyle = '#ffffffdd';
                ctx.fillRect(pos.x, pos.y, width + 10, height + 6);

                ctx.fillStyle = 'black';
                for (let i = 0; i < lines.length; i++) {
                    let y = pos.y + (i + 1) * 16;
                    ctx.fillText(lines[i], pos.x + 5, y);
                }
            }

            ctx.restore();
        }

        if (missing_assets) {
            let start = Math.min(fetch_queue.length, MAX_FETCHERS - active_fetchers);

            for (let i = 0; i < start; i++)
                startFetcher();
        } else {
            stopFetchers();
        }
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

            let handle = {
                valid: true,
                cache: cache,
                url: url,
                controller: null
            };

            fetch_queue.push(handle);
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

    async function startFetcher() {
        active_fetchers++;

        while (fetch_queue.length) {
            let idx = Util.getRandomInt(0, fetch_queue.length);
            let [handle] = fetch_queue.splice(idx, 1);

            fetch_handles.set(handle.url, handle);

            try {
                let img = await fetchImage(handle);

                if (img != null) {
                    handle.cache.set(handle.url, img);
                    runner.busy();
                }
            } catch (err) {
                console.error(err);
            } finally {
                fetch_handles.delete(handle.url);
            }
        }

        active_fetchers--;
    }

    async function fetchImage(handle) {
        if (!handle.valid)
            return null;

        handle.controller = new AbortController;

        try {
            let response = await Net.fetch(handle.url, { signal: handle.controller.signal });
            let blob = await response.blob();
            let img = await createImageBitmap(blob);

            return img;
        } catch (err) {
            if (!handle.valid)
                return null;

            handle.valid = false;
            throw new Error(`Failed to load texture '${handle.url}'`);
        }
    }

    function stopFetchers() {
        fetch_queue.length = 0;

        for (let handle of fetch_handles.values()) {
            handle.valid = false;

            if (handle.controller != null)
                handle.controller.abort();
        }
    }

    function latLongToXY(latitude, longitude, zoom) {
        const MinLatitude = -85.05112878;
        const MaxLatitude = 85.05112878;
        const MinLongitude = -180;
        const MaxLongitude = 180;

        latitude = Util.clamp(latitude, MinLatitude, MaxLatitude);
        longitude = Util.clamp(longitude, MinLongitude, MaxLongitude);

        let x = (longitude + 180) / 360;
        let sinLatitude = Math.sin(latitude * Math.PI / 180);
        let y = 0.5 - Math.log((1 + sinLatitude) / (1 - sinLatitude)) / (4 * Math.PI);

        let size = mapSize(zoom);
        let px = Util.clamp(Math.round(x * size + 0.5), 0, ((size - 1) >>> 0));
        let py = Util.clamp(Math.round(y * size + 0.5), 0, ((size - 1) >>> 0));

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
        let px = Util.clamp(pos.x + state.pos.x - canvas.width / 2, 0, size);
        let py = size - Util.clamp(pos.y + state.pos.y - canvas.height / 2, 0, size);

        let x = (Util.clamp(px, 0, ((size - 1) >>> 0)) - 0.5) / size;
        let y = (Util.clamp(py, 0, ((size - 1) >>> 0)) - 0.5) / size;

        let longitude = (x * 360) - 180;
        let latitude = Math.atan(Math.sinh(2 * (y - 0.5) * Math.PI)) * (180 / Math.PI);

        return [latitude, longitude];
    };

    function mapSize(zoom) {
        return tiles.tilesize * Math.pow(2, zoom);
    }

    function markerToItem(marker, zoom) {
        let pos = latLongToXY(marker.latitude, marker.longitude, zoom);

        let item = {
            x: pos.x,
            y: pos.y,
            size: marker.size,
            cluster: marker.cluster,
            marker: marker
        };

        return item;
    }
}

function clusterize(items, treshold) {
    items = items.slice().sort((item1, item2) => item1.x - item2.x);

    let max_delta = Math.max(...items.map(item => item.size)) * 2;
    let clusters = [];

    for (let i = 0; i < items.length;) {
        let item = items[i];

        let cluster = [item];
        let remain = [];

        if (item.cluster != null) {
            let accumulator = {
                x: item.x,
                y: item.y
            };
            let center = Object.assign({}, accumulator);

            for (let j = i + 1; j < items.length; j++) {
                let other = items[j];

                if (other.x - center.x >= max_delta)
                    break;

                if (other.cluster != item.cluster) {
                    remain.push(other);
                    continue;
                }

                let dist = distance(center, other);
                let treshold_adj = Math.min(item.size, other.size) / treshold;

                if (dist < treshold_adj) {
                    cluster.push(other);

                    accumulator.x += other.x;
                    accumulator.y += other.y;
                    center.x = accumulator.x / cluster.length;
                    center.y = accumulator.y / cluster.length;
                } else {
                    remain.push(other);
                }
            }

            i += items.splice(i, cluster.length, ...cluster).length;
            items.splice(i, remain.length, ...remain);
        } else {
            i++;
        }

        clusters.push(cluster);
    }

    return clusters;
}

function distance(p1, p2) {
    let dx = p1.x - p2.x;
    let dy = p1.y - p2.y;

    return Math.sqrt(dx * dx + dy * dy);
}

export { TileMap }
