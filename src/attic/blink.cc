// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#include <Arduino.h>
#pragma GCC diagnostic pop

const int LedPin = 13;

void setup()
{
    pinMode(LedPin, OUTPUT);
}

void loop()
{
    digitalWrite(LedPin, 1);
    delay(500);
    digitalWrite(LedPin, 0);
    delay(500);
}
