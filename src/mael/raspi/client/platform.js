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
// along with this program. If not, see <https://www.gnu.org/licenses/>.

let canvas;
let ctx;
let audio;

let prev_timestamp = 0;

let updates = 0;
let update_speed = 1;

let update_counter = 0;
let draw_counter = 0;

let frame_times = [];
let frame_time = 0;
let update_times = [];
let update_time = 0;
let draw_times = [];
let draw_time = 0;

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
    p: { x: 0, y: 0 },
    left: 0,
    middle: 0,
    right: 0
};

let buttons = [];
let cursor = 'default';

let prev_widgets = new Map;
let new_widgets = new Map;

let old_sources = new Map;
let new_sources = new Map;

let log_entries = [];

// ------------------------------------------------------------------------
// Init
// ------------------------------------------------------------------------

async function checkBrowser() {
    // Test WebP support
    {
        let urls = [
            'data:image/webp;base64,UklGRjIAAABXRUJQVlA4ICYAAACyAgCdASoCAAEALmk0mk0iIiIiIgBoSygABc6zbAAA/v56QAAAAA==',
            'data:image/webp;base64,UklGRh4AAABXRUJQVlA4TBEAAAAvAQAAAAfQ//73v/+BiOh/AAA='
        ];

        for (let url of urls) {
            let img = new Image();

            await new Promise((resolve, reject) => {
                img.onload = () => resolve();
                img.onerror = () => reject(new Error('Browser does not support WebP image format'));
                img.src = url;
            });
        }
    }

    // That's it for now!
}

async function start() {
    log.pushHandler(notifyHandler);
    log.defaultTimeout = 6000;

    registerSW();

    canvas = document.querySelector('#game');
    ctx = canvas.getContext('2d');
    audio = new AudioContext;

    // Setup event handlers
    document.addEventListener('keydown', handleKeyEvent);
    document.addEventListener('keyup', handleKeyEvent);
    document.addEventListener('mousemove', handleMouseEvent);
    document.addEventListener('mousedown', handleMouseEvent);
    document.addEventListener('mouseup', handleMouseEvent);
    document.addEventListener('wheel', handleMouseEvent);

    await init();

    window.requestAnimationFrame(loop);
}

function notifyHandler(action, entry) {
    if (entry.type !== 'debug') {
        switch (action) {
            case 'open': { log_entries.unshift(entry); } break;
            case 'close': { log_entries = log_entries.filter(it => it !== entry); } break;
        }
    }

    log.defaultHandler(action, entry);
}

async function registerSW() {
    if (navigator.serviceWorker) {
        if (ENV.pwa) {
            let registration = await navigator.serviceWorker.register('sw.pk.js');
            let progress = new log.Entry;

            if (registration.waiting) {
                progress.error('Fermez tous les onglets pour terminer la mise à jour puis rafraichissez la page');
            } else {
                registration.addEventListener('updatefound', () => {
                    if (registration.active) {
                        progress.progress('Mise à jour en cours, veuillez patienter');

                        registration.installing.addEventListener('statechange', e => {
                            if (e.target.state === 'installed') {
                                progress.success('Mise à jour effectuée, l\'application va redémarrer');
                                setTimeout(() => document.location.reload(), 3000);
                            }
                        });
                    }
                });
            }
        } else {
            let registration = await navigator.serviceWorker.getRegistration();
            let progress = new log.Entry;

            if (registration != null) {
                progress.progress('Nettoyage de l\'instance en cache, veuillez patienter');

                await registration.unregister();

                progress.success('Nettoyage effectué, l\'application va redémarrer');
                setTimeout(() => document.location.reload(), 3000);
            }
        }
    }
}

async function loadTexture(url) {
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
}

async function loadSound(url) {
    let response = await fetch(url);

    let buf = await response.arrayBuffer();
    let sound = await audio.decodeAudioData(buf);

    return sound;
}

// ------------------------------------------------------------------------
// Event handling
// ------------------------------------------------------------------------

