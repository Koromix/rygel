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
