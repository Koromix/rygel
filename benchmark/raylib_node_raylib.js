#!/usr/bin/env node

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

const r = require('raylib');

main();

function main() {
    let iterations = 360000;

    if (process.argv.length >= 3) {
        iterations = parseInt(process.argv[2], 10);
        if (Number.isNaN(iterations))
            throw new Error('Not a valid number');
        if (iterations < 1)
            throw new Error('Value must be positive');
    }

    // We need to call InitWindow before using anything else (such as fonts)
    r.SetTraceLogLevel(4); // Warnings
    r.SetWindowState(0x80); // Hidden
    r.InitWindow(640, 480, "Raylib Test");

    let img = r.GenImageColor(800, 600, { r: 0, g: 0, b: 0, a: 255 });
    let font = r.GetFontDefault();

    let start = performance.now();

    for (let i = 0; i < iterations; i += 3600) {
        r.ImageClearBackground(img, { r: 0, g: 0, b: 0, a: 255 });

        for (let j = 0; j < 3600; j++) {
            let text = 'Hello World!';
            let text_width = r.MeasureTextEx(font, text, 10, 1).x;

            let angle = (j * 7) * Math.PI / 180;
            let color = {
                r: 127.5 + 127.5 * Math.sin(angle),
                g: 127.5 + 127.5 * Math.sin(angle + Math.PI / 2),
                b: 127.5 + 127.5 * Math.sin(angle + Math.PI),
                a: 255
            };
            let pos = {
                x: (img.width / 2 - text_width / 2) + j * 0.1 * Math.cos(angle - Math.PI / 2),
                y: (img.height / 2 - 16) + j * 0.1 * Math.sin(angle - Math.PI / 2)
            };

            r.ImageDrawTextEx(img, font, text, pos, 10, 1, color);
        }
    }

    let time = performance.now() - start;
    console.log(JSON.stringify({ iterations: iterations, time: Math.round(time) }));
}
