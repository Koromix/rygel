import { render, directive } from 'lit-html/lit-html';
import { defaultTemplateProcessor } from 'lit-html/lib/default-template-processor';
import { SVGTemplateResult, TemplateResult } from 'lit-html/lib/template-result';
import { guard } from 'lit-html/directives/guard';
import { live } from 'lit-html/directives/live';
import { until } from 'lit-html/directives/until';

const html = (strings, ...values) => {
    transformValues(values);
    return new TemplateResult(strings, values, 'html', defaultTemplateProcessor);
};
const svg = (strings, ...values) => {
    transformValues(values);
    return new SVGTemplateResult(strings, values, 'svg', defaultTemplateProcessor);
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
