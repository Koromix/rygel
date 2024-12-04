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

import { Util, Log, Net } from './common.js';

let audio = null;

function AppRunner(canvas) {
    let self = this;

    let ctx = canvas.getContext('2d');

    let prev_canvas_width = 0;
    let prev_canvas_height = 0;
    let prev_timestamp = 0;

    let run = true;
    let idle_timeout = 0;
    let run_next = false;
    let run_until = 0;
    let is_idle = false;

    let update_frequency = 240;
    let updates = 0;
    let update_counter = 0;
    let draw_counter = 0;

    let frame_times = [];
    let frame_time = 0;
    let update_times = [];
    let update_time = 0;
    let draw_times = [];
    let draw_time = 0;

    let handle_update = null;
    let handle_draw = null;
    let handle_context_menu = null;

    let touch_digits = -1;
    let touch_start = null;
    let touch_distance = 0;

    let pressed_keys = {
        up: 0,
        down: 0,
        left: 0,
        right: 0,

        tab: 0,
        space: 0,
        escape: 0,
        ctrl: 0,
        alt: 0,
        delete: 0,

        a: 0, b: 0, c: 0, d: 0, e: 0, f: 0, g: 0, h: 0, i: 0,
        j: 0, k: 0, l: 0, m: 0, n: 0, o: 0, p: 0, q: 0, r: 0,
        s: 0, t: 0, u: 0, v: 0, w: 0, x: 0, y: 0, z: 0
    };
    let mouse_state = {
        x: 0,
        y: 0,

        contact: false,
        moving: false,

        left: 0,
        middle: 0,
        right: 0,
        wheel: 0
    };
    let mouse_mirror = {};
    let skip_clicks = 0;

    let cursor = 'default';
    let ignore_new_cursor = false;

    let old_sources = new Map;
    let new_sources = new Map;

    Object.defineProperties(this,  {
        canvas: { value: canvas, writable: false, enumerable: true },

        onUpdate: { get: () => handle_draw, set: func => { handle_update = func; }, enumerable: true },
        onDraw: { get: () => handle_draw, set: func => { handle_draw = func; }, enumerable: true },
        onContextMenu: { get: () => handle_draw, set: func => { handle_context_menu = func; }, enumerable: true },

        pressedKeys: { value: pressed_keys, writable: false, enumerable: true },
        mouseState: { value: mouse_state, writable: false, enumerable: true },

        idleTimeout: { get: () => idle_timeout, set: value => { idle_timeout = value; }, enumerable: true },

        updateFrequency: { get: () => update_frequency, set: frequency => { update_frequency = frequency; }, enumerable: true},
        updateCounter: { get: () => update_counter, enumerable: true },
        drawCounter: { get: () => draw_counter, enumerable: true },
        frameTime: { get: () => frame_time, enumerable: true },
        updateTime: { get: () => update_time, enumerable: true },
        drawTime: { get: () => draw_time, enumerable: true },

        cursor: {
            get: () => cursor,
            set: value => {
                if (!ignore_new_cursor)
                    cursor = value;
                ignore_new_cursor = true;
            },
            enumerable: true
        }
    });

    // ------------------------------------------------------------------------
    // Init
    // ------------------------------------------------------------------------

    this.start = function() {
        canvas.addEventListener('contextmenu', e => {
            if (handle_context_menu == null)
                return;

            let rect = canvas.getBoundingClientRect();
            let at = { x: e.clientX - rect.left, y: e.clientY - rect.top };

            handle_context_menu(at);

            e.preventDefault();
        });

        canvas.addEventListener('keydown', handleKeyEvent);
        canvas.addEventListener('keyup', handleKeyEvent);
        canvas.setAttribute('tabindex', '0');

        if (!isTouchDevice()) {
            canvas.addEventListener('mousemove', handleMouseEvent);
            canvas.addEventListener('mousedown', handleMouseEvent);
            canvas.addEventListener('mouseup', handleMouseEvent);
            canvas.addEventListener('wheel', handleMouseEvent);
        }

        canvas.addEventListener('touchstart', handleTouchEvent);
        canvas.addEventListener('touchmove', handleTouchEvent);
        canvas.addEventListener('touchend', handleTouchEvent);

        new ResizeObserver(() => self.busy()).observe(canvas);
        syncSize();

        window.requestAnimationFrame(loop);
    };

    this.stop = function() {
        // Please make a new canvas, because event handlers remain connected
        run = false;
    };

    function isTouchDevice() {
        if (!('ontouchstart' in window))
            return false;
        if (!navigator.maxTouchPoints)
            return false;
        if (navigator.msMaxTouchPoints)
            return false;

        return true;
    }

    // ------------------------------------------------------------------------
    // Event handling
    // ------------------------------------------------------------------------

    function handleKeyEvent(e) {
        canvas.focus();
        self.busy();

        if (e.repeat)
            return;

        let key = e.key.toLowerCase();
        let code = e.keyCode;

        if (key.match(/^[a-z]$/)) {
            updateKey(key, e.type);
        } else if (code == 9) { // Tab
            updateKey('tab', e.type);
            e.preventDefault();
        } else if (key == ' ') {
            updateKey('space', e.type);
        } else if (code == 27) {
            updateKey('escape', e.type);
        } else if (code == 17) {
            updateKey('ctrl', e.type);
        } else if (code == 18) {
            updateKey('alt', e.type);
        } else if (code == 46) {
            updateKey('delete', e.type);
        }

        if (key == 'z' || code == 38) {
            updateKey('up', e.type);
        } else if (key == 's' || code == 40) {
            updateKey('down', e.type);
        } else if (key == 'q' || code == 37) {
            updateKey('left', e.type);
        } else if (key == 'd' || code == 39) {
            updateKey('right', e.type);
        }
    }

    function updateKey(key, type) {
        if (type == 'keydown') {
            pressed_keys[key] = 1;
        } else if (type == 'keyup') {
            pressed_keys[key] = (pressed_keys[key] > 0) ? -1 : 0;
        }
    }

    function handleMouseEvent(e) {
        canvas.focus();
        self.busy();

        let rect = canvas.getBoundingClientRect();

        mouse_state.x = e.clientX - rect.left;
        mouse_state.y = e.clientY - rect.top;

        if ((e.buttons & 0b001) && !mouse_state.left) {
            mouse_state.left = 1;
        } else if (!(e.buttons & 0b001) && mouse_state.left)  {
            mouse_state.left = (skip_clicks & 0b001) ? 0 : -1;
            skip_clicks &= ~0b001;
        }
        if ((e.buttons & 0b100) && !mouse_state.middle) {
            mouse_state.middle = 1;
        } else if (!(e.buttons & 0b100) && mouse_state.middle)  {
            mouse_state.middle = (skip_clicks & 0b100) ? 0 : -1;
            skip_clicks &= ~0b100;
        }
        if ((e.buttons & 0b010) && !mouse_state.right) {
            mouse_state.right = 1;
        } else if (!(e.buttons & 0b010) && mouse_state.right)  {
            mouse_state.right = (skip_clicks & 0b010) ? 0 : -1;
            skip_clicks &= ~0b010;
        }

        mouse_state.contact = true;

        mouse_state.moving = (e.type == 'mousemove' && e.movementX * e.movementX + e.movementY * e.movementY >= 5) ||
                             (mouse_state.moving && skip_clicks);

        if (mouse_state.moving) {
            skip_clicks |= e.buttons;
        } else {
            skip_clicks = 0;
        }

        if (e.deltaY) {
            mouse_state.wheel += Util.clamp(e.deltaY, -1, 1);

            e.preventDefault();
            e.stopPropagation();
        }
    }

    function handleTouchEvent(e) {
        canvas.focus();
        self.busy();

        let rect = canvas.getBoundingClientRect();

        // Prevent refresh when swipping up and other stuff
        if (e.type == 'touchmove') {
            e.preventDefault();
            e.stopPropagation();
        }

        if (e.type == 'touchstart' || e.type == 'touchmove') {
            if (e.type == 'touchstart')
                touch_digits = e.touches.length;
            if (e.touches.length != touch_digits)
                return;

            if (touch_digits == 1) {
                mouse_state.x = e.touches[0].pageX - rect.left;
                mouse_state.y = e.touches[0].pageY - rect.top;

                if (!mouse_state.left)
                    mouse_state.left = 1;
            } else if (touch_digits == 2) {
                let p1 = { x: e.touches[0].pageX - rect.left, y: e.touches[0].pageY - rect.top };
                let p2 = { x: e.touches[1].pageX - rect.left, y: e.touches[1].pageY - rect.top };

                mouse_state.x = (p1.x + p2.x) / 2;
                mouse_state.y = (p1.y + p2.y) / 2;

                let new_distance = computeDistance(p1, p2);

                mouse_state.left = 0;

                // Give some time for stabilisation :)
                if (update_counter - touch_start.counter >= 6) {
                    if (new_distance / touch_distance >= 1.2) {
                        mouse_state.wheel--;
                        touch_distance = new_distance;
                    } else if (new_distance / touch_distance <= 0.8) {
                        mouse_state.wheel++;
                        touch_distance = new_distance;
                    }
                } else {
                    touch_distance = new_distance;
                }
            }

            mouse_state.contact = true;

            if (e.type == 'touchmove') {
                skip_clicks = 1;
                mouse_state.moving = true;
            } else {
                touch_start = {
                    x: mouse_state.x,
                    y: mouse_state.y,
                    counter: update_counter
                };

                mouse_state.moving = false;
            }
        } else if (e.type == 'touchend') {
            if (touch_start == null)
                return;

            mouse_state.contact = false;

            if (touch_digits == 1) {
                mouse_state.left = skip_clicks ? 0 : -1;
                mouse_state.moving = false;
            }
            skip_clicks = 0;

            touch_start = null;
            touch_distance = null;
        }
    }

    function computeDistance(p1, p2) {
        let dx = p1.x - p2.x;
        let dy = p1.y - p2.y;

        return Math.sqrt(dx * dx + dy * dy);
    }

    // ------------------------------------------------------------------------
    // Main loop
    // ------------------------------------------------------------------------

    function loop(timestamp) {
        if (!run)
            return;

        syncSize();

        let delay = timestamp - prev_timestamp;
        prev_timestamp = timestamp;

        // Big delay probably means tab switch
        if (delay >= 167) {
            window.requestAnimationFrame(loop);
            return;
        }

        frame_time = measurePerf(frame_times, delay);

        // Keep physics at stable frequency
        updates += Math.round(delay / (1000 / update_frequency));

        update_time = measurePerf(update_times, () => {
            while (updates >= 0) {
                updates--;

                Object.assign(mouse_mirror, mouse_state);
                cursor = 'default';

                handle_update();
                update_counter++;

                Object.assign(mouse_state, mouse_mirror);

                for (let key in pressed_keys) {
                    if (pressed_keys[key])
                        pressed_keys[key]++;
                }
                if (mouse_state.left)
                    mouse_state.left++;
                if (mouse_state.middle)
                    mouse_state.middle++;
                if (mouse_state.right)
                    mouse_state.right++;
                mouse_state.wheel = 0;

                if (audio != null) {
                    for (let sfx of old_sources.values()) {
                        sfx.gain.gain.linearRampToValueAtTime(0, audio.currentTime + 0.5);
                        setTimeout(() => sfx.src.stop(), 2000);
                    }
                    old_sources = new_sources;
                    new_sources = new Map;
                }

                ignore_new_cursor = false;
            }
        });

        draw_time = measurePerf(draw_times, () => {
            ctx.clearRect(0, 0, canvas.width, canvas.height);

            handle_draw();
            draw_counter++;
        });

        canvas.style.cursor = cursor;

        if (idle_timeout && !run_next) {
            if (performance.now() > run_until) {
                is_idle = true;
                return;
            }
        }
        run_next = false;

        window.requestAnimationFrame(loop);
    }

    function syncSize() {
        let rect = canvas.getBoundingClientRect();

        if (!rect.width && !rect.height)
            return;

        // Accessing canvas.width or canvas.height (even for reading) seems to trigger
        // a reset or something, and can cause flicker on Firefox Mobile.
        if (rect.width != prev_canvas_width) {
            canvas.width = rect.width;
            prev_canvas_width = rect.width;
        }
        if (rect.height != prev_canvas_height) {
            canvas.height = rect.height;
            prev_canvas_height = rect.height;
        }
    }

    function measurePerf(times, time) {
        if (typeof time == 'function') {
            let start = performance.now();

            time();
            time = performance.now() - start;
        }

        times.push(time);
        while (times.length > 30)
            times.shift();

        let mean = times.reduce((acc, time) => acc + time, 0) / times.length;
        return mean;
    }

    this.busy = function() {
        if (!idle_timeout)
            return;

        run_until = performance.now() + idle_timeout;
        run_next = true;

        if (is_idle) {
            is_idle = false;
            window.requestAnimationFrame(loop);
        }
    };

    // ------------------------------------------------------------------------
    // Tools
    // ------------------------------------------------------------------------

    this.align = function(pos, width, height, align) {
        let x = pos.x;
        let y = pos.y;

        if (align == 2 || align == 5 || align == 8) {
            x -= width / 2;
        } else if (align == 3 || align == 6 || align == 9) {
            x -= width;
        }
        if (align == 1 || align == 2 || align == 3) {
            y -= height;
        } else if (align == 4 || align == 5 || align == 6) {
            y -= height / 2;
        }

        return { x: x, y: y };
    };

    this.text = function(x, y, text, options = {}) {
        let mat = ctx.getTransform();

        ctx.save();

        // Unfortunately we have to go back to leave screen space to draw text with
        // correct size, so transform coordinates manually!
        ctx.resetTransform();
        x = x * mat.a + y * mat.c + mat.e;
        y = y * mat.b + y * mat.d + mat.f;

        let padding = (options.padding != null) ? options.padding : 0;

        let width = null;
        let height = null;

        if (options.width != null) {
            width = options.width + padding * 2;
        } else if (typeof text == 'object') {
            width = 22 + padding * 2;
        } else {
            width = ctx.measureText(text).width + padding * 2;
        }
        if (options.height != null) {
            height = options.height;
        } else if (typeof text == 'object') {
            height = 38;
        } else {
            height = ctx.measureText(text).actualBoundingBoxAscent;
        }

        let align = null;

        if (options.align != null) {
            align = options.align;
        } else if (x < 0) {
            x += canvas.width;
            if (y < 0) {
                y += canvas.height;
                align = 3;
            } else {
                align = 9;
            }
        } else {
            if (y < 0) {
                y += canvas.height;
                align = 1;
            } else {
                align = 7;
            }
        }

        let pos = { x: x, y: y };
        let aligned = self.align(pos, width, height, align);

        if (options.background != null) {
            ctx.fillStyle = options.background;
            ctx.fillRect(aligned.x - 4, aligned.y - 4, width + 8, height + 9);
        }

        ctx.textAlign = 'left';
        ctx.fillStyle = (options.color != null) ? options.color : 'black';
        ctx.fillText(text, aligned.x + padding, aligned.y + height);

        ctx.restore();
    };

    // ------------------------------------------------------------------------
    // Sound
    // ------------------------------------------------------------------------

    this.playSound = function(asset, options = {}) {
        if (audio == null)
            audio = new AudioContext;

        let sfx = old_sources.get(asset) || new_sources.get(asset);

        if (options.loop == null)
            options.loop = false;
        if (options.start == null)
            options.start = true;

        if (sfx == null && options.start) {
            sfx = {
                src: audio.createBufferSource(),
                gain: audio.createGain()
            };

            sfx.gain.connect(audio.destination);
            sfx.src.buffer = asset;
            sfx.src.loop = options.loop;
            sfx.src.connect(sfx.gain);

            sfx.gain.gain.setValueAtTime(0, audio.currentTime);
            sfx.gain.gain.linearRampToValueAtTime(1, audio.currentTime + 0.2);

            sfx.src.start();
        }

        old_sources.delete(asset);
        if (sfx != null)
            new_sources.set(asset, sfx);
    };

    this.play = function(asset) {
        self.playSound(asset, { loop: true });
    };

    this.playOnce = function(asset, start = true) {
        self.playSound(asset, { loop: false, start: start });
    };
}

async function loadTexture(url) {
    let texture = await Net.loadImage(url);
    return texture;
}

async function loadSound(url) {
    if (audio == null)
        audio = new AudioContext;

    let response = await fetch(url);
    let buf = await response.arrayBuffer();
    let sound = await audio.decodeAudioData(buf);

    return sound;
}

export {
    AppRunner,

    loadTexture,
    loadSound
}
