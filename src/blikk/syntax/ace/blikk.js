// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

define(function(require, exports, module) {
"use strict";

var oop = require("../lib/oop");
var TextMode = require("./text").Mode;
var HighlightRules = require("./blikk_highlight_rules").blikkHighlightRules;

var Mode = function() {
    this.HighlightRules = HighlightRules;
    this.foldingRules = null;
};
oop.inherits(Mode, TextMode);

(function() {
    this.lineCommentStart = "#";
    this.blockComment = null;
    this.$id = "ace/mode/blikk"
}).call(Mode.prototype);

exports.Mode = Mode;
});
