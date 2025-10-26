// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import '../../../src/web/flat/static.js';
import * as hljs from '../../../vendor/highlight.js/highlight.js';

window.addEventListener('load', e => {
    hljs.highlightAll();
});
