// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

define(function(require, exports, module) {
"use strict";

var oop = require("../lib/oop");
var TextHighlightRules = require("./text_highlight_rules").TextHighlightRules;

var blikkHighlightRules = function() {
    this.$rules = {
        start: [{
            token: "comment",
            regex: "#.*$"
        }, {
            token: "string",
            regex: '"(?:[^\\\\]|\\\\.)*?"'
        }, {
            token: "constant.numeric",
            regex: "(0x[0-9a-fA-F_]*|[0-9_]+(\\.?[0-9_]*)?|0o[0-8_]*|0b[01_]*)\\b"
        }, {
            token: "keyword.control",
            regex: "(?:let|for|func|if|else|while|begin|end|return|in|do|mut|break|continue|record|enum|pass)\\b"
        }, {
            token: "keyword.operator",
            regex: /<<=?|>>=?|[:*%\/+\-&\^|~!<>=]=?/
        }, {
            token: "keyword.operator",
            regex: "(?:and|or)\\b"
        }, {
            token: "storage.type",
            regex: "(Null|Bool|Int|Float|String|Type)\\b"
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

oop.inherits(blikkHighlightRules, TextHighlightRules);

exports.blikkHighlightRules = blikkHighlightRules;
});
