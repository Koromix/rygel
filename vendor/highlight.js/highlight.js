import {
    highlight,
    highlightAuto,
    highlightElement,
    highlightAll,
    registerLanguage
} from './repo/build/highlight.js';
import ini from './ini.js';

registerLanguage('ini', ini);

export {
    highlight,
    highlightAuto,
    highlightElement,
    highlightAll
}