function handleKeyEvent(e) {
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
    let rect = canvas.getBoundingClientRect();

    mouse_state.p.x = e.clientX - rect.left;
    mouse_state.p.y = e.clientY - rect.top;

    if ((e.buttons & 0b001) && !mouse_state.left) {
        mouse_state.left = 1;
    } else if (!(e.buttons & 0b001) && mouse_state.left)  {
        mouse_state.left = -1;
    }
    if ((e.buttons & 0b100) && !mouse_state.middle) {
        mouse_state.middle = 1;
    } else if (!(e.buttons & 0b100) && mouse_state.middle)  {
        mouse_state.middle = -1;
    }
    if ((e.buttons & 0b010) && !mouse_state.right) {
        mouse_state.right = 1;
    } else if (!(e.buttons & 0b010) && mouse_state.right)  {
        mouse_state.right = -1;
    }

    if (e.deltaMode != null) {
        switch (e.deltaMode) {
            case WheelEvent.DOM_DELTA_PIXEL: { mouse_state.wheel += Math.round(e.deltaY / 120); } break;
            case WheelEvent.DOM_DELTA_LINE: { mouse_state.wheel += e.deltaY; } break;
            case WheelEvent.DOM_DELTA_PAGE: { mouse_state.wheel += e.deltaY; } break;
        }
    }
}

// ------------------------------------------------------------------------
// Main loop
// ------------------------------------------------------------------------

