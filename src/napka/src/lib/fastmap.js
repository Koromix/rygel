import { AppRunner } from './runner.js';
import { LruMap } from './util.js';

function FastMap(canvas) {
    let self = this;

    let runner = new AppRunner(canvas, update, draw);

    let ctx = runner.ctx;
    let mouse_state = runner.mouse_state;
    let pressed_keys = runner.pressed_keys;

    let tiles = null;

    let marker_groups = {};

    const DEFAULT_ZOOM = 7;
    const MAX_FETCHERS = 8;

    let state = null;

    let missing_assets = 0;
    let active_fetchers = 0;
    let fetch_queue = [];

    let known_tiles = new LruMap(1024);
    let marker_textures = new LruMap(64);

    this.start = async function(config) {
        runner.init();

        tiles = Object.assign({
            min_zoom: 1
        }, config);

        canvas.style.cursor = 'grab';

        ctx.imageSmoothingEnabled = true;
        ctx.imageSmoothingQuality = 'high';

        state = {
            zoom: DEFAULT_ZOOM,
            pos: latLongToPixelXY(48.866667, 2.333333, DEFAULT_ZOOM), // Paris
            grab: null
        };

        known_tiles = new LruMap(1024);
    };

    this.move = function(lat, lng, zoom = null) {
        if (zoom != null)
            state.zoom = zoom;

        state.pos = latLongToPixelXY(lat, lng, state.zoom);
    };

    this.setMarkers = function(key, markers) {
        if (!Array.isArray(markers))
            throw new Error('Not an array of markers');

        marker_groups[key] = markers;
    };

    this.clearMarkers = function(key) {
        delete marker_groups[key];
    };

    function update() {
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

                    let pos = latLongToPixelXY(marker.latitude, marker.longitude, state.zoom);
                    pos.x -= state.pos.x - canvas.width / 2;
                    pos.y -= state.pos.y - canvas.height / 2;

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
            canvas.style.cursor = 'grabbing';
        } else if (target != null) {
            canvas.style.cursor = 'pointer';
        } else {
            canvas.style.cursor = 'grab';
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
                state.pos.x = clamp(state.pos.x, canvas.width / 2, size - canvas.width / 2);
            if (size >= canvas.height)
                state.pos.y = clamp(state.pos.y, canvas.height / 2, size - canvas.height / 2);
        }
    };

    function zoom(delta, at) {
        if (state.zoom + delta < tiles.min_zoom || state.zoom + delta > tiles.max_zoom)
            return;

        state.pos.x *= Math.pow(2, delta);
        state.pos.y *= Math.pow(2, delta);
        state.pos.x += delta * (at.x - canvas.width / 2);
        state.pos.y += delta * (at.y - canvas.height / 2);

        state.zoom += delta;
    }

    function draw() {
        let origin = {
            x: Math.floor(state.pos.x - canvas.width / 2),
            y: Math.floor(state.pos.y - canvas.height / 2)
        };

        fetch_queue.length = 0;
        missing_assets = 0;

        // Draw tiles
        {
            ctx.save();

            let adjust = {
                x: -(origin.x % tiles.tilesize),
                y: -(origin.y % tiles.tilesize)
            };

            if (origin.x < 0 && adjust.x)
                adjust.x -= tiles.tilesize;
            if (origin.y < 0 && adjust.y)
                adjust.y -= tiles.tilesize;

            ctx.translate(adjust.x, adjust.y);

            for (let x = 0; x < canvas.width + tiles.tilesize; x += tiles.tilesize) {
                for (let y = 0; y < canvas.height + tiles.tilesize; y += tiles.tilesize) {
                    drawTile(origin, x, y);
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
                    let pos = latLongToPixelXY(marker.latitude, marker.longitude, state.zoom);

                    pos.x -= origin.x;
                    pos.y -= origin.y;

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
            fetch_queue.length = 0;

        ctx.restore();
    };

    function adaptMarkerSize(size, zoom) {
        if (zoom >= 7) {
            return size;
        } else {
            return size * ((zoom + 3) / 10);
        }
    }

    function drawTile(origin, x, y) {
        let i = Math.floor((x + origin.x) / tiles.tilesize);
        let j = Math.floor((y + origin.y) / tiles.tilesize);

        // Start with appropriate tile (if any)
        {
            let tile = getTile(state.zoom, i, j);

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
                return;
            }
        }

        ctx.clearRect(x, y, tiles.tilesize, tiles.tilesize);

        // Last restort, try zoomed in tiles (could be partial)
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

    function getImage(map, url, fetch = true) {
        let img = map.get(url);

        if (img == null && fetch) {
            if (fetch_queue.includes(url))
                return null;

            missing_assets++;
            fetch_queue.push(url);

            if (active_fetchers < MAX_FETCHERS) {
                let run_fetcher = async function() {
                    active_fetchers++;

                    while (fetch_queue.length) {
                        let url = fetch_queue.pop();

                        try {
                            let img = await runner.loadTexture(url);
                            map.set(url, img);
                        } catch (err) {
                            console.error(err);
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

    function latLongToPixelXY(latitude, longitude, zoom) {
        const EarthRadius = 6378137;
        const MinLatitude = -85.05112878;
        const MaxLatitude = 85.05112878;
        const MinLongitude = -180;
        const MaxLongitude = 180;

        latitude = clamp(latitude, MinLatitude, MaxLatitude);
        longitude = clamp(longitude, MinLongitude, MaxLongitude);

        let x = (longitude + 180) / 360;
        let sinLatitude = Math.sin(latitude * Math.PI / 180);
        let y = 0.5 - Math.log((1 + sinLatitude) / (1 - sinLatitude)) / (4 * Math.PI);

        let size = mapSize(zoom);
        let px = clamp(x * size + 0.5, 0, ((size - 1) >>> 0));
        let py = clamp(y * size + 0.5, 0, ((size - 1) >>> 0));

        return { x: px, y: py };
    }

    function mapSize(zoom) {
        return ((tiles.tilesize << zoom) >>> 0);
    }

    // JS should have this!
    function clamp(value, min, max) {
        if (value > max) {
            return max;
        } else if (value < min) {
            return min;
        } else {
            return value;
        }
    }

    function distance(p1, p2) {
        let dx = p1.x - p2.x;
        let dy = p1.y - p2.y;

        return Math.sqrt(dx * dx + dy * dy);
    }
}

module.exports = {
    FastMap
};
