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

#include "../vendor/libcc/libcc.hh"
#include "../vendor/raylib/src/raylib.h"

namespace RG {

int Main(int argc, char **argv)
{
    int iterations = 100;

    if (argc >= 2) {
        if (!ParseInt(argv[1], &iterations))
            return 1;
    }
    LogInfo("Iterations: %1", iterations);

    // We need to call InitWindow before using anything else (such as fonts)
    SetTraceLogLevel(LOG_WARNING);
    SetWindowState(FLAG_WINDOW_HIDDEN);
    InitWindow(640, 480, "Raylib Test");

    Image img = GenImageColor(800, 600, (Color){ .r = 0, .g = 0, .b = 0, .a = 255 });
    Font font = GetFontDefault();

    int64_t start = GetMonotonicTime();

    for (int i = 0; i < iterations; i++) {
        ImageClearBackground(&img, (Color){ .r = 0, .g = 0, .b = 0, .a = 255 });

        for (int j = 0; j < 3600; j++) {
            const char *text = "Hello World!";
            int text_width = MeasureTextEx(font, text, 10, 1).x;

            double angle = (j * 7) * PI / 180;
            Color color = {
                .r = (unsigned char)(127.5 + 127.5 * sin(angle)),
                .g = (unsigned char)(127.5 + 127.5 * sin(angle + PI / 2)),
                .b = (unsigned char)(127.5 + 127.5 * sin(angle + PI)),
                .a = 255
            };
            Vector2 pos = {
                .x = (float)((img.width / 2 - text_width / 2) + j * 0.1 * cos(angle - PI / 2)),
                .y = (float)((img.height / 2 - 16) + j * 0.1 * sin(angle - PI / 2))
            };

            ImageDrawTextEx(&img, font, text, pos, 10, 1, color);
        }
    }

    int64_t time = GetMonotonicTime() - start;
    LogInfo("Time: %1s", FmtDouble((double)time / 1000.0, 2));

    return 0;
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::Main(argc, argv); }