function loop(timestamp) {
    // Sync canvas dimensions
    {
        let rect = canvas.getBoundingClientRect();

        canvas.width = rect.width;
        canvas.height = rect.height;
    }

    let delay = timestamp - prev_timestamp;
    prev_timestamp = timestamp;

    // Big delay probably means tab switch
    if (delay >= 167) {
        window.requestAnimationFrame(loop);
        return;
    }

    frame_time = measurePerf(frame_times, delay);

    // Keep physics at 600 Hz...
    updates += update_speed * Math.round(delay / (1000 / 600));

    // ... unless it takes too long, in which case game slows down
    if (draw_counter % 8 == 0) {
        let total = update_time + draw_time;

        if (total > 16) {
            update_speed = clamp(update_speed - 0.02, 0.125, 1);
        } else if (total < 13) {
            update_speed = clamp(update_speed + 0.02, 0.125, 1);
        }
    }

    update_time = measurePerf(update_times, () => {
        while (updates >= 0) {
            updates--;

            updateUI();
            update();
            update_counter++;

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
        }
    });

    draw_time = measurePerf(draw_times, () => {
        draw();
        drawUI();
        draw_counter++;
    });

    switch (cursor) {
        case 'default': { document.body.style.cursor = 'default'; } break;
        case 'pointer': { document.body.style.cursor = 'pointer'; } break;
        case 'crosshair': { document.body.style.cursor = 'url("assets/ui/crosshair.png") 8 8, crosshair'; } break;
    }

    window.requestAnimationFrame(loop);
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
                   mouse_state.p.x >= btn.p.x &&
                   mouse_state.p.x <= btn.p.x + btn.width &&
                   mouse_state.p.y >= btn.p.y &&
                   mouse_state.p.y <= btn.p.y + btn.height;
        apply_over &= !btn.over;
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
    // Draw log
    for (let i = 0, y = canvas.height - 12; i < log_entries.length; i++) {
        let entry = log_entries[i];

        let msg = (entry.msg instanceof Error) ? entry.msg.message : entry.msg;
        let color = (entry.type === 'error') ? '#ff0000' : 'white';
        let icon = (entry.type === 'error') ? assets.ui.error : assets.ui.info;

        y -= label(canvas.width / 2, y, msg, { align: 2, color: color, icon: icon }).height + 8;
    }

    // Draw buttons
    for (let btn of buttons) {
        ctx.save();

        let gradient = ctx.createLinearGradient(0, btn.p.y, 0, btn.p.y + btn.height);

        if (btn.highlight && btn.over)
            cursor = 'pointer';
        if (btn.active) {
            gradient.addColorStop(0, '#8c4515');
            gradient.addColorStop(0.5, '#d76f28');
            gradient.addColorStop(1, '#8c4515');
        } else if (btn.pressed || btn.busy) {
            gradient.addColorStop(0, '#bbbbbb');
            gradient.addColorStop(0.5, '#cccccc');
            gradient.addColorStop(1, '#bbbbbb');
        } else if (btn.highlight && btn.over) {
            gradient.addColorStop(0, '#cccccc');
            gradient.addColorStop(0.5, '#dddddd');
            gradient.addColorStop(1, '#cccccc');
        } else {
            gradient.addColorStop(0, '#dddddd');
            gradient.addColorStop(0.5, '#eeeeee');
            gradient.addColorStop(1, '#dddddd');
        }

        ctx.fillStyle = gradient;
        ctx.beginPath();
        ctx.moveTo(btn.p.x + btn.width / 2, btn.p.y);
        ctx.arcTo(btn.p.x + btn.width, btn.p.y, btn.p.x + btn.width, btn.p.y + btn.height / 2, (btn.corners & 0b0100) ? 14 : 0);
        ctx.arcTo(btn.p.x + btn.width, btn.p.y + btn.height, btn.p.x + btn.width / 2, btn.p.y + btn.height, (btn.corners & 0b0010) ? 14 : 0);
        ctx.arcTo(btn.p.x, btn.p.y + btn.height, btn.p.x, btn.p.y + btn.height / 2, (btn.corners & 0b0001) ? 14 : 0);
        ctx.arcTo(btn.p.x, btn.p.y, btn.p.x + btn.width / 2, btn.p.y, (btn.corners & 0b1000) ? 14 : 0);
        ctx.closePath();
        ctx.fill();

        if (btn.busy) {
            ctx.save();
            ctx.translate(btn.p.x + btn.width / 2, btn.p.y + btn.height / 2);
            ctx.rotate(draw_counter / 8);
            ctx.globalCompositeOperation = 'multiply';
            ctx.drawImage(assets.ui.busy, -10, -12, 24, 24);
            ctx.globalCompositeOperation = 'source-over';
            ctx.drawImage(assets.ui.busy, -10, -10, 20, 20);
            ctx.restore();
        } else if (typeof btn.text == 'object') {
            ctx.globalCompositeOperation = 'multiply';
            ctx.drawImage(btn.text, btn.p.x + btn.padding - 2, btn.p.y + 7, 24, 24);
            ctx.globalCompositeOperation = 'source-over';
            ctx.drawImage(btn.text, btn.p.x + btn.padding, btn.p.y + 9, 20, 20);
        } else {
            if (btn.icon != null) {
                ctx.globalCompositeOperation = 'multiply';
                ctx.drawImage(btn.icon, btn.p.x + btn.padding - 2, btn.p.y + 7, 24, 24);
                ctx.globalCompositeOperation = 'source-over';
                ctx.drawImage(btn.icon, btn.p.x + btn.padding, btn.p.y + 9, 20, 20);

                ctx.fillStyle = btn.color;
                ctx.fillText(btn.text, btn.p.x + 50, btn.p.y + 26);
            } else {
                ctx.fillStyle = btn.color;
                ctx.fillText(btn.text, btn.p.x + 20, btn.p.y + 26);
            }
        }

        ctx.restore();
    }
}

// ------------------------------------------------------------------------
// Widgets
// ------------------------------------------------------------------------

