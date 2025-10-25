// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

async function calibrate(video, draw, input) {
    let tracker = new Tracker(draw);
    tracker.calibrate(draw);

    return tracker;
}

function Tracker() {
    let self = this;

    let canvas = null;

    let timer = null;

    let handlers = [];
    let gaze = null;

    Object.defineProperties(this, {
        gaze: { get: () => gaze, enumerable: true }
    });

    this.calibrate = function(draw) {
        self.stop();

        canvas = draw.canvas;
        canvas.addEventListener('mousemove', handleMouseEvent);

        timer = setInterval(() => {
            if (gaze == null)
                return;

            for (let handler of handlers)
                handler(gaze);
        }, 10);
    };

    this.stop = function() {
        if (canvas == null)
            return;

        handlers.length = 0;

        clearInterval(timer);
        timer = null;

        canvas.removeEventListener('mousemove', handleMouseEvent);
        canvas = null;
    };

    this.on = function(func) { handlers.push(func); };

    function handleMouseEvent(e) {
        let rect = canvas.getBoundingClientRect();

        gaze = {
            timestamp: performance.now(),
            x: e.clientX - rect.left,
            y: e.clientY - rect.top
        };
    }
}

export { calibrate }
