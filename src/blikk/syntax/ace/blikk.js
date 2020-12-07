// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

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
