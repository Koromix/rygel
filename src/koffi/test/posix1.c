// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

#define EXPORT __attribute__((visibility("default")))

EXPORT int DoSumInts(int a, int b)
{
    return a + b;
}

EXPORT int DoGetInt()
{
    return 1;
}

EXPORT int GetInt()
{
    return DoGetInt();
}
