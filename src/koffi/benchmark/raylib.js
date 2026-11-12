#!/usr/bin/env node
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

const pkg = require('./package.json');
const koffi = require('..');
const koffi2 = optional('koffi');
const r = optional('raylib');
const ctypes = optional('node-ctypes');
const ffi = optional('node:ffi');
const ffi_napi = optional('@napi-ffi/ffi-napi');
const ref = optional('@napi-ffi/ref-napi');
const struct = optional('@napi-ffi/ref-struct-di')?.(ref);
const { spawnSync } = require('node:child_process');
const path = require('node:path');
const { performance } = require('node:perf_hooks');

const ITERATIONS = 8;
const ITERATION_TIME = 250;

main();

function main() {
    let args = process.argv.slice(2);

    let tests = {
        'C++': time => runCxx(time),
        'Node-API': r ? time => runNapi(time) : undefined,
        'Koffi v3': time => runKoffi(koffi, time),
        'Koffi v2': koffi2 ? time => runKoffi(koffi2, time) : undefined,
        'ffi-napi': ffi_napi ? time => runFfiNapi(time) : undefined
    };

    let keys = Object.keys(tests).filter(key => {
        let [engine] = key.split(' ');

        if (tests[key] == null)
            return false;
        if (args.length && !args.includes(engine))
            return false;

        return true;
    });

    let perf = Object.fromEntries(keys.map(key => [key, { iterations: 0, time: 0 }]));

    for (let i = 0; i < ITERATIONS; i++) {
        for (let key of shuffle(keys)) {
            let func = tests[key];
            let result = func(ITERATION_TIME);

            // Ignore warm-up results
            if (!i)
                continue;

            perf[key].iterations += result.iterations;
            perf[key].time += result.time;
        }
    }

    console.log(JSON.stringify(perf, null, 4));
}

function runCxx(time) {
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

function runNapi(time) {
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

        for (let i = 0; i < 360; i++) {
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

        iterations += 360;
    }

    time = performance.now() - start;
    return { iterations: iterations, time: Math.round(time) };
}

function runKoffi(koffi, time) {
    koffi.reset();

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

        for (let i = 0; i < 360; i++) {
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

        iterations += 360;
    }

    time = performance.now() - start;
    return { iterations: iterations, time: Math.round(time) };
}

function runFfiNapi(time) {
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

    const r = ffi_napi.Library(lib_filename, {
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

        for (let i = 0; i < 360; i++) {
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

        iterations += 360;
    }

    time = performance.now() - start;
    return { iterations: iterations, time: Math.round(time) };
}

function optional(mod) {
    if (process.isBun && mod.startsWith('@napi-ffi/'))
        return null;

    try {
        return require(mod);
    } catch (err) {
        return null;
    }
}

function shuffle(array) {
    let copy = array.slice();
    let shuffled = [];

    while (copy.length) {
        let idx = randInt(0, copy.length);

        shuffled.push(copy[idx]);

        copy[idx] = copy[copy.length - 1];
        copy.length--;
    }

    return shuffled;
}

function randInt(min, max) {
    min = Math.ceil(min);
    max = Math.floor(max);

    let rnd = Math.floor(Math.random() * (max - min)) + min;
    return rnd;
}
