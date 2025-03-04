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

import { Util, Log, Net, LruMap } from './base.js';

function AppRunner(canvas) {
    let self = this;

    let ctx = canvas.getContext('2d');

    let prev_width = 0;
    let prev_height = 0;
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
        return: 0,
        pause: 0,

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

    let audio = null;
    let audio_started = false;

    let old_sources = new Map;
    let new_sources = new Map;

    Object.defineProperties(this,  {
        canvas: { value: canvas, writable: false, enumerable: true },
        ctx: { value: ctx, writable: false, enumerable: true },

        isTouch: { get: isTouchDevice, enumerable: true },
        isPortrait: { get: isPortrait, enumerable: true },

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

        canvas.addEventListener('mousemove', handleMouseEvent);
        canvas.addEventListener('mousedown', handleMouseEvent);
        canvas.addEventListener('mouseup', handleMouseEvent);
        canvas.addEventListener('wheel', handleMouseEvent);

        canvas.addEventListener('touchstart', handleTouchEvent);
        canvas.addEventListener('touchmove', handleTouchEvent);
        canvas.addEventListener('touchend', handleTouchEvent);

        window.requestAnimationFrame(loop);
    };

    this.stop = function() {
        // Please make a new canvas, because event handlers remain connected
        run = false;
    };

    this.resize = function(width, height) {
        if (width != prev_width) {
            canvas.width = width * window.devicePixelRatio;
            canvas.style.width = width + 'px';

            prev_width = width;
        }
        if (height != prev_height) {
            canvas.height = height * window.devicePixelRatio;
            canvas.style.height = height + 'px';

            prev_height = height;
        }

        // Reduce flickering after resize
        if (handle_draw != null)
            handle_draw();

        self.busy();
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

    function isPortrait() {
        let portrait = (canvas.height > canvas.width);
        return portrait;
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
        } else if (code == 13) {
            updateKey('return', e.type);
        } else if (code == 38) {
            updateKey('up', e.type);
        } else if (code == 40) {
            updateKey('down', e.type);
        } else if (code == 37) {
            updateKey('left', e.type);
        } else if (code == 39) {
            updateKey('right', e.type);
        } else if (code == 19) {
            updateKey('pause', e.type);
        }
    }

    function updateKey(key, type) {
        if (type == 'keydown') {
            if (pressed_keys[key] < 1)
                pressed_keys[key] = 1;
        } else if (type == 'keyup') {
            pressed_keys[key] = (pressed_keys[key] > 0) ? -1 : 0;
        }
    }

    function handleMouseEvent(e) {
        if (isTouchDevice())
            return;

        canvas.focus();
        self.busy();

        let rect = canvas.getBoundingClientRect();

        mouse_state.x = (e.clientX - rect.left) * window.devicePixelRatio;
        mouse_state.y = (e.clientY - rect.top) * window.devicePixelRatio;

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
        e.preventDefault();
        e.stopPropagation();

        if (e.type == 'touchstart' || e.type == 'touchmove') {
            if (e.type == 'touchstart')
                touch_digits = e.touches.length;
            if (e.touches.length != touch_digits)
                return;

            if (touch_digits == 1) {
                mouse_state.x = (e.touches[0].pageX - rect.left) * window.devicePixelRatio;
                mouse_state.y = (e.touches[0].pageY - rect.top) * window.devicePixelRatio;

                if (e.type == 'touchstart' && !mouse_state.left)
                    mouse_state.left = 1;
            } else if (touch_digits == 2) {
                let p1 = { x: e.touches[0].pageX - rect.left, y: e.touches[0].pageY - rect.top };
                let p2 = { x: e.touches[1].pageX - rect.left, y: e.touches[1].pageY - rect.top };

                mouse_state.x = (p1.x + p2.x) / 2 * window.devicePixelRatio;
                mouse_state.y = (p1.y + p2.y) / 2 * window.devicePixelRatio;

                let new_distance = computeDistance(p1, p2);

                mouse_state.left = 0;

                // Give some time for stabilisation :)
                if (draw_counter - touch_start.counter >= 6) {
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
                    counter: draw_counter
                };

                mouse_state.moving = false;
            }
        } else if (e.type == 'touchend') {
            if (touch_start == null)
                return;

            mouse_state.contact = false;

            if (draw_counter - touch_start.counter >= 20)
                skip_clicks = 1;
            if (touch_digits == 1) {
                if (mouse_state.left)
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
                        if (sfx.persist)
                            continue;

                        setTimeout(() => {
                            if (sfx.src.buffer != null)
                                sfx.src.stop();
                        }, 2000);
                        sfx.gain.gain.linearRampToValueAtTime(0, audio.currentTime + 0.2);

                        sfx.play = false;
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

    this.measure = function(text) {
        let measure = ctx.measureText(text);

        let m = {
            width: measure.width,
            height: measure.actualBoundingBoxAscent + measure.actualBoundingBoxDescent
        };

        return m;
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

        let m = null;
        let width = null;
        let height = null;

        if (options.width != null) {
            width = options.width + padding * 2;
        } else if (typeof text == 'object') {
            width = 22 + padding * 2;
        } else {
            if (m == null)
                m = self.measure(text);
            width = m.width + padding * 2;
        }
        if (options.height != null) {
            height = options.height;
        } else if (typeof text == 'object') {
            height = 38;
        } else {
            if (m == null)
                m = self.measureText(text);
            height = m.height;
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
        let fill = options.color ?? ctx.fillStyle ?? 'black';

        if (options.background != null) {
            ctx.fillStyle = options.background;
            ctx.fillRect(aligned.x - 4, aligned.y - 4, width + 8, height + 9);
        }

        ctx.textAlign = 'left';
        ctx.fillStyle = fill;
        ctx.fillText(text, aligned.x + padding, aligned.y + height);

        ctx.restore();

        let size = {
            width: width,
            height: height
        };

        return size;
    };

    // ------------------------------------------------------------------------
    // Sound
    // ------------------------------------------------------------------------

    function initAudio() {
        if (audio != null) {
            let running = (audio.state == 'running');

            // Force audio reset after suspend to make sure things work
            if (running || !audio_started) {
                audio_started ||= running;
                return;
            }

            audio.close();

            audio = null;
            audio_started = false;
        }

        audio = new AudioContext;

        old_sources.clear();
        new_sources.clear();
    }

    this.createTrack = function(cache) {
        let track = new AudioTrack(self, cache);
        return track;
    };

    function AudioTrack(runner, cache) {
        let self = this;

        let node = null;

        let volume = 1;
        let sound_buffers = new LruMap(cache);

        Object.defineProperties(this,  {
            volume: { get: () => volume, set: setVolume, enumerable: true }
        });

        function initTrack() {
            initAudio();

            if (node?.context != audio) {
                node = new GainNode(audio);
                node.gain.setValueAtTime(volume, audio.currentTime);
                node.connect(audio.destination);

                sound_buffers.clear();
            }
        }

        this.playOnce = function(asset, cache = true) {
            return self.playSound(asset, { persist: false, cache: cache });
        };

        this.playFull = function(asset, cache = true) {
            return self.playSound(asset, { persist: true, cache: cache });
        };

        this.playSound = function(asset, options = {}) {
            initTrack();

            let sfx = old_sources.get(asset) ?? new_sources.get(asset);

            if (options.cache == null)
                options.cache = true;
            if (options.loop == null)
                options.loop = false;
            if (options.persist == null)
                options.persist = true;

            if (sfx == null) {
                sfx = {
                    play: true,

                    src: audio.createBufferSource(),
                    gain: audio.createGain(),
                    persist: options.persist,

                    handle: {
                        ended: false
                    }
                };

                let buf = sound_buffers.get(asset);

                if (buf instanceof AudioBuffer) {
                    startSound(sfx, buf);
                } else {
                    if (buf == null) {
                        let copy = new ArrayBuffer(asset.byteLength);
                        new Uint8Array(copy).set(new Uint8Array(asset));

                        buf = audio.decodeAudioData(copy);
                    }

                    buf.then(decoded => {
                        if (options.cache)
                            sound_buffers.set(asset, decoded);
                        startSound(sfx, decoded);
                    });
                    buf.catch(err => {
                        sound_buffers.delete(asset);
                        console.error(err);
                    });

                    if (options.cache)
                        sound_buffers.set(asset, buf);
                }

                sfx.gain.connect(node);
                sfx.src.loop = options.loop;
                sfx.src.connect(sfx.gain);

                sfx.src.addEventListener('ended', () => {
                    sfx.handle.ended = true;
                    runner.busy();
                });
            }

            if (sfx != null)
                new_sources.set(asset, sfx);
            old_sources.delete(asset);

            return sfx.handle;
        };

        function startSound(sfx, buf) {
            if (!sfx.play)
                return;

            sfx.src.buffer = buf;
            sfx.gain.gain.setValueAtTime(1, audio.currentTime);

            sfx.src.start();
        }

        function setVolume(value) {
            if (value == volume)
                return;

            value = Util.clamp(value, 0, 1);
            volume = value;

            if (node != null)
                node.gain.setValueAtTime(volume, audio.currentTime + 0.2);
        }
    }
}

export { AppRunner }
