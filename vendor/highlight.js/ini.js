export default function(hljs) {
    const regex = hljs.regex;

    const NUMBERS = {
        className: 'number',
        variants: [
            { begin: regex.concat(hljs.NUMBER_RE, /[a-zA-Z+]*/) }
        ],
        relevance: 0
    };
    const COMMENTS = Object.assign(hljs.COMMENT(), {
        variants: [
            { begin: /;/, end: /$/ },
            { begin: /#/, end: /$/ }
        ],
        relevance: 0
    });
    const BOOLEANS = {
        className: 'literal',
        begin: /\bon|off|true|false|yes|no\b/
    };

    const KEY = /[A-Za-z0-9_-]+/;
    const KEY_PREFIX = regex.concat(KEY, / *= */);

    return {
        name: 'INI',
        case_insensitive: true,
        illegal: /\S/,
        contains: [
            COMMENTS,
            {
                className: 'keyword',
                begin: /\[+/,
                end: /\]+/
            },
            {
                begin: KEY_PREFIX,
                className: 'title',
                starts: {
                    end: /$/,
                    contains: [
                        BOOLEANS,
                        NUMBERS
                    ]
                }
            }
        ]
    };
}
