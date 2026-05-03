// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

#define EXPORT __attribute__((visibility("default")))

int DoSumInts(int a, int b);

EXPORT int SumInts(int a, int b)
{
    return DoSumInts(a, b);
}

EXPORT int TestLazyBinding()
{
    int DoesNotExist();
    return DoesNotExist();
}

EXPORT int DoGetInt()
{
    return 2;
}

EXPORT int GetInt()
{
    return DoGetInt();
}
