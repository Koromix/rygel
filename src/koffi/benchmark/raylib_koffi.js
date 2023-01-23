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

const koffi = require('./build/koffi.node');

const Color = koffi.struct('Color', {
    r: 'uchar',
    g: 'uchar',
    b: 'uchar',
    a: 'uchar'
});

const Image = koffi.struct('Image', {
    data: koffi.pointer('void'),
    width: 'int',
    height: 'int',
    mipmaps: 'int',
    format: 'int'
});

const GlyphInfo = koffi.struct('GlyphInfo', {
    value: 'int',
    offsetX: 'int',
    offsetY: 'int',
    advanceX: 'int',
    image: Image
});

const Vector2 = koffi.struct('Vector2', {
    x: 'float',
    y: 'float'
});

const Rectangle = koffi.struct('Rectangle', {
    x: 'float',
    y: 'float',
    width: 'float',
    height: 'float'
});

const Texture = koffi.struct('Texture', {
    id: 'uint',
    width: 'int',
    height: 'int',
    mipmaps: 'int',
    format: 'int'
});

const Font = koffi.struct('Font', {
    baseSize: 'int',
    glyphCount: 'int',
    glyphPadding: 'int',
    texture: Texture,
    recs: koffi.pointer(Rectangle),
    glyphs: koffi.pointer(GlyphInfo)
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
    let lib = koffi.load(lib_filename);

    const InitWindow = lib.cdecl('InitWindow', 'void', ['int', 'int', 'str']);
    const SetTraceLogLevel = lib.cdecl('SetTraceLogLevel', 'void', ['int']);
    const SetWindowState = lib.cdecl('SetWindowState', 'void', ['uint']);
    const GenImageColor = lib.cdecl('GenImageColor', Image, ['int', 'int', Color]);
    const GetFontDefault = lib.cdecl('GetFontDefault', Font, []);
    const MeasureTextEx = lib.cdecl('MeasureTextEx', Vector2, [Font, 'str', 'float', 'float']);
    const ImageClearBackground = lib.cdecl('ImageClearBackground', 'void', [koffi.pointer(Image), Color]);
    const ImageDrawTextEx = lib.cdecl('ImageDrawTextEx', 'void', [koffi.pointer(Image), Font, 'str', Vector2, 'float', 'float', Color]);
    const ExportImage = lib.cdecl('ExportImage', 'bool', [Image, 'str']);

    // We need to call InitWindow before using anything else (such as fonts)
    SetTraceLogLevel(4); // Warnings
    SetWindowState(0x80); // Hidden
    InitWindow(640, 480, "Raylib Test");

    let img = GenImageColor(800, 600, { r: 0, g: 0, b: 0, a: 255 });
    let font = GetFontDefault();

    let start = performance.now();
    let iterations = 0;

    while (performance.now() - start < time) {
        ImageClearBackground(img, { r: 0, g: 0, b: 0, a: 255 });

        for (let i = 0; i < 3600; i++) {
            let text = 'Hello World!';
            let text_width = MeasureTextEx(font, text, 10, 1).x;

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

            ImageDrawTextEx(img, font, text, pos, 10, 1, color);
        }

        iterations += 3600;
    }

    time = performance.now() - start;
    console.log(JSON.stringify({ iterations: iterations, time: Math.round(time) }));
}
