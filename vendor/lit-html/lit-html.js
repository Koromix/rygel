import { render } from 'lit/html.js';
import { directive } from 'lit/directive.js';
import { guard } from 'lit/directives/guard.js';
import { live } from 'lit/directives/live.js';
import { until } from 'lit/directives/until.js';

const html = (strings, ...values) => {
    transformValues(values);
    return {
        ['_$litType$']: 1, // HTML_RESULT
        strings,
        values
    };
};
const svg = (strings, ...values) => {
    transformValues(values);
    return {
        ['_$litType$']: 2, // SVG_RESULT
        strings,
        values
    };
};

// Small addition to lit-html: objects with a toHTML() method are handled by
// calling this function before the templating stuff. This is used by goupile to
// allow the user to embed widgets in HTML code automatically.
function transformValues(values) {
    for (let i = 0; i < values.length; i++) {
        const value = values[i];

        if (value != null && typeof value.toHTML === 'function')
            values[i] = value.toHTML();
    }
}

export { html, svg, render, directive, guard, live, until };