function button(x, y, text, options = {}, func = null) {
    let id = options.id || text;

    let btn = {
        id: id,

        p: { x: 0, y: 0 },
        width: 0,
        height: 0,
        text: text,
        icon: options.icon,
        color: options.color || 'black',
        padding: (options.padding != null) ? options.padding : 20,
        corners: (options.corners != null) ? options.corners : 0b1111,
        active: !!options.active,
        highlight: (options.highlight != null) ? options.highlight : true,
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
        ctx.font = '20px Open Sans';
        btn.width = ctx.measureText(text).width + btn.padding * 2;
        btn.width = Math.round(btn.width);
        if (btn.icon != null)
            btn.width += 40;
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
        btn.p.x = x - btn.width / 2;
    } else if (align == 3 || align == 6 || align == 9) {
        btn.p.x = x - btn.width;
    } else {
        btn.p.x = x;
    }
    if (align == 1 || align == 2 || align == 3) {
        btn.p.y = y - btn.height;
    } else if (align == 4 || align == 5 || align == 6) {
        btn.p.y = y - btn.height / 2;
    } else {
        btn.p.y = y;
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
                case 'left': { sub = button(btn.p.x - 30, btn.p.y, assets.ui.left,
                                            { id: id + '+', padding: 5, corners: 0b1001, active: btn.active }); } break;
                case 'right': { sub = button(btn.p.x + btn.width - 5, btn.p.y, assets.ui.right,
                                             { id: id + '+', padding: 5, corners: 0b0110, active: btn.active }); } break;
            }

            btn.over |= sub.over;
            btn.expand |= sub.over;
        }
    }

    buttons.push(btn);
    new_widgets.set(id, btn);

    return btn;
}

function label(x, y, text, options = {}) {
    options = Object.assign({}, options);
    options.highlight = false;
    if (options.corners == null)
        options.corners = 0;

    let btn = button(x, y, text, options);
    return btn;
}

// ------------------------------------------------------------------------
// Sound
// ------------------------------------------------------------------------

function playSound(asset, loop) {
    let sfx = old_sources.get(asset) || new_sources.get(asset);

    if (sfx == null) {
        sfx = {
            src: audio.createBufferSource(),
            gain: audio.createGain()
        };

        sfx.gain.connect(audio.destination);
        sfx.src.buffer = asset;
        sfx.src.loop = loop;
        sfx.src.connect(sfx.gain);

        sfx.gain.gain.setValueAtTime(0, audio.currentTime);
        sfx.gain.gain.linearRampToValueAtTime(1, audio.currentTime + 0.2);

        sfx.src.start();
    }

    old_sources.delete(asset);
    new_sources.set(asset, sfx);
}

// ------------------------------------------------------------------------
// Utility
// ------------------------------------------------------------------------

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

function normalize(x, y) {
    let length = Math.sqrt(x * x + y * y);

    x /= length;
    y /= length;

    return { x: x, y: y };
}

function dot2line(p, p1, p2, length) {
    let dot = (((p.x - p1.x) * (p2.x - p1.x)) + ((p.y - p1.y) * (p2.y - p1.y))) / (length * length);

    let col = {
        x: p1.x + (dot * (p2.x - p1.x)),
        y: p1.y + (dot * (p2.y - p1.y))
    };
    let dist = distance(p, col);

    return [col, dist];
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

function saveFile(blob, filename) {
    let url = URL.createObjectURL(blob);

    let a = document.createElement('a');
    a.download = filename;
    a.href = url;

    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);

    if (URL.revokeObjectURL)
        setTimeout(() => URL.revokeObjectURL(url), 60000);
};

async function loadFile() {
    return new Promise((resolve, reject) => {
        let input = document.createElement('input');

        input.type = 'file';
        input.onchange = async (e) => {
            let file = e.target.files[0];

            if (file != null) {
                resolve(file);
            } else {
                reject();
            }
        };

        input.click();
    });
}

function mulberry32(a) {
    return () => {
        a |= 0; a = a + 0x6D2B79F5 | 0;
        let t = Math.imul(a ^ a >>> 15, 1 | a);
        t = t + Math.imul(t ^ t >>> 7, 61 | t) ^ t;
        return ((t ^ t >>> 14) >>> 0) / 4294967296;
    };
}
