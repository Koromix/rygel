// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the “Software”), to deal in
// the Software without restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
// Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.

#include "src/core/base/base.hh"
#include "vendor/raylib/src/raylib.h"

namespace K {

// #define RENDER

int Main(int argc, char **argv)
{
    int time = 5000;

    if (argc >= 2) {
        if (!ParseInt(argv[1], &time))
            return 1;
    }

    // We need to call InitWindow before using anything else (such as fonts)
    SetTraceLogLevel(LOG_WARNING);
#if !defined(RENDER)
    SetWindowState(FLAG_WINDOW_HIDDEN);
#endif
    InitWindow(800, 600, "Raylib Test");

    Image img = GenImageColor(800, 600, Color { 0, 0, 0, 255 });
    Font font = GetFontDefault();

    int64_t start = GetMonotonicTime();
    int64_t iterations = 0;

    while (GetMonotonicTime() - start < time) {
        ImageClearBackground(&img, Color { 0, 0, 0, 255 });

        for (int i = 0; i < 3600; i++) {
            const char *text = "Hello World!";
            float text_width = MeasureTextEx(font, text, 10, 1).x;

            double angle = (i * 7) * PI / 180;
            Color color = {
                (unsigned char)(127.5 + 127.5 * sin(angle)),
                (unsigned char)(127.5 + 127.5 * sin(angle + PI / 2)),
                (unsigned char)(127.5 + 127.5 * sin(angle + PI)),
                255
            };
            Vector2 pos = {
                (float)((img.width / 2 - text_width / 2) + i * 0.1 * cos(angle - PI / 2)),
                (float)((img.height / 2 - 16) + i * 0.1 * sin(angle - PI / 2))
            };

            ImageDrawTextEx(&img, font, text, pos, 10, 1, color);
        }

#if defined(RENDER)
        BeginDrawing();
        Texture2D tex = LoadTextureFromImage(img);
        DrawTexture(tex, 0, 0, WHITE);
        EndDrawing();
        UnloadTexture(tex);
#endif

        iterations += 3600;
    }

    time = GetMonotonicTime() - start;
    PrintLn("{\"iterations\": %1, \"time\": %2}", iterations, time);

    return 0;
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return K::RunApp(argc, argv); }
