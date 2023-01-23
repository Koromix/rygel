#!/usr/bin/env node

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program. If not, see https://www.gnu.org/licenses/.

const ref = require('ref-napi');
const ffi = require('ffi-napi');
const struct = require('ref-struct-di')(ref);
const koffi = require('./build/koffi.node');

const Color = struct({
    r: 'uchar',
    g: 'uchar',
    b: 'uchar',
    a: 'uchar'
});

const Image = struct({
    data: ref.refType(ref.types.void),
    width: 'int',
    height: 'int',
    mipmaps: 'int',
    format: 'int'
});

const GlyphInfo = struct({
    value: 'int',
    offsetX: 'int',
    offsetY: 'int',
    advanceX: 'int',
    image: Image
});

const Vector2 = struct({
    x: 'float',
    y: 'float'
});

const Vector3 = struct({
    x: 'float',
    y: 'float',
    z: 'float'
});

const Vector4 = struct({
    x: 'float',
    y: 'float',
    z: 'float',
    w: 'float'
});

const Rectangle = struct({
    x: 'float',
    y: 'float',
    width: 'float',
    height: 'float'
});

const Texture = struct({
    id: 'uint',
    width: 'int',
    height: 'int',
    mipmaps: 'int',
    format: 'int'
});

const Font = struct({
    baseSize: 'int',
    glyphCount: 'int',
    glyphPadding: 'int',
    texture: Texture,
    recs: ref.refType(Rectangle),
    glyphs: ref.refType(GlyphInfo)
});

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

    let lib_filename = __dirname + '/build/raylib' + koffi.extension;

    const r = ffi.Library(lib_filename, {
        InitWindow: ['void', ['int', 'int', 'string']],
        SetTraceLogLevel: ['void', ['int']],
        SetWindowState: ['void', ['uint']],
        GenImageColor: [Image, ['int', 'int', Color]],
        GetFontDefault: [Font, []],
        MeasureTextEx: [Vector2, [Font, 'string', 'float', 'float']],
        ImageClearBackground: ['void', [ref.refType(Image), Color]],
        ImageDrawTextEx: ['void', [ref.refType(Image), Font, 'string', Vector2, 'float', 'float', Color]],
        ExportImage: ['bool', [Image, 'string']]
    });

    // We need to call InitWindow before using anything else (such as fonts)
    r.SetTraceLogLevel(4); // Warnings
    r.SetWindowState(0x80); // Hidden
    r.InitWindow(640, 480, "Raylib Test");

    let img = r.GenImageColor(800, 600, new Color({ r: 0, g: 0, b: 0, a: 255 }));
    let imgp = img.ref();
    let font = r.GetFontDefault();

    let start = performance.now();
    let iterations = 0;

    while (performance.now() - start < time) {
        r.ImageClearBackground(imgp, new Color({ r: 0, g: 0, b: 0, a: 255 }));

        for (let i = 0; i < 3600; i++) {
            let text = 'Hello World!';
            let text_width = r.MeasureTextEx(font, text, 10, 1).x;

            let angle = (i * 7) * Math.PI / 180;
            let color = new Color({
                r: 127.5 + 127.5 * Math.sin(angle),
                g: 127.5 + 127.5 * Math.sin(angle + Math.PI / 2),
                b: 127.5 + 127.5 * Math.sin(angle + Math.PI),
                a: 255
            });
            let pos = new Vector2({
                x: (img.width / 2 - text_width / 2) + i * 0.1 * Math.cos(angle - Math.PI / 2),
                y: (img.height / 2 - 16) + i * 0.1 * Math.sin(angle - Math.PI / 2)
            });

            r.ImageDrawTextEx(imgp, font, text, pos, 10, 1, color);
        }

        iterations += 3600;
    }

    time = performance.now() - start;
    console.log(JSON.stringify({ iterations: iterations, time: Math.round(time) }));
}
