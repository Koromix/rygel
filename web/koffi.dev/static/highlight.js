// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

import * as hljs from '../../../vendor/highlight.js/highlight.bundle.js';
import CopyButtonPlugin from '../../../vendor/highlight.js/copy/highlightjs-copy.js';

window.addEventListener('load', e => {
    hljs.addPlugin(new CopyButtonPlugin);
    hljs.highlightAll();
});
