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

const koffi = require('./build/koffi.node');
const crypto = require('crypto');
const fs = require('fs');
const os = require('os');
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
    id: 'unsigned  int', // Extra space is on purpose, leave it!
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

async function main() {
    try {
        await test();
    } catch (err) {
        console.error(err);
        process.exit(1);
    }
}

async function test() {
    let lib_filename = path.dirname(__filename) + '/build/raylib' + koffi.extension;
    let lib = koffi.load(lib_filename);

    const InitWindow = lib.func('InitWindow', 'void', ['int', 'int', 'str']);
    const SetTraceLogLevel = lib.func('SetTraceLogLevel', 'void', ['int']);
    const SetWindowState = lib.func('SetWindowState', 'void', ['uint']);
    const GenImageColor = lib.func('GenImageColor', Image, ['int', 'int', Color]);
    const GetFontDefault = lib.func('GetFontDefault', Font, []);
    const MeasureTextEx = lib.func('MeasureTextEx', Vector2, [Font, 'str', 'float', 'float']);
    const ImageDrawTextEx = lib.func('ImageDrawTextEx', 'void', [koffi.pointer(Image), Font, 'str', Vector2, 'float', 'float', Color]);
    const ExportImage = lib.func('ExportImage', 'bool', [Image, 'str']);

    // We need to call InitWindow before using anything else (such as fonts)
    SetTraceLogLevel(4); // Warnings
    SetWindowState(0x80); // Hidden
    InitWindow(640, 480, "Raylib Test");

    let img = GenImageColor(800, 600, { r: 0, g: 0, b: 0, a: 255 });
    let font = GetFontDefault();

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

    // In theory we could directly checksum the image data, but we can't do it easily right now
    // until koffi gives us something to transform a pointer to a buffer.
    let tmp_dir = fs.mkdtempSync(path.join(os.tmpdir(), 'raylib'));

    try {
        ExportImage(img, tmp_dir + '/hello.png');

        let sha256 = await new Promise((resolve, reject) => {
            try {
                let reader = fs.createReadStream(tmp_dir + '/hello.png');
                let sha = crypto.createHash('sha256');

                reader.on('error', reject);
                reader.on('data', buf => sha.update(buf));
                reader.on('end', () => {
                    let hash = sha.digest('hex');
                    resolve(hash);
                });
            } catch (err) {
                reject(err);
            }
        });
        let expected = '5ec23005377082582a14d261f5d07f51a6615b3bcdf5b91ada29aba65da22c5f';

        console.log('Computed checksum: ' + sha256);
        console.log('Expected: ' + expected);
        console.log('');

        if (sha256 == expected) {
            console.log('Success!');
        } else {
            throw new Error('Image mismatch');
        }
    } finally {
        if (fs.rmSync != null) {
            fs.rmSync(tmp_dir, { recursive: true });
        } else {
            fs.rmdirSync(tmp_dir, { recursive: true });
        }
    }
}
