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
    int time = 5000;

    if (argc >= 2) {
        if (!ParseInt(argv[1], &time))
            return 1;
    }

    // We need to call InitWindow before using anything else (such as fonts)
    SetTraceLogLevel(LOG_WARNING);
    SetWindowState(FLAG_WINDOW_HIDDEN);
    InitWindow(640, 480, "Raylib Test");

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

        iterations += 3600;
    }

    time = GetMonotonicTime() - start;
    PrintLn("{\"iterations\": %1, \"time\": %2}", iterations, time);

    return 0;
}

}

// C++ namespaces are stupid
int main(int argc, char **argv) { return RG::Main(argc, argv); }
