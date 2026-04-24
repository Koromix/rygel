#!/usr/bin/env node
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

const pkg = require('./package.json');
const r = require('raylib');
const koffi = require('..');
const { node_ffi, ref, struct } = (() => {
    if (process.platform == 'win32')
        return {};
    if (process.isBun)
        return {};

    const node_ffi = require('@napi-ffi/ffi-napi');
    const ref = require('@napi-ffi/ref-napi');
    const struct = require('@napi-ffi/ref-struct-di')(ref);

    return { node_ffi, ref, struct };
})();
const { spawnSync } = require('child_process');
const path = require('path');
const { performance } = require('perf_hooks');

const TIME = 8000;

main();

function main() {
    let args = process.argv.slice(2);

    let tests = {
        'cxx': time => run_cxx(time),
        'napi': time => run_napi(time),
        'koffi': time => run_koffi(time),
        'node-ffi-napi': node_ffi ? time => run_node_ffi(time) : undefined
    };

    let perf = Object.fromEntries(Object.keys(tests).map(key => {
        let [engine] = key.split(' ');
        let func = tests[key];

        if (func == null)
            return [key, undefined];
        if (args.length && !args.includes(engine))
            return [key, undefined];

        return [key, func(TIME)];
    }));

    console.log(JSON.stringify(perf, null, 4));
}

function run_cxx(time) {
    let filename = path.join(__dirname, pkg.cnoke.output, 'raylib_cc' + (process.platform == 'win32' ? '.exe' : ''));
    let proc = spawnSync(filename, [time]);

    if (proc.status == null)
        throw new Error(proc.error);
    if (proc.status != 0) {
        let output = proc.stderr?.toString?.('utf-8')?.trim?.() ||
                     proc.stdout?.toString?.('utf-8')?.trim?.() ||
                     'Unknown error';
        throw new Error(output);
    }

    return JSON.parse(proc.stdout);
}

function run_napi(time) {
    // We need to call InitWindow before using anything else (such as fonts)
    r.SetTraceLogLevel(5); // Errors
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
    return { iterations: iterations, time: Math.round(time) };
}

function run_koffi(time) {
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

    let lib_filename = path.join(__dirname, pkg.cnoke.output, 'raylib' + koffi.extension);
    let lib = koffi.load(lib_filename);

    const InitWindow = lib.func('InitWindow', 'void', ['int', 'int', 'str']);
    const SetTraceLogLevel = lib.func('SetTraceLogLevel', 'void', ['int']);
    const GenImageColor = lib.func('GenImageColor', Image, ['int', 'int', Color]);
    const GetFontDefault = lib.func('GetFontDefault', Font, []);
    const MeasureTextEx = lib.func('MeasureTextEx', Vector2, [Font, 'str', 'float', 'float']);
    const ImageClearBackground = lib.func('ImageClearBackground', 'void', [koffi.pointer(Image), Color]);
    const ImageDrawTextEx = lib.func('ImageDrawTextEx', 'void', [koffi.pointer(Image), Font, 'str', Vector2, 'float', 'float', Color]);
    const ExportImage = lib.func('ExportImage', 'bool', [Image, 'str']);

    // We need to call InitWindow before using anything else (such as fonts)
    SetTraceLogLevel(5); // Errors
    InitWindow(800, 600, "Raylib Test");

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
    return { iterations: iterations, time: Math.round(time) };
}

function run_node_ffi(time) {
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

    let lib_filename = path.join(__dirname, pkg.cnoke.output, 'raylib' + koffi.extension);

    const r = node_ffi.Library(lib_filename, {
        InitWindow: ['void', ['int', 'int', 'string']],
        SetTraceLogLevel: ['void', ['int']],
        GenImageColor: [Image, ['int', 'int', Color]],
        GetFontDefault: [Font, []],
        MeasureTextEx: [Vector2, [Font, 'string', 'float', 'float']],
        ImageClearBackground: ['void', [ref.refType(Image), Color]],
        ImageDrawTextEx: ['void', [ref.refType(Image), Font, 'string', Vector2, 'float', 'float', Color]],
        ExportImage: ['bool', [Image, 'string']]
    });

    // We need to call InitWindow before using anything else (such as fonts)
    r.SetTraceLogLevel(5); // Errors
    r.InitWindow(800, 600, "Raylib Test");

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
    return { iterations: iterations, time: Math.round(time) };
}
