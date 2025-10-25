// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#define EXPORT __attribute__((visibility("default")))

EXPORT int DoGetInt()
{
    return 3;
}

EXPORT int GetInt()
{
    return DoGetInt();
}
