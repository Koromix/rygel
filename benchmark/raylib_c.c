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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "../vendor/raylib/src/raylib.h"

#define STRINGIFY(Value) # Value

static int ParseInt(const char *str)
{
    char *end;
    long long value = strtoll(str, &end, 10);

    if (end == str || *end) {
        fprintf(stderr, "Not a valid integer number\n");
        return -1;
    }
    if (value < 1 || value == LLONG_MAX) {
        fprintf(stderr, "Value must be between 1 and " STRINGIFY(LLONG_MAX) "\n");
        return -1;
    }

    return (int)value;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Missing number of iterations\n");
        return 1;
    }

    int iterations = ParseInt(argv[1]);
    if (iterations < 0)
        return 1;
    printf("Iterations: %d\n", iterations);

    // We need to call InitWindow before using anything else (such as fonts)
    SetTraceLogLevel(4); // Warnings
    SetWindowState(0x80); // Hidden
    InitWindow(640, 480, "Raylib Test");

    Image img = GenImageColor(800, 600, (Color){ .r = 0, .g = 0, .b = 0, .a = 255 });
    Font font = GetFontDefault();

    for (int i = 0; i < iterations; i++) {
        ImageClearBackground(&img, (Color){ .r = 0, .g = 0, .b = 0, .a = 255 });

        for (int j = 0; j < 360; j++) {
            const char *text = "Hello World!";
            int text_width = MeasureTextEx(font, text, 10, 1).x;

            double angle = (j * 4) * PI / 180;
            Color color = {
                .r = 127.5 + 127.5 * sin(angle),
                .g = 127.5 + 127.5 * sin(angle + PI / 2),
                .b = 127.5 + 127.5 * sin(angle + PI),
                .a = 255
            };
            Vector2 pos = {
                .x = (img.width / 2 - text_width / 2) + j * cos(angle - PI / 2),
                .y = (img.height / 2 - 16) + j * sin(angle - PI / 2)
            };

            ImageDrawTextEx(&img, font, text, pos, 10, 1, color);
        }
    }

    return 0;
}
