// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
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

function DrawAPI(canvas, db, test) {
    let self = this;

    let ctx = canvas.getContext('2d');

    let prev_width = null;
    let prev_height = null;
    let prev_func = null;

    let sequence = 0;

    let last_gaze = null;
    let draw_gaze = false;

    let handlers = null;

    Object.defineProperties(this, {
        canvas: { get: () => canvas, enumerable: true },
        ctx: { get: () => ctx, enumerable: true },
        width: { get: () => canvas.width, enumerable: true },
        height: { get: () => canvas.height, enumerable: true }
    });

    new ResizeObserver(() => {
        if (prev_func != null)
            self.frame(prev_func);
    }).observe(canvas);

    this.setInput = function(input) {
        input.on('key', handleKey);
        input.on('mouse', handleMouse);
    };

    this.setTracker = function(tracker) {
        tracker.on(handleTrackerEvent);
    };

    this.test = async function(obj) {
        if (typeof obj?.setup != 'function')
            throw new Error('Missing test setup function');
        if (typeof obj?.frame != 'function')
            throw new Error('Missing test frame function');

        try {
            handlers = {};

            await self.frame(obj.frame);
            await obj.setup();

            while (handlers != null)
                await self.frame(obj.frame);
        } finally {
            handlers = null;
        }
    };

    this.repeat = async function(ms, func = () => {}) {
        let start = performance.now();

        do {
            await self.frame(func);
        } while (performance.now() - start < ms);
    };

    this.frame = async function(func) {
        let ret = await new Promise((resolve, reject) => {
            requestAnimationFrame(() => {
                syncCanvas();

                ctx.clearRect(0, 0, canvas.width, canvas.height);
                let ret = func(ctx);

                if (draw_gaze && last_gaze != null) {
                    ctx.strokeStyle = '#ff6600';
                    ctx.lineWidth = 3;

                    ctx.beginPath();
                    ctx.arc(last_gaze.x, last_gaze.y, 15, 0, 2 * Math.PI);
                    ctx.stroke();
                }

                prev_func = func;

                resolve(ret);
            });
        });

        return ret;
    };

    function syncCanvas() {
        let rect = canvas.getBoundingClientRect();

        if (!rect.width && !rect.height)
            return;

        let resized = false;

        // Accessing canvas.width or canvas.height (even for reading) seems to trigger
        // a reset or something, and can cause flicker on Firefox Mobile.
        if (rect.width != prev_width) {
            canvas.width = rect.width;
            prev_width = rect.width;

            resized = true;
        }
        if (rect.height != prev_height) {
            canvas.height = rect.height;
            prev_height = rect.height;

            resized = true;
        }

        if (resized)
            self.event('size', { width: canvas.width, height: canvas.height });
    }

    this.text = function(x, y, align, text) {
        let lines = text.split('\n').map(line => {
            let m = ctx.measureText(line || 'x');

            return {
                width: m.width,
                height: m.actualBoundingBoxAscent + m.actualBoundingBoxDescent,
                text: line
            };
        });
        let height = lines.reduce((acc, line, idx) => acc + line.height * (idx ? 1.5 : 1), 0);

        if (align == 1 || align == 2 || align == 3) {
            y -= height;
        } else if (align == 4 || align == 5 || align == 6) {
            y -= height / 2;
        }

        for (let i = 0; i < lines.length; i++) {
            let line = lines[i];

            y += line.height * (i ? 1.5 : 1);

            if (align == 2 || align == 5 || align == 8) {
                let x2 = x - line.width / 2;
                ctx.fillText(line.text, x2, y);
            } else if (align == 3 || align == 6 || align == 9) {
                let x2 = x - line.width;
                ctx.fillText(line.text, x2, y);
            } else {
                let x2 = x;
                ctx.fillText(line.text, x2, y);
            }
        }
    };

    this.on = function(key, func) {
        if (handlers == null)
            throw new Error('Cannot set handler outside test');

        handlers[key.toLowerCase()] = func;
    };

    this.event = function(type, data = null) {
        if (data != null)
            data = JSON.stringify(data);

        db.exec('INSERT INTO events (test, sequence, timestamp, type, data) VALUES (?, ?, ?, ?, ?)',
                test.id, ++sequence, performance.now(), type, data);
    };

    function handleKey(type, info) {
        self.event(type, info);

        if (type == 'keydown') {
            if (info.alt && info.key == 'o')
                draw_gaze = !draw_gaze;
        }

        if (handlers == null)
            return;
        if (type != 'keydown')
            return;

        let key = info.key.toLowerCase();
        let handler = handlers[key];

        if (handler != null) {
            let success = handler();

            if (success instanceof Promise) {
                success.then(value => {
                    if (!value)
                        handlers = null;
                });
            } else if (success) {
                handlers = null;
            }
        }
    }

    function handleMouse(type, info) {
        self.event(type, info);
    }

    function handleTrackerEvent(gaze) {
        last_gaze = gaze;
        self.event('gaze', gaze);
    }
}

function InputAPI(canvas) {
    let self = this;

    let mouse = {
        x: null,
        y: null,
        buttons: 0,
        wheel: null
    };

    let handlers = {
        'key': [],
        'mouse': []
    };

    this.start = function() {
        window.addEventListener('keydown', handleKeyEvent);
        window.addEventListener('keyup', handleKeyEvent);

        canvas.addEventListener('mousedown', handleMouseEvent);
        canvas.addEventListener('mousemove', handleMouseEvent);
        canvas.addEventListener('mouseup', handleMouseEvent);
        canvas.addEventListener('wheel', handleMouseEvent);
    };

    this.stop = function() {
        window.removeEventListener('keydown', handleKeyEvent);
        window.removeEventListener('keyup', handleKeyEvent);

        canvas.removeEventListener('mousedown', handleMouseEvent);
        canvas.removeEventListener('mousemove', handleMouseEvent);
        canvas.removeEventListener('mouseup', handleMouseEvent);
        canvas.removeEventListener('wheel', handleMouseEvent);

        for (let key in handlers)
            handlers[key].length = 0;
    };

    this.on = function(type, func) {
        if (!handlers.hasOwnProperty(type))
            throw new Error(`Unsupport input type '${type}'`);
        handlers[type].push(func);
    };

    function handleKeyEvent(e) {
        if (e.ctrlKey && e.key != 'Control')
            return;

        e.preventDefault();

        let info = {
            key: e.key,
            code: e.keyCode,
            location: e.location,
            alt: e.altKey,
            meta: e.metaKey,
            shift: e.shiftKey
        };

        for (let handler of handlers.key)
            handler(e.type, info);
    }

    function handleMouseEvent(e) {
        mouse.wheel = 0;

        switch (e.type) {
            case 'mousedown': { mouse.buttons |= 1 << e.button; } break;
            case 'mouseup': { mouse.buttons &= ~(1 << e.button); } break;
            case 'mousemove': {
                let rect = canvas.getBoundingClientRect();

                mouse.x = e.clientX - rect.left;
                mouse.y = e.clientY - rect.top;
            } break;
            case 'wheel': { mouse.wheel = e.deltaY; } break;
        }

        if (mouse.x == null)
            return;

        for (let handler of handlers.mouse)
            handler(e.type, mouse);
    }
}

export {
    DrawAPI,
    InputAPI
}
