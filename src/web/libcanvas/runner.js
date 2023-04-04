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

import { util, net } from '../libjs/util.js';

function AppRunner(canvas) {
    let self = this;

    let ctx = canvas.getContext('2d');

    let prev_canvas_width = 0;
    let prev_canvas_height = 0;
    let prev_timestamp = 0;

    let idle_timeout = 0;
    let run_next = false;
    let run_until = 0;
    let is_idle = false;

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
        debug: 0,
        editor: 0,
        escape: 0,
        ctrl: 0,
        alt: 0
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

    let buttons = [];

    let cursor = 'default';
    let ignore_new_cursor = false;

    let prev_widgets = new Map;
    let new_widgets = new Map;

    let old_sources = new Map;
    let new_sources = new Map;

    Object.defineProperties(this,  {
        canvas: { value: canvas, writable: false, enumerable: true },
        ctx: { value: ctx, writable: false, enumerable: true },

        onUpdate: { get: () => handle_draw, set: func => { handle_update = func; }, enumerable: true },
        onDraw: { get: () => handle_draw, set: func => { handle_draw = func; }, enumerable: true },
        onContextMenu: { get: () => handle_draw, set: func => { handle_context_menu = func; }, enumerable: true },

        pressedKeys: { value: pressed_keys, writable: false, enumerable: true },
        mouseState: { value: mouse_state, writable: false, enumerable: true },

        idleTimeout: { get: () => idle_timeout, set: value => { idle_timeout = value; }, enumerable: true },

        updateCounter: { get: () => update_counter, enumerate: true },
        drawCounter: { get: () => draw_counter, enumerate: true },

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

    this.start = async function() {
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

        window.requestAnimationFrame(loop);
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

    this.loadTexture = async function(url) {
        let texture = await new Promise((resolve, reject) => {
            let img = new Image();

            img.src = url;
            img.onload = () => resolve(img);
            img.onerror = () => reject(new Error(`Failed to load texture '${url}'`));
        });

        // Fix latency spikes caused by image decoding
        if (typeof createImageBitmap != 'undefined')
            texture = await createImageBitmap(texture);

        return texture;
    };

    this.loadSound = async function(url) {
        let response = await net.fetch(url);

        let buf = await response.arrayBuffer();
        let sound = await audio.decodeAudioData(buf);

        return sound;
    };

    // ------------------------------------------------------------------------
    // Event handling
    // ------------------------------------------------------------------------

    function handleKeyEvent(e) {
        canvas.focus();
        self.busy();

        if (e.repeat)
            return;

        if (e.key == 'z' || e.keyCode == 38) {
            pressed_keys.up = 0 + (e.type === 'keydown');
        } else if (e.key == 's' || e.keyCode == 40) {
            pressed_keys.down = 0 + (e.type === 'keydown');
        } else if (e.key == 'q' || e.keyCode == 37) {
            pressed_keys.left = 0 + (e.type === 'keydown');
        } else if (e.key == 'd' || e.keyCode == 39) {
            pressed_keys.right = 0 + (e.type === 'keydown');
        } else if (e.keyCode == 9) { // Tab
            pressed_keys.debug = 0 + (e.type === 'keydown');
            e.preventDefault();
        } else if (e.key == ' ') {
            pressed_keys.editor = 0 + (e.type === 'keydown');
        } else if (e.keyCode == 27) {
            pressed_keys.escape = 0 + (e.type === 'keydown');
        } else if (e.keyCode == 17) {
            pressed_keys.ctrl = 0 + (e.type === 'keydown');
        } else if (e.keyCode == 18) {
            pressed_keys.alt = 0 + (e.type === 'keydown');
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

        if (e.type == 'mousemove') {
            skip_clicks |= e.buttons;
            mouse_state.moving = true;
        } else {
            mouse_state.moving = false;
        }

        if (e.deltaMode != null) {
            switch (e.deltaMode) {
                case WheelEvent.DOM_DELTA_PIXEL: {
                    let mult = util.clamp(e.deltaY, -1, 1);
                    mouse_state.wheel += mult * Math.ceil(mult * e.deltaY / 120);
                } break;
                case WheelEvent.DOM_DELTA_LINE:
                case WheelEvent.DOM_DELTA_PAGE: { mouse_state.wheel += util.clamp(e.deltaY, -1, 1); } break;
            }

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

                let new_distance = distance(p1, p2);

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

    // ------------------------------------------------------------------------
    // Main loop
    // ------------------------------------------------------------------------

    function loop(timestamp) {
        // Sync canvas dimensions
        {
            let rect = canvas.getBoundingClientRect();

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

        let delay = timestamp - prev_timestamp;
        prev_timestamp = timestamp;

        // Big delay probably means tab switch
        if (delay >= 167) {
            window.requestAnimationFrame(loop);
            return;
        }

        frame_time = measurePerf(frame_times, delay);

        // Keep physics at 120 Hz
        updates += Math.round(delay / (1000 / 120));

        update_time = measurePerf(update_times, () => {
            while (updates >= 0) {
                updates--;

                Object.assign(mouse_mirror, mouse_state);

                updateUI();
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

                for (let sfx of old_sources.values()) {
                    sfx.gain.gain.linearRampToValueAtTime(0, audio.currentTime + 0.5);
                    setTimeout(() => sfx.src.stop(), 2000);
                }
                old_sources = new_sources;
                new_sources = new Map;

                ignore_new_cursor = false;
            }
        });

        draw_time = measurePerf(draw_times, () => {
            ctx.clearRect(0, 0, canvas.width, canvas.height);

            handle_draw();
            drawUI();
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

    function updateUI() {
        let apply_over = true;

        for (let i = buttons.length - 1; i >= 0; i--) {
            let btn = buttons[i];

            if (btn.over) {
                if (btn.func != null)
                    btn.pressed = !!mouse_state.left;

                if (mouse_state.left == -1) {
                    mouse_state.left = 0;

                    if (btn.func != null)
                        runButtonAction(btn);
                }
            } else {
                btn.pressed = false;
                btn.expand = false;
            }

            btn.over = apply_over &&
                       mouse_state.x >= btn.x &&
                       mouse_state.x <= btn.x + btn.width &&
                       mouse_state.y >= btn.y &&
                       mouse_state.y <= btn.y + btn.height;
            apply_over &&= !btn.over;
        }

        prev_widgets = new_widgets;
        new_widgets = new Map;

        buttons = [];
        cursor = 'default';
    }

    function runButtonAction(btn) {
        let ret = btn.func(btn);

        if (ret instanceof Promise) {
            btn.busy = true;

            ret.finally(() => {
                btn = new_widgets.get(btn.id);
                if (btn != null)
                    btn.busy = false;
            });
        }
    }

    function drawUI() {
        for (let btn of buttons) {
            ctx.save();

            let gradient = ctx.createLinearGradient(0, btn.y, 0, btn.y + btn.height);

            if (btn.over)
                cursor = 'pointer';
            if (btn.active) {
                gradient.addColorStop(0, '#8c4515');
                gradient.addColorStop(0.5, '#d76f28');
                gradient.addColorStop(1, '#8c4515');
            } else if (btn.pressed || btn.busy) {
                gradient.addColorStop(0, '#3d0c5f');
                gradient.addColorStop(0.5, '#6b1da0');
                gradient.addColorStop(1, '#3d0c5f');
            } else if (btn.over) {
                gradient.addColorStop(0, '#5f198f');
                gradient.addColorStop(0.5, '#842ac0');
                gradient.addColorStop(1, '#5f198f');
            } else {
                gradient.addColorStop(0, '#7a22b5');
                gradient.addColorStop(0.5, '#ae3ff7');
                gradient.addColorStop(1, '#7a22b5');
            }

            ctx.fillStyle = gradient;
            ctx.beginPath();
            ctx.moveTo(btn.x + btn.width / 2, btn.y);
            ctx.arcTo(btn.x + btn.width, btn.y, btn.x + btn.width, btn.y + btn.height / 2, (btn.corners & 0b0100) ? 14 : 0);
            ctx.arcTo(btn.x + btn.width, btn.y + btn.height, btn.x + btn.width / 2, btn.y + btn.height, (btn.corners & 0b0010) ? 14 : 0);
            ctx.arcTo(btn.x, btn.y + btn.height, btn.x, btn.y + btn.height / 2, (btn.corners & 0b0001) ? 14 : 0);
            ctx.arcTo(btn.x, btn.y, btn.x + btn.width / 2, btn.y, (btn.corners & 0b1000) ? 14 : 0);
            ctx.closePath();
            ctx.fill();

            if (btn.busy) {
                ctx.save();
                ctx.translate(btn.x + btn.width / 2, btn.y + btn.height / 2);
                ctx.rotate(draw_counter / 8);
                ctx.globalCompositeOperation = 'multiply';
                ctx.drawImage(assets.ui.busy, -10, -12, 24, 24);
                ctx.globalCompositeOperation = 'source-over';
                ctx.drawImage(assets.ui.busy, -10, -10, 20, 20);
                ctx.restore();
            } else if (typeof btn.text == 'object') {
                ctx.globalCompositeOperation = 'multiply';
                ctx.drawImage(btn.text, btn.x + btn.padding - 2, btn.y + 7, 24, 24);
                ctx.globalCompositeOperation = 'source-over';
                ctx.drawImage(btn.text, btn.x + btn.padding, btn.y + 9, 20, 20);
            } else {
                ctx.fillStyle = 'white';
                ctx.fillText(btn.text, btn.x + 20, btn.y + 26);
            }

            ctx.restore();
        }
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
    // Widgets
    // ------------------------------------------------------------------------

    this.button = function(x, y, text, options, func = null) {
        let id = options.id || text;

        let btn = {
            id: id,

            x: 0,
            y: 0,
            width: 0,
            height: 0,

            text: text,
            padding: (options.padding != null) ? options.padding : 20,
            corners: (options.corners != null) ? options.corners : 0b1111,
            active: !!options.active,
            func: func,

            over: false,
            pressed: false,
            expand: false,
            busy: false
        };

        // Copy over interaction state
        {
            let prev_btn = prev_widgets.get(id);

            if (prev_btn != null) {
                btn.over = prev_btn.over;
                btn.pressed = prev_btn.pressed;
                btn.expand = prev_btn.expand;
                btn.busy = prev_btn.busy;
            }
        }

        if (options.width != null) {
            btn.width = options.width + btn.padding * 2;
        } else if (typeof text == 'object') {
            btn.width = 22 + btn.padding * 2;
        } else {
            ctx.font = '18px Open Sans';
            btn.width = ctx.measureText(text).width + btn.padding * 2;
            btn.width = Math.round(btn.width);
        }
        if (options.height == null)
            btn.height = 38;

        let align;
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

        if (align == 2 || align == 5 || align == 8) {
            btn.x = x - btn.width / 2;
        } else if (align == 3 || align == 6 || align == 9) {
            btn.x = x - btn.width;
        } else {
            btn.x = x;
        }
        if (align == 1 || align == 2 || align == 3) {
            btn.y = y - btn.height;
        } else if (align == 4 || align == 5 || align == 6) {
            btn.y = y - btn.height / 2;
        } else {
            btn.y = y;
        }

        if (options.more) {
            switch (options.more) {
                case 'left': { btn.corners &= 0b0110; } break;
                case 'right': { btn.corners &= 0b1001; } break;
                default: { throw new Error('Invalid more option value'); } break;
            }

            if (!btn.expand) {
                let sub;
                switch (options.more) {
                    case 'left': { sub = self.button(btn.x - 30, btn.y, assets.ui.left,
                                                     { id: id + '+', padding: 5, corners: 0b1001, active: btn.active }); } break;
                    case 'right': { sub = self.button(btn.x + btn.width - 5, btn.y, assets.ui.right,
                                                      { id: id + '+', padding: 5, corners: 0b0110, active: btn.active }); } break;
                }

                btn.over |= sub.over;
                btn.expand |= sub.over;
            }
        }

        buttons.push(btn);
        new_widgets.set(id, btn);

        return btn;
    };

    // ------------------------------------------------------------------------
    // Sound
    // ------------------------------------------------------------------------

    this.playSound = function(asset, options = {}) {
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
        playSound(asset, { loop: true });
    };

    this.playOnce = function(asset, start = true) {
        playSound(asset, { loop: false, start: start });
    };

    function distance(p1, p2) {
        let dx = p1.x - p2.x;
        let dy = p1.y - p2.y;

        return Math.sqrt(dx * dx + dy * dy);
    }
}

module.exports = {
    AppRunner
};
