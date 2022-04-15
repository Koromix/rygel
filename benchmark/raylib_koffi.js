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

const koffi = require('../build/koffi.node');
const path = require('path');

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
    if (process.argv.length < 3)
        throw new Error('Missing number of iterations');

    let iterations = parseInt(process.argv[2], 10);
    if (Number.isNaN(iterations))
        throw new Error('Not a valid number');
    if (iterations < 1)
        throw new Error('Value must be positive');
    console.log('Iterations:', iterations);

    let lib_filename = path.dirname(__filename) + '/../build/raylib' +
                       (process.platform == 'win32' ? '.dll' : '.so');
    let lib = koffi.load(lib_filename);

    const InitWindow = lib.cdecl('InitWindow', 'void', ['int', 'int', 'string']);
    const SetTraceLogLevel = lib.cdecl('SetTraceLogLevel', 'void', ['int']);
    const SetWindowState = lib.cdecl('SetWindowState', 'void', ['uint']);
    const GenImageColor = lib.cdecl('GenImageColor', Image, ['int', 'int', Color]);
    const GetFontDefault = lib.cdecl('GetFontDefault', Font, []);
    const MeasureTextEx = lib.cdecl('MeasureTextEx', Vector2, [Font, 'string', 'float', 'float']);
    const ImageClearBackground = lib.cdecl('ImageClearBackground', 'void', [koffi.pointer(Image), Color]);
    const ImageDrawTextEx = lib.cdecl('ImageDrawTextEx', 'void', [koffi.pointer(Image), Font, 'string', Vector2, 'float', 'float', Color]);
    const ExportImage = lib.cdecl('ExportImage', 'bool', [Image, 'string']);

    // We need to call InitWindow before using anything else (such as fonts)
    SetTraceLogLevel(4); // Warnings
    SetWindowState(0x80); // Hidden
    InitWindow(640, 480, "Raylib Test");

    let img = GenImageColor(800, 600, { r: 0, g: 0, b: 0, a: 255 });
    let font = GetFontDefault();

    for (let i = 0; i < iterations; i++) {
        ImageClearBackground(img, { r: 0, g: 0, b: 0, a: 255 });

        for (let j = 0; j < 360; j++) {
            let text = 'Hello World!';
            let text_width = MeasureTextEx(font, text, 10, 1).x;

            let angle = (j * 4) * Math.PI / 180;
            let color = {
                r: 127.5 + 127.5 * Math.sin(angle),
                g: 127.5 + 127.5 * Math.sin(angle + Math.PI / 2),
                b: 127.5 + 127.5 * Math.sin(angle + Math.PI),
                a: 255
            };
            let pos = {
                x: (img.width / 2 - text_width / 2) + j * Math.cos(angle - Math.PI / 2),
                y: (img.height / 2 - 16) + j * Math.sin(angle - Math.PI / 2)
            };

            ImageDrawTextEx(img, font, text, pos, 10, 1, color);
        }
    }
}
