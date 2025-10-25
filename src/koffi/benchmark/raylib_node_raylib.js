#!/usr/bin/env node
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

const r = require('raylib');
const { performance } = require('perf_hooks');

main();

function main() {
    let time = 5000;

    if (process.argv.length >= 3) {
        time = parseFloat(process.argv[2]) * 1000;
        if (Number.isNaN(time))
            throw new Error('Not a valid number');
        if (time < 0)
            throw new Error('Time must be positive');
    }

    // We need to call InitWindow before using anything else (such as fonts)
    r.SetTraceLogLevel(4); // Warnings
    r.SetWindowState(0x80); // Hidden
    r.InitWindow(800, 600, "Raylib Test");

    let img = r.GenImageColor(800, 600, { r: 0, g: 0, b: 0, a: 255 });
    let font = r.GetFontDefault();

    let start = performance.now();
    let iterations = 0;

    while (performance.now() - start < time) {
        r.ImageClearBackground(img, { r: 0, g: 0, b: 0, a: 255 });

        for (let i = 0; i < 3600; i++) {
            let text = 'Hello World!';
            let text_width = r.MeasureTextEx(font, text, 10, 1).x;

            let angle = (i * 7) * Math.PI / 180;
            let color = {
                r: 127.5 + 127.5 * Math.sin(angle),
                g: 127.5 + 127.5 * Math.sin(angle + Math.PI / 2),
                b: 127.5 + 127.5 * Math.sin(angle + Math.PI),
                a: 255
            };
            let pos = {
                x: (img.width / 2 - text_width / 2) + i * 0.1 * Math.cos(angle - Math.PI / 2),
                y: (img.height / 2 - 16) + i * 0.1 * Math.sin(angle - Math.PI / 2)
            };

            r.ImageDrawTextEx(img, font, text, pos, 10, 1, color);
        }

        iterations += 3600;
    }

    time = performance.now() - start;
    console.log(JSON.stringify({ iterations: iterations, time: Math.round(time) }));
}
