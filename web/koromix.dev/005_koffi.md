<!-- Title: koromix.dev — Koffi
     Menu: Koffi
     Created: 2022-05-16 -->

#overview# Overview

Koffi is a fast and easy-to-use FFI module for [Node.js](https://nodejs.org/), with support for complex data types such as structs.

The following platforms are **officially supported and tested** at the moment:

- Windows x86 *(cdecl, stdcall, fastcall)*
- Windows x86_64
- Linux x86
- Linux x86_64
- Linux ARM32+VFP Little Endian
- Linux ARM64 Little Endian
- FreeBSD x86
- FreeBSD x86_64
- FreeBSD ARM64 Little Endian
- macOS x86_64
- macOS ARM64 (M1) Little Endian

The following platforms **may be supported** but are not tested:

- NetBSD x86_64
- NetBSD ARM64
- OpenBSD x86_64
- OpenBSD ARM64

This is still in development, bugs are to expected. More tests will come in the near future.

You can find more information about Koffi (installation, tests, etc.) [in the official repository](https://github.com/Koromix/luigi/tree/master/koffi).

#example# Example

This example illustrates how to use Koffi with a Raylib shared library:

    const koffi = require('koffi');

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

    // Fix the path to Raylib DLL if needed
    let lib = koffi.load('build/raylib' + koffi.extension);

    const InitWindow = lib.cdecl('InitWindow', 'void', ['int', 'int', 'string']);
    const SetTargetFPS = lib.cdecl('SetTargetFPS', 'void', ['int']);
    const GetScreenWidth = lib.cdecl('GetScreenWidth', 'int', []);
    const GetScreenHeight = lib.cdecl('GetScreenHeight', 'int', []);
    const ClearBackground = lib.cdecl('ClearBackground', 'void', [Color]);
    const BeginDrawing = lib.cdecl('BeginDrawing', 'void', []);
    const EndDrawing = lib.cdecl('EndDrawing', 'void', []);
    const WindowShouldClose = lib.cdecl('WindowShouldClose', 'bool', []);
    const GetFontDefault = lib.cdecl('GetFontDefault', Font, []);
    const MeasureTextEx = lib.cdecl('MeasureTextEx', Vector2, [Font, 'string', 'float', 'float']);
    const DrawTextEx = lib.cdecl('DrawTextEx', 'void', [Font, 'string', Vector2, 'float', 'float', Color]);

    800, 600, 'Test Raylib');
    60);

    let angle = 0;

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground({ r: 0, g: 0, b: 0, a: 255 }); // black

        let win_width = GetScreenWidth();
        let win_height = GetScreenHeight();

        let text = 'Hello World!';
        let text_width = MeasureTextEx(GetFontDefault(), text, 32, 1).x;

        let color = {
            r: 127.5 + 127.5 * Math.sin(angle),
            g: 127.5 + 127.5 * Math.sin(angle + Math.PI / 2),
            b: 127.5 + 127.5 * Math.sin(angle + Math.PI),
            a: 255
        };
        let pos = {
            x: (win_width / 2 - text_width / 2) + 120 * Math.cos(angle - Math.PI / 2),
            y: (win_height / 2 - 16) + 120 * Math.sin(angle - Math.PI / 2)
        };

        DrawTextEx(GetFontDefault(), text, pos, 32, 1, color);

        EndDrawing();

        angle += Math.PI / 180;
    }
