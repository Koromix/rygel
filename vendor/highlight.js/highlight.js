import {
    highlight,
    highlightAuto,
    highlightElement,
    highlightAll,
    registerLanguage,
    addPlugin
} from './repo/build/highlight.js';

import ini from './ini.js';

registerLanguage('ini', ini);

export {
    highlight,
    highlightAuto,
    highlightElement,
    highlightAll,
    registerLanguage,
    addPlugin
}
