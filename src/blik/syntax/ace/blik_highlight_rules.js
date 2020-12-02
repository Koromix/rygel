// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

define(function(require, exports, module) {
"use strict";

var oop = require("../lib/oop");
var TextHighlightRules = require("./text_highlight_rules").TextHighlightRules;

var BlikHighlightRules = function() {
    this.$rules = {
        start: [{
            token: "comment",
            regex: "#.*$"
        }, {
            token: "string",
            regex: '"(?:[^\\\\]|\\\\.)*?"'
        }, {
            token: "string",
            regex: "'(?:[^\\\\]|\\\\.)*?'"
        }, {
            token: "constant.numeric",
            regex: "(0x[0-9a-fA-F_]*|[0-9_]+(\\.?[0-9_]*)?|0o[0-8_]*|0b[01_]*)\\b"
        }, {
            token: "keyword.control",
            regex: "(?:let|for|func|if|else|while|begin|end|return|in|do|mut|break|continue)\\b"
        }, {
            token: "keyword.operator",
            regex: /<<=?|>>=?|&&|\|\||[:*%\/+\-&\^|~!<>=]=?/
        }, {
            token: "storage.type",
            regex: "(Null|Bool|Int|Float)\\b"
        }, {
            token: "constant.language",
            regex: "(?:true|false|null)\\b"
        }, {
            token: "paren.lparen",
            regex: "\\("
        }, {
            token: "paren.rparen",
            regex: "\\)"
        }, {
            token : "text",
            regex : "\\s+|\\w+"
        }]
    };

    this.normalizeRules();
};

oop.inherits(BlikHighlightRules, TextHighlightRules);

exports.BlikHighlightRules = BlikHighlightRules;
});
