// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"
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
