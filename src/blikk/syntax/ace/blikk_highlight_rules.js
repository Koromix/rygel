// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

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
