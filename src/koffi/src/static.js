// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

// Dummy, it gets replaced in package builds ;)

const BINARY_ROOT = import.meta.dirname + '/../../../bin/Koffi';

function loadStatic(pkg) {
    return null;
}

export {
    BINARY_ROOT,
    loadStatic
}
