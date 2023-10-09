define("ace/ext/rtl",["require","exports","module","ace/editor","ace/config"], function(require, exports, module){"use strict";
var commands = [{
        name: "leftToRight",
        bindKey: { win: "Ctrl-Alt-Shift-L", mac: "Command-Alt-Shift-L" },
        exec: function (editor) {
            editor.session.$bidiHandler.setRtlDirection(editor, false);
        },
        readOnly: true
    }, {
        name: "rightToLeft",
        bindKey: { win: "Ctrl-Alt-Shift-R", mac: "Command-Alt-Shift-R" },
        exec: function (editor) {
            editor.session.$bidiHandler.setRtlDirection(editor, true);
        },
        readOnly: true
    }];
var Editor = require("../editor").Editor;
require("../config").defineOptions(Editor.prototype, "editor", {
    rtlText: {
        set: function (val) {
            if (val) {
                this.on("change", onChange);
                this.on("changeSelection", onChangeSelection);
                this.renderer.on("afterRender", updateLineDirection);
                this.commands.on("exec", onCommandEmitted);
                this.commands.addCommands(commands);
            }
            else {
                this.off("change", onChange);
                this.off("changeSelection", onChangeSelection);
                this.renderer.off("afterRender", updateLineDirection);
                this.commands.off("exec", onCommandEmitted);
                this.commands.removeCommands(commands);
                clearTextLayer(this.renderer);
            }
            this.renderer.updateFull();
        }
    },
    rtl: {
        set: function (val) {
            this.session.$bidiHandler.$isRtl = val;
            if (val) {
                this.setOption("rtlText", false);
                this.renderer.on("afterRender", updateLineDirection);
                this.session.$bidiHandler.seenBidi = true;
            }
            else {
                this.renderer.off("afterRender", updateLineDirection);
                clearTextLayer(this.renderer);
            }
            this.renderer.updateFull();
        }
    }
});
function onChangeSelection(e, editor) {
    var lead = editor.getSelection().lead;
    if (editor.session.$bidiHandler.isRtlLine(lead.row)) {
        if (lead.column === 0) {
            if (editor.session.$bidiHandler.isMoveLeftOperation && lead.row > 0) {
                editor.getSelection().moveCursorTo(lead.row - 1, editor.session.getLine(lead.row - 1).length);
            }
            else {
                if (editor.getSelection().isEmpty())
                    lead.column += 1;
                else
                    lead.setPosition(lead.row, lead.column + 1);
            }
        }
    }
}
function onCommandEmitted(commadEvent) {
    commadEvent.editor.session.$bidiHandler.isMoveLeftOperation = /gotoleft|selectleft|backspace|removewordleft/.test(commadEvent.command.name);
}
function onChange(delta, editor) {
    var session = editor.session;
    session.$bidiHandler.currentRow = null;
    if (session.$bidiHandler.isRtlLine(delta.start.row) && delta.action === 'insert' && delta.lines.length > 1) {
        for (var row = delta.start.row; row < delta.end.row; row++) {
            if (session.getLine(row + 1).charAt(0) !== session.$bidiHandler.RLE)
                session.doc.$lines[row + 1] = session.$bidiHandler.RLE + session.getLine(row + 1);
        }
    }
}
function updateLineDirection(e, renderer) {
    var session = renderer.session;
    var $bidiHandler = session.$bidiHandler;
    var cells = renderer.$textLayer.$lines.cells;
    var width = renderer.layerConfig.width - renderer.layerConfig.padding + "px";
    cells.forEach(function (cell) {
        var style = cell.element.style;
        if ($bidiHandler && $bidiHandler.isRtlLine(cell.row)) {
            style.direction = "rtl";
            style.textAlign = "right";
            style.width = width;
        }
        else {
            style.direction = "";
            style.textAlign = "";
            style.width = "";
        }
    });
}
function clearTextLayer(renderer) {
    var lines = renderer.$textLayer.$lines;
    lines.cells.forEach(clear);
    lines.cellCache.forEach(clear);
    function clear(cell) {
        var style = cell.element.style;
        style.direction = style.textAlign = style.width = "";
    }
}

});

define("kitchen-sink/inline_editor",["require","exports","module","ace/line_widgets","ace/editor","ace/virtual_renderer","ace/lib/dom","ace/commands/default_commands"], function(require, exports, module) {"use strict";

var LineWidgets = require("ace/line_widgets").LineWidgets;
var Editor = require("ace/editor").Editor;
var Renderer = require("ace/virtual_renderer").VirtualRenderer;
var dom = require("ace/lib/dom");


require("ace/commands/default_commands").commands.push({
    name: "openInlineEditor",
    bindKey: "F3",
    exec: function(editor) {
        var split = window.env.split;
        var s = editor.session;
        var inlineEditor = new Editor(new Renderer());
        var splitSession = split.$cloneSession(s);

        var row = editor.getCursorPosition().row;
        if (editor.session.lineWidgets && editor.session.lineWidgets[row]) {
            editor.session.lineWidgets[row].destroy();
            return;
        }
        
        var rowCount = 10;
        var w = {
            row: row, 
            fixedWidth: true,
            el: dom.createElement("div"),
            editor: inlineEditor
        };
        var el = w.el;
        el.appendChild(inlineEditor.container);

        if (!editor.session.widgetManager) {
            editor.session.widgetManager = new LineWidgets(editor.session);
            editor.session.widgetManager.attach(editor);
        }
        
        var h = rowCount*editor.renderer.layerConfig.lineHeight;
        inlineEditor.container.style.height = h + "px";

        el.style.position = "absolute";
        el.style.zIndex = "4";
        el.style.borderTop = "solid blue 2px";
        el.style.borderBottom = "solid blue 2px";
        
        inlineEditor.setSession(splitSession);
        editor.session.widgetManager.addLineWidget(w);
        
        var kb = {
            handleKeyboard:function(_,hashId, keyString) {
                if (hashId === 0 && keyString === "esc") {
                    w.destroy();
                    return true;
                }
            }
        };
        
        w.destroy = function() {
            editor.keyBinding.removeKeyboardHandler(kb);
            s.widgetManager.removeLineWidget(w);
        };
        
        editor.keyBinding.addKeyboardHandler(kb);
        inlineEditor.keyBinding.addKeyboardHandler(kb);
        inlineEditor.setTheme("ace/theme/solarized_light");
    }
});

});

define("ace/test/user",["require","exports","module"], function(require, exports, module){"use strict";
var keyCodeToKey = {};
var keyCodeToCode = {};
var alias = {};
alias.Ctrl = "Control";
alias.Option = "Alt";
alias.Cmd = alias.Super = alias.Meta = "Command";
var controlKeys = {
    Shift: 16, Control: 17, Alt: 18, Meta: 224, Command: 224,
    Backspace: 8, Tab: 9, Return: 13, Enter: 13,
    Pause: 19, Escape: 27, PageUp: 33, PageDown: 34, End: 35, Home: 36,
    Left: 37, Up: 38, Right: 39, Down: 40, Insert: 45, Delete: 46,
    ArrowLeft: 37, ArrowUp: 38, ArrowRight: 39, ArrowDown: 40
};
var shiftedKeys = {};
var printableKeys = {};
var specialKeys = {
    Backquote: [192, "`", "~"], Minus: [189, "-", "_"], Equal: [187, "=", "+"],
    BracketLeft: [219, "[", "{"], Backslash: [220, "\\", "|"], BracketRight: [221, "]", "}"],
    Semicolon: [186, ";", ":"], Quote: [222, "'", '"'], Comma: [188, ",", "<"],
    Period: [190, ".", ">"], Slash: [191, "/", "?"], Space: [32, " "], NumpadAdd: [107, "+"],
    NumpadDecimal: [110, "."], NumpadSubtract: [109, "-"], NumpadDivide: [111, "/"], NumpadMultiply: [106, "*"]
};
for (var i in specialKeys) {
    var key = specialKeys[i];
    printableKeys[i] = printableKeys[key[1]] = shiftedKeys[key[2]] = key[0];
    keyCodeToCode[key[0]] = i;
}
for (var i = 0; i < 10; i++) {
    printableKeys[i] = shiftedKeys["!@#$%^&*()"[i]] = 48 + i;
    keyCodeToCode[48 + i] = "Digit" + i;
}
for (var i = 65; i < 91; i++) {
    var chr = String.fromCharCode(i + 32);
    printableKeys[chr] = shiftedKeys[chr.toUpperCase()] = i;
    keyCodeToCode[i] = "Key" + chr.toUpperCase();
}
for (var i = 1; i < 13; i++) {
    controlKeys["F" + i] = 111 + i;
}
for (var i in controlKeys) {
    keyCodeToKey[controlKeys[i]] = i;
    keyCodeToCode[controlKeys[i]] = i;
}
controlKeys["\n"] = controlKeys.Return;
var shift = false;
var ctrl = false;
var meta = false;
var alt = false;
function reset() {
    shift = ctrl = meta = alt = false;
}
function updateModifierStates(keyCode) {
    if (keyCode == controlKeys.Shift)
        return shift = true;
    if (keyCode == controlKeys.Control)
        return ctrl = true;
    if (keyCode == controlKeys.Meta)
        return meta = true;
    if (keyCode == controlKeys.Alt)
        return alt = true;
}
function sendKey(letter, timeout) {
    var keyCode = controlKeys[letter] || printableKeys[letter] || shiftedKeys[letter];
    var isModifier = updateModifierStates(keyCode);
    var text = letter;
    if (ctrl || alt || meta || controlKeys[letter]) {
        text = "";
    }
    else if (shift) {
        text = text.toUpperCase();
    }
    var target = document.activeElement;
    var prevented = emit("keydown", true);
    if (isModifier)
        return;
    if (text)
        emit("keypress", true);
    if (!prevented)
        updateTextInput();
    emit("keyup", true);
    function emit(type, bubbles) {
        var data = { bubbles: bubbles, cancelable: true };
        data.charCode = text.charCodeAt(0);
        data.keyCode = type == "keypress" ? data.charCode : keyCode;
        data.which = data.keyCode;
        data.shiftKey = shift || shiftedKeys[text];
        data.ctrlKey = ctrl;
        data.altKey = alt;
        data.metaKey = meta;
        data.key = text || keyCodeToKey[keyCode];
        data.code = keyCodeToCode[keyCode];
        var event = new KeyboardEvent(type, data);
        var el = document.activeElement;
        el.dispatchEvent(event);
        return event.defaultPrevented;
    }
    function updateTextInput() {
        var el = target;
        var isTextarea = "selectionStart" in el && typeof el.value == "string";
        var isContentEditable = /text|true/.test(el.contentEditable);
        if (!isTextarea && !isContentEditable)
            return;
        var s = el.selectionStart;
        var e = el.selectionEnd;
        var value = el.value;
        if (isContentEditable) {
            value = el.textContent;
            var range = window.getSelection().getRangeAt(0);
            s = range.startOffset;
            e = range.endOffset;
        }
        if (!text) {
            if (keyCode == 13) { // enter
                text = "\n";
            }
            else if (keyCode == 8) { // backspace
                if (s != e)
                    s = Math.max(s - 1, 0);
            }
            else if (keyCode == 46) { // delete
                if (s != e)
                    e = Math.min(e + 1, value.length);
            }
            else {
                return;
            }
        }
        var newValue = value.slice(0, s) + text + value.slice(e);
        s = e = s + text.length;
        if (newValue != value) {
            if (isContentEditable) {
                el.textContent = newValue;
                range.setStart(el.firstChild, s);
                range.setEnd(el.firstChild, e);
            }
            else {
                el.value = newValue;
                el.setSelectionRange(s, e);
            }
            emit("input", false);
        }
    }
}
function type() {
    var keys = Array.prototype.slice.call(arguments);
    for (var i = 0; i < keys.length; i++) {
        var key = keys[i];
        if (Array.isArray(key)) {
            type.apply(null, key);
            continue;
        }
        reset();
        if (key.length > 1) {
            var isKeyName = controlKeys[key] || printableKeys[key] || shiftedKeys[key];
            if (!isKeyName) {
                var parts = key.split("-");
                var modifier = alias[parts[0]] || parts[0];
                if (!updateModifierStates(controlKeys[modifier])) {
                    type.apply(null, key.split(""));
                    continue;
                }
                key = parts.pop();
                parts.forEach(function (part) {
                    var keyCode = controlKeys[part];
                    updateModifierStates(keyCode);
                });
            }
        }
        sendKey(key);
    }
}
exports.type = type;

});

define("ace/test/asyncjs/assert",["require","exports","module","ace/lib/oop"], function(require, exports, module){// http://wiki.commonjs.org/wiki/Unit_Testing/1.0
var oop = require("ace/lib/oop");
var pSlice = Array.prototype.slice;
var assert = exports;
assert.AssertionError = function AssertionError(options) {
    this.name = 'AssertionError';
    this.message = options.message;
    this.actual = options.actual;
    this.expected = options.expected;
    this.operator = options.operator;
    var stackStartFunction = options.stackStartFunction || fail;
    if (Error.captureStackTrace) {
        Error.captureStackTrace(this, stackStartFunction);
    }
};
oop.inherits(assert.AssertionError, Error);
toJSON = function (obj) {
    if (typeof JSON !== "undefined")
        return JSON.stringify(obj);
    else
        return obj.toString();
};
assert.AssertionError.prototype.toString = function () {
    if (this.message) {
        return [this.name + ':', this.message].join(' ');
    }
    else {
        return [this.name + ':',
            toJSON(this.expected),
            this.operator,
            toJSON(this.actual)].join(' ');
    }
};
assert.AssertionError.__proto__ = Error.prototype;
function fail(actual, expected, message, operator, stackStartFunction) {
    throw new assert.AssertionError({
        message: message,
        actual: actual,
        expected: expected,
        operator: operator,
        stackStartFunction: stackStartFunction
    });
}
assert.fail = fail;
assert.ok = function ok(value, message) {
    if (!!!value)
        fail(value, true, message, '==', assert.ok);
};
assert.equal = function equal(actual, expected, message) {
    if (actual != expected)
        fail(actual, expected, message, '==', assert.equal);
};
assert.notEqual = function notEqual(actual, expected, message) {
    if (actual == expected) {
        fail(actual, expected, message, '!=', assert.notEqual);
    }
};
assert.deepEqual = function deepEqual(actual, expected, message) {
    if (!_deepEqual(actual, expected)) {
        fail(actual, expected, message, 'deepEqual', assert.deepEqual);
    }
};
function _deepEqual(actual, expected) {
    if (actual === expected) {
        return true;
    }
    else if (typeof Buffer !== "undefined" && Buffer.isBuffer(actual) && Buffer.isBuffer(expected)) {
        if (actual.length != expected.length)
            return false;
        for (var i = 0; i < actual.length; i++) {
            if (actual[i] !== expected[i])
                return false;
        }
        return true;
    }
    else if (actual instanceof Date && expected instanceof Date) {
        return actual.getTime() === expected.getTime();
    }
    else if (typeof actual != 'object' && typeof expected != 'object') {
        return actual == expected;
    }
    else {
        return objEquiv(actual, expected);
    }
}
function isUndefinedOrNull(value) {
    return value === null || value === undefined;
}
function isArguments(object) {
    return Object.prototype.toString.call(object) == '[object Arguments]';
}
function objEquiv(a, b) {
    if (isUndefinedOrNull(a) || isUndefinedOrNull(b))
        return false;
    if (a.prototype !== b.prototype)
        return false;
    if (isArguments(a)) {
        if (!isArguments(b)) {
            return false;
        }
        a = pSlice.call(a);
        b = pSlice.call(b);
        return _deepEqual(a, b);
    }
    try {
        var ka = Object.keys(a), kb = Object.keys(b), key, i;
    }
    catch (e) { //happens when one is a string literal and the other isn't
        return false;
    }
    if (ka.length != kb.length)
        return false;
    ka.sort();
    kb.sort();
    for (i = ka.length - 1; i >= 0; i--) {
        if (ka[i] != kb[i])
            return false;
    }
    for (i = ka.length - 1; i >= 0; i--) {
        key = ka[i];
        if (!_deepEqual(a[key], b[key]))
            return false;
    }
    return true;
}
assert.notDeepEqual = function notDeepEqual(actual, expected, message) {
    if (_deepEqual(actual, expected)) {
        fail(actual, expected, message, 'notDeepEqual', assert.notDeepEqual);
    }
};
assert.strictEqual = function strictEqual(actual, expected, message) {
    if (actual !== expected) {
        fail(actual, expected, message, '===', assert.strictEqual);
    }
};
assert.notStrictEqual = function notStrictEqual(actual, expected, message) {
    if (actual === expected) {
        fail(actual, expected, message, '!==', assert.notStrictEqual);
    }
};
function expectedException(actual, expected) {
    if (!actual || !expected) {
        return false;
    }
    if (expected instanceof RegExp) {
        return expected.test(actual);
    }
    else if (actual instanceof expected) {
        return true;
    }
    else if (expected.call({}, actual) === true) {
        return true;
    }
    return false;
}
function _throws(shouldThrow, block, expected, message) {
    var actual;
    if (typeof expected === 'string') {
        message = expected;
        expected = null;
    }
    try {
        block();
    }
    catch (e) {
        actual = e;
    }
    message = (expected && expected.name ? ' (' + expected.name + ').' : '.') +
        (message ? ' ' + message : '.');
    if (shouldThrow && !actual) {
        fail('Missing expected exception' + message);
    }
    if (!shouldThrow && expectedException(actual, expected)) {
        fail('Got unwanted exception' + message);
    }
    if ((shouldThrow && actual && expected &&
        !expectedException(actual, expected)) || (!shouldThrow && actual)) {
        throw actual;
    }
}
assert.throws = function (block, /*optional*/ error, /*optional*/ message) {
    _throws.apply(this, [true].concat(pSlice.call(arguments)));
};
assert.doesNotThrow = function (block, /*optional*/ error, /*optional*/ message) {
    _throws.apply(this, [false].concat(pSlice.call(arguments)));
};
assert.ifError = function (err) { if (err) {
    throw err;
} };

});

define("ace/test/asyncjs/async",["require","exports","module"], function(require, exports, module){/*!
 * async.js
 * Copyright(c) 2010 Fabian Jakobs <fabian.jakobs@web.de>
 * MIT Licensed
 */
var STOP = exports.STOP = {};
exports.Generator = function (source) {
    if (typeof source == "function")
        this.source = {
            next: source
        };
    else
        this.source = source;
};
(function () {
    this.next = function (callback) {
        this.source.next(callback);
    };
    this.map = function (mapper) {
        if (!mapper)
            return this;
        mapper = makeAsync(1, mapper);
        var source = this.source;
        this.next = function (callback) {
            source.next(function (err, value) {
                if (err)
                    callback(err);
                else {
                    mapper(value, function (err, value) {
                        if (err)
                            callback(err);
                        else
                            callback(null, value);
                    });
                }
            });
        };
        return new this.constructor(this);
    };
    this.filter = function (filter) {
        if (!filter)
            return this;
        filter = makeAsync(1, filter);
        var source = this.source;
        this.next = function (callback) {
            source.next(function handler(err, value) {
                if (err)
                    callback(err);
                else {
                    filter(value, function (err, takeIt) {
                        if (err)
                            callback(err);
                        else if (takeIt)
                            callback(null, value);
                        else
                            source.next(handler);
                    });
                }
            });
        };
        return new this.constructor(this);
    };
    this.slice = function (begin, end) {
        var count = -1;
        if (!end || end < 0)
            var end = Infinity;
        var source = this.source;
        this.next = function (callback) {
            source.next(function handler(err, value) {
                count++;
                if (err)
                    callback(err);
                else if (count >= begin && count < end)
                    callback(null, value);
                else if (count >= end)
                    callback(STOP);
                else
                    source.next(handler);
            });
        };
        return new this.constructor(this);
    };
    this.reduce = function (reduce, initialValue) {
        reduce = makeAsync(3, reduce);
        var index = 0;
        var done = false;
        var previousValue = initialValue;
        var source = this.source;
        this.next = function (callback) {
            if (done)
                return callback(STOP);
            if (initialValue === undefined) {
                source.next(function (err, currentValue) {
                    if (err)
                        return callback(err, previousValue);
                    previousValue = currentValue;
                    reduceAll();
                });
            }
            else
                reduceAll();
            function reduceAll() {
                source.next(function handler(err, currentValue) {
                    if (err) {
                        done = true;
                        if (err == STOP)
                            return callback(null, previousValue);
                        else
                            return (err);
                    }
                    reduce(previousValue, currentValue, index++, function (err, value) {
                        previousValue = value;
                        source.next(handler);
                    });
                });
            }
        };
        return new this.constructor(this);
    };
    this.forEach =
        this.each = function (fn) {
            fn = makeAsync(1, fn);
            var source = this.source;
            this.next = function (callback) {
                source.next(function handler(err, value) {
                    if (err)
                        callback(err);
                    else {
                        fn(value, function (err) {
                            callback(err, value);
                        });
                    }
                });
            };
            return new this.constructor(this);
        };
    this.some = function (condition) {
        condition = makeAsync(1, condition);
        var source = this.source;
        var done = false;
        this.next = function (callback) {
            if (done)
                return callback(STOP);
            source.next(function handler(err, value) {
                if (err)
                    return callback(err);
                condition(value, function (err, result) {
                    if (err) {
                        done = true;
                        if (err == STOP)
                            callback(null, false);
                        else
                            callback(err);
                    }
                    else if (result) {
                        done = true;
                        callback(null, true);
                    }
                    else
                        source.next(handler);
                });
            });
        };
        return new this.constructor(this);
    };
    this.every = function (condition) {
        condition = makeAsync(1, condition);
        var source = this.source;
        var done = false;
        this.next = function (callback) {
            if (done)
                return callback(STOP);
            source.next(function handler(err, value) {
                if (err)
                    return callback(err);
                condition(value, function (err, result) {
                    if (err) {
                        done = true;
                        if (err == STOP)
                            callback(null, true);
                        else
                            callback(err);
                    }
                    else if (!result) {
                        done = true;
                        callback(null, false);
                    }
                    else
                        source.next(handler);
                });
            });
        };
        return new this.constructor(this);
    };
    this.call = function (context) {
        var source = this.source;
        return this.map(function (fn, next) {
            fn = makeAsync(0, fn, context);
            fn.call(context, function (err, value) {
                next(err, value);
            });
        });
    };
    this.concat = function (generator) {
        var generators = [this];
        generators.push.apply(generators, arguments);
        var index = 0;
        var source = generators[index++];
        return new this.constructor(function (callback) {
            source.next(function handler(err, value) {
                if (err) {
                    if (err == STOP) {
                        source = generators[index++];
                        if (!source)
                            return callback(STOP);
                        else
                            return source.next(handler);
                    }
                    else
                        return callback(err);
                }
                else
                    return callback(null, value);
            });
        });
    };
    this.zip = function (generator) {
        var generators = [this];
        generators.push.apply(generators, arguments);
        return new this.constructor(function (callback) {
            exports.list(generators)
                .map(function (gen, next) {
                gen.next(next);
            })
                .toArray(callback);
        });
    };
    this.expand = function (inserter, constructor) {
        if (!inserter)
            return this;
        var inserter = makeAsync(1, inserter);
        var constructor = constructor || this.constructor;
        var source = this.source;
        var spliced = null;
        return new constructor(function next(callback) {
            if (!spliced) {
                source.next(function (err, value) {
                    if (err)
                        return callback(err);
                    inserter(value, function (err, toInsert) {
                        if (err)
                            return callback(err);
                        spliced = toInsert;
                        next(callback);
                    });
                });
            }
            else {
                spliced.next(function (err, value) {
                    if (err == STOP) {
                        spliced = null;
                        return next(callback);
                    }
                    else if (err)
                        return callback(err);
                    callback(err, value);
                });
            }
        });
    };
    this.sort = function (compare) {
        var self = this;
        var arrGen;
        this.next = function (callback) {
            if (arrGen)
                return arrGen.next(callback);
            self.toArray(function (err, arr) {
                if (err)
                    callback(err);
                else {
                    arrGen = exports.list(arr.sort(compare));
                    arrGen.next(callback);
                }
            });
        };
        return new this.constructor(this);
    };
    this.join = function (separator) {
        return this.$arrayOp(Array.prototype.join, separator !== undefined ? [separator] : null);
    };
    this.reverse = function () {
        return this.$arrayOp(Array.prototype.reverse);
    };
    this.$arrayOp = function (arrayMethod, args) {
        var self = this;
        var i = 0;
        this.next = function (callback) {
            if (i++ > 0)
                return callback(STOP);
            self.toArray(function (err, arr) {
                if (err)
                    callback(err, "");
                else {
                    if (args)
                        callback(null, arrayMethod.apply(arr, args));
                    else
                        callback(null, arrayMethod.call(arr));
                }
            });
        };
        return new this.constructor(this);
    };
    this.end = function (breakOnError, callback) {
        if (!callback) {
            callback = arguments[0];
            breakOnError = true;
        }
        var source = this.source;
        var last;
        var lastError;
        source.next(function handler(err, value) {
            if (err) {
                if (err == STOP)
                    callback && callback(lastError, last);
                else if (!breakOnError) {
                    lastError = err;
                    source.next(handler);
                }
                else
                    callback && callback(err, value);
            }
            else {
                last = value;
                source.next(handler);
            }
        });
    };
    this.toArray = function (breakOnError, callback) {
        if (!callback) {
            callback = arguments[0];
            breakOnError = true;
        }
        var values = [];
        var errors = [];
        var source = this.source;
        source.next(function handler(err, value) {
            if (err) {
                if (err == STOP) {
                    if (breakOnError)
                        return callback(null, values);
                    else {
                        errors.length = values.length;
                        return callback(errors, values);
                    }
                }
                else {
                    if (breakOnError)
                        return callback(err);
                    else
                        errors[values.length] = err;
                }
            }
            values.push(value);
            source.next(handler);
        });
    };
}).call(exports.Generator.prototype);
var makeAsync = exports.makeAsync = function (args, fn, context) {
    if (fn.length > args)
        return fn;
    else {
        return function () {
            var value;
            var next = arguments[args];
            try {
                value = fn.apply(context || this, arguments);
            }
            catch (e) {
                return next(e);
            }
            next(null, value);
        };
    }
};
exports.list = function (arr, construct) {
    var construct = construct || exports.Generator;
    var i = 0;
    var len = arr.length;
    return new construct(function (callback) {
        if (i < len)
            callback(null, arr[i++]);
        else
            callback(STOP);
    });
};
exports.values = function (map, construct) {
    var values = [];
    for (var key in map)
        values.push(map[key]);
    return exports.list(values, construct);
};
exports.keys = function (map, construct) {
    var keys = [];
    for (var key in map)
        keys.push(key);
    return exports.list(keys, construct);
};
exports.range = function (start, stop, step, construct) {
    var construct = construct || exports.Generator;
    start = start || 0;
    step = step || 1;
    if (stop === undefined || stop === null)
        stop = step > 0 ? Infinity : -Infinity;
    var value = start;
    return new construct(function (callback) {
        if (step > 0 && value >= stop || step < 0 && value <= stop)
            callback(STOP);
        else {
            var current = value;
            value += step;
            callback(null, current);
        }
    });
};
exports.concat = function (first, varargs) {
    if (arguments.length > 1)
        return first.concat.apply(first, Array.prototype.slice.call(arguments, 1));
    else
        return first;
};
exports.zip = function (first, varargs) {
    if (arguments.length > 1)
        return first.zip.apply(first, Array.prototype.slice.call(arguments, 1));
    else
        return first.map(function (item, next) {
            next(null, [item]);
        });
};
exports.plugin = function (members, constructors) {
    if (members) {
        for (var key in members) {
            exports.Generator.prototype[key] = members[key];
        }
    }
    if (constructors) {
        for (var key in constructors) {
            exports[key] = constructors[key];
        }
    }
};

});

define("ace/test/mockrenderer",["require","exports","module"], function(require, exports, module){"use strict";
var MockRenderer = exports.MockRenderer = function (visibleRowCount) {
    if (typeof document == "object") {
        this.container = document.createElement("div");
        this.scroller = document.createElement("div");
        this.$gutter = document.createElement("div");
    }
    this.visibleRowCount = visibleRowCount || 20;
    this.layerConfig = {
        firstVisibleRow: 0,
        lastVisibleRow: this.visibleRowCount
    };
    this.isMockRenderer = true;
};
MockRenderer.prototype.getFirstVisibleRow = function () {
    return this.layerConfig.firstVisibleRow;
};
MockRenderer.prototype.getLastVisibleRow = function () {
    return this.layerConfig.lastVisibleRow;
};
MockRenderer.prototype.getFirstFullyVisibleRow = function () {
    return this.layerConfig.firstVisibleRow;
};
MockRenderer.prototype.getLastFullyVisibleRow = function () {
    return this.layerConfig.lastVisibleRow;
};
MockRenderer.prototype.getContainerElement = function () {
    return this.container;
};
MockRenderer.prototype.getMouseEventTarget = function () {
    return this.container;
};
MockRenderer.prototype.getTextAreaContainer = function () {
    return this.container;
};
MockRenderer.prototype.addGutterDecoration = function () {
};
MockRenderer.prototype.removeGutterDecoration = function () {
};
MockRenderer.prototype.moveTextAreaToCursor = function () {
};
MockRenderer.prototype.setSession = function (session) {
    this.session = session;
};
MockRenderer.prototype.getSession = function (session) {
    return this.session;
};
MockRenderer.prototype.setTokenizer = function () {
};
MockRenderer.prototype.on = function () {
};
MockRenderer.prototype.updateCursor = function () {
};
MockRenderer.prototype.animateScrolling = function (fromValue, callback) {
    callback && callback();
};
MockRenderer.prototype.scrollToX = function (scrollTop) { };
MockRenderer.prototype.scrollToY = function (scrollLeft) { };
MockRenderer.prototype.scrollToLine = function (line, center) {
    var lineHeight = 16;
    var row = 0;
    for (var l = 1; l < line; l++) {
        row += this.session.getRowLength(l - 1);
    }
    if (center) {
        row -= this.visibleRowCount / 2;
    }
    this.scrollToRow(row);
};
MockRenderer.prototype.scrollSelectionIntoView = function () {
};
MockRenderer.prototype.scrollCursorIntoView = function () {
    var cursor = this.session.getSelection().getCursor();
    if (cursor.row < this.layerConfig.firstVisibleRow) {
        this.scrollToRow(cursor.row);
    }
    else if (cursor.row > this.layerConfig.lastVisibleRow) {
        this.scrollToRow(cursor.row);
    }
};
MockRenderer.prototype.scrollToRow = function (row) {
    var row = Math.min(this.session.getLength() - this.visibleRowCount, Math.max(0, row));
    this.layerConfig.firstVisibleRow = row;
    this.layerConfig.lastVisibleRow = row + this.visibleRowCount;
};
MockRenderer.prototype.getScrollTopRow = function () {
    return this.layerConfig.firstVisibleRow;
};
MockRenderer.prototype.draw = function () {
};
MockRenderer.prototype.onChangeTabSize = function (startRow, endRow) {
};
MockRenderer.prototype.updateLines = function (startRow, endRow) {
};
MockRenderer.prototype.updateBackMarkers = function () {
};
MockRenderer.prototype.updateFrontMarkers = function () {
};
MockRenderer.prototype.updateBreakpoints = function () {
};
MockRenderer.prototype.onResize = function () {
};
MockRenderer.prototype.updateFull = function () {
};
MockRenderer.prototype.updateText = function () {
};
MockRenderer.prototype.showCursor = function () {
};
MockRenderer.prototype.visualizeFocus = function () {
};
MockRenderer.prototype.setAnnotations = function () {
};
MockRenderer.prototype.setStyle = function () {
};
MockRenderer.prototype.unsetStyle = function () {
};
MockRenderer.prototype.textToScreenCoordinates = function () {
    return {
        pageX: 0,
        pageY: 0
    };
};
MockRenderer.prototype.screenToTextCoordinates = function () {
    return {
        row: 0,
        column: 0
    };
};
MockRenderer.prototype.adjustWrapLimit = function () {
};
MockRenderer.prototype.getHighlightIndentGuides = function () {
};
MockRenderer.prototype.setHighlightIndentGuides = function () {
};

});

define("kitchen-sink/dev_util",["require","exports","module","ace/ace","ace/lib/dom","ace/lib/event","ace/range","ace/edit_session","ace/undomanager","ace/lib/oop","ace/lib/dom","ace/test/user","ace/range","ace/editor","ace/test/asyncjs/assert","ace/test/asyncjs/async","ace/undomanager","ace/edit_session","ace/test/mockrenderer","ace/lib/event_emitter"], function(require, exports, module) {var ace = require("ace/ace");
var dom = require("ace/lib/dom");
var event = require("ace/lib/event");
var Range = require("ace/range").Range;
var EditSession = require("ace/edit_session").EditSession;
var UndoManager = require("ace/undomanager").UndoManager;
function def(o, key, get) {
    try {
        Object.defineProperty(o, key, {
            configurable: true, 
            get: get,
            set: function(val) {
                delete o[key];
                o[key] = val;
            }
        });
    } catch(e) {
        console.error(e);
    }
}
def(window, "ace", function(){  return ace });
def(window, "editor", function(){  return window.env.editor == logEditor ? editor : window.env.editor });
def(window, "session", function(){ return window.editor.session });
def(window, "split", function(){  return window.env.split });


def(window, "devUtil", function(){ return exports });

exports.addGlobals = function() {
    window.oop = require("ace/lib/oop");
    window.dom = require("ace/lib/dom");
    window.user = require("ace/test/user");
    window.Range = require("ace/range").Range;
    window.Editor = require("ace/editor").Editor;
    window.assert = require("ace/test/asyncjs/assert");
    window.asyncjs = require("ace/test/asyncjs/async");
    window.UndoManager = require("ace/undomanager").UndoManager;
    window.EditSession = require("ace/edit_session").EditSession;
    window.MockRenderer = require("ace/test/mockrenderer").MockRenderer;
    window.EventEmitter = require("ace/lib/event_emitter").EventEmitter;
    
    window.getSelection = getSelection;
    window.setSelection = setSelection;
    window.testSelection = testSelection;
    window.setValue = setValue;
    window.testValue = testValue;
    window.logToAce = exports.log;
};

function getSelection(editor) {
    var data = editor.multiSelect.toJSON();
    if (!data.length) data = [data];
    data = data.map(function(x) {
        var a, c;
        if (x.isBackwards) {
            a = x.end;
            c = x.start;
        } else {
            c = x.end;
            a = x.start;
        }
        return Range.comparePoints(a, c) 
            ? [a.row, a.column, c.row, c.column]
            : [a.row, a.column];
    });
    return data.length > 1 ? data : data[0];
}
function setSelection(editor, data) {
    if (typeof data[0] == "number")
        data = [data];
    editor.selection.fromJSON(data.map(function(x) {
        var start = {row: x[0], column: x[1]};
        var end = x.length == 2 ? start : {row: x[2], column: x[3]};
        var isBackwards = Range.comparePoints(start, end) > 0;
        return isBackwards ? {
            start: end,
            end: start,
            isBackwards: true
        } : {
            start: start,
            end: end,
            isBackwards: true
        };
    }));
}
function testSelection(editor, data) {
    assert.equal(getSelection(editor) + "", data + "");
}
function setValue(editor, value) {
    editor.setValue(value, 1);
}
function testValue(editor, value) {
    assert.equal(editor.getValue(), value);
}

 
var editor;
var logEditor;
var logSession
exports.openLogView = function() {
    exports.addGlobals();
    var sp = window.env.split;
    sp.setSplits(1);
    sp.setSplits(2);
    sp.setOrientation(sp.BESIDE);
    editor = sp.$editors[0];
    logEditor = sp.$editors[1];
    
    if (!logSession) {
        logSession = new EditSession(localStorage.lastTestCase || "", "ace/mode/javascript");
        logSession.setUndoManager(new UndoManager)
    }
    logEditor.setSession(logSession);
    logEditor.session.foldAll();
    logEditor.on("input", save);
}
exports.record = function() {
    exports.addGlobals();
    exports.openLogView();
    
    logEditor.setValue("var Range = require(\"ace/range\").Range;\n"
        + getSelection + "\n"
        + testSelection + "\n"
        + setSelection + "\n"
        + testValue + "\n"
        + setValue + "\n"
        + "\n//-------------------------------------\n", 1);
    logEditor.session.foldAll();

    addAction({
        type: "setValue",
        data: editor.getValue()
    });
    addAction({
        type: "setSelection",
        data: getSelection(editor)
    });
    editor.commands.on("afterExec", onAfterExec);
    editor.on("mouseup", onMouseUp);
    editor.selection.on("beforeEndOperation", onBeforeEndOperation);
    editor.session.on("change", reportChange);
    editor.selection.on("changeCursor", reportCursorChange);
    editor.selection.on("changeSelection", reportSelectionChange);
}

exports.stop = function() {
    save();
    editor.commands.off("afterExec", onAfterExec);
    editor.off("mouseup", onMouseUp);
    editor.off("beforeEndOperation", onBeforeEndOperation);
    editor.session.off("change", reportChange);
    editor.selection.off("changeCursor", reportCursorChange);
    editor.selection.off("changeSelection", reportSelectionChange);
    logEditor.off("input", save);
}
exports.closeLogView = function() {
    exports.stop(); 
    var sp = window.env.split;
    sp.setSplits(1);
}

exports.play = function() {
    exports.openLogView();
    exports.stop();
    var code = logEditor ? logEditor.getValue() : localStorage.lastTestCase;
    var fn = new Function("editor", "debugger;\n" + code);
    fn(editor);
}
var reportChange = reportEvent.bind(null, "change");
var reportCursorChange = reportEvent.bind(null, "CursorChange");
var reportSelectionChange = reportEvent.bind(null, "SelectionChange");

function save() {
    localStorage.lastTestCase = logEditor.getValue();
}

function reportEvent(name) {
    addAction({
        type: "event",
        source: name
    });
} 
function onSelection() {
    addAction({
        type: "event",
        data: "change",
        source: "operationEnd"
    });
} 
function onBeforeEndOperation() {
    addAction({
        type: "setSelection",
        data: getSelection(editor),
        source: "operationEnd"
    });
} 
function onMouseUp() {
    addAction({
        type: "setSelection",
        data: getSelection(editor),
        source: "mouseup"
    });
}
function onAfterExec(e) {
    addAction({
        type: "exec",
        data: e
    });
    addAction({
        type: "value",
        data: editor.getValue()
    });
    addAction({
        type: "selection",
        data: getSelection(editor)
    });
}

function addAction(a) {
    var str = toString(a);
    if (str) {
        logEditor.insert(str + "\n");
        logEditor.renderer.scrollCursorIntoView();
    }
}

var lastValue = "";
function toString(x) {
    var str = "";
    var data = x.data;
    switch (x.type) {
        case "exec": 
            str = 'editor.execCommand("' 
                + data.command.name
                + (data.args ? '", ' + JSON.stringify(data.args) : '"')
            + ')';
            break;
        case "setSelection":
            str = 'setSelection(editor, ' + JSON.stringify(data)  + ')';
            break;
        case "setValue":
            if (lastValue != data) {
                lastValue = data;
                str = 'editor.setValue(' + JSON.stringify(data) + ', -1)';
            }
            else {
                return;
            }
            break;
        case "selection":
            str = 'testSelection(editor, ' + JSON.stringify(data) + ')';
            break;
        case "value":
            if (lastValue != data) {
                lastValue = data;
                str = 'testValue(editor, ' + JSON.stringify(data) + ')';
            }
            else  {
                return;
            }
            break;
    }
    return str + (x.source ? " // " + x.source : "");
}

exports.getUI = function(container) {
    return ["div", {role: "group", "aria-label": "Test"},
        " Test ", 
        ["button", {"aria-label": "Open Log View", onclick: exports.openLogView}, "O"],
        ["button", {onclick: exports.record}, "Record"],
        ["button", {onclick: exports.stop}, "Stop"],
        ["button", {onclick: exports.play}, "Play"],
        ["button", {"aria-label": "Close Log View", onclick: exports.closeLogView}, "X"],
    ];
};


var ignoreEvents = false;
exports.textInputDebugger = {
    position: 2000,
    path: "textInputDebugger",
    onchange: function(value) {
        var sp = env.split;
        if (sp.getSplits() == 2) {
            sp.setSplits(1);
        }
        if (env.textarea) {
            if (env.textarea.detach)
                env.textarea.detach();
            env.textarea.oldParent.appendChild(env.textarea);
            env.textarea.className = env.textarea.oldClassName;
            env.textarea = null;
        }
        if (value) {
            this.showConsole();
        }
    },
    showConsole: function() {
        var editor = env.split.$editors[0];
        var text = editor.textInput.getElement();
        text.oldParent = text.parentNode;
        text.oldClassName = text.className;
        text.className = "text-input-debug";
        document.body.appendChild(text);
        env.textarea = text;
        
        var addToLog = function(e) {
            if (ignoreEvents) return;
            var data = {
                _: e.type, 
                data: e.data,
                inputType: e.inputType,
                range: [text.selectionStart, text.selectionEnd], 
                value: text.value, 
                key: e.key && {
                    code: e.code,
                    key: e.key, 
                    keyCode: e.keyCode
                },
                modifier: event.getModifierString(e) || undefined
            };
            var str = JSON.stringify(data).replace(/"(\w+)":/g, " $1: ");
            exports.log(str);
        };
        var events = ["select", "input", "keypress", "keydown", "keyup", 
            "compositionstart", "compositionupdate", "compositionend", "cut", "copy", "paste"
        ];
        events.forEach(function(name) {
            text.addEventListener(name, addToLog, true);
        });
        function onMousedown(ev) {
            if (ev.domEvent.target == text)
                ev.$pos = editor.getCursorPosition();
        }
        text.detach = function() {
            delete text.value;
            delete text.setSelectionRange;
            
            events.forEach(function(name) {
                text.removeEventListener(name, addToLog, true);
            });
            editor.off("mousedown", onMousedown);
        };
        editor.on("mousedown", onMousedown);
        
        text.__defineSetter__("value", function(v) {
            this.__proto__.__lookupSetter__("value").call(this, v); 
            console.log(v);
        });
        text.__defineGetter__("value", function(v) {
            var v = this.__proto__.__lookupGetter__("value").call(this); 
            return v;
        });
        text.setSelectionRange = function(start, end) {
            ignoreEvents = true;
            this.__proto__.setSelectionRange.call(this, start, end)
            ignoreEvents = false;
        }
        exports.openConsole();
        editor.focus();
    },
    getValue: function() {
        return !!env.textarea;
    }
};

exports.textPositionDebugger = {
    position: 2000,
    path: "textPositionDebugger",
    onchange: function(value) {
        document.body.classList[value ? "add" : "remove"]("show-text-input")
    },
    getValue: function() {
        return document.body.classList.contains("show-text-input");
    }
};

exports.openConsole = function() {
    var sp = env.split;
    var logEditor = sp.$editors[1];
    if (!logEditor) {
        sp.setSplits(2);
        sp.setOrientation(sp.BELOW);
        logEditor = sp.$editors[1];
    }
    if (!exports.session)
        exports.session = new EditSession("");
    logEditor.setSession(exports.session);
    return logEditor
};
exports.log = function(str) {   
    var logEditor = exports.openConsole();
    logEditor.navigateFileEnd();
    logEditor.insert(str + ",\n");
    logEditor.renderer.scrollCursorIntoView();
};

exports.addGlobals();

});

define("ace/ext/modelist",["require","exports","module"], function(require, exports, module){"use strict";
var modes = [];
function getModeForPath(path) {
    var mode = modesByName.text;
    var fileName = path.split(/[\/\\]/).pop();
    for (var i = 0; i < modes.length; i++) {
        if (modes[i].supportsFile(fileName)) {
            mode = modes[i];
            break;
        }
    }
    return mode;
}
var Mode = /** @class */ (function () {
    function Mode(name, caption, extensions) {
        this.name = name;
        this.caption = caption;
        this.mode = "ace/mode/" + name;
        this.extensions = extensions;
        var re;
        if (/\^/.test(extensions)) {
            re = extensions.replace(/\|(\^)?/g, function (a, b) {
                return "$|" + (b ? "^" : "^.*\\.");
            }) + "$";
        }
        else {
            re = "^.*\\.(" + extensions + ")$";
        }
        this.extRe = new RegExp(re, "gi");
    }
    Mode.prototype.supportsFile = function (filename) {
        return filename.match(this.extRe);
    };
    return Mode;
}());
var supportedModes = {
    ABAP: ["abap"],
    ABC: ["abc"],
    ActionScript: ["as"],
    ADA: ["ada|adb"],
    Alda: ["alda"],
    Apache_Conf: ["^htaccess|^htgroups|^htpasswd|^conf|htaccess|htgroups|htpasswd"],
    Apex: ["apex|cls|trigger|tgr"],
    AQL: ["aql"],
    AsciiDoc: ["asciidoc|adoc"],
    ASL: ["dsl|asl|asl.json"],
    Assembly_x86: ["asm|a"],
    AutoHotKey: ["ahk"],
    BatchFile: ["bat|cmd"],
    BibTeX: ["bib"],
    C_Cpp: ["cpp|c|cc|cxx|h|hh|hpp|ino"],
    C9Search: ["c9search_results"],
    Cirru: ["cirru|cr"],
    Clojure: ["clj|cljs"],
    Cobol: ["CBL|COB"],
    coffee: ["coffee|cf|cson|^Cakefile"],
    ColdFusion: ["cfm|cfc"],
    Crystal: ["cr"],
    CSharp: ["cs"],
    Csound_Document: ["csd"],
    Csound_Orchestra: ["orc"],
    Csound_Score: ["sco"],
    CSS: ["css"],
    Curly: ["curly"],
    Cuttlefish: ["conf"],
    D: ["d|di"],
    Dart: ["dart"],
    Diff: ["diff|patch"],
    Dockerfile: ["^Dockerfile"],
    Dot: ["dot"],
    Drools: ["drl"],
    Edifact: ["edi"],
    Eiffel: ["e|ge"],
    EJS: ["ejs"],
    Elixir: ["ex|exs"],
    Elm: ["elm"],
    Erlang: ["erl|hrl"],
    Forth: ["frt|fs|ldr|fth|4th"],
    Fortran: ["f|f90"],
    FSharp: ["fsi|fs|ml|mli|fsx|fsscript"],
    FSL: ["fsl"],
    FTL: ["ftl"],
    Gcode: ["gcode"],
    Gherkin: ["feature"],
    Gitignore: ["^.gitignore"],
    Glsl: ["glsl|frag|vert"],
    Gobstones: ["gbs"],
    golang: ["go"],
    GraphQLSchema: ["gql"],
    Groovy: ["groovy"],
    HAML: ["haml"],
    Handlebars: ["hbs|handlebars|tpl|mustache"],
    Haskell: ["hs"],
    Haskell_Cabal: ["cabal"],
    haXe: ["hx"],
    Hjson: ["hjson"],
    HTML: ["html|htm|xhtml|vue|we|wpy"],
    HTML_Elixir: ["eex|html.eex"],
    HTML_Ruby: ["erb|rhtml|html.erb"],
    INI: ["ini|conf|cfg|prefs"],
    Io: ["io"],
    Ion: ["ion"],
    Jack: ["jack"],
    Jade: ["jade|pug"],
    Java: ["java"],
    JavaScript: ["js|jsm|jsx|cjs|mjs"],
    JEXL: ["jexl"],
    JSON: ["json"],
    JSON5: ["json5"],
    JSONiq: ["jq"],
    JSP: ["jsp"],
    JSSM: ["jssm|jssm_state"],
    JSX: ["jsx"],
    Julia: ["jl"],
    Kotlin: ["kt|kts"],
    LaTeX: ["tex|latex|ltx|bib"],
    Latte: ["latte"],
    LESS: ["less"],
    Liquid: ["liquid"],
    Lisp: ["lisp"],
    LiveScript: ["ls"],
    Log: ["log"],
    LogiQL: ["logic|lql"],
    Logtalk: ["lgt"],
    LSL: ["lsl"],
    Lua: ["lua"],
    LuaPage: ["lp"],
    Lucene: ["lucene"],
    Makefile: ["^Makefile|^GNUmakefile|^makefile|^OCamlMakefile|make"],
    Markdown: ["md|markdown"],
    Mask: ["mask"],
    MATLAB: ["matlab"],
    Maze: ["mz"],
    MediaWiki: ["wiki|mediawiki"],
    MEL: ["mel"],
    MIPS: ["s|asm"],
    MIXAL: ["mixal"],
    MUSHCode: ["mc|mush"],
    MySQL: ["mysql"],
    Nginx: ["nginx|conf"],
    Nim: ["nim"],
    Nix: ["nix"],
    NSIS: ["nsi|nsh"],
    Nunjucks: ["nunjucks|nunjs|nj|njk"],
    ObjectiveC: ["m|mm"],
    OCaml: ["ml|mli"],
    Odin: ["odin"],
    PartiQL: ["partiql|pql"],
    Pascal: ["pas|p"],
    Perl: ["pl|pm"],
    pgSQL: ["pgsql"],
    PHP: ["php|inc|phtml|shtml|php3|php4|php5|phps|phpt|aw|ctp|module"],
    PHP_Laravel_blade: ["blade.php"],
    Pig: ["pig"],
    PLSQL: ["plsql"],
    Powershell: ["ps1"],
    Praat: ["praat|praatscript|psc|proc"],
    Prisma: ["prisma"],
    Prolog: ["plg|prolog"],
    Properties: ["properties"],
    Protobuf: ["proto"],
    PRQL: ["prql"],
    Puppet: ["epp|pp"],
    Python: ["py"],
    QML: ["qml"],
    R: ["r"],
    Raku: ["raku|rakumod|rakutest|p6|pl6|pm6"],
    Razor: ["cshtml|asp"],
    RDoc: ["Rd"],
    Red: ["red|reds"],
    RHTML: ["Rhtml"],
    Robot: ["robot|resource"],
    RST: ["rst"],
    Ruby: ["rb|ru|gemspec|rake|^Guardfile|^Rakefile|^Gemfile"],
    Rust: ["rs"],
    SaC: ["sac"],
    SASS: ["sass"],
    SCAD: ["scad"],
    Scala: ["scala|sbt"],
    Scheme: ["scm|sm|rkt|oak|scheme"],
    Scrypt: ["scrypt"],
    SCSS: ["scss"],
    SH: ["sh|bash|^.bashrc"],
    SJS: ["sjs"],
    Slim: ["slim|skim"],
    Smarty: ["smarty|tpl"],
    Smithy: ["smithy"],
    snippets: ["snippets"],
    Soy_Template: ["soy"],
    Space: ["space"],
    SPARQL: ["rq"],
    SQL: ["sql"],
    SQLServer: ["sqlserver"],
    Stylus: ["styl|stylus"],
    SVG: ["svg"],
    Swift: ["swift"],
    Tcl: ["tcl"],
    Terraform: ["tf", "tfvars", "terragrunt"],
    Tex: ["tex"],
    Text: ["txt"],
    Textile: ["textile"],
    Toml: ["toml"],
    TSX: ["tsx"],
    Turtle: ["ttl"],
    Twig: ["twig|swig"],
    Typescript: ["ts|mts|cts|typescript|str"],
    Vala: ["vala"],
    VBScript: ["vbs|vb"],
    Velocity: ["vm"],
    Verilog: ["v|vh|sv|svh"],
    VHDL: ["vhd|vhdl"],
    Visualforce: ["vfp|component|page"],
    Wollok: ["wlk|wpgm|wtest"],
    XML: ["xml|rdf|rss|wsdl|xslt|atom|mathml|mml|xul|xbl|xaml"],
    XQuery: ["xq"],
    YAML: ["yaml|yml"],
    Zeek: ["zeek|bro"],
    Django: ["html"]
};
var nameOverrides = {
    ObjectiveC: "Objective-C",
    CSharp: "C#",
    golang: "Go",
    C_Cpp: "C and C++",
    Csound_Document: "Csound Document",
    Csound_Orchestra: "Csound",
    Csound_Score: "Csound Score",
    coffee: "CoffeeScript",
    HTML_Ruby: "HTML (Ruby)",
    HTML_Elixir: "HTML (Elixir)",
    FTL: "FreeMarker",
    PHP_Laravel_blade: "PHP (Blade Template)",
    Perl6: "Perl 6",
    AutoHotKey: "AutoHotkey / AutoIt"
};
var modesByName = {};
for (var name in supportedModes) {
    var data = supportedModes[name];
    var displayName = (nameOverrides[name] || name).replace(/_/g, " ");
    var filename = name.toLowerCase();
    var mode = new Mode(filename, displayName, data[0]);
    modesByName[filename] = mode;
    modes.push(mode);
}
module.exports = {
    getModeForPath: getModeForPath,
    modes: modes,
    modesByName: modesByName
};

});

define("kitchen-sink/file_drop",["require","exports","module","ace/config","ace/lib/event","ace/ext/modelist","ace/editor"], function(require, exports, module) {var config = require("ace/config");
var event = require("ace/lib/event");
var modelist = require("ace/ext/modelist");

module.exports = function(editor) {
    event.addListener(editor.container, "dragover", function(e) {
        var types = e.dataTransfer.types;
        if (types && Array.prototype.indexOf.call(types, 'Files') !== -1)
            return event.preventDefault(e);
    });

    event.addListener(editor.container, "drop", function(e) {
        var file;
        try {
            file = e.dataTransfer.files[0];
            if (window.FileReader) {
                var reader = new FileReader();
                reader.onload = function() {
                    var mode = modelist.getModeForPath(file.name);
                    editor.session.doc.setValue(reader.result);
                    editor.session.setMode(mode.mode);
                    editor.session.modeName = mode.name;
                };
                reader.readAsText(file);
            }
            return event.preventDefault(e);
        } catch(err) {
            return event.stopEvent(e);
        }
    });
};

var Editor = require("ace/editor").Editor;
config.defineOptions(Editor.prototype, "editor", {
    loadDroppedFile: {
        set: function() { module.exports(this); },
        value: true
    }
});

});

define("ace/ext/whitespace",["require","exports","module","ace/lib/lang"], function(require, exports, module){"use strict";
var lang = require("../lib/lang");
exports.$detectIndentation = function (lines, fallback) {
    var stats = [];
    var changes = [];
    var tabIndents = 0;
    var prevSpaces = 0;
    var max = Math.min(lines.length, 1000);
    for (var i = 0; i < max; i++) {
        var line = lines[i];
        if (!/^\s*[^*+\-\s]/.test(line))
            continue;
        if (line[0] == "\t") {
            tabIndents++;
            prevSpaces = -Number.MAX_VALUE;
        }
        else {
            var spaces = line.match(/^ */)[0].length;
            if (spaces && line[spaces] != "\t") {
                var diff = spaces - prevSpaces;
                if (diff > 0 && !(prevSpaces % diff) && !(spaces % diff))
                    changes[diff] = (changes[diff] || 0) + 1;
                stats[spaces] = (stats[spaces] || 0) + 1;
            }
            prevSpaces = spaces;
        }
        while (i < max && line[line.length - 1] == "\\")
            line = lines[i++];
    }
    function getScore(indent) {
        var score = 0;
        for (var i = indent; i < stats.length; i += indent)
            score += stats[i] || 0;
        return score;
    }
    var changesTotal = changes.reduce(function (a, b) { return a + b; }, 0);
    var first = { score: 0, length: 0 };
    var spaceIndents = 0;
    for (var i = 1; i < 12; i++) {
        var score = getScore(i);
        if (i == 1) {
            spaceIndents = score;
            score = stats[1] ? 0.9 : 0.8;
            if (!stats.length)
                score = 0;
        }
        else
            score /= spaceIndents;
        if (changes[i])
            score += changes[i] / changesTotal;
        if (score > first.score)
            first = { score: score, length: i };
    }
    if (first.score && first.score > 1.4)
        var tabLength = first.length;
    if (tabIndents > spaceIndents + 1) {
        if (tabLength == 1 || spaceIndents < tabIndents / 4 || first.score < 1.8)
            tabLength = undefined;
        return { ch: "\t", length: tabLength };
    }
    if (spaceIndents > tabIndents + 1)
        return { ch: " ", length: tabLength };
};
exports.detectIndentation = function (session) {
    var lines = session.getLines(0, 1000);
    var indent = exports.$detectIndentation(lines) || {};
    if (indent.ch)
        session.setUseSoftTabs(indent.ch == " ");
    if (indent.length)
        session.setTabSize(indent.length);
    return indent;
};
exports.trimTrailingSpace = function (session, options) {
    var doc = session.getDocument();
    var lines = doc.getAllLines();
    var min = options && options.trimEmpty ? -1 : 0;
    var cursors = [], ci = -1;
    if (options && options.keepCursorPosition) {
        if (session.selection.rangeCount) {
            session.selection.rangeList.ranges.forEach(function (x, i, ranges) {
                var next = ranges[i + 1];
                if (next && next.cursor.row == x.cursor.row)
                    return;
                cursors.push(x.cursor);
            });
        }
        else {
            cursors.push(session.selection.getCursor());
        }
        ci = 0;
    }
    var cursorRow = cursors[ci] && cursors[ci].row;
    for (var i = 0, l = lines.length; i < l; i++) {
        var line = lines[i];
        var index = line.search(/\s+$/);
        if (i == cursorRow) {
            if (index < cursors[ci].column && index > min)
                index = cursors[ci].column;
            ci++;
            cursorRow = cursors[ci] ? cursors[ci].row : -1;
        }
        if (index > min)
            doc.removeInLine(i, index, line.length);
    }
};
exports.convertIndentation = function (session, ch, len) {
    var oldCh = session.getTabString()[0];
    var oldLen = session.getTabSize();
    if (!len)
        len = oldLen;
    if (!ch)
        ch = oldCh;
    var tab = ch == "\t" ? ch : lang.stringRepeat(ch, len);
    var doc = session.doc;
    var lines = doc.getAllLines();
    var cache = {};
    var spaceCache = {};
    for (var i = 0, l = lines.length; i < l; i++) {
        var line = lines[i];
        var match = line.match(/^\s*/)[0];
        if (match) {
            var w = session.$getStringScreenWidth(match)[0];
            var tabCount = Math.floor(w / oldLen);
            var reminder = w % oldLen;
            var toInsert = cache[tabCount] || (cache[tabCount] = lang.stringRepeat(tab, tabCount));
            toInsert += spaceCache[reminder] || (spaceCache[reminder] = lang.stringRepeat(" ", reminder));
            if (toInsert != match) {
                doc.removeInLine(i, 0, match.length);
                doc.insertInLine({ row: i, column: 0 }, toInsert);
            }
        }
    }
    session.setTabSize(len);
    session.setUseSoftTabs(ch == " ");
};
exports.$parseStringArg = function (text) {
    var indent = {};
    if (/t/.test(text))
        indent.ch = "\t";
    else if (/s/.test(text))
        indent.ch = " ";
    var m = text.match(/\d+/);
    if (m)
        indent.length = parseInt(m[0], 10);
    return indent;
};
exports.$parseArg = function (arg) {
    if (!arg)
        return {};
    if (typeof arg == "string")
        return exports.$parseStringArg(arg);
    if (typeof arg.text == "string")
        return exports.$parseStringArg(arg.text);
    return arg;
};
exports.commands = [{
        name: "detectIndentation",
        description: "Detect indentation from content",
        exec: function (editor) {
            exports.detectIndentation(editor.session);
        }
    }, {
        name: "trimTrailingSpace",
        description: "Trim trailing whitespace",
        exec: function (editor, args) {
            exports.trimTrailingSpace(editor.session, args);
        }
    }, {
        name: "convertIndentation",
        description: "Convert indentation to ...",
        exec: function (editor, arg) {
            var indent = exports.$parseArg(arg);
            exports.convertIndentation(editor.session, indent.ch, indent.length);
        }
    }, {
        name: "setIndentation",
        description: "Set indentation",
        exec: function (editor, arg) {
            var indent = exports.$parseArg(arg);
            indent.length && editor.session.setTabSize(indent.length);
            indent.ch && editor.session.setUseSoftTabs(indent.ch == " ");
        }
    }];

});

define("kitchen-sink/doclist",["require","exports","module","ace/edit_session","ace/undomanager","ace/lib/net","ace/ext/modelist"], function(require, exports, module) {"use strict";

var EditSession = require("ace/edit_session").EditSession;
var UndoManager = require("ace/undomanager").UndoManager;
var net = require("ace/lib/net");

var modelist = require("ace/ext/modelist");
var fileCache = {};

function initDoc(file, path, doc) {
    if (doc.prepare)
        file = doc.prepare(file);

    var session = new EditSession(file);
    session.setUndoManager(new UndoManager());
    doc.session = session;
    doc.path = path;
    session.name = doc.name;
    if (doc.wrapped) {
        session.setUseWrapMode(true);
        session.setWrapLimitRange(80, 80);
    }
    var mode = modelist.getModeForPath(path);
    session.modeName = mode.name;
    session.setMode(mode.mode);
    return session;
}


function makeHuge(txt) {
    for (var i = 0; i < 5; i++)
        txt += txt;
    return txt;
}

var docs = {
    "docs/javascript.js": {order: 1, name: "JavaScript"},

    "docs/latex.tex": {name: "LaTeX", wrapped: true},
    "docs/markdown.md": {name: "Markdown", wrapped: true},
    "docs/mushcode.mc": {name: "MUSHCode", wrapped: true},
    "docs/pgsql.pgsql": {name: "pgSQL", wrapped: true},
    "docs/plaintext.txt": {name: "Plain Text", prepare: makeHuge, wrapped: true},
    "docs/sql.sql": {name: "SQL", wrapped: true},

    "docs/textile.textile": {name: "Textile", wrapped: true},

    "docs/c9search.c9search_results": "C9 Search Results",
    "docs/mel.mel": "MEL",
};

var ownSource = {
};

var hugeDocs = require.toUrl ? {
    "build/src/ace.js": "",
    "build/src-min/ace.js": ""
} : {
    "src/ace.js": "",
    "src-min/ace.js": ""
};

modelist.modes.forEach(function(m) {
    var ext = m.extensions.split("|")[0];
    if (ext[0] === "^") {
        path = ext.substr(1);
    } else {
        var path = m.name + "." + ext;
    }
    path = "docs/" + path;
    if (!docs[path]) {
        docs[path] = {name: m.caption};
    } else if (typeof docs[path] == "object" && !docs[path].name) {
        docs[path].name = m.caption;
    }
});



if (window.define && window.define.modules) try {
    for (var path in window.define.modules) {
        if (path.indexOf("!") != -1)
            path = path.split("!").pop();
        else
            path = path + ".js";
        ownSource[path] = "";
    }
} catch(e) {}

function sort(list) {
    return list.sort(function(a, b) {
        var cmp = (b.order || 0) - (a.order || 0);
        return cmp || a.name && a.name.localeCompare(b.name);
    });
}

function prepareDocList(docs) {
    var list = [];
    for (var path in docs) {
        var doc = docs[path];
        if (typeof doc != "object")
            doc = {name: doc || path};

        doc.path = path;
        doc.desc = doc.name.replace(/^(ace|docs|demo|build)\//, "");
        if (doc.desc.length > 18)
            doc.desc = doc.desc.slice(0, 7) + ".." + doc.desc.slice(-9);

        fileCache[doc.name.toLowerCase()] = doc;
        list.push(doc);
    }

    return list;
}

function loadDoc(name, callback) {
    var doc = fileCache[name.toLowerCase()];
    if (!doc)
        return callback(null);

    if (doc.session)
        return callback(doc.session);
    var path = doc.path;
    var parts = path.split("/");
    if (parts[0] == "docs")
        path = "demo/kitchen-sink/" + path;
    else if (parts[0] == "ace")
        path = "src/" + parts.slice(1).join("/");

    net.get(path, function(x) {
        initDoc(x, path, doc);
        callback(doc.session);
    });
}

function saveDoc(name, callback) {
    var doc = name;
    if (typeof(name) === 'string') {
        doc = fileCache[name.toLowerCase()];
    }
    if (!doc || !doc.session)
        return callback("Unknown document: " + name);

    var path = doc.path;
    var parts = path.split("/");
    if (parts[0] == "docs")
        path = "demo/kitchen-sink/" + path;
    else if (parts[0] == "ace")
        path = "src/" + parts.slice(1).join("/");

    upload(path, doc.session.getValue(), callback);
}

function upload(url, data, callback) {
    var absUrl = net.qualifyURL(url);
    if (/^file:/.test(absUrl))
        absUrl = "http://localhost:8888/" + url;
    url = absUrl;
    if (!/^https?:/.test(url))
        return callback(new Error("Unsupported url scheme"));
    var xhr = new XMLHttpRequest();
    xhr.open("PUT", url, true);
    xhr.onreadystatechange = function () {
        if (xhr.readyState === 4) {
            callback(!/^2../.test(xhr.status));
        }
    };
    xhr.send(data);
}

module.exports = {
    fileCache: fileCache,
    docs: sort(prepareDocList(docs)),
    ownSource: prepareDocList(ownSource),
    hugeDocs: prepareDocList(hugeDocs),
    initDoc: initDoc,
    loadDoc: loadDoc,
    saveDoc: saveDoc
};
module.exports.all = {
    "Mode Examples": module.exports.docs,
    "Huge documents": module.exports.hugeDocs,
    "own source": module.exports.ownSource
};

});

define("kitchen-sink/layout",["require","exports","module","ace/lib/dom","ace/lib/event","ace/edit_session","ace/undomanager","ace/virtual_renderer","ace/editor","ace/multi_select","ace/theme/textmate"], function(require, exports, module) {"use strict";

var dom = require("ace/lib/dom");
var event = require("ace/lib/event");

var EditSession = require("ace/edit_session").EditSession;
var UndoManager = require("ace/undomanager").UndoManager;
var Renderer = require("ace/virtual_renderer").VirtualRenderer;
var Editor = require("ace/editor").Editor;
var MultiSelect = require("ace/multi_select").MultiSelect;

dom.importCssString("\
splitter {\
    border: 1px solid #C6C6D2;\
    width: 0px;\
    cursor: ew-resize;\
    z-index:10}\
splitter:hover {\
    margin-left: -2px;\
    width:3px;\
    border-color: #B5B4E0;\
}\
", "splitEditor");

exports.edit = function(el) {
    if (typeof(el) == "string")
        el = document.getElementById(el);

    var editor = new Editor(new Renderer(el, require("ace/theme/textmate")));

    editor.resize();
    event.addListener(window, "resize", function() {
        editor.resize();
    });
    return editor;
};


var SplitRoot = function(el, theme, position, getSize) {
    el.style.position = position || "relative";
    this.container = el;
    this.getSize = getSize || this.getSize;
    this.resize = this.$resize.bind(this);

    event.addListener(el.ownerDocument.defaultView, "resize", this.resize);
    this.editor = this.createEditor();
};

(function(){
    this.createEditor = function() {
        var el = document.createElement("div");
        el.className = this.$editorCSS;
        el.style.cssText = "position: absolute; top:0px; bottom:0px";
        this.$container.appendChild(el);
        var session = new EditSession("");
        var editor = new Editor(new Renderer(el, this.$theme));

        this.$editors.push(editor);
        editor.setFontSize(this.$fontSize);
        return editor;
    };
    this.$resize = function() {
        var size = this.getSize(this.container);
        this.rect = {
            x: size.left,
            y: size.top,
            w: size.width,
            h: size.height
        };
        this.item.resize(this.rect);
    };
    this.getSize = function(el) {
        return el.getBoundingClientRect();
    };
    this.destroy = function() {
        var win = this.container.ownerDocument.defaultView;
        event.removeListener(win, "resize", this.resize);
    };


}).call(SplitRoot.prototype);



var Split = function(){

};
(function(){
    this.execute = function(options) {
        this.$u.execute(options);
    };

}).call(Split.prototype);



exports.singleLineEditor = function(el) {
    var renderer = new Renderer(el);
    el.style.overflow = "hidden";

    renderer.screenToTextCoordinates = function(x, y) {
        var pos = this.pixelToScreenCoordinates(x, y);
        return this.session.screenToDocumentPosition(
            Math.min(this.session.getScreenLength() - 1, Math.max(pos.row, 0)),
            Math.max(pos.column, 0)
        );
    };

    renderer.$maxLines = 4;

    renderer.setStyle("ace_one-line");
    var editor = new Editor(renderer);
    editor.session.setUndoManager(new UndoManager());

    editor.setShowPrintMargin(false);
    editor.renderer.setShowGutter(false);
    editor.renderer.setHighlightGutterLine(false);
    editor.$mouseHandler.$focusWaitTimout = 0;

    return editor;
};

});

define("kitchen-sink/util",["require","exports","module","ace/lib/dom","ace/lib/event","ace/edit_session","ace/undomanager","ace/virtual_renderer","ace/editor","ace/multi_select"], function(require, exports, module) {"use strict";

var dom = require("ace/lib/dom");
var event = require("ace/lib/event");

var EditSession = require("ace/edit_session").EditSession;
var UndoManager = require("ace/undomanager").UndoManager;
var Renderer = require("ace/virtual_renderer").VirtualRenderer;
var Editor = require("ace/editor").Editor;
var MultiSelect = require("ace/multi_select").MultiSelect;

var urlOptions = {}
try {
    window.location.search.slice(1).split(/[&]/).forEach(function(e) {
        var parts = e.split("=");
        urlOptions[decodeURIComponent(parts[0])] = decodeURIComponent(parts[1]);
    });
} catch(e) {
    console.error(e);
}
exports.createEditor = function(el) {
    return new Editor(new Renderer(el));
};

exports.getOption = function(name) {
    if (urlOptions[name])
        return urlOptions[name];
    return localStorage && localStorage.getItem(name);
};

exports.saveOption = function(name, value) {
    if (value == false)
        value = "";
    localStorage && localStorage.setItem(name, value);
};

exports.createSplitEditor = function(el) {
    if (typeof(el) == "string")
        el = document.getElementById(el);

    var e0 = document.createElement("div");
    var s = document.createElement("splitter");
    var e1 = document.createElement("div");
    el.appendChild(e0);
    el.appendChild(e1);
    el.appendChild(s);
    e0.style.position = e1.style.position = s.style.position = "absolute";
    el.style.position = "relative";
    var split = {$container: el};

    split.editor0 = split[0] = new Editor(new Renderer(e0));
    split.editor1 = split[1] = new Editor(new Renderer(e1));
    split.splitter = s;

    s.ratio = 0.5;

    split.resize = function resize(){
        var height = el.parentNode.clientHeight - el.offsetTop;
        var total = el.clientWidth;
        var w1 = total * s.ratio;
        var w2 = total * (1- s.ratio);
        s.style.left = w1 - 1 + "px";
        s.style.height = el.style.height = height + "px";

        var st0 = split[0].container.style;
        var st1 = split[1].container.style;
        st0.width = w1 + "px";
        st1.width = w2 + "px";
        st0.left = 0 + "px";
        st1.left = w1 + "px";

        st0.top = st1.top = "0px";
        st0.height = st1.height = height + "px";

        split[0].resize();
        split[1].resize();
    };

    split.onMouseDown = function(e) {
        var rect = el.getBoundingClientRect();
        var x = e.clientX;
        var y = e.clientY;

        var button = e.button;
        if (button !== 0) {
            return;
        }

        var onMouseMove = function(e) {
            x = e.clientX;
            y = e.clientY;
        };
        var onResizeEnd = function(e) {
            clearInterval(timerId);
        };

        var onResizeInterval = function() {
            s.ratio = (x - rect.left) / rect.width;
            split.resize();
        };

        event.capture(s, onMouseMove, onResizeEnd);
        var timerId = setInterval(onResizeInterval, 40);

        return e.preventDefault();
    };



    event.addListener(s, "mousedown", split.onMouseDown);
    event.addListener(window, "resize", split.resize);
    split.resize();
    return split;
};
exports.stripLeadingComments = function(str) {
    if(str.slice(0,2)=='/*') {
        var j = str.indexOf('*/')+2;
        str = str.substr(j);
    }
    return str.trim() + "\n";
};
function saveOptionFromElement(el, val) {
    if (!el.onchange && !el.onclick)
        return;

    if ("checked" in el) {
        localStorage && localStorage.setItem(el.id, el.checked ? 1 : 0);
    }
    else {
        localStorage && localStorage.setItem(el.id, el.value);
    }
}

exports.bindCheckbox = function(id, callback, noInit) {
    if (typeof id == "string")
        var el = document.getElementById(id);
    else {
        var el = id;
        id = el.id;
    }
    var el = document.getElementById(id);
    
    if (urlOptions[id])
        el.checked = urlOptions[id] == "1";
    else if (localStorage && localStorage.getItem(id))
        el.checked = localStorage.getItem(id) == "1";

    var onCheck = function() {
        callback(!!el.checked);
        saveOptionFromElement(el);
    };
    el.onclick = onCheck;
    noInit || onCheck();
    return el;
};

exports.bindDropdown = function(id, callback, noInit) {
    if (typeof id == "string")
        var el = document.getElementById(id);
    else {
        var el = id;
        id = el.id;
    }
    
    if (urlOptions[id])
        el.value = urlOptions[id];
    else if (localStorage && localStorage.getItem(id))
        el.value = localStorage.getItem(id);

    var onChange = function() {
        callback(el.value);
        saveOptionFromElement(el);
    };

    el.onchange = onChange;
    noInit || onChange();
};

exports.fillDropdown = function(el, values) {
    if (typeof el == "string")
        el = document.getElementById(el);

    dropdown(values).forEach(function(e) {
        el.appendChild(e);
    });
};

function elt(tag, attributes, content) {
    var el = dom.createElement(tag);
    if (typeof content == "string") {
        el.appendChild(document.createTextNode(content));
    } else if (content) {
        content.forEach(function(ch) {
            el.appendChild(ch);
        });
    }

    for (var i in attributes)
        el.setAttribute(i, attributes[i]);
    return el;
}

function optgroup(values) {
    return values.map(function(item) {
        if (typeof item == "string")
            item = {name: item, caption: item};
        return elt("option", {value: item.value || item.name}, item.caption || item.desc);
    });
}

function dropdown(values) {
    if (Array.isArray(values))
        return optgroup(values);

    return Object.keys(values).map(function(i) {
        return elt("optgroup", {"label": i}, optgroup(values[i]));
    });
}

});

define("ace/ext/elastic_tabstops_lite",["require","exports","module","ace/editor","ace/config"], function(require, exports, module){"use strict";
var ElasticTabstopsLite = /** @class */ (function () {
    function ElasticTabstopsLite(editor) {
        this.$editor = editor;
        var self = this;
        var changedRows = [];
        var recordChanges = false;
        this.onAfterExec = function () {
            recordChanges = false;
            self.processRows(changedRows);
            changedRows = [];
        };
        this.onExec = function () {
            recordChanges = true;
        };
        this.onChange = function (delta) {
            if (recordChanges) {
                if (changedRows.indexOf(delta.start.row) == -1)
                    changedRows.push(delta.start.row);
                if (delta.end.row != delta.start.row)
                    changedRows.push(delta.end.row);
            }
        };
    }
    ElasticTabstopsLite.prototype.processRows = function (rows) {
        this.$inChange = true;
        var checkedRows = [];
        for (var r = 0, rowCount = rows.length; r < rowCount; r++) {
            var row = rows[r];
            if (checkedRows.indexOf(row) > -1)
                continue;
            var cellWidthObj = this.$findCellWidthsForBlock(row);
            var cellWidths = this.$setBlockCellWidthsToMax(cellWidthObj.cellWidths);
            var rowIndex = cellWidthObj.firstRow;
            for (var w = 0, l = cellWidths.length; w < l; w++) {
                var widths = cellWidths[w];
                checkedRows.push(rowIndex);
                this.$adjustRow(rowIndex, widths);
                rowIndex++;
            }
        }
        this.$inChange = false;
    };
    ElasticTabstopsLite.prototype.$findCellWidthsForBlock = function (row) {
        var cellWidths = [], widths;
        var rowIter = row;
        while (rowIter >= 0) {
            widths = this.$cellWidthsForRow(rowIter);
            if (widths.length == 0)
                break;
            cellWidths.unshift(widths);
            rowIter--;
        }
        var firstRow = rowIter + 1;
        rowIter = row;
        var numRows = this.$editor.session.getLength();
        while (rowIter < numRows - 1) {
            rowIter++;
            widths = this.$cellWidthsForRow(rowIter);
            if (widths.length == 0)
                break;
            cellWidths.push(widths);
        }
        return { cellWidths: cellWidths, firstRow: firstRow };
    };
    ElasticTabstopsLite.prototype.$cellWidthsForRow = function (row) {
        var selectionColumns = this.$selectionColumnsForRow(row);
        var tabs = [-1].concat(this.$tabsForRow(row));
        var widths = tabs.map(function (el) { return 0; }).slice(1);
        var line = this.$editor.session.getLine(row);
        for (var i = 0, len = tabs.length - 1; i < len; i++) {
            var leftEdge = tabs[i] + 1;
            var rightEdge = tabs[i + 1];
            var rightmostSelection = this.$rightmostSelectionInCell(selectionColumns, rightEdge);
            var cell = line.substring(leftEdge, rightEdge);
            widths[i] = Math.max(cell.replace(/\s+$/g, '').length, rightmostSelection - leftEdge);
        }
        return widths;
    };
    ElasticTabstopsLite.prototype.$selectionColumnsForRow = function (row) {
        var selections = [], cursor = this.$editor.getCursorPosition();
        if (this.$editor.session.getSelection().isEmpty()) {
            if (row == cursor.row)
                selections.push(cursor.column);
        }
        return selections;
    };
    ElasticTabstopsLite.prototype.$setBlockCellWidthsToMax = function (cellWidths) {
        var startingNewBlock = true, blockStartRow, blockEndRow, maxWidth;
        var columnInfo = this.$izip_longest(cellWidths);
        for (var c = 0, l = columnInfo.length; c < l; c++) {
            var column = columnInfo[c];
            if (!column.push) {
                console.error(column);
                continue;
            }
            column.push(NaN);
            for (var r = 0, s = column.length; r < s; r++) {
                var width = column[r];
                if (startingNewBlock) {
                    blockStartRow = r;
                    maxWidth = 0;
                    startingNewBlock = false;
                }
                if (isNaN(width)) {
                    blockEndRow = r;
                    for (var j = blockStartRow; j < blockEndRow; j++) {
                        cellWidths[j][c] = maxWidth;
                    }
                    startingNewBlock = true;
                }
                maxWidth = Math.max(maxWidth, width);
            }
        }
        return cellWidths;
    };
    ElasticTabstopsLite.prototype.$rightmostSelectionInCell = function (selectionColumns, cellRightEdge) {
        var rightmost = 0;
        if (selectionColumns.length) {
            var lengths = [];
            for (var s = 0, length = selectionColumns.length; s < length; s++) {
                if (selectionColumns[s] <= cellRightEdge)
                    lengths.push(s);
                else
                    lengths.push(0);
            }
            rightmost = Math.max.apply(Math, lengths);
        }
        return rightmost;
    };
    ElasticTabstopsLite.prototype.$tabsForRow = function (row) {
        var rowTabs = [], line = this.$editor.session.getLine(row), re = /\t/g, match;
        while ((match = re.exec(line)) != null) {
            rowTabs.push(match.index);
        }
        return rowTabs;
    };
    ElasticTabstopsLite.prototype.$adjustRow = function (row, widths) {
        var rowTabs = this.$tabsForRow(row);
        if (rowTabs.length == 0)
            return;
        var bias = 0, location = -1;
        var expandedSet = this.$izip(widths, rowTabs);
        for (var i = 0, l = expandedSet.length; i < l; i++) {
            var w = expandedSet[i][0], it = expandedSet[i][1];
            location += 1 + w;
            it += bias;
            var difference = location - it;
            if (difference == 0)
                continue;
            var partialLine = this.$editor.session.getLine(row).substr(0, it);
            var strippedPartialLine = partialLine.replace(/\s*$/g, "");
            var ispaces = partialLine.length - strippedPartialLine.length;
            if (difference > 0) {
                this.$editor.session.getDocument().insertInLine({ row: row, column: it + 1 }, Array(difference + 1).join(" ") + "\t");
                this.$editor.session.getDocument().removeInLine(row, it, it + 1);
                bias += difference;
            }
            if (difference < 0 && ispaces >= -difference) {
                this.$editor.session.getDocument().removeInLine(row, it + difference, it);
                bias += difference;
            }
        }
    };
    ElasticTabstopsLite.prototype.$izip_longest = function (iterables) {
        if (!iterables[0])
            return [];
        var longest = iterables[0].length;
        var iterablesLength = iterables.length;
        for (var i = 1; i < iterablesLength; i++) {
            var iLength = iterables[i].length;
            if (iLength > longest)
                longest = iLength;
        }
        var expandedSet = [];
        for (var l = 0; l < longest; l++) {
            var set = [];
            for (var i = 0; i < iterablesLength; i++) {
                if (iterables[i][l] === "")
                    set.push(NaN);
                else
                    set.push(iterables[i][l]);
            }
            expandedSet.push(set);
        }
        return expandedSet;
    };
    ElasticTabstopsLite.prototype.$izip = function (widths, tabs) {
        var size = widths.length >= tabs.length ? tabs.length : widths.length;
        var expandedSet = [];
        for (var i = 0; i < size; i++) {
            var set = [widths[i], tabs[i]];
            expandedSet.push(set);
        }
        return expandedSet;
    };
    return ElasticTabstopsLite;
}());
exports.ElasticTabstopsLite = ElasticTabstopsLite;
var Editor = require("../editor").Editor;
require("../config").defineOptions(Editor.prototype, "editor", {
    useElasticTabstops: {
        set: function (val) {
            if (val) {
                if (!this.elasticTabstops)
                    this.elasticTabstops = new ElasticTabstopsLite(this);
                this.commands.on("afterExec", this.elasticTabstops.onAfterExec);
                this.commands.on("exec", this.elasticTabstops.onExec);
                this.on("change", this.elasticTabstops.onChange);
            }
            else if (this.elasticTabstops) {
                this.commands.removeListener("afterExec", this.elasticTabstops.onAfterExec);
                this.commands.removeListener("exec", this.elasticTabstops.onExec);
                this.removeListener("change", this.elasticTabstops.onChange);
            }
        }
    }
});

});

define("ace/occur",["require","exports","module","ace/lib/oop","ace/search","ace/edit_session","ace/search_highlight","ace/lib/dom"], function(require, exports, module){"use strict";
var __extends = (this && this.__extends) || (function () {
    var extendStatics = function (d, b) {
        extendStatics = Object.setPrototypeOf ||
            ({ __proto__: [] } instanceof Array && function (d, b) { d.__proto__ = b; }) ||
            function (d, b) { for (var p in b) if (Object.prototype.hasOwnProperty.call(b, p)) d[p] = b[p]; };
        return extendStatics(d, b);
    };
    return function (d, b) {
        if (typeof b !== "function" && b !== null)
            throw new TypeError("Class extends value " + String(b) + " is not a constructor or null");
        extendStatics(d, b);
        function __() { this.constructor = d; }
        d.prototype = b === null ? Object.create(b) : (__.prototype = b.prototype, new __());
    };
})();
var oop = require("./lib/oop");
var Search = require("./search").Search;
var EditSession = require("./edit_session").EditSession;
var SearchHighlight = require("./search_highlight").SearchHighlight;
var Occur = /** @class */ (function (_super) {
    __extends(Occur, _super);
    function Occur() {
        return _super !== null && _super.apply(this, arguments) || this;
    }
    Occur.prototype.enter = function (editor, options) {
        if (!options.needle)
            return false;
        var pos = editor.getCursorPosition();
        this.displayOccurContent(editor, options);
        var translatedPos = this.originalToOccurPosition(editor.session, pos);
        editor.moveCursorToPosition(translatedPos);
        return true;
    };
    Occur.prototype.exit = function (editor, options) {
        var pos = options.translatePosition && editor.getCursorPosition();
        var translatedPos = pos && this.occurToOriginalPosition(editor.session, pos);
        this.displayOriginalContent(editor);
        if (translatedPos)
            editor.moveCursorToPosition(translatedPos);
        return true;
    };
    Occur.prototype.highlight = function (sess, regexp) {
        var hl = sess.$occurHighlight = sess.$occurHighlight || sess.addDynamicMarker(new SearchHighlight(null, "ace_occur-highlight", "text"));
        hl.setRegexp(regexp);
        sess._emit("changeBackMarker"); // force highlight layer redraw
    };
    Occur.prototype.displayOccurContent = function (editor, options) {
        this.$originalSession = editor.session;
        var found = this.matchingLines(editor.session, options);
        var lines = found.map(function (foundLine) { return foundLine.content; });
        var occurSession = new EditSession(lines.join('\n'));
        occurSession.$occur = this;
        occurSession.$occurMatchingLines = found;
        editor.setSession(occurSession);
        this.$useEmacsStyleLineStart = this.$originalSession.$useEmacsStyleLineStart;
        occurSession.$useEmacsStyleLineStart = this.$useEmacsStyleLineStart;
        this.highlight(occurSession, options.re);
        occurSession._emit('changeBackMarker');
    };
    Occur.prototype.displayOriginalContent = function (editor) {
        editor.setSession(this.$originalSession);
        this.$originalSession.$useEmacsStyleLineStart = this.$useEmacsStyleLineStart;
    };
    Occur.prototype.originalToOccurPosition = function (session, pos) {
        var lines = session.$occurMatchingLines;
        var nullPos = { row: 0, column: 0 };
        if (!lines)
            return nullPos;
        for (var i = 0; i < lines.length; i++) {
            if (lines[i].row === pos.row)
                return { row: i, column: pos.column };
        }
        return nullPos;
    };
    Occur.prototype.occurToOriginalPosition = function (session, pos) {
        var lines = session.$occurMatchingLines;
        if (!lines || !lines[pos.row])
            return pos;
        return { row: lines[pos.row].row, column: pos.column };
    };
    Occur.prototype.matchingLines = function (session, options) {
        options = oop.mixin({}, options);
        if (!session || !options.needle)
            return [];
        var search = new Search();
        search.set(options);
        return search.findAll(session).reduce(function (lines, range) {
            var row = range.start.row;
            var last = lines[lines.length - 1];
            return last && last.row === row ?
                lines :
                lines.concat({ row: row, content: session.getLine(row) });
        }, []);
    };
    return Occur;
}(Search));
var dom = require('./lib/dom');
dom.importCssString(".ace_occur-highlight {\n\
    border-radius: 4px;\n\
    background-color: rgba(87, 255, 8, 0.25);\n\
    position: absolute;\n\
    z-index: 4;\n\
    box-sizing: border-box;\n\
    box-shadow: 0 0 4px rgb(91, 255, 50);\n\
}\n\
.ace_dark .ace_occur-highlight {\n\
    background-color: rgb(80, 140, 85);\n\
    box-shadow: 0 0 4px rgb(60, 120, 70);\n\
}\n", "incremental-occur-highlighting", false);
exports.Occur = Occur;

});

define("ace/commands/occur_commands",["require","exports","module","ace/config","ace/occur","ace/keyboard/hash_handler","ace/lib/oop"], function(require, exports, module){var config = require("../config"), Occur = require("../occur").Occur;
var occurStartCommand = {
    name: "occur",
    exec: function (editor, options) {
        var alreadyInOccur = !!editor.session.$occur;
        var occurSessionActive = new Occur().enter(editor, options);
        if (occurSessionActive && !alreadyInOccur)
            OccurKeyboardHandler.installIn(editor);
    },
    readOnly: true
};
var occurCommands = [{
        name: "occurexit",
        bindKey: 'esc|Ctrl-G',
        exec: function (editor) {
            var occur = editor.session.$occur;
            if (!occur)
                return;
            occur.exit(editor, {});
            if (!editor.session.$occur)
                OccurKeyboardHandler.uninstallFrom(editor);
        },
        readOnly: true
    }, {
        name: "occuraccept",
        bindKey: 'enter',
        exec: function (editor) {
            var occur = editor.session.$occur;
            if (!occur)
                return;
            occur.exit(editor, { translatePosition: true });
            if (!editor.session.$occur)
                OccurKeyboardHandler.uninstallFrom(editor);
        },
        readOnly: true
    }];
var HashHandler = require("../keyboard/hash_handler").HashHandler;
var oop = require("../lib/oop");
function OccurKeyboardHandler() { }
oop.inherits(OccurKeyboardHandler, HashHandler);
(function () {
    this.isOccurHandler = true;
    this.attach = function (editor) {
        HashHandler.call(this, occurCommands, editor.commands.platform);
        this.$editor = editor;
    };
    var handleKeyboard$super = this.handleKeyboard;
    this.handleKeyboard = function (data, hashId, key, keyCode) {
        var cmd = handleKeyboard$super.call(this, data, hashId, key, keyCode);
        return (cmd && cmd.command) ? cmd : undefined;
    };
}).call(OccurKeyboardHandler.prototype);
OccurKeyboardHandler.installIn = function (editor) {
    var handler = new this();
    editor.keyBinding.addKeyboardHandler(handler);
    editor.commands.addCommands(occurCommands);
};
OccurKeyboardHandler.uninstallFrom = function (editor) {
    editor.commands.removeCommands(occurCommands);
    var handler = editor.getKeyboardHandler();
    if (handler.isOccurHandler)
        editor.keyBinding.removeKeyboardHandler(handler);
};
exports.occurStartCommand = occurStartCommand;

});

define("ace/commands/incremental_search_commands",["require","exports","module","ace/config","ace/lib/oop","ace/keyboard/hash_handler","ace/commands/occur_commands"], function(require, exports, module){var config = require("../config");
var oop = require("../lib/oop");
var HashHandler = require("../keyboard/hash_handler").HashHandler;
var occurStartCommand = require("./occur_commands").occurStartCommand;
exports.iSearchStartCommands = [{
        name: "iSearch",
        bindKey: { win: "Ctrl-F", mac: "Command-F" },
        exec: function (editor, options) {
            config.loadModule(["core", "ace/incremental_search"], function (e) {
                var iSearch = e.iSearch = e.iSearch || new e.IncrementalSearch();
                iSearch.activate(editor, options.backwards);
                if (options.jumpToFirstMatch)
                    iSearch.next(options);
            });
        },
        readOnly: true
    }, {
        name: "iSearchBackwards",
        exec: function (editor, jumpToNext) { editor.execCommand('iSearch', { backwards: true }); },
        readOnly: true
    }, {
        name: "iSearchAndGo",
        bindKey: { win: "Ctrl-K", mac: "Command-G" },
        exec: function (editor, jumpToNext) { editor.execCommand('iSearch', { jumpToFirstMatch: true, useCurrentOrPrevSearch: true }); },
        readOnly: true
    }, {
        name: "iSearchBackwardsAndGo",
        bindKey: { win: "Ctrl-Shift-K", mac: "Command-Shift-G" },
        exec: function (editor) { editor.execCommand('iSearch', { jumpToFirstMatch: true, backwards: true, useCurrentOrPrevSearch: true }); },
        readOnly: true
    }];
exports.iSearchCommands = [{
        name: "restartSearch",
        bindKey: { win: "Ctrl-F", mac: "Command-F" },
        exec: function (iSearch) {
            iSearch.cancelSearch(true);
        }
    }, {
        name: "searchForward",
        bindKey: { win: "Ctrl-S|Ctrl-K", mac: "Ctrl-S|Command-G" },
        exec: function (iSearch, options) {
            options.useCurrentOrPrevSearch = true;
            iSearch.next(options);
        }
    }, {
        name: "searchBackward",
        bindKey: { win: "Ctrl-R|Ctrl-Shift-K", mac: "Ctrl-R|Command-Shift-G" },
        exec: function (iSearch, options) {
            options.useCurrentOrPrevSearch = true;
            options.backwards = true;
            iSearch.next(options);
        }
    }, {
        name: "extendSearchTerm",
        exec: function (iSearch, string) {
            iSearch.addString(string);
        }
    }, {
        name: "extendSearchTermSpace",
        bindKey: "space",
        exec: function (iSearch) { iSearch.addString(' '); }
    }, {
        name: "shrinkSearchTerm",
        bindKey: "backspace",
        exec: function (iSearch) {
            iSearch.removeChar();
        }
    }, {
        name: 'confirmSearch',
        bindKey: 'return',
        exec: function (iSearch) { iSearch.deactivate(); }
    }, {
        name: 'cancelSearch',
        bindKey: 'esc|Ctrl-G',
        exec: function (iSearch) { iSearch.deactivate(true); }
    }, {
        name: 'occurisearch',
        bindKey: 'Ctrl-O',
        exec: function (iSearch) {
            var options = oop.mixin({}, iSearch.$options);
            iSearch.deactivate();
            occurStartCommand.exec(iSearch.$editor, options);
        }
    }, {
        name: "yankNextWord",
        bindKey: "Ctrl-w",
        exec: function (iSearch) {
            var ed = iSearch.$editor, range = ed.selection.getRangeOfMovements(function (sel) { sel.moveCursorWordRight(); }), string = ed.session.getTextRange(range);
            iSearch.addString(string);
        }
    }, {
        name: "yankNextChar",
        bindKey: "Ctrl-Alt-y",
        exec: function (iSearch) {
            var ed = iSearch.$editor, range = ed.selection.getRangeOfMovements(function (sel) { sel.moveCursorRight(); }), string = ed.session.getTextRange(range);
            iSearch.addString(string);
        }
    }, {
        name: 'recenterTopBottom',
        bindKey: 'Ctrl-l',
        exec: function (iSearch) { iSearch.$editor.execCommand('recenterTopBottom'); }
    }, {
        name: 'selectAllMatches',
        bindKey: 'Ctrl-space',
        exec: function (iSearch) {
            var ed = iSearch.$editor, hl = ed.session.$isearchHighlight, ranges = hl && hl.cache ? hl.cache
                .reduce(function (ranges, ea) {
                return ranges.concat(ea ? ea : []);
            }, []) : [];
            iSearch.deactivate(false);
            ranges.forEach(ed.selection.addRange.bind(ed.selection));
        }
    }, {
        name: 'searchAsRegExp',
        bindKey: 'Alt-r',
        exec: function (iSearch) {
            iSearch.convertNeedleToRegExp();
        }
    }].map(function (cmd) {
    cmd.readOnly = true;
    cmd.isIncrementalSearchCommand = true;
    cmd.scrollIntoView = "animate-cursor";
    return cmd;
});
function IncrementalSearchKeyboardHandler(iSearch) {
    this.$iSearch = iSearch;
}
oop.inherits(IncrementalSearchKeyboardHandler, HashHandler);
(function () {
    this.attach = function (editor) {
        var iSearch = this.$iSearch;
        HashHandler.call(this, exports.iSearchCommands, editor.commands.platform);
        this.$commandExecHandler = editor.commands.on('exec', function (e) {
            if (!e.command.isIncrementalSearchCommand)
                return iSearch.deactivate();
            e.stopPropagation();
            e.preventDefault();
            var scrollTop = editor.session.getScrollTop();
            var result = e.command.exec(iSearch, e.args || {});
            editor.renderer.scrollCursorIntoView(null, 0.5);
            editor.renderer.animateScrolling(scrollTop);
            return result;
        });
    };
    this.detach = function (editor) {
        if (!this.$commandExecHandler)
            return;
        editor.commands.off('exec', this.$commandExecHandler);
        delete this.$commandExecHandler;
    };
    var handleKeyboard$super = this.handleKeyboard;
    this.handleKeyboard = function (data, hashId, key, keyCode) {
        if (((hashId === 1 /*ctrl*/ || hashId === 8 /*command*/) && key === 'v')
            || (hashId === 1 /*ctrl*/ && key === 'y'))
            return null;
        var cmd = handleKeyboard$super.call(this, data, hashId, key, keyCode);
        if (cmd && cmd.command) {
            return cmd;
        }
        if (hashId == -1) {
            var extendCmd = this.commands.extendSearchTerm;
            if (extendCmd) {
                return { command: extendCmd, args: key };
            }
        }
        return false;
    };
}).call(IncrementalSearchKeyboardHandler.prototype);
exports.IncrementalSearchKeyboardHandler = IncrementalSearchKeyboardHandler;

});

define("ace/incremental_search",["require","exports","module","ace/range","ace/search","ace/search_highlight","ace/commands/incremental_search_commands","ace/lib/dom","ace/commands/command_manager","ace/editor","ace/config"], function(require, exports, module){"use strict";
var __extends = (this && this.__extends) || (function () {
    var extendStatics = function (d, b) {
        extendStatics = Object.setPrototypeOf ||
            ({ __proto__: [] } instanceof Array && function (d, b) { d.__proto__ = b; }) ||
            function (d, b) { for (var p in b) if (Object.prototype.hasOwnProperty.call(b, p)) d[p] = b[p]; };
        return extendStatics(d, b);
    };
    return function (d, b) {
        if (typeof b !== "function" && b !== null)
            throw new TypeError("Class extends value " + String(b) + " is not a constructor or null");
        extendStatics(d, b);
        function __() { this.constructor = d; }
        d.prototype = b === null ? Object.create(b) : (__.prototype = b.prototype, new __());
    };
})();
var Range = require("./range").Range;
var Search = require("./search").Search;
var SearchHighlight = require("./search_highlight").SearchHighlight;
var iSearchCommandModule = require("./commands/incremental_search_commands");
var ISearchKbd = iSearchCommandModule.IncrementalSearchKeyboardHandler;
function isRegExp(obj) {
    return obj instanceof RegExp;
}
function regExpToObject(re) {
    var string = String(re), start = string.indexOf('/'), flagStart = string.lastIndexOf('/');
    return {
        expression: string.slice(start + 1, flagStart),
        flags: string.slice(flagStart + 1)
    };
}
function stringToRegExp(string, flags) {
    try {
        return new RegExp(string, flags);
    }
    catch (e) {
        return string;
    }
}
function objectToRegExp(obj) {
    return stringToRegExp(obj.expression, obj.flags);
}
var IncrementalSearch = /** @class */ (function (_super) {
    __extends(IncrementalSearch, _super);
    function IncrementalSearch() {
        var _this = _super.call(this) || this;
        _this.$options = { wrap: false, skipCurrent: false };
        _this.$keyboardHandler = new ISearchKbd(_this);
        return _this;
    }
    IncrementalSearch.prototype.activate = function (editor, backwards) {
        this.$editor = editor;
        this.$startPos = this.$currentPos = editor.getCursorPosition();
        this.$options.needle = '';
        this.$options.backwards = backwards;
        editor.keyBinding.addKeyboardHandler(this.$keyboardHandler);
        this.$originalEditorOnPaste = editor.onPaste;
        editor.onPaste = this.onPaste.bind(this);
        this.$mousedownHandler = editor.on('mousedown', this.onMouseDown.bind(this));
        this.selectionFix(editor);
        this.statusMessage(true);
    };
    IncrementalSearch.prototype.deactivate = function (reset) {
        this.cancelSearch(reset);
        var editor = this.$editor;
        editor.keyBinding.removeKeyboardHandler(this.$keyboardHandler);
        if (this.$mousedownHandler) {
            editor.off('mousedown', this.$mousedownHandler);
            delete this.$mousedownHandler;
        }
        editor.onPaste = this.$originalEditorOnPaste;
        this.message('');
    };
    IncrementalSearch.prototype.selectionFix = function (editor) {
        if (editor.selection.isEmpty() && !editor.session.$emacsMark) {
            editor.clearSelection();
        }
    };
    IncrementalSearch.prototype.highlight = function (regexp) {
        var sess = this.$editor.session, hl = sess.$isearchHighlight = sess.$isearchHighlight || sess.addDynamicMarker(new SearchHighlight(null, "ace_isearch-result", "text"));
        hl.setRegexp(regexp);
        sess._emit("changeBackMarker"); // force highlight layer redraw
    };
    IncrementalSearch.prototype.cancelSearch = function (reset) {
        var e = this.$editor;
        this.$prevNeedle = this.$options.needle;
        this.$options.needle = '';
        if (reset) {
            e.moveCursorToPosition(this.$startPos);
            this.$currentPos = this.$startPos;
        }
        else {
            e.pushEmacsMark && e.pushEmacsMark(this.$startPos, false);
        }
        this.highlight(null);
        return Range.fromPoints(this.$currentPos, this.$currentPos);
    };
    IncrementalSearch.prototype.highlightAndFindWithNeedle = function (moveToNext, needleUpdateFunc) {
        if (!this.$editor)
            return null;
        var options = this.$options;
        if (needleUpdateFunc) {
            options.needle = needleUpdateFunc.call(this, options.needle || '') || '';
        }
        if (options.needle.length === 0) {
            this.statusMessage(true);
            return this.cancelSearch(true);
        }
        options.start = this.$currentPos;
        var session = this.$editor.session, found = this.find(session), shouldSelect = this.$editor.emacsMark ?
            !!this.$editor.emacsMark() : !this.$editor.selection.isEmpty();
        if (found) {
            if (options.backwards)
                found = Range.fromPoints(found.end, found.start);
            this.$editor.selection.setRange(Range.fromPoints(shouldSelect ? this.$startPos : found.end, found.end));
            if (moveToNext)
                this.$currentPos = found.end;
            this.highlight(options.re);
        }
        this.statusMessage(found);
        return found;
    };
    IncrementalSearch.prototype.addString = function (s) {
        return this.highlightAndFindWithNeedle(false, function (needle) {
            if (!isRegExp(needle))
                return needle + s;
            var reObj = regExpToObject(needle);
            reObj.expression += s;
            return objectToRegExp(reObj);
        });
    };
    IncrementalSearch.prototype.removeChar = function (c) {
        return this.highlightAndFindWithNeedle(false, function (needle) {
            if (!isRegExp(needle))
                return needle.substring(0, needle.length - 1);
            var reObj = regExpToObject(needle);
            reObj.expression = reObj.expression.substring(0, reObj.expression.length - 1);
            return objectToRegExp(reObj);
        });
    };
    IncrementalSearch.prototype.next = function (options) {
        options = options || {};
        this.$options.backwards = !!options.backwards;
        this.$currentPos = this.$editor.getCursorPosition();
        return this.highlightAndFindWithNeedle(true, function (needle) {
            return options.useCurrentOrPrevSearch && needle.length === 0 ?
                this.$prevNeedle || '' : needle;
        });
    };
    IncrementalSearch.prototype.onMouseDown = function (evt) {
        this.deactivate();
        return true;
    };
    IncrementalSearch.prototype.onPaste = function (text) {
        this.addString(text);
    };
    IncrementalSearch.prototype.convertNeedleToRegExp = function () {
        return this.highlightAndFindWithNeedle(false, function (needle) {
            return isRegExp(needle) ? needle : stringToRegExp(needle, 'ig');
        });
    };
    IncrementalSearch.prototype.convertNeedleToString = function () {
        return this.highlightAndFindWithNeedle(false, function (needle) {
            return isRegExp(needle) ? regExpToObject(needle).expression : needle;
        });
    };
    IncrementalSearch.prototype.statusMessage = function (found) {
        var options = this.$options, msg = '';
        msg += options.backwards ? 'reverse-' : '';
        msg += 'isearch: ' + options.needle;
        msg += found ? '' : ' (not found)';
        this.message(msg);
    };
    IncrementalSearch.prototype.message = function (msg) {
        if (this.$editor.showCommandLine) {
            this.$editor.showCommandLine(msg);
            this.$editor.focus();
        }
    };
    return IncrementalSearch;
}(Search));
exports.IncrementalSearch = IncrementalSearch;
var dom = require('./lib/dom');
dom.importCssString("\n.ace_marker-layer .ace_isearch-result {\n  position: absolute;\n  z-index: 6;\n  box-sizing: border-box;\n}\ndiv.ace_isearch-result {\n  border-radius: 4px;\n  background-color: rgba(255, 200, 0, 0.5);\n  box-shadow: 0 0 4px rgb(255, 200, 0);\n}\n.ace_dark div.ace_isearch-result {\n  background-color: rgb(100, 110, 160);\n  box-shadow: 0 0 4px rgb(80, 90, 140);\n}", "incremental-search-highlighting", false);
var commands = require("./commands/command_manager");
(function () {
    this.setupIncrementalSearch = function (editor, val) {
        if (this.usesIncrementalSearch == val)
            return;
        this.usesIncrementalSearch = val;
        var iSearchCommands = iSearchCommandModule.iSearchStartCommands;
        var method = val ? 'addCommands' : 'removeCommands';
        this[method](iSearchCommands);
    };
}).call(commands.CommandManager.prototype);
var Editor = require("./editor").Editor;
require("./config").defineOptions(Editor.prototype, "editor", {
    useIncrementalSearch: {
        set: function (val) {
            this.keyBinding.$handlers.forEach(function (handler) {
                if (handler.setupIncrementalSearch) {
                    handler.setupIncrementalSearch(this, val);
                }
            });
            this._emit('incrementalSearchSettingChanged', { isEnabled: val });
        }
    }
});

});

define("kitchen-sink/token_tooltip",["require","exports","module","ace/lib/dom","ace/lib/oop","ace/lib/event","ace/range","ace/tooltip"], function(require, exports, module) {"use strict";

var dom = require("ace/lib/dom");
var oop = require("ace/lib/oop");
var event = require("ace/lib/event");
var Range = require("ace/range").Range;
var Tooltip = require("ace/tooltip").Tooltip;

class TokenTooltip extends Tooltip {
    constructor(editor) {
        if (editor.tokenTooltip)
            return;
        super(editor.container);
        editor.tokenTooltip = this;
        this.editor = editor;

        this.update = this.update.bind(this);
        this.onMouseMove = this.onMouseMove.bind(this);
        this.onMouseOut = this.onMouseOut.bind(this);
        event.addListener(editor.renderer.scroller, "mousemove", this.onMouseMove);
        event.addListener(editor.renderer.content, "mouseout", this.onMouseOut);
    }
    token = {};
    
    range = new Range();
    
    update() {
        this.$timer = null;
        
        var r = this.editor.renderer;
        if (this.lastT - (r.timeStamp || 0) > 1000) {
            r.rect = null;
            r.timeStamp = this.lastT;
            this.maxHeight = window.innerHeight;
            this.maxWidth = window.innerWidth;
        }

        var canvasPos = r.rect || (r.rect = r.scroller.getBoundingClientRect());
        var offset = (this.x + r.scrollLeft - canvasPos.left - r.$padding) / r.characterWidth;
        var row = Math.floor((this.y + r.scrollTop - canvasPos.top) / r.lineHeight);
        var col = Math.round(offset);

        var screenPos = {row: row, column: col, side: offset - col > 0 ? 1 : -1};
        var session = this.editor.session;
        var docPos = session.screenToDocumentPosition(screenPos.row, screenPos.column);
        var token = session.getTokenAt(docPos.row, docPos.column);

        if (!token && !session.getLine(docPos.row)) {
            token = {
                type: "",
                value: "",
                state: session.bgTokenizer.getState(0)
            };
        }
        if (!token) {
            session.removeMarker(this.marker);
            this.hide();
            return;
        }

        var tokenText = token.type;
        if (token.state)
            tokenText += "|" + token.state;
        if (token.merge)
            tokenText += "\n  merge";
        if (token.stateTransitions)
            tokenText += "\n  " + token.stateTransitions.join("\n  ");

        if (this.tokenText != tokenText) {
            this.setText(tokenText);
            this.width = this.getWidth();
            this.height = this.getHeight();
            this.tokenText = tokenText;
        }
        if (!this.isOpen) {
            this.setTheme(r.theme);
        }

        this.show(null, this.x, this.y);

        this.token = token;
        session.removeMarker(this.marker);
        this.range = new Range(docPos.row, token.start, docPos.row, token.start + token.value.length);
        this.marker = session.addMarker(this.range, "ace_bracket", "text");
    };
    
    onMouseMove(e) {
        this.x = e.clientX;
        this.y = e.clientY;
        if (this.isOpen) {
            this.lastT = e.timeStamp;
            this.setPosition(this.x, this.y);
        }
        if (!this.$timer)
            this.$timer = setTimeout(this.update, 100);
    };

    onMouseOut(e) {
        if (e && e.currentTarget.contains(e.relatedTarget))
            return;
        this.hide();
        this.editor.session.removeMarker(this.marker);
        this.$timer = clearTimeout(this.$timer);
    };

    setPosition(x, y) {
        if (x + 10 + this.width > this.maxWidth)
            x = window.innerWidth - this.width - 10;
        if (y > window.innerHeight * 0.75 || y + 20 + this.height > this.maxHeight)
            y = y - this.height - 30;

        Tooltip.prototype.setPosition.call(this, x + 10, y + 20);
    };

    destroy() {
        this.onMouseOut();
        event.removeListener(this.editor.renderer.scroller, "mousemove", this.onMouseMove);
        event.removeListener(this.editor.renderer.content, "mouseout", this.onMouseOut);
        delete this.editor.tokenTooltip;
    };

}

exports.TokenTooltip = TokenTooltip;

});

define("ace/marker_group",["require","exports","module"], function(require, exports, module){"use strict";
var MarkerGroup = /** @class */ (function () {
    function MarkerGroup(session) {
        this.markers = [];
        this.session = session;
        session.addDynamicMarker(this);
    }
    MarkerGroup.prototype.getMarkerAtPosition = function (pos) {
        return this.markers.find(function (marker) {
            return marker.range.contains(pos.row, pos.column);
        });
    };
    MarkerGroup.prototype.markersComparator = function (a, b) {
        return a.range.start.row - b.range.start.row;
    };
    MarkerGroup.prototype.setMarkers = function (markers) {
        this.markers = markers.sort(this.markersComparator).slice(0, this.MAX_MARKERS);
        this.session._signal("changeBackMarker");
    };
    MarkerGroup.prototype.update = function (html, markerLayer, session, config) {
        if (!this.markers || !this.markers.length)
            return;
        var visibleRangeStartRow = config.firstRow, visibleRangeEndRow = config.lastRow;
        var foldLine;
        var markersOnOneLine = 0;
        var lastRow = 0;
        for (var i = 0; i < this.markers.length; i++) {
            var marker = this.markers[i];
            if (marker.range.end.row < visibleRangeStartRow)
                continue;
            if (marker.range.start.row > visibleRangeEndRow)
                continue;
            if (marker.range.start.row === lastRow) {
                markersOnOneLine++;
            }
            else {
                lastRow = marker.range.start.row;
                markersOnOneLine = 0;
            }
            if (markersOnOneLine > 200) {
                continue;
            }
            var markerVisibleRange = marker.range.clipRows(visibleRangeStartRow, visibleRangeEndRow);
            if (markerVisibleRange.start.row === markerVisibleRange.end.row
                && markerVisibleRange.start.column === markerVisibleRange.end.column) {
                continue; // visible range is empty
            }
            var screenRange = markerVisibleRange.toScreenRange(session);
            if (screenRange.isEmpty()) {
                foldLine = session.getNextFoldLine(markerVisibleRange.end.row, foldLine);
                if (foldLine && foldLine.end.row > markerVisibleRange.end.row) {
                    visibleRangeStartRow = foldLine.end.row;
                }
                continue;
            }
            if (screenRange.isMultiLine()) {
                markerLayer.drawTextMarker(html, screenRange, marker.className, config);
            }
            else {
                markerLayer.drawSingleLineMarker(html, screenRange, marker.className, config);
            }
        }
    };
    return MarkerGroup;
}());
MarkerGroup.prototype.MAX_MARKERS = 10000;
exports.MarkerGroup = MarkerGroup;

});

define("ace/split",["require","exports","module","ace/lib/oop","ace/lib/lang","ace/lib/event_emitter","ace/editor","ace/virtual_renderer","ace/edit_session"], function(require, exports, module){"use strict";
var oop = require("./lib/oop");
var lang = require("./lib/lang");
var EventEmitter = require("./lib/event_emitter").EventEmitter;
var Editor = require("./editor").Editor;
var Renderer = require("./virtual_renderer").VirtualRenderer;
var EditSession = require("./edit_session").EditSession;
var Split = function (container, theme, splits) {
    this.BELOW = 1;
    this.BESIDE = 0;
    this.$container = container;
    this.$theme = theme;
    this.$splits = 0;
    this.$editorCSS = "";
    this.$editors = [];
    this.$orientation = this.BESIDE;
    this.setSplits(splits || 1);
    this.$cEditor = this.$editors[0];
    this.on("focus", function (editor) {
        this.$cEditor = editor;
    }.bind(this));
};
(function () {
    oop.implement(this, EventEmitter);
    this.$createEditor = function () {
        var el = document.createElement("div");
        el.className = this.$editorCSS;
        el.style.cssText = "position: absolute; top:0px; bottom:0px";
        this.$container.appendChild(el);
        var editor = new Editor(new Renderer(el, this.$theme));
        editor.on("focus", function () {
            this._emit("focus", editor);
        }.bind(this));
        this.$editors.push(editor);
        editor.setFontSize(this.$fontSize);
        return editor;
    };
    this.setSplits = function (splits) {
        var editor;
        if (splits < 1) {
            throw "The number of splits have to be > 0!";
        }
        if (splits == this.$splits) {
            return;
        }
        else if (splits > this.$splits) {
            while (this.$splits < this.$editors.length && this.$splits < splits) {
                editor = this.$editors[this.$splits];
                this.$container.appendChild(editor.container);
                editor.setFontSize(this.$fontSize);
                this.$splits++;
            }
            while (this.$splits < splits) {
                this.$createEditor();
                this.$splits++;
            }
        }
        else {
            while (this.$splits > splits) {
                editor = this.$editors[this.$splits - 1];
                this.$container.removeChild(editor.container);
                this.$splits--;
            }
        }
        this.resize();
    };
    this.getSplits = function () {
        return this.$splits;
    };
    this.getEditor = function (idx) {
        return this.$editors[idx];
    };
    this.getCurrentEditor = function () {
        return this.$cEditor;
    };
    this.focus = function () {
        this.$cEditor.focus();
    };
    this.blur = function () {
        this.$cEditor.blur();
    };
    this.setTheme = function (theme) {
        this.$editors.forEach(function (editor) {
            editor.setTheme(theme);
        });
    };
    this.setKeyboardHandler = function (keybinding) {
        this.$editors.forEach(function (editor) {
            editor.setKeyboardHandler(keybinding);
        });
    };
    this.forEach = function (callback, scope) {
        this.$editors.forEach(callback, scope);
    };
    this.$fontSize = "";
    this.setFontSize = function (size) {
        this.$fontSize = size;
        this.forEach(function (editor) {
            editor.setFontSize(size);
        });
    };
    this.$cloneSession = function (session) {
        var s = new EditSession(session.getDocument(), session.getMode());
        var undoManager = session.getUndoManager();
        s.setUndoManager(undoManager);
        s.setTabSize(session.getTabSize());
        s.setUseSoftTabs(session.getUseSoftTabs());
        s.setOverwrite(session.getOverwrite());
        s.setBreakpoints(session.getBreakpoints());
        s.setUseWrapMode(session.getUseWrapMode());
        s.setUseWorker(session.getUseWorker());
        s.setWrapLimitRange(session.$wrapLimitRange.min, session.$wrapLimitRange.max);
        s.$foldData = session.$cloneFoldData();
        return s;
    };
    this.setSession = function (session, idx) {
        var editor;
        if (idx == null) {
            editor = this.$cEditor;
        }
        else {
            editor = this.$editors[idx];
        }
        var isUsed = this.$editors.some(function (editor) {
            return editor.session === session;
        });
        if (isUsed) {
            session = this.$cloneSession(session);
        }
        editor.setSession(session);
        return session;
    };
    this.getOrientation = function () {
        return this.$orientation;
    };
    this.setOrientation = function (orientation) {
        if (this.$orientation == orientation) {
            return;
        }
        this.$orientation = orientation;
        this.resize();
    };
    this.resize = function () {
        var width = this.$container.clientWidth;
        var height = this.$container.clientHeight;
        var editor;
        if (this.$orientation == this.BESIDE) {
            var editorWidth = width / this.$splits;
            for (var i = 0; i < this.$splits; i++) {
                editor = this.$editors[i];
                editor.container.style.width = editorWidth + "px";
                editor.container.style.top = "0px";
                editor.container.style.left = i * editorWidth + "px";
                editor.container.style.height = height + "px";
                editor.resize();
            }
        }
        else {
            var editorHeight = height / this.$splits;
            for (var i = 0; i < this.$splits; i++) {
                editor = this.$editors[i];
                editor.container.style.width = width + "px";
                editor.container.style.top = i * editorHeight + "px";
                editor.container.style.left = "0px";
                editor.container.style.height = editorHeight + "px";
                editor.resize();
            }
        }
    };
}).call(Split.prototype);
exports.Split = Split;

});

define("ace/ext/menu_tools/settings_menu.css",["require","exports","module"], function(require, exports, module){module.exports = "#ace_settingsmenu, #kbshortcutmenu {\n    background-color: #F7F7F7;\n    color: black;\n    box-shadow: -5px 4px 5px rgba(126, 126, 126, 0.55);\n    padding: 1em 0.5em 2em 1em;\n    overflow: auto;\n    position: absolute;\n    margin: 0;\n    bottom: 0;\n    right: 0;\n    top: 0;\n    z-index: 9991;\n    cursor: default;\n}\n\n.ace_dark #ace_settingsmenu, .ace_dark #kbshortcutmenu {\n    box-shadow: -20px 10px 25px rgba(126, 126, 126, 0.25);\n    background-color: rgba(255, 255, 255, 0.6);\n    color: black;\n}\n\n.ace_optionsMenuEntry:hover {\n    background-color: rgba(100, 100, 100, 0.1);\n    transition: all 0.3s\n}\n\n.ace_closeButton {\n    background: rgba(245, 146, 146, 0.5);\n    border: 1px solid #F48A8A;\n    border-radius: 50%;\n    padding: 7px;\n    position: absolute;\n    right: -8px;\n    top: -8px;\n    z-index: 100000;\n}\n.ace_closeButton{\n    background: rgba(245, 146, 146, 0.9);\n}\n.ace_optionsMenuKey {\n    color: darkslateblue;\n    font-weight: bold;\n}\n.ace_optionsMenuCommand {\n    color: darkcyan;\n    font-weight: normal;\n}\n.ace_optionsMenuEntry input, .ace_optionsMenuEntry button {\n    vertical-align: middle;\n}\n\n.ace_optionsMenuEntry button[ace_selected_button=true] {\n    background: #e7e7e7;\n    box-shadow: 1px 0px 2px 0px #adadad inset;\n    border-color: #adadad;\n}\n.ace_optionsMenuEntry button {\n    background: white;\n    border: 1px solid lightgray;\n    margin: 0px;\n}\n.ace_optionsMenuEntry button:hover{\n    background: #f0f0f0;\n}";

});

define("ace/ext/menu_tools/overlay_page",["require","exports","module","ace/lib/dom","ace/ext/menu_tools/settings_menu.css"], function(require, exports, module){/*jslint indent: 4, maxerr: 50, white: true, browser: true, vars: true*/
'use strict';
var dom = require("../../lib/dom");
var cssText = require("./settings_menu.css");
dom.importCssString(cssText, "settings_menu.css", false);
module.exports.overlayPage = function overlayPage(editor, contentElement, callback) {
    var closer = document.createElement('div');
    var ignoreFocusOut = false;
    function documentEscListener(e) {
        if (e.keyCode === 27) {
            close();
        }
    }
    function close() {
        if (!closer)
            return;
        document.removeEventListener('keydown', documentEscListener);
        closer.parentNode.removeChild(closer);
        if (editor) {
            editor.focus();
        }
        closer = null;
        callback && callback();
    }
    function setIgnoreFocusOut(ignore) {
        ignoreFocusOut = ignore;
        if (ignore) {
            closer.style.pointerEvents = "none";
            contentElement.style.pointerEvents = "auto";
        }
    }
    closer.style.cssText = 'margin: 0; padding: 0; ' +
        'position: fixed; top:0; bottom:0; left:0; right:0;' +
        'z-index: 9990; ' +
        (editor ? 'background-color: rgba(0, 0, 0, 0.3);' : '');
    closer.addEventListener('click', function (e) {
        if (!ignoreFocusOut) {
            close();
        }
    });
    document.addEventListener('keydown', documentEscListener);
    contentElement.addEventListener('click', function (e) {
        e.stopPropagation();
    });
    closer.appendChild(contentElement);
    document.body.appendChild(closer);
    if (editor) {
        editor.blur();
    }
    return {
        close: close,
        setIgnoreFocusOut: setIgnoreFocusOut
    };
};

});

define("ace/ext/themelist",["require","exports","module"], function(require, exports, module){/**
 * Generates a list of themes available when ace was built.
 * @fileOverview Generates a list of themes available when ace was built.
 * @author <a href="mailto:matthewkastor@gmail.com">
 *  Matthew Christopher Kastor-Inare III </a><br />
 *  ☭ Hial Atropa!! ☭
 */
"use strict";
var themeData = [
    ["Chrome"],
    ["Clouds"],
    ["Crimson Editor"],
    ["Dawn"],
    ["Dreamweaver"],
    ["Eclipse"],
    ["GitHub"],
    ["IPlastic"],
    ["Solarized Light"],
    ["TextMate"],
    ["Tomorrow"],
    ["XCode"],
    ["Kuroir"],
    ["KatzenMilch"],
    ["SQL Server", "sqlserver", "light"],
    ["Ambiance", "ambiance", "dark"],
    ["Chaos", "chaos", "dark"],
    ["Clouds Midnight", "clouds_midnight", "dark"],
    ["Dracula", "", "dark"],
    ["Cobalt", "cobalt", "dark"],
    ["Gruvbox", "gruvbox", "dark"],
    ["Green on Black", "gob", "dark"],
    ["idle Fingers", "idle_fingers", "dark"],
    ["krTheme", "kr_theme", "dark"],
    ["Merbivore", "merbivore", "dark"],
    ["Merbivore Soft", "merbivore_soft", "dark"],
    ["Mono Industrial", "mono_industrial", "dark"],
    ["Monokai", "monokai", "dark"],
    ["Nord Dark", "nord_dark", "dark"],
    ["One Dark", "one_dark", "dark"],
    ["Pastel on dark", "pastel_on_dark", "dark"],
    ["Solarized Dark", "solarized_dark", "dark"],
    ["Terminal", "terminal", "dark"],
    ["Tomorrow Night", "tomorrow_night", "dark"],
    ["Tomorrow Night Blue", "tomorrow_night_blue", "dark"],
    ["Tomorrow Night Bright", "tomorrow_night_bright", "dark"],
    ["Tomorrow Night 80s", "tomorrow_night_eighties", "dark"],
    ["Twilight", "twilight", "dark"],
    ["Vibrant Ink", "vibrant_ink", "dark"],
    ["GitHub Dark", "github_dark", "dark"]
];
exports.themesByName = {};
exports.themes = themeData.map(function (data) {
    var name = data[1] || data[0].replace(/ /g, "_").toLowerCase();
    var theme = {
        caption: data[0],
        theme: "ace/theme/" + name,
        isDark: data[2] == "dark",
        name: name
    };
    exports.themesByName[name] = theme;
    return theme;
});

});

define("ace/ext/options",["require","exports","module","ace/ext/menu_tools/overlay_page","ace/lib/dom","ace/lib/oop","ace/config","ace/lib/event_emitter","ace/ext/modelist","ace/ext/themelist"], function(require, exports, module){"use strict";
require("./menu_tools/overlay_page");
var dom = require("../lib/dom");
var oop = require("../lib/oop");
var config = require("../config");
var EventEmitter = require("../lib/event_emitter").EventEmitter;
var buildDom = dom.buildDom;
var modelist = require("./modelist");
var themelist = require("./themelist");
var themes = { Bright: [], Dark: [] };
themelist.themes.forEach(function (x) {
    themes[x.isDark ? "Dark" : "Bright"].push({ caption: x.caption, value: x.theme });
});
var modes = modelist.modes.map(function (x) {
    return { caption: x.caption, value: x.mode };
});
var optionGroups = {
    Main: {
        Mode: {
            path: "mode",
            type: "select",
            items: modes
        },
        Theme: {
            path: "theme",
            type: "select",
            items: themes
        },
        "Keybinding": {
            type: "buttonBar",
            path: "keyboardHandler",
            items: [
                { caption: "Ace", value: null },
                { caption: "Vim", value: "ace/keyboard/vim" },
                { caption: "Emacs", value: "ace/keyboard/emacs" },
                { caption: "Sublime", value: "ace/keyboard/sublime" },
                { caption: "VSCode", value: "ace/keyboard/vscode" }
            ]
        },
        "Font Size": {
            path: "fontSize",
            type: "number",
            defaultValue: 12,
            defaults: [
                { caption: "12px", value: 12 },
                { caption: "24px", value: 24 }
            ]
        },
        "Soft Wrap": {
            type: "buttonBar",
            path: "wrap",
            items: [
                { caption: "Off", value: "off" },
                { caption: "View", value: "free" },
                { caption: "margin", value: "printMargin" },
                { caption: "40", value: "40" }
            ]
        },
        "Cursor Style": {
            path: "cursorStyle",
            items: [
                { caption: "Ace", value: "ace" },
                { caption: "Slim", value: "slim" },
                { caption: "Smooth", value: "smooth" },
                { caption: "Smooth And Slim", value: "smooth slim" },
                { caption: "Wide", value: "wide" }
            ]
        },
        "Folding": {
            path: "foldStyle",
            items: [
                { caption: "Manual", value: "manual" },
                { caption: "Mark begin", value: "markbegin" },
                { caption: "Mark begin and end", value: "markbeginend" }
            ]
        },
        "Soft Tabs": [{
                path: "useSoftTabs"
            }, {
                ariaLabel: "Tab Size",
                path: "tabSize",
                type: "number",
                values: [2, 3, 4, 8, 16]
            }],
        "Overscroll": {
            type: "buttonBar",
            path: "scrollPastEnd",
            items: [
                { caption: "None", value: 0 },
                { caption: "Half", value: 0.5 },
                { caption: "Full", value: 1 }
            ]
        }
    },
    More: {
        "Atomic soft tabs": {
            path: "navigateWithinSoftTabs"
        },
        "Enable Behaviours": {
            path: "behavioursEnabled"
        },
        "Wrap with quotes": {
            path: "wrapBehavioursEnabled"
        },
        "Enable Auto Indent": {
            path: "enableAutoIndent"
        },
        "Full Line Selection": {
            type: "checkbox",
            values: "text|line",
            path: "selectionStyle"
        },
        "Highlight Active Line": {
            path: "highlightActiveLine"
        },
        "Show Invisibles": {
            path: "showInvisibles"
        },
        "Show Indent Guides": {
            path: "displayIndentGuides"
        },
        "Highlight Indent Guides": {
            path: "highlightIndentGuides"
        },
        "Persistent HScrollbar": {
            path: "hScrollBarAlwaysVisible"
        },
        "Persistent VScrollbar": {
            path: "vScrollBarAlwaysVisible"
        },
        "Animate scrolling": {
            path: "animatedScroll"
        },
        "Show Gutter": {
            path: "showGutter"
        },
        "Show Line Numbers": {
            path: "showLineNumbers"
        },
        "Relative Line Numbers": {
            path: "relativeLineNumbers"
        },
        "Fixed Gutter Width": {
            path: "fixedWidthGutter"
        },
        "Show Print Margin": [{
                path: "showPrintMargin"
            }, {
                ariaLabel: "Print Margin",
                type: "number",
                path: "printMarginColumn"
            }],
        "Indented Soft Wrap": {
            path: "indentedSoftWrap"
        },
        "Highlight selected word": {
            path: "highlightSelectedWord"
        },
        "Fade Fold Widgets": {
            path: "fadeFoldWidgets"
        },
        "Use textarea for IME": {
            path: "useTextareaForIME"
        },
        "Merge Undo Deltas": {
            path: "mergeUndoDeltas",
            items: [
                { caption: "Always", value: "always" },
                { caption: "Never", value: "false" },
                { caption: "Timed", value: "true" }
            ]
        },
        "Elastic Tabstops": {
            path: "useElasticTabstops"
        },
        "Incremental Search": {
            path: "useIncrementalSearch"
        },
        "Read-only": {
            path: "readOnly"
        },
        "Copy without selection": {
            path: "copyWithEmptySelection"
        },
        "Live Autocompletion": {
            path: "enableLiveAutocompletion"
        },
        "Custom scrollbar": {
            path: "customScrollbar"
        },
        "Use SVG gutter icons": {
            path: "useSvgGutterIcons"
        },
        "Annotations for folded lines": {
            path: "showFoldedAnnotations"
        },
        "Keyboard Accessibility Mode": {
            path: "enableKeyboardAccessibility"
        },
        "Gutter tooltip follows mouse": {
            path: "tooltipFollowsMouse",
            defaultValue: true
        }
    }
};
var OptionPanel = /** @class */ (function () {
    function OptionPanel(editor, element) {
        this.editor = editor;
        this.container = element || document.createElement("div");
        this.groups = [];
        this.options = {};
    }
    OptionPanel.prototype.add = function (config) {
        if (config.Main)
            oop.mixin(optionGroups.Main, config.Main);
        if (config.More)
            oop.mixin(optionGroups.More, config.More);
    };
    OptionPanel.prototype.render = function () {
        this.container.innerHTML = "";
        buildDom(["table", { role: "presentation", id: "controls" },
            this.renderOptionGroup(optionGroups.Main),
            ["tr", null, ["td", { colspan: 2 },
                    ["table", { role: "presentation", id: "more-controls" },
                        this.renderOptionGroup(optionGroups.More)
                    ]
                ]],
            ["tr", null, ["td", { colspan: 2 }, "version " + config.version]]
        ], this.container);
    };
    OptionPanel.prototype.renderOptionGroup = function (group) {
        return Object.keys(group).map(function (key, i) {
            var item = group[key];
            if (!item.position)
                item.position = i / 10000;
            if (!item.label)
                item.label = key;
            return item;
        }).sort(function (a, b) {
            return a.position - b.position;
        }).map(function (item) {
            return this.renderOption(item.label, item);
        }, this);
    };
    OptionPanel.prototype.renderOptionControl = function (key, option) {
        var self = this;
        if (Array.isArray(option)) {
            return option.map(function (x) {
                return self.renderOptionControl(key, x);
            });
        }
        var control;
        var value = self.getOption(option);
        if (option.values && option.type != "checkbox") {
            if (typeof option.values == "string")
                option.values = option.values.split("|");
            option.items = option.values.map(function (v) {
                return { value: v, name: v };
            });
        }
        if (option.type == "buttonBar") {
            control = ["div", { role: "group", "aria-labelledby": option.path + "-label" }, option.items.map(function (item) {
                    return ["button", {
                            value: item.value,
                            ace_selected_button: value == item.value,
                            'aria-pressed': value == item.value,
                            onclick: function () {
                                self.setOption(option, item.value);
                                var nodes = this.parentNode.querySelectorAll("[ace_selected_button]");
                                for (var i = 0; i < nodes.length; i++) {
                                    nodes[i].removeAttribute("ace_selected_button");
                                    nodes[i].setAttribute("aria-pressed", false);
                                }
                                this.setAttribute("ace_selected_button", true);
                                this.setAttribute("aria-pressed", true);
                            }
                        }, item.desc || item.caption || item.name];
                })];
        }
        else if (option.type == "number") {
            control = ["input", { type: "number", value: value || option.defaultValue, style: "width:3em", oninput: function () {
                        self.setOption(option, parseInt(this.value));
                    } }];
            if (option.ariaLabel) {
                control[1]["aria-label"] = option.ariaLabel;
            }
            else {
                control[1].id = key;
            }
            if (option.defaults) {
                control = [control, option.defaults.map(function (item) {
                        return ["button", { onclick: function () {
                                    var input = this.parentNode.firstChild;
                                    input.value = item.value;
                                    input.oninput();
                                } }, item.caption];
                    })];
            }
        }
        else if (option.items) {
            var buildItems = function (items) {
                return items.map(function (item) {
                    return ["option", { value: item.value || item.name }, item.desc || item.caption || item.name];
                });
            };
            var items = Array.isArray(option.items)
                ? buildItems(option.items)
                : Object.keys(option.items).map(function (key) {
                    return ["optgroup", { "label": key }, buildItems(option.items[key])];
                });
            control = ["select", { id: key, value: value, onchange: function () {
                        self.setOption(option, this.value);
                    } }, items];
        }
        else {
            if (typeof option.values == "string")
                option.values = option.values.split("|");
            if (option.values)
                value = value == option.values[1];
            control = ["input", { type: "checkbox", id: key, checked: value || null, onchange: function () {
                        var value = this.checked;
                        if (option.values)
                            value = option.values[value ? 1 : 0];
                        self.setOption(option, value);
                    } }];
            if (option.type == "checkedNumber") {
                control = [control, []];
            }
        }
        return control;
    };
    OptionPanel.prototype.renderOption = function (key, option) {
        if (option.path && !option.onchange && !this.editor.$options[option.path])
            return;
        var path = Array.isArray(option) ? option[0].path : option.path;
        this.options[path] = option;
        var safeKey = "-" + path;
        var safeId = path + "-label";
        var control = this.renderOptionControl(safeKey, option);
        return ["tr", { class: "ace_optionsMenuEntry" }, ["td",
                ["label", { for: safeKey, id: safeId }, key]
            ], ["td", control]];
    };
    OptionPanel.prototype.setOption = function (option, value) {
        if (typeof option == "string")
            option = this.options[option];
        if (value == "false")
            value = false;
        if (value == "true")
            value = true;
        if (value == "null")
            value = null;
        if (value == "undefined")
            value = undefined;
        if (typeof value == "string" && parseFloat(value).toString() == value)
            value = parseFloat(value);
        if (option.onchange)
            option.onchange(value);
        else if (option.path)
            this.editor.setOption(option.path, value);
        this._signal("setOption", { name: option.path, value: value });
    };
    OptionPanel.prototype.getOption = function (option) {
        if (option.getValue)
            return option.getValue();
        return this.editor.getOption(option.path);
    };
    return OptionPanel;
}());
oop.implement(OptionPanel.prototype, EventEmitter);
exports.OptionPanel = OptionPanel;

});

define("ace/autocomplete/popup",["require","exports","module","ace/virtual_renderer","ace/editor","ace/range","ace/lib/event","ace/lib/lang","ace/lib/dom","ace/config"], function(require, exports, module){"use strict";
var Renderer = require("../virtual_renderer").VirtualRenderer;
var Editor = require("../editor").Editor;
var Range = require("../range").Range;
var event = require("../lib/event");
var lang = require("../lib/lang");
var dom = require("../lib/dom");
var nls = require("../config").nls;
var getAriaId = function (index) {
    return "suggest-aria-id:".concat(index);
};
var $singleLineEditor = function (el) {
    var renderer = new Renderer(el);
    renderer.$maxLines = 4;
    var editor = new Editor(renderer);
    editor.setHighlightActiveLine(false);
    editor.setShowPrintMargin(false);
    editor.renderer.setShowGutter(false);
    editor.renderer.setHighlightGutterLine(false);
    editor.$mouseHandler.$focusTimeout = 0;
    editor.$highlightTagPending = true;
    return editor;
};
var AcePopup = /** @class */ (function () {
    function AcePopup(parentNode) {
        var el = dom.createElement("div");
        var popup = new $singleLineEditor(el);
        if (parentNode) {
            parentNode.appendChild(el);
        }
        el.style.display = "none";
        popup.renderer.content.style.cursor = "default";
        popup.renderer.setStyle("ace_autocomplete");
        popup.renderer.$textLayer.element.setAttribute("role", "listbox");
        popup.renderer.$textLayer.element.setAttribute("aria-label", nls("Autocomplete suggestions"));
        popup.renderer.textarea.setAttribute("aria-hidden", "true");
        popup.setOption("displayIndentGuides", false);
        popup.setOption("dragDelay", 150);
        var noop = function () { };
        popup.focus = noop;
        popup.$isFocused = true;
        popup.renderer.$cursorLayer.restartTimer = noop;
        popup.renderer.$cursorLayer.element.style.opacity = 0;
        popup.renderer.$maxLines = 8;
        popup.renderer.$keepTextAreaAtCursor = false;
        popup.setHighlightActiveLine(false);
        popup.session.highlight("");
        popup.session.$searchHighlight.clazz = "ace_highlight-marker";
        popup.on("mousedown", function (e) {
            var pos = e.getDocumentPosition();
            popup.selection.moveToPosition(pos);
            selectionMarker.start.row = selectionMarker.end.row = pos.row;
            e.stop();
        });
        var lastMouseEvent;
        var hoverMarker = new Range(-1, 0, -1, Infinity);
        var selectionMarker = new Range(-1, 0, -1, Infinity);
        selectionMarker.id = popup.session.addMarker(selectionMarker, "ace_active-line", "fullLine");
        popup.setSelectOnHover = function (val) {
            if (!val) {
                hoverMarker.id = popup.session.addMarker(hoverMarker, "ace_line-hover", "fullLine");
            }
            else if (hoverMarker.id) {
                popup.session.removeMarker(hoverMarker.id);
                hoverMarker.id = null;
            }
        };
        popup.setSelectOnHover(false);
        popup.on("mousemove", function (e) {
            if (!lastMouseEvent) {
                lastMouseEvent = e;
                return;
            }
            if (lastMouseEvent.x == e.x && lastMouseEvent.y == e.y) {
                return;
            }
            lastMouseEvent = e;
            lastMouseEvent.scrollTop = popup.renderer.scrollTop;
            var row = lastMouseEvent.getDocumentPosition().row;
            if (hoverMarker.start.row != row) {
                if (!hoverMarker.id)
                    popup.setRow(row);
                setHoverMarker(row);
            }
        });
        popup.renderer.on("beforeRender", function () {
            if (lastMouseEvent && hoverMarker.start.row != -1) {
                lastMouseEvent.$pos = null;
                var row = lastMouseEvent.getDocumentPosition().row;
                if (!hoverMarker.id)
                    popup.setRow(row);
                setHoverMarker(row, true);
            }
        });
        popup.renderer.on("afterRender", function () {
            var row = popup.getRow();
            var t = popup.renderer.$textLayer;
            var selected = t.element.childNodes[row - t.config.firstRow];
            var el = document.activeElement; // Active element is textarea of main editor
            if (selected !== t.selectedNode && t.selectedNode) {
                dom.removeCssClass(t.selectedNode, "ace_selected");
                el.removeAttribute("aria-activedescendant");
                t.selectedNode.removeAttribute("id");
            }
            t.selectedNode = selected;
            if (selected) {
                dom.addCssClass(selected, "ace_selected");
                var ariaId = getAriaId(row);
                selected.id = ariaId;
                t.element.setAttribute("aria-activedescendant", ariaId);
                el.setAttribute("aria-activedescendant", ariaId);
                selected.setAttribute("role", "option");
                selected.setAttribute("aria-label", popup.getData(row).value);
                selected.setAttribute("aria-setsize", popup.data.length);
                selected.setAttribute("aria-posinset", row + 1);
                selected.setAttribute("aria-describedby", "doc-tooltip");
            }
        });
        var hideHoverMarker = function () { setHoverMarker(-1); };
        var setHoverMarker = function (row, suppressRedraw) {
            if (row !== hoverMarker.start.row) {
                hoverMarker.start.row = hoverMarker.end.row = row;
                if (!suppressRedraw)
                    popup.session._emit("changeBackMarker");
                popup._emit("changeHoverMarker");
            }
        };
        popup.getHoveredRow = function () {
            return hoverMarker.start.row;
        };
        event.addListener(popup.container, "mouseout", hideHoverMarker);
        popup.on("hide", hideHoverMarker);
        popup.on("changeSelection", hideHoverMarker);
        popup.session.doc.getLength = function () {
            return popup.data.length;
        };
        popup.session.doc.getLine = function (i) {
            var data = popup.data[i];
            if (typeof data == "string")
                return data;
            return (data && data.value) || "";
        };
        var bgTokenizer = popup.session.bgTokenizer;
        bgTokenizer.$tokenizeRow = function (row) {
            var data = popup.data[row];
            var tokens = [];
            if (!data)
                return tokens;
            if (typeof data == "string")
                data = { value: data };
            var caption = data.caption || data.value || data.name;
            function addToken(value, className) {
                value && tokens.push({
                    type: (data.className || "") + (className || ""),
                    value: value
                });
            }
            var lower = caption.toLowerCase();
            var filterText = (popup.filterText || "").toLowerCase();
            var lastIndex = 0;
            var lastI = 0;
            for (var i = 0; i <= filterText.length; i++) {
                if (i != lastI && (data.matchMask & (1 << i) || i == filterText.length)) {
                    var sub = filterText.slice(lastI, i);
                    lastI = i;
                    var index = lower.indexOf(sub, lastIndex);
                    if (index == -1)
                        continue;
                    addToken(caption.slice(lastIndex, index), "");
                    lastIndex = index + sub.length;
                    addToken(caption.slice(index, lastIndex), "completion-highlight");
                }
            }
            addToken(caption.slice(lastIndex, caption.length), "");
            tokens.push({ type: "completion-spacer", value: " " });
            if (data.meta)
                tokens.push({ type: "completion-meta", value: data.meta });
            if (data.message)
                tokens.push({ type: "completion-message", value: data.message });
            return tokens;
        };
        bgTokenizer.$updateOnChange = noop;
        bgTokenizer.start = noop;
        popup.session.$computeWidth = function () {
            return this.screenWidth = 0;
        };
        popup.isOpen = false;
        popup.isTopdown = false;
        popup.autoSelect = true;
        popup.filterText = "";
        popup.data = [];
        popup.setData = function (list, filterText) {
            popup.filterText = filterText || "";
            popup.setValue(lang.stringRepeat("\n", list.length), -1);
            popup.data = list || [];
            popup.setRow(0);
        };
        popup.getData = function (row) {
            return popup.data[row];
        };
        popup.getRow = function () {
            return selectionMarker.start.row;
        };
        popup.setRow = function (line) {
            line = Math.max(this.autoSelect ? 0 : -1, Math.min(this.data.length - 1, line));
            if (selectionMarker.start.row != line) {
                popup.selection.clearSelection();
                selectionMarker.start.row = selectionMarker.end.row = line || 0;
                popup.session._emit("changeBackMarker");
                popup.moveCursorTo(line || 0, 0);
                if (popup.isOpen)
                    popup._signal("select");
            }
        };
        popup.on("changeSelection", function () {
            if (popup.isOpen)
                popup.setRow(popup.selection.lead.row);
            popup.renderer.scrollCursorIntoView();
        });
        popup.hide = function () {
            this.container.style.display = "none";
            popup.anchorPos = null;
            popup.anchor = null;
            if (popup.isOpen) {
                popup.isOpen = false;
                this._signal("hide");
            }
        };
        popup.tryShow = function (pos, lineHeight, anchor, forceShow) {
            if (!forceShow && popup.isOpen && popup.anchorPos && popup.anchor &&
                popup.anchorPos.top === pos.top && popup.anchorPos.left === pos.left &&
                popup.anchor === anchor) {
                return true;
            }
            var el = this.container;
            var screenHeight = window.innerHeight;
            var screenWidth = window.innerWidth;
            var renderer = this.renderer;
            var maxH = renderer.$maxLines * lineHeight * 1.4;
            var dims = { top: 0, bottom: 0, left: 0 };
            var spaceBelow = screenHeight - pos.top - 3 * this.$borderSize - lineHeight;
            var spaceAbove = pos.top - 3 * this.$borderSize;
            if (!anchor) {
                if (spaceAbove <= spaceBelow || spaceBelow >= maxH) {
                    anchor = "bottom";
                }
                else {
                    anchor = "top";
                }
            }
            if (anchor === "top") {
                dims.bottom = pos.top - this.$borderSize;
                dims.top = dims.bottom - maxH;
            }
            else if (anchor === "bottom") {
                dims.top = pos.top + lineHeight + this.$borderSize;
                dims.bottom = dims.top + maxH;
            }
            var fitsX = dims.top >= 0 && dims.bottom <= screenHeight;
            if (!forceShow && !fitsX) {
                return false;
            }
            if (!fitsX) {
                if (anchor === "top") {
                    renderer.$maxPixelHeight = spaceAbove;
                }
                else {
                    renderer.$maxPixelHeight = spaceBelow;
                }
            }
            else {
                renderer.$maxPixelHeight = null;
            }
            if (anchor === "top") {
                el.style.top = "";
                el.style.bottom = (screenHeight - dims.bottom) + "px";
                popup.isTopdown = false;
            }
            else {
                el.style.top = dims.top + "px";
                el.style.bottom = "";
                popup.isTopdown = true;
            }
            el.style.display = "";
            var left = pos.left;
            if (left + el.offsetWidth > screenWidth)
                left = screenWidth - el.offsetWidth;
            el.style.left = left + "px";
            el.style.right = "";
            if (!popup.isOpen) {
                popup.isOpen = true;
                this._signal("show");
                lastMouseEvent = null;
            }
            popup.anchorPos = pos;
            popup.anchor = anchor;
            return true;
        };
        popup.show = function (pos, lineHeight, topdownOnly) {
            this.tryShow(pos, lineHeight, topdownOnly ? "bottom" : undefined, true);
        };
        popup.goTo = function (where) {
            var row = this.getRow();
            var max = this.session.getLength() - 1;
            switch (where) {
                case "up":
                    row = row <= 0 ? max : row - 1;
                    break;
                case "down":
                    row = row >= max ? -1 : row + 1;
                    break;
                case "start":
                    row = 0;
                    break;
                case "end":
                    row = max;
                    break;
            }
            this.setRow(row);
        };
        popup.getTextLeftOffset = function () {
            return this.$borderSize + this.renderer.$padding + this.$imageSize;
        };
        popup.$imageSize = 0;
        popup.$borderSize = 1;
        return popup;
    }
    return AcePopup;
}());
dom.importCssString("\n.ace_editor.ace_autocomplete .ace_marker-layer .ace_active-line {\n    background-color: #CAD6FA;\n    z-index: 1;\n}\n.ace_dark.ace_editor.ace_autocomplete .ace_marker-layer .ace_active-line {\n    background-color: #3a674e;\n}\n.ace_editor.ace_autocomplete .ace_line-hover {\n    border: 1px solid #abbffe;\n    margin-top: -1px;\n    background: rgba(233,233,253,0.4);\n    position: absolute;\n    z-index: 2;\n}\n.ace_dark.ace_editor.ace_autocomplete .ace_line-hover {\n    border: 1px solid rgba(109, 150, 13, 0.8);\n    background: rgba(58, 103, 78, 0.62);\n}\n.ace_completion-meta {\n    opacity: 0.5;\n    margin-left: 0.9em;\n}\n.ace_completion-message {\n    color: blue;\n}\n.ace_editor.ace_autocomplete .ace_completion-highlight{\n    color: #2d69c7;\n}\n.ace_dark.ace_editor.ace_autocomplete .ace_completion-highlight{\n    color: #93ca12;\n}\n.ace_editor.ace_autocomplete {\n    width: 300px;\n    z-index: 200000;\n    border: 1px lightgray solid;\n    position: fixed;\n    box-shadow: 2px 3px 5px rgba(0,0,0,.2);\n    line-height: 1.4;\n    background: #fefefe;\n    color: #111;\n}\n.ace_dark.ace_editor.ace_autocomplete {\n    border: 1px #484747 solid;\n    box-shadow: 2px 3px 5px rgba(0, 0, 0, 0.51);\n    line-height: 1.4;\n    background: #25282c;\n    color: #c1c1c1;\n}\n.ace_autocomplete .ace_text-layer  {\n    width: calc(100% - 8px);\n}\n.ace_autocomplete .ace_line {\n    display: flex;\n    align-items: center;\n}\n.ace_autocomplete .ace_line > * {\n    min-width: 0;\n    flex: 0 0 auto;\n}\n.ace_autocomplete .ace_line .ace_ {\n    flex: 0 1 auto;\n    overflow: hidden;\n    white-space: nowrap;\n    text-overflow: ellipsis;\n}\n.ace_autocomplete .ace_completion-spacer {\n    flex: 1;\n}\n", "autocompletion.css", false);
exports.AcePopup = AcePopup;
exports.$singleLineEditor = $singleLineEditor;
exports.getAriaId = getAriaId;

});

define("ace/snippets",["require","exports","module","ace/lib/dom","ace/lib/oop","ace/lib/event_emitter","ace/lib/lang","ace/range","ace/range_list","ace/keyboard/hash_handler","ace/tokenizer","ace/clipboard","ace/editor"], function(require, exports, module){"use strict";
var dom = require("./lib/dom");
var oop = require("./lib/oop");
var EventEmitter = require("./lib/event_emitter").EventEmitter;
var lang = require("./lib/lang");
var Range = require("./range").Range;
var RangeList = require("./range_list").RangeList;
var HashHandler = require("./keyboard/hash_handler").HashHandler;
var Tokenizer = require("./tokenizer").Tokenizer;
var clipboard = require("./clipboard");
var VARIABLES = {
    CURRENT_WORD: function (editor) {
        return editor.session.getTextRange(editor.session.getWordRange());
    },
    SELECTION: function (editor, name, indentation) {
        var text = editor.session.getTextRange();
        if (indentation)
            return text.replace(/\n\r?([ \t]*\S)/g, "\n" + indentation + "$1");
        return text;
    },
    CURRENT_LINE: function (editor) {
        return editor.session.getLine(editor.getCursorPosition().row);
    },
    PREV_LINE: function (editor) {
        return editor.session.getLine(editor.getCursorPosition().row - 1);
    },
    LINE_INDEX: function (editor) {
        return editor.getCursorPosition().row;
    },
    LINE_NUMBER: function (editor) {
        return editor.getCursorPosition().row + 1;
    },
    SOFT_TABS: function (editor) {
        return editor.session.getUseSoftTabs() ? "YES" : "NO";
    },
    TAB_SIZE: function (editor) {
        return editor.session.getTabSize();
    },
    CLIPBOARD: function (editor) {
        return clipboard.getText && clipboard.getText();
    },
    FILENAME: function (editor) {
        return /[^/\\]*$/.exec(this.FILEPATH(editor))[0];
    },
    FILENAME_BASE: function (editor) {
        return /[^/\\]*$/.exec(this.FILEPATH(editor))[0].replace(/\.[^.]*$/, "");
    },
    DIRECTORY: function (editor) {
        return this.FILEPATH(editor).replace(/[^/\\]*$/, "");
    },
    FILEPATH: function (editor) { return "/not implemented.txt"; },
    WORKSPACE_NAME: function () { return "Unknown"; },
    FULLNAME: function () { return "Unknown"; },
    BLOCK_COMMENT_START: function (editor) {
        var mode = editor.session.$mode || {};
        return mode.blockComment && mode.blockComment.start || "";
    },
    BLOCK_COMMENT_END: function (editor) {
        var mode = editor.session.$mode || {};
        return mode.blockComment && mode.blockComment.end || "";
    },
    LINE_COMMENT: function (editor) {
        var mode = editor.session.$mode || {};
        return mode.lineCommentStart || "";
    },
    CURRENT_YEAR: date.bind(null, { year: "numeric" }),
    CURRENT_YEAR_SHORT: date.bind(null, { year: "2-digit" }),
    CURRENT_MONTH: date.bind(null, { month: "numeric" }),
    CURRENT_MONTH_NAME: date.bind(null, { month: "long" }),
    CURRENT_MONTH_NAME_SHORT: date.bind(null, { month: "short" }),
    CURRENT_DATE: date.bind(null, { day: "2-digit" }),
    CURRENT_DAY_NAME: date.bind(null, { weekday: "long" }),
    CURRENT_DAY_NAME_SHORT: date.bind(null, { weekday: "short" }),
    CURRENT_HOUR: date.bind(null, { hour: "2-digit", hour12: false }),
    CURRENT_MINUTE: date.bind(null, { minute: "2-digit" }),
    CURRENT_SECOND: date.bind(null, { second: "2-digit" })
};
VARIABLES.SELECTED_TEXT = VARIABLES.SELECTION;
function date(dateFormat) {
    var str = new Date().toLocaleString("en-us", dateFormat);
    return str.length == 1 ? "0" + str : str;
}
var SnippetManager = /** @class */ (function () {
    function SnippetManager() {
        this.snippetMap = {};
        this.snippetNameMap = {};
        this.variables = VARIABLES;
    }
    SnippetManager.prototype.getTokenizer = function () {
        return SnippetManager.$tokenizer || this.createTokenizer();
    };
    SnippetManager.prototype.createTokenizer = function () {
        function TabstopToken(str) {
            str = str.substr(1);
            if (/^\d+$/.test(str))
                return [{ tabstopId: parseInt(str, 10) }];
            return [{ text: str }];
        }
        function escape(ch) {
            return "(?:[^\\\\" + ch + "]|\\\\.)";
        }
        var formatMatcher = {
            regex: "/(" + escape("/") + "+)/",
            onMatch: function (val, state, stack) {
                var ts = stack[0];
                ts.fmtString = true;
                ts.guard = val.slice(1, -1);
                ts.flag = "";
                return "";
            },
            next: "formatString"
        };
        SnippetManager.$tokenizer = new Tokenizer({
            start: [
                { regex: /\\./, onMatch: function (val, state, stack) {
                        var ch = val[1];
                        if (ch == "}" && stack.length) {
                            val = ch;
                        }
                        else if ("`$\\".indexOf(ch) != -1) {
                            val = ch;
                        }
                        return [val];
                    } },
                { regex: /}/, onMatch: function (val, state, stack) {
                        return [stack.length ? stack.shift() : val];
                    } },
                { regex: /\$(?:\d+|\w+)/, onMatch: TabstopToken },
                { regex: /\$\{[\dA-Z_a-z]+/, onMatch: function (str, state, stack) {
                        var t = TabstopToken(str.substr(1));
                        stack.unshift(t[0]);
                        return t;
                    }, next: "snippetVar" },
                { regex: /\n/, token: "newline", merge: false }
            ],
            snippetVar: [
                { regex: "\\|" + escape("\\|") + "*\\|", onMatch: function (val, state, stack) {
                        var choices = val.slice(1, -1).replace(/\\[,|\\]|,/g, function (operator) {
                            return operator.length == 2 ? operator[1] : "\x00";
                        }).split("\x00").map(function (value) {
                            return { value: value };
                        });
                        stack[0].choices = choices;
                        return [choices[0]];
                    }, next: "start" },
                formatMatcher,
                { regex: "([^:}\\\\]|\\\\.)*:?", token: "", next: "start" }
            ],
            formatString: [
                { regex: /:/, onMatch: function (val, state, stack) {
                        if (stack.length && stack[0].expectElse) {
                            stack[0].expectElse = false;
                            stack[0].ifEnd = { elseEnd: stack[0] };
                            return [stack[0].ifEnd];
                        }
                        return ":";
                    } },
                { regex: /\\./, onMatch: function (val, state, stack) {
                        var ch = val[1];
                        if (ch == "}" && stack.length)
                            val = ch;
                        else if ("`$\\".indexOf(ch) != -1)
                            val = ch;
                        else if (ch == "n")
                            val = "\n";
                        else if (ch == "t")
                            val = "\t";
                        else if ("ulULE".indexOf(ch) != -1)
                            val = { changeCase: ch, local: ch > "a" };
                        return [val];
                    } },
                { regex: "/\\w*}", onMatch: function (val, state, stack) {
                        var next = stack.shift();
                        if (next)
                            next.flag = val.slice(1, -1);
                        this.next = next && next.tabstopId ? "start" : "";
                        return [next || val];
                    }, next: "start" },
                { regex: /\$(?:\d+|\w+)/, onMatch: function (val, state, stack) {
                        return [{ text: val.slice(1) }];
                    } },
                { regex: /\${\w+/, onMatch: function (val, state, stack) {
                        var token = { text: val.slice(2) };
                        stack.unshift(token);
                        return [token];
                    }, next: "formatStringVar" },
                { regex: /\n/, token: "newline", merge: false },
                { regex: /}/, onMatch: function (val, state, stack) {
                        var next = stack.shift();
                        this.next = next && next.tabstopId ? "start" : "";
                        return [next || val];
                    }, next: "start" }
            ],
            formatStringVar: [
                { regex: /:\/\w+}/, onMatch: function (val, state, stack) {
                        var ts = stack[0];
                        ts.formatFunction = val.slice(2, -1);
                        return [stack.shift()];
                    }, next: "formatString" },
                formatMatcher,
                { regex: /:[\?\-+]?/, onMatch: function (val, state, stack) {
                        if (val[1] == "+")
                            stack[0].ifEnd = stack[0];
                        if (val[1] == "?")
                            stack[0].expectElse = true;
                    }, next: "formatString" },
                { regex: "([^:}\\\\]|\\\\.)*:?", token: "", next: "formatString" }
            ]
        });
        return SnippetManager.$tokenizer;
    };
    SnippetManager.prototype.tokenizeTmSnippet = function (str, startState) {
        return this.getTokenizer().getLineTokens(str, startState).tokens.map(function (x) {
            return x.value || x;
        });
    };
    SnippetManager.prototype.getVariableValue = function (editor, name, indentation) {
        if (/^\d+$/.test(name))
            return (this.variables.__ || {})[name] || "";
        if (/^[A-Z]\d+$/.test(name))
            return (this.variables[name[0] + "__"] || {})[name.substr(1)] || "";
        name = name.replace(/^TM_/, "");
        if (!this.variables.hasOwnProperty(name))
            return "";
        var value = this.variables[name];
        if (typeof value == "function")
            value = this.variables[name](editor, name, indentation);
        return value == null ? "" : value;
    };
    SnippetManager.prototype.tmStrFormat = function (str, ch, editor) {
        if (!ch.fmt)
            return str;
        var flag = ch.flag || "";
        var re = ch.guard;
        re = new RegExp(re, flag.replace(/[^gim]/g, ""));
        var fmtTokens = typeof ch.fmt == "string" ? this.tokenizeTmSnippet(ch.fmt, "formatString") : ch.fmt;
        var _self = this;
        var formatted = str.replace(re, function () {
            var oldArgs = _self.variables.__;
            _self.variables.__ = [].slice.call(arguments);
            var fmtParts = _self.resolveVariables(fmtTokens, editor);
            var gChangeCase = "E";
            for (var i = 0; i < fmtParts.length; i++) {
                var ch = fmtParts[i];
                if (typeof ch == "object") {
                    fmtParts[i] = "";
                    if (ch.changeCase && ch.local) {
                        var next = fmtParts[i + 1];
                        if (next && typeof next == "string") {
                            if (ch.changeCase == "u")
                                fmtParts[i] = next[0].toUpperCase();
                            else
                                fmtParts[i] = next[0].toLowerCase();
                            fmtParts[i + 1] = next.substr(1);
                        }
                    }
                    else if (ch.changeCase) {
                        gChangeCase = ch.changeCase;
                    }
                }
                else if (gChangeCase == "U") {
                    fmtParts[i] = ch.toUpperCase();
                }
                else if (gChangeCase == "L") {
                    fmtParts[i] = ch.toLowerCase();
                }
            }
            _self.variables.__ = oldArgs;
            return fmtParts.join("");
        });
        return formatted;
    };
    SnippetManager.prototype.tmFormatFunction = function (str, ch, editor) {
        if (ch.formatFunction == "upcase")
            return str.toUpperCase();
        if (ch.formatFunction == "downcase")
            return str.toLowerCase();
        return str;
    };
    SnippetManager.prototype.resolveVariables = function (snippet, editor) {
        var result = [];
        var indentation = "";
        var afterNewLine = true;
        for (var i = 0; i < snippet.length; i++) {
            var ch = snippet[i];
            if (typeof ch == "string") {
                result.push(ch);
                if (ch == "\n") {
                    afterNewLine = true;
                    indentation = "";
                }
                else if (afterNewLine) {
                    indentation = /^\t*/.exec(ch)[0];
                    afterNewLine = /\S/.test(ch);
                }
                continue;
            }
            if (!ch)
                continue;
            afterNewLine = false;
            if (ch.fmtString) {
                var j = snippet.indexOf(ch, i + 1);
                if (j == -1)
                    j = snippet.length;
                ch.fmt = snippet.slice(i + 1, j);
                i = j;
            }
            if (ch.text) {
                var value = this.getVariableValue(editor, ch.text, indentation) + "";
                if (ch.fmtString)
                    value = this.tmStrFormat(value, ch, editor);
                if (ch.formatFunction)
                    value = this.tmFormatFunction(value, ch, editor);
                if (value && !ch.ifEnd) {
                    result.push(value);
                    gotoNext(ch);
                }
                else if (!value && ch.ifEnd) {
                    gotoNext(ch.ifEnd);
                }
            }
            else if (ch.elseEnd) {
                gotoNext(ch.elseEnd);
            }
            else if (ch.tabstopId != null) {
                result.push(ch);
            }
            else if (ch.changeCase != null) {
                result.push(ch);
            }
        }
        function gotoNext(ch) {
            var i1 = snippet.indexOf(ch, i + 1);
            if (i1 != -1)
                i = i1;
        }
        return result;
    };
    SnippetManager.prototype.getDisplayTextForSnippet = function (editor, snippetText) {
        var processedSnippet = processSnippetText.call(this, editor, snippetText);
        return processedSnippet.text;
    };
    SnippetManager.prototype.insertSnippetForSelection = function (editor, snippetText, options) {
        if (options === void 0) { options = {}; }
        var processedSnippet = processSnippetText.call(this, editor, snippetText, options);
        var range = editor.getSelectionRange();
        var end = editor.session.replace(range, processedSnippet.text);
        var tabstopManager = new TabstopManager(editor);
        var selectionId = editor.inVirtualSelectionMode && editor.selection.index;
        tabstopManager.addTabstops(processedSnippet.tabstops, range.start, end, selectionId);
    };
    SnippetManager.prototype.insertSnippet = function (editor, snippetText, options) {
        if (options === void 0) { options = {}; }
        var self = this;
        if (editor.inVirtualSelectionMode)
            return self.insertSnippetForSelection(editor, snippetText, options);
        editor.forEachSelection(function () {
            self.insertSnippetForSelection(editor, snippetText, options);
        }, null, { keepOrder: true });
        if (editor.tabstopManager)
            editor.tabstopManager.tabNext();
    };
    SnippetManager.prototype.$getScope = function (editor) {
        var scope = editor.session.$mode.$id || "";
        scope = scope.split("/").pop();
        if (scope === "html" || scope === "php") {
            if (scope === "php" && !editor.session.$mode.inlinePhp)
                scope = "html";
            var c = editor.getCursorPosition();
            var state = editor.session.getState(c.row);
            if (typeof state === "object") {
                state = state[0];
            }
            if (state.substring) {
                if (state.substring(0, 3) == "js-")
                    scope = "javascript";
                else if (state.substring(0, 4) == "css-")
                    scope = "css";
                else if (state.substring(0, 4) == "php-")
                    scope = "php";
            }
        }
        return scope;
    };
    SnippetManager.prototype.getActiveScopes = function (editor) {
        var scope = this.$getScope(editor);
        var scopes = [scope];
        var snippetMap = this.snippetMap;
        if (snippetMap[scope] && snippetMap[scope].includeScopes) {
            scopes.push.apply(scopes, snippetMap[scope].includeScopes);
        }
        scopes.push("_");
        return scopes;
    };
    SnippetManager.prototype.expandWithTab = function (editor, options) {
        var self = this;
        var result = editor.forEachSelection(function () {
            return self.expandSnippetForSelection(editor, options);
        }, null, { keepOrder: true });
        if (result && editor.tabstopManager)
            editor.tabstopManager.tabNext();
        return result;
    };
    SnippetManager.prototype.expandSnippetForSelection = function (editor, options) {
        var cursor = editor.getCursorPosition();
        var line = editor.session.getLine(cursor.row);
        var before = line.substring(0, cursor.column);
        var after = line.substr(cursor.column);
        var snippetMap = this.snippetMap;
        var snippet;
        this.getActiveScopes(editor).some(function (scope) {
            var snippets = snippetMap[scope];
            if (snippets)
                snippet = this.findMatchingSnippet(snippets, before, after);
            return !!snippet;
        }, this);
        if (!snippet)
            return false;
        if (options && options.dryRun)
            return true;
        editor.session.doc.removeInLine(cursor.row, cursor.column - snippet.replaceBefore.length, cursor.column + snippet.replaceAfter.length);
        this.variables.M__ = snippet.matchBefore;
        this.variables.T__ = snippet.matchAfter;
        this.insertSnippetForSelection(editor, snippet.content);
        this.variables.M__ = this.variables.T__ = null;
        return true;
    };
    SnippetManager.prototype.findMatchingSnippet = function (snippetList, before, after) {
        for (var i = snippetList.length; i--;) {
            var s = snippetList[i];
            if (s.startRe && !s.startRe.test(before))
                continue;
            if (s.endRe && !s.endRe.test(after))
                continue;
            if (!s.startRe && !s.endRe)
                continue;
            s.matchBefore = s.startRe ? s.startRe.exec(before) : [""];
            s.matchAfter = s.endRe ? s.endRe.exec(after) : [""];
            s.replaceBefore = s.triggerRe ? s.triggerRe.exec(before)[0] : "";
            s.replaceAfter = s.endTriggerRe ? s.endTriggerRe.exec(after)[0] : "";
            return s;
        }
    };
    SnippetManager.prototype.register = function (snippets, scope) {
        var snippetMap = this.snippetMap;
        var snippetNameMap = this.snippetNameMap;
        var self = this;
        if (!snippets)
            snippets = [];
        function wrapRegexp(src) {
            if (src && !/^\^?\(.*\)\$?$|^\\b$/.test(src))
                src = "(?:" + src + ")";
            return src || "";
        }
        function guardedRegexp(re, guard, opening) {
            re = wrapRegexp(re);
            guard = wrapRegexp(guard);
            if (opening) {
                re = guard + re;
                if (re && re[re.length - 1] != "$")
                    re = re + "$";
            }
            else {
                re = re + guard;
                if (re && re[0] != "^")
                    re = "^" + re;
            }
            return new RegExp(re);
        }
        function addSnippet(s) {
            if (!s.scope)
                s.scope = scope || "_";
            scope = s.scope;
            if (!snippetMap[scope]) {
                snippetMap[scope] = [];
                snippetNameMap[scope] = {};
            }
            var map = snippetNameMap[scope];
            if (s.name) {
                var old = map[s.name];
                if (old)
                    self.unregister(old);
                map[s.name] = s;
            }
            snippetMap[scope].push(s);
            if (s.prefix)
                s.tabTrigger = s.prefix;
            if (!s.content && s.body)
                s.content = Array.isArray(s.body) ? s.body.join("\n") : s.body;
            if (s.tabTrigger && !s.trigger) {
                if (!s.guard && /^\w/.test(s.tabTrigger))
                    s.guard = "\\b";
                s.trigger = lang.escapeRegExp(s.tabTrigger);
            }
            if (!s.trigger && !s.guard && !s.endTrigger && !s.endGuard)
                return;
            s.startRe = guardedRegexp(s.trigger, s.guard, true);
            s.triggerRe = new RegExp(s.trigger);
            s.endRe = guardedRegexp(s.endTrigger, s.endGuard, true);
            s.endTriggerRe = new RegExp(s.endTrigger);
        }
        if (Array.isArray(snippets)) {
            snippets.forEach(addSnippet);
        }
        else {
            Object.keys(snippets).forEach(function (key) {
                addSnippet(snippets[key]);
            });
        }
        this._signal("registerSnippets", { scope: scope });
    };
    SnippetManager.prototype.unregister = function (snippets, scope) {
        var snippetMap = this.snippetMap;
        var snippetNameMap = this.snippetNameMap;
        function removeSnippet(s) {
            var nameMap = snippetNameMap[s.scope || scope];
            if (nameMap && nameMap[s.name]) {
                delete nameMap[s.name];
                var map = snippetMap[s.scope || scope];
                var i = map && map.indexOf(s);
                if (i >= 0)
                    map.splice(i, 1);
            }
        }
        if (snippets.content)
            removeSnippet(snippets);
        else if (Array.isArray(snippets))
            snippets.forEach(removeSnippet);
    };
    SnippetManager.prototype.parseSnippetFile = function (str) {
        str = str.replace(/\r/g, "");
        var list = [], snippet = {};
        var re = /^#.*|^({[\s\S]*})\s*$|^(\S+) (.*)$|^((?:\n*\t.*)+)/gm;
        var m;
        while (m = re.exec(str)) {
            if (m[1]) {
                try {
                    snippet = JSON.parse(m[1]);
                    list.push(snippet);
                }
                catch (e) { }
            }
            if (m[4]) {
                snippet.content = m[4].replace(/^\t/gm, "");
                list.push(snippet);
                snippet = {};
            }
            else {
                var key = m[2], val = m[3];
                if (key == "regex") {
                    var guardRe = /\/((?:[^\/\\]|\\.)*)|$/g;
                    snippet.guard = guardRe.exec(val)[1];
                    snippet.trigger = guardRe.exec(val)[1];
                    snippet.endTrigger = guardRe.exec(val)[1];
                    snippet.endGuard = guardRe.exec(val)[1];
                }
                else if (key == "snippet") {
                    snippet.tabTrigger = val.match(/^\S*/)[0];
                    if (!snippet.name)
                        snippet.name = val;
                }
                else if (key) {
                    snippet[key] = val;
                }
            }
        }
        return list;
    };
    SnippetManager.prototype.getSnippetByName = function (name, editor) {
        var snippetMap = this.snippetNameMap;
        var snippet;
        this.getActiveScopes(editor).some(function (scope) {
            var snippets = snippetMap[scope];
            if (snippets)
                snippet = snippets[name];
            return !!snippet;
        }, this);
        return snippet;
    };
    return SnippetManager;
}());
oop.implement(SnippetManager.prototype, EventEmitter);
var processSnippetText = function (editor, snippetText, options) {
    if (options === void 0) { options = {}; }
    var cursor = editor.getCursorPosition();
    var line = editor.session.getLine(cursor.row);
    var tabString = editor.session.getTabString();
    var indentString = line.match(/^\s*/)[0];
    if (cursor.column < indentString.length)
        indentString = indentString.slice(0, cursor.column);
    snippetText = snippetText.replace(/\r/g, "");
    var tokens = this.tokenizeTmSnippet(snippetText);
    tokens = this.resolveVariables(tokens, editor);
    tokens = tokens.map(function (x) {
        if (x == "\n" && !options.excludeExtraIndent)
            return x + indentString;
        if (typeof x == "string")
            return x.replace(/\t/g, tabString);
        return x;
    });
    var tabstops = [];
    tokens.forEach(function (p, i) {
        if (typeof p != "object")
            return;
        var id = p.tabstopId;
        var ts = tabstops[id];
        if (!ts) {
            ts = tabstops[id] = [];
            ts.index = id;
            ts.value = "";
            ts.parents = {};
        }
        if (ts.indexOf(p) !== -1)
            return;
        if (p.choices && !ts.choices)
            ts.choices = p.choices;
        ts.push(p);
        var i1 = tokens.indexOf(p, i + 1);
        if (i1 === -1)
            return;
        var value = tokens.slice(i + 1, i1);
        var isNested = value.some(function (t) { return typeof t === "object"; });
        if (isNested && !ts.value) {
            ts.value = value;
        }
        else if (value.length && (!ts.value || typeof ts.value !== "string")) {
            ts.value = value.join("");
        }
    });
    tabstops.forEach(function (ts) { ts.length = 0; });
    var expanding = {};
    function copyValue(val) {
        var copy = [];
        for (var i = 0; i < val.length; i++) {
            var p = val[i];
            if (typeof p == "object") {
                if (expanding[p.tabstopId])
                    continue;
                var j = val.lastIndexOf(p, i - 1);
                p = copy[j] || { tabstopId: p.tabstopId };
            }
            copy[i] = p;
        }
        return copy;
    }
    for (var i = 0; i < tokens.length; i++) {
        var p = tokens[i];
        if (typeof p != "object")
            continue;
        var id = p.tabstopId;
        var ts = tabstops[id];
        var i1 = tokens.indexOf(p, i + 1);
        if (expanding[id]) {
            if (expanding[id] === p) {
                delete expanding[id];
                Object.keys(expanding).forEach(function (parentId) {
                    ts.parents[parentId] = true;
                });
            }
            continue;
        }
        expanding[id] = p;
        var value = ts.value;
        if (typeof value !== "string")
            value = copyValue(value);
        else if (p.fmt)
            value = this.tmStrFormat(value, p, editor);
        tokens.splice.apply(tokens, [i + 1, Math.max(0, i1 - i)].concat(value, p));
        if (ts.indexOf(p) === -1)
            ts.push(p);
    }
    var row = 0, column = 0;
    var text = "";
    tokens.forEach(function (t) {
        if (typeof t === "string") {
            var lines = t.split("\n");
            if (lines.length > 1) {
                column = lines[lines.length - 1].length;
                row += lines.length - 1;
            }
            else
                column += t.length;
            text += t;
        }
        else if (t) {
            if (!t.start)
                t.start = { row: row, column: column };
            else
                t.end = { row: row, column: column };
        }
    });
    return {
        text: text,
        tabstops: tabstops,
        tokens: tokens
    };
};
var TabstopManager = /** @class */ (function () {
    function TabstopManager(editor) {
        this.index = 0;
        this.ranges = [];
        this.tabstops = [];
        if (editor.tabstopManager)
            return editor.tabstopManager;
        editor.tabstopManager = this;
        this.$onChange = this.onChange.bind(this);
        this.$onChangeSelection = lang.delayedCall(this.onChangeSelection.bind(this)).schedule;
        this.$onChangeSession = this.onChangeSession.bind(this);
        this.$onAfterExec = this.onAfterExec.bind(this);
        this.attach(editor);
    }
    TabstopManager.prototype.attach = function (editor) {
        this.$openTabstops = null;
        this.selectedTabstop = null;
        this.editor = editor;
        this.session = editor.session;
        this.editor.on("change", this.$onChange);
        this.editor.on("changeSelection", this.$onChangeSelection);
        this.editor.on("changeSession", this.$onChangeSession);
        this.editor.commands.on("afterExec", this.$onAfterExec);
        this.editor.keyBinding.addKeyboardHandler(this.keyboardHandler);
    };
    TabstopManager.prototype.detach = function () {
        this.tabstops.forEach(this.removeTabstopMarkers, this);
        this.ranges.length = 0;
        this.tabstops.length = 0;
        this.selectedTabstop = null;
        this.editor.off("change", this.$onChange);
        this.editor.off("changeSelection", this.$onChangeSelection);
        this.editor.off("changeSession", this.$onChangeSession);
        this.editor.commands.off("afterExec", this.$onAfterExec);
        this.editor.keyBinding.removeKeyboardHandler(this.keyboardHandler);
        this.editor.tabstopManager = null;
        this.session = null;
        this.editor = null;
    };
    TabstopManager.prototype.onChange = function (delta) {
        var isRemove = delta.action[0] == "r";
        var selectedTabstop = this.selectedTabstop || {};
        var parents = selectedTabstop.parents || {};
        var tabstops = this.tabstops.slice();
        for (var i = 0; i < tabstops.length; i++) {
            var ts = tabstops[i];
            var active = ts == selectedTabstop || parents[ts.index];
            ts.rangeList.$bias = active ? 0 : 1;
            if (delta.action == "remove" && ts !== selectedTabstop) {
                var parentActive = ts.parents && ts.parents[selectedTabstop.index];
                var startIndex = ts.rangeList.pointIndex(delta.start, parentActive);
                startIndex = startIndex < 0 ? -startIndex - 1 : startIndex + 1;
                var endIndex = ts.rangeList.pointIndex(delta.end, parentActive);
                endIndex = endIndex < 0 ? -endIndex - 1 : endIndex - 1;
                var toRemove = ts.rangeList.ranges.slice(startIndex, endIndex);
                for (var j = 0; j < toRemove.length; j++)
                    this.removeRange(toRemove[j]);
            }
            ts.rangeList.$onChange(delta);
        }
        var session = this.session;
        if (!this.$inChange && isRemove && session.getLength() == 1 && !session.getValue())
            this.detach();
    };
    TabstopManager.prototype.updateLinkedFields = function () {
        var ts = this.selectedTabstop;
        if (!ts || !ts.hasLinkedRanges || !ts.firstNonLinked)
            return;
        this.$inChange = true;
        var session = this.session;
        var text = session.getTextRange(ts.firstNonLinked);
        for (var i = 0; i < ts.length; i++) {
            var range = ts[i];
            if (!range.linked)
                continue;
            var original = range.original;
            var fmt = exports.snippetManager.tmStrFormat(text, original, this.editor);
            session.replace(range, fmt);
        }
        this.$inChange = false;
    };
    TabstopManager.prototype.onAfterExec = function (e) {
        if (e.command && !e.command.readOnly)
            this.updateLinkedFields();
    };
    TabstopManager.prototype.onChangeSelection = function () {
        if (!this.editor)
            return;
        var lead = this.editor.selection.lead;
        var anchor = this.editor.selection.anchor;
        var isEmpty = this.editor.selection.isEmpty();
        for (var i = 0; i < this.ranges.length; i++) {
            if (this.ranges[i].linked)
                continue;
            var containsLead = this.ranges[i].contains(lead.row, lead.column);
            var containsAnchor = isEmpty || this.ranges[i].contains(anchor.row, anchor.column);
            if (containsLead && containsAnchor)
                return;
        }
        this.detach();
    };
    TabstopManager.prototype.onChangeSession = function () {
        this.detach();
    };
    TabstopManager.prototype.tabNext = function (dir) {
        var max = this.tabstops.length;
        var index = this.index + (dir || 1);
        index = Math.min(Math.max(index, 1), max);
        if (index == max)
            index = 0;
        this.selectTabstop(index);
        if (index === 0)
            this.detach();
    };
    TabstopManager.prototype.selectTabstop = function (index) {
        this.$openTabstops = null;
        var ts = this.tabstops[this.index];
        if (ts)
            this.addTabstopMarkers(ts);
        this.index = index;
        ts = this.tabstops[this.index];
        if (!ts || !ts.length)
            return;
        this.selectedTabstop = ts;
        var range = ts.firstNonLinked || ts;
        if (ts.choices)
            range.cursor = range.start;
        if (!this.editor.inVirtualSelectionMode) {
            var sel = this.editor.multiSelect;
            sel.toSingleRange(range);
            for (var i = 0; i < ts.length; i++) {
                if (ts.hasLinkedRanges && ts[i].linked)
                    continue;
                sel.addRange(ts[i].clone(), true);
            }
        }
        else {
            this.editor.selection.fromOrientedRange(range);
        }
        this.editor.keyBinding.addKeyboardHandler(this.keyboardHandler);
        if (this.selectedTabstop && this.selectedTabstop.choices)
            this.editor.execCommand("startAutocomplete", { matches: this.selectedTabstop.choices });
    };
    TabstopManager.prototype.addTabstops = function (tabstops, start, end) {
        var useLink = this.useLink || !this.editor.getOption("enableMultiselect");
        if (!this.$openTabstops)
            this.$openTabstops = [];
        if (!tabstops[0]) {
            var p = Range.fromPoints(end, end);
            moveRelative(p.start, start);
            moveRelative(p.end, start);
            tabstops[0] = [p];
            tabstops[0].index = 0;
        }
        var i = this.index;
        var arg = [i + 1, 0];
        var ranges = this.ranges;
        tabstops.forEach(function (ts, index) {
            var dest = this.$openTabstops[index] || ts;
            for (var i = 0; i < ts.length; i++) {
                var p = ts[i];
                var range = Range.fromPoints(p.start, p.end || p.start);
                movePoint(range.start, start);
                movePoint(range.end, start);
                range.original = p;
                range.tabstop = dest;
                ranges.push(range);
                if (dest != ts)
                    dest.unshift(range);
                else
                    dest[i] = range;
                if (p.fmtString || (dest.firstNonLinked && useLink)) {
                    range.linked = true;
                    dest.hasLinkedRanges = true;
                }
                else if (!dest.firstNonLinked)
                    dest.firstNonLinked = range;
            }
            if (!dest.firstNonLinked)
                dest.hasLinkedRanges = false;
            if (dest === ts) {
                arg.push(dest);
                this.$openTabstops[index] = dest;
            }
            this.addTabstopMarkers(dest);
            dest.rangeList = dest.rangeList || new RangeList();
            dest.rangeList.$bias = 0;
            dest.rangeList.addList(dest);
        }, this);
        if (arg.length > 2) {
            if (this.tabstops.length)
                arg.push(arg.splice(2, 1)[0]);
            this.tabstops.splice.apply(this.tabstops, arg);
        }
    };
    TabstopManager.prototype.addTabstopMarkers = function (ts) {
        var session = this.session;
        ts.forEach(function (range) {
            if (!range.markerId)
                range.markerId = session.addMarker(range, "ace_snippet-marker", "text");
        });
    };
    TabstopManager.prototype.removeTabstopMarkers = function (ts) {
        var session = this.session;
        ts.forEach(function (range) {
            session.removeMarker(range.markerId);
            range.markerId = null;
        });
    };
    TabstopManager.prototype.removeRange = function (range) {
        var i = range.tabstop.indexOf(range);
        if (i != -1)
            range.tabstop.splice(i, 1);
        i = this.ranges.indexOf(range);
        if (i != -1)
            this.ranges.splice(i, 1);
        i = range.tabstop.rangeList.ranges.indexOf(range);
        if (i != -1)
            range.tabstop.splice(i, 1);
        this.session.removeMarker(range.markerId);
        if (!range.tabstop.length) {
            i = this.tabstops.indexOf(range.tabstop);
            if (i != -1)
                this.tabstops.splice(i, 1);
            if (!this.tabstops.length)
                this.detach();
        }
    };
    return TabstopManager;
}());
TabstopManager.prototype.keyboardHandler = new HashHandler();
TabstopManager.prototype.keyboardHandler.bindKeys({
    "Tab": function (editor) {
        if (exports.snippetManager && exports.snippetManager.expandWithTab(editor))
            return;
        editor.tabstopManager.tabNext(1);
        editor.renderer.scrollCursorIntoView();
    },
    "Shift-Tab": function (editor) {
        editor.tabstopManager.tabNext(-1);
        editor.renderer.scrollCursorIntoView();
    },
    "Esc": function (editor) {
        editor.tabstopManager.detach();
    }
});
var movePoint = function (point, diff) {
    if (point.row == 0)
        point.column += diff.column;
    point.row += diff.row;
};
var moveRelative = function (point, start) {
    if (point.row == start.row)
        point.column -= start.column;
    point.row -= start.row;
};
dom.importCssString("\n.ace_snippet-marker {\n    -moz-box-sizing: border-box;\n    box-sizing: border-box;\n    background: rgba(194, 193, 208, 0.09);\n    border: 1px dotted rgba(211, 208, 235, 0.62);\n    position: absolute;\n}", "snippets.css", false);
exports.snippetManager = new SnippetManager();
var Editor = require("./editor").Editor;
(function () {
    this.insertSnippet = function (content, options) {
        return exports.snippetManager.insertSnippet(this, content, options);
    };
    this.expandSnippet = function (options) {
        return exports.snippetManager.expandWithTab(this, options);
    };
}).call(Editor.prototype);

});

define("ace/autocomplete/inline",["require","exports","module","ace/snippets"], function(require, exports, module){"use strict";
var snippetManager = require("../snippets").snippetManager;
var AceInline = /** @class */ (function () {
    function AceInline() {
        this.editor = null;
    }
    AceInline.prototype.show = function (editor, completion, prefix) {
        prefix = prefix || "";
        if (editor && this.editor && this.editor !== editor) {
            this.hide();
            this.editor = null;
        }
        if (!editor || !completion) {
            return false;
        }
        var displayText = completion.snippet ? snippetManager.getDisplayTextForSnippet(editor, completion.snippet) : completion.value;
        if (completion.hideInlinePreview || !displayText || !displayText.startsWith(prefix)) {
            return false;
        }
        this.editor = editor;
        displayText = displayText.slice(prefix.length);
        if (displayText === "") {
            editor.removeGhostText();
        }
        else {
            editor.setGhostText(displayText);
        }
        return true;
    };
    AceInline.prototype.isOpen = function () {
        if (!this.editor) {
            return false;
        }
        return !!this.editor.renderer.$ghostText;
    };
    AceInline.prototype.hide = function () {
        if (!this.editor) {
            return false;
        }
        this.editor.removeGhostText();
        return true;
    };
    AceInline.prototype.destroy = function () {
        this.hide();
        this.editor = null;
    };
    return AceInline;
}());
exports.AceInline = AceInline;

});

define("ace/autocomplete/util",["require","exports","module"], function(require, exports, module){"use strict";
exports.parForEach = function (array, fn, callback) {
    var completed = 0;
    var arLength = array.length;
    if (arLength === 0)
        callback();
    for (var i = 0; i < arLength; i++) {
        fn(array[i], function (result, err) {
            completed++;
            if (completed === arLength)
                callback(result, err);
        });
    }
};
var ID_REGEX = /[a-zA-Z_0-9\$\-\u00A2-\u2000\u2070-\uFFFF]/;
exports.retrievePrecedingIdentifier = function (text, pos, regex) {
    regex = regex || ID_REGEX;
    var buf = [];
    for (var i = pos - 1; i >= 0; i--) {
        if (regex.test(text[i]))
            buf.push(text[i]);
        else
            break;
    }
    return buf.reverse().join("");
};
exports.retrieveFollowingIdentifier = function (text, pos, regex) {
    regex = regex || ID_REGEX;
    var buf = [];
    for (var i = pos; i < text.length; i++) {
        if (regex.test(text[i]))
            buf.push(text[i]);
        else
            break;
    }
    return buf;
};
exports.getCompletionPrefix = function (editor) {
    var pos = editor.getCursorPosition();
    var line = editor.session.getLine(pos.row);
    var prefix;
    editor.completers.forEach(function (completer) {
        if (completer.identifierRegexps) {
            completer.identifierRegexps.forEach(function (identifierRegex) {
                if (!prefix && identifierRegex)
                    prefix = this.retrievePrecedingIdentifier(line, pos.column, identifierRegex);
            }.bind(this));
        }
    }.bind(this));
    return prefix || this.retrievePrecedingIdentifier(line, pos.column);
};
exports.triggerAutocomplete = function (editor) {
    var pos = editor.getCursorPosition();
    var line = editor.session.getLine(pos.row);
    var column = (pos.column === 0) ? 0 : pos.column - 1;
    var previousChar = line[column];
    return editor.completers.some(function (el) {
        if (el.triggerCharacters && Array.isArray(el.triggerCharacters)) {
            return el.triggerCharacters.includes(previousChar);
        }
    });
};

});

define("ace/autocomplete",["require","exports","module","ace/keyboard/hash_handler","ace/autocomplete/popup","ace/autocomplete/inline","ace/autocomplete/popup","ace/autocomplete/util","ace/lib/lang","ace/lib/dom","ace/snippets","ace/config"], function(require, exports, module){"use strict";
var HashHandler = require("./keyboard/hash_handler").HashHandler;
var AcePopup = require("./autocomplete/popup").AcePopup;
var AceInline = require("./autocomplete/inline").AceInline;
var getAriaId = require("./autocomplete/popup").getAriaId;
var util = require("./autocomplete/util");
var lang = require("./lib/lang");
var dom = require("./lib/dom");
var snippetManager = require("./snippets").snippetManager;
var config = require("./config");
var destroyCompleter = function (e, editor) {
    editor.completer && editor.completer.destroy();
};
var Autocomplete = /** @class */ (function () {
    function Autocomplete() {
        this.autoInsert = false;
        this.autoSelect = true;
        this.autoShown = false;
        this.exactMatch = false;
        this.inlineEnabled = false;
        this.keyboardHandler = new HashHandler();
        this.keyboardHandler.bindKeys(this.commands);
        this.parentNode = null;
        this.setSelectOnHover = false;
        this.stickySelectionDelay = 500;
        this.blurListener = this.blurListener.bind(this);
        this.changeListener = this.changeListener.bind(this);
        this.mousedownListener = this.mousedownListener.bind(this);
        this.mousewheelListener = this.mousewheelListener.bind(this);
        this.onLayoutChange = this.onLayoutChange.bind(this);
        this.changeTimer = lang.delayedCall(function () {
            this.updateCompletions(true);
        }.bind(this));
        this.tooltipTimer = lang.delayedCall(this.updateDocTooltip.bind(this), 50);
        this.stickySelectionTimer = lang.delayedCall(function () {
            this.stickySelection = true;
        }.bind(this), this.stickySelectionDelay);
    }
    Autocomplete.prototype.$init = function () {
        this.popup = new AcePopup(this.parentNode || document.body || document.documentElement);
        this.popup.on("click", function (e) {
            this.insertMatch();
            e.stop();
        }.bind(this));
        this.popup.focus = this.editor.focus.bind(this.editor);
        this.popup.on("show", this.$onPopupShow.bind(this));
        this.popup.on("hide", this.$onHidePopup.bind(this));
        this.popup.on("select", this.$onPopupChange.bind(this));
        this.popup.on("changeHoverMarker", this.tooltipTimer.bind(null, null));
        return this.popup;
    };
    Autocomplete.prototype.$initInline = function () {
        if (!this.inlineEnabled || this.inlineRenderer)
            return;
        this.inlineRenderer = new AceInline();
        return this.inlineRenderer;
    };
    Autocomplete.prototype.getPopup = function () {
        return this.popup || this.$init();
    };
    Autocomplete.prototype.$onHidePopup = function () {
        if (this.inlineRenderer) {
            this.inlineRenderer.hide();
        }
        this.hideDocTooltip();
        this.stickySelectionTimer.cancel();
        this.stickySelection = false;
    };
    Autocomplete.prototype.$onPopupChange = function (hide) {
        if (this.inlineRenderer && this.inlineEnabled) {
            var completion = hide ? null : this.popup.getData(this.popup.getRow());
            var prefix = util.getCompletionPrefix(this.editor);
            if (!this.inlineRenderer.show(this.editor, completion, prefix)) {
                this.inlineRenderer.hide();
            }
            this.$updatePopupPosition();
        }
        this.tooltipTimer.call(null, null);
    };
    Autocomplete.prototype.$onPopupShow = function (hide) {
        this.$onPopupChange(hide);
        this.stickySelection = false;
        if (this.stickySelectionDelay >= 0)
            this.stickySelectionTimer.schedule(this.stickySelectionDelay);
    };
    Autocomplete.prototype.observeLayoutChanges = function () {
        if (this.$elements || !this.editor)
            return;
        window.addEventListener("resize", this.onLayoutChange, { passive: true });
        window.addEventListener("wheel", this.mousewheelListener);
        var el = this.editor.container.parentNode;
        var elements = [];
        while (el) {
            elements.push(el);
            el.addEventListener("scroll", this.onLayoutChange, { passive: true });
            el = el.parentNode;
        }
        this.$elements = elements;
    };
    Autocomplete.prototype.unObserveLayoutChanges = function () {
        var _this = this;
        window.removeEventListener("resize", this.onLayoutChange, { passive: true });
        window.removeEventListener("wheel", this.mousewheelListener);
        this.$elements && this.$elements.forEach(function (el) {
            el.removeEventListener("scroll", _this.onLayoutChange, { passive: true });
        });
        this.$elements = null;
    };
    Autocomplete.prototype.onLayoutChange = function () {
        if (!this.popup.isOpen)
            return this.unObserveLayoutChanges();
        this.$updatePopupPosition();
        this.updateDocTooltip();
    };
    Autocomplete.prototype.$updatePopupPosition = function () {
        var editor = this.editor;
        var renderer = editor.renderer;
        var lineHeight = renderer.layerConfig.lineHeight;
        var pos = renderer.$cursorLayer.getPixelPosition(this.base, true);
        pos.left -= this.popup.getTextLeftOffset();
        var rect = editor.container.getBoundingClientRect();
        pos.top += rect.top - renderer.layerConfig.offset;
        pos.left += rect.left - editor.renderer.scrollLeft;
        pos.left += renderer.gutterWidth;
        var posGhostText = {
            top: pos.top,
            left: pos.left
        };
        if (renderer.$ghostText && renderer.$ghostTextWidget) {
            if (this.base.row === renderer.$ghostText.position.row) {
                posGhostText.top += renderer.$ghostTextWidget.el.offsetHeight;
            }
        }
        if (this.popup.tryShow(posGhostText, lineHeight, "bottom")) {
            return;
        }
        if (this.popup.tryShow(pos, lineHeight, "top")) {
            return;
        }
        this.popup.show(pos, lineHeight);
    };
    Autocomplete.prototype.openPopup = function (editor, prefix, keepPopupPosition) {
        if (!this.popup)
            this.$init();
        if (this.inlineEnabled && !this.inlineRenderer)
            this.$initInline();
        this.popup.autoSelect = this.autoSelect;
        this.popup.setSelectOnHover(this.setSelectOnHover);
        var previousSelectedItem = this.popup.data[this.popup.getRow()];
        this.popup.setData(this.completions.filtered, this.completions.filterText);
        if (this.editor.textInput.setAriaOptions) {
            this.editor.textInput.setAriaOptions({
                activeDescendant: getAriaId(this.popup.getRow()),
                inline: this.inlineEnabled
            });
        }
        editor.keyBinding.addKeyboardHandler(this.keyboardHandler);
        var newRow = this.popup.data.indexOf(previousSelectedItem);
        if (newRow && this.stickySelection)
            this.popup.setRow(this.autoSelect ? newRow : -1);
        else
            this.popup.setRow(this.autoSelect ? 0 : -1);
        if (!keepPopupPosition) {
            this.popup.setTheme(editor.getTheme());
            this.popup.setFontSize(editor.getFontSize());
            this.$updatePopupPosition();
            if (this.tooltipNode) {
                this.updateDocTooltip();
            }
        }
        else if (keepPopupPosition && !prefix) {
            this.detach();
        }
        this.changeTimer.cancel();
        this.observeLayoutChanges();
    };
    Autocomplete.prototype.detach = function () {
        if (this.editor) {
            this.editor.keyBinding.removeKeyboardHandler(this.keyboardHandler);
            this.editor.off("changeSelection", this.changeListener);
            this.editor.off("blur", this.blurListener);
            this.editor.off("mousedown", this.mousedownListener);
            this.editor.off("mousewheel", this.mousewheelListener);
        }
        this.changeTimer.cancel();
        this.hideDocTooltip();
        if (this.completionProvider) {
            this.completionProvider.detach();
        }
        if (this.popup && this.popup.isOpen)
            this.popup.hide();
        if (this.base)
            this.base.detach();
        this.activated = false;
        this.completionProvider = this.completions = this.base = null;
        this.unObserveLayoutChanges();
    };
    Autocomplete.prototype.changeListener = function (e) {
        var cursor = this.editor.selection.lead;
        if (cursor.row != this.base.row || cursor.column < this.base.column) {
            this.detach();
        }
        if (this.activated)
            this.changeTimer.schedule();
        else
            this.detach();
    };
    Autocomplete.prototype.blurListener = function (e) {
        var el = document.activeElement;
        var text = this.editor.textInput.getElement();
        var fromTooltip = e.relatedTarget && this.tooltipNode && this.tooltipNode.contains(e.relatedTarget);
        var container = this.popup && this.popup.container;
        if (el != text && el.parentNode != container && !fromTooltip
            && el != this.tooltipNode && e.relatedTarget != text) {
            this.detach();
        }
    };
    Autocomplete.prototype.mousedownListener = function (e) {
        this.detach();
    };
    Autocomplete.prototype.mousewheelListener = function (e) {
        this.detach();
    };
    Autocomplete.prototype.goTo = function (where) {
        this.popup.goTo(where);
    };
    Autocomplete.prototype.insertMatch = function (data, options) {
        if (!data)
            data = this.popup.getData(this.popup.getRow());
        if (!data)
            return false;
        if (data.value === "") // Explicitly given nothing to insert, e.g. "No suggestion state"
            return this.detach();
        var completions = this.completions;
        var result = this.getCompletionProvider().insertMatch(this.editor, data, completions.filterText, options);
        if (this.completions == completions)
            this.detach();
        return result;
    };
    Autocomplete.prototype.showPopup = function (editor, options) {
        if (this.editor)
            this.detach();
        this.activated = true;
        this.editor = editor;
        if (editor.completer != this) {
            if (editor.completer)
                editor.completer.detach();
            editor.completer = this;
        }
        editor.on("changeSelection", this.changeListener);
        editor.on("blur", this.blurListener);
        editor.on("mousedown", this.mousedownListener);
        editor.on("mousewheel", this.mousewheelListener);
        this.updateCompletions(false, options);
    };
    Autocomplete.prototype.getCompletionProvider = function (initialPosition) {
        if (!this.completionProvider)
            this.completionProvider = new CompletionProvider(initialPosition);
        return this.completionProvider;
    };
    Autocomplete.prototype.gatherCompletions = function (editor, callback) {
        return this.getCompletionProvider().gatherCompletions(editor, callback);
    };
    Autocomplete.prototype.updateCompletions = function (keepPopupPosition, options) {
        if (keepPopupPosition && this.base && this.completions) {
            var pos = this.editor.getCursorPosition();
            var prefix = this.editor.session.getTextRange({ start: this.base, end: pos });
            if (prefix == this.completions.filterText)
                return;
            this.completions.setFilter(prefix);
            if (!this.completions.filtered.length)
                return this.detach();
            if (this.completions.filtered.length == 1
                && this.completions.filtered[0].value == prefix
                && !this.completions.filtered[0].snippet)
                return this.detach();
            this.openPopup(this.editor, prefix, keepPopupPosition);
            return;
        }
        if (options && options.matches) {
            var pos = this.editor.getSelectionRange().start;
            this.base = this.editor.session.doc.createAnchor(pos.row, pos.column);
            this.base.$insertRight = true;
            this.completions = new FilteredList(options.matches);
            return this.openPopup(this.editor, "", keepPopupPosition);
        }
        var session = this.editor.getSession();
        var pos = this.editor.getCursorPosition();
        var prefix = util.getCompletionPrefix(this.editor);
        this.base = session.doc.createAnchor(pos.row, pos.column - prefix.length);
        this.base.$insertRight = true;
        var completionOptions = {
            exactMatch: this.exactMatch,
            ignoreCaption: this.ignoreCaption
        };
        this.getCompletionProvider({
            prefix: prefix,
            pos: pos
        }).provideCompletions(this.editor, completionOptions, function (err, completions, finished) {
            var filtered = completions.filtered;
            var prefix = util.getCompletionPrefix(this.editor);
            if (finished) {
                if (!filtered.length) {
                    var emptyMessage = !this.autoShown && this.emptyMessage;
                    if (typeof emptyMessage == "function")
                        emptyMessage = this.emptyMessage(prefix);
                    if (emptyMessage) {
                        var completionsForEmpty = [{
                                caption: this.emptyMessage(prefix),
                                value: ""
                            }];
                        this.completions = new FilteredList(completionsForEmpty);
                        this.openPopup(this.editor, prefix, keepPopupPosition);
                        return;
                    }
                    return this.detach();
                }
                if (filtered.length == 1 && filtered[0].value == prefix && !filtered[0].snippet)
                    return this.detach();
                if (this.autoInsert && !this.autoShown && filtered.length == 1)
                    return this.insertMatch(filtered[0]);
            }
            this.completions = completions;
            this.openPopup(this.editor, prefix, keepPopupPosition);
        }.bind(this));
    };
    Autocomplete.prototype.cancelContextMenu = function () {
        this.editor.$mouseHandler.cancelContextMenu();
    };
    Autocomplete.prototype.updateDocTooltip = function () {
        var popup = this.popup;
        var all = popup.data;
        var selected = all && (all[popup.getHoveredRow()] || all[popup.getRow()]);
        var doc = null;
        if (!selected || !this.editor || !this.popup.isOpen)
            return this.hideDocTooltip();
        var completersLength = this.editor.completers.length;
        for (var i = 0; i < completersLength; i++) {
            var completer = this.editor.completers[i];
            if (completer.getDocTooltip && selected.completerId === completer.id) {
                doc = completer.getDocTooltip(selected);
                break;
            }
        }
        if (!doc && typeof selected != "string")
            doc = selected;
        if (typeof doc == "string")
            doc = { docText: doc };
        if (!doc || !(doc.docHTML || doc.docText))
            return this.hideDocTooltip();
        this.showDocTooltip(doc);
    };
    Autocomplete.prototype.showDocTooltip = function (item) {
        if (!this.tooltipNode) {
            this.tooltipNode = dom.createElement("div");
            this.tooltipNode.style.margin = 0;
            this.tooltipNode.style.pointerEvents = "auto";
            this.tooltipNode.tabIndex = -1;
            this.tooltipNode.onblur = this.blurListener.bind(this);
            this.tooltipNode.onclick = this.onTooltipClick.bind(this);
            this.tooltipNode.id = "doc-tooltip";
            this.tooltipNode.setAttribute("role", "tooltip");
        }
        var theme = this.editor.renderer.theme;
        this.tooltipNode.className = "ace_tooltip ace_doc-tooltip " +
            (theme.isDark ? "ace_dark " : "") + (theme.cssClass || "");
        var tooltipNode = this.tooltipNode;
        if (item.docHTML) {
            tooltipNode.innerHTML = item.docHTML;
        }
        else if (item.docText) {
            tooltipNode.textContent = item.docText;
        }
        if (!tooltipNode.parentNode)
            this.popup.container.appendChild(this.tooltipNode);
        var popup = this.popup;
        var rect = popup.container.getBoundingClientRect();
        tooltipNode.style.top = popup.container.style.top;
        tooltipNode.style.bottom = popup.container.style.bottom;
        tooltipNode.style.display = "block";
        if (window.innerWidth - rect.right < 320) {
            if (rect.left < 320) {
                if (popup.isTopdown) {
                    tooltipNode.style.top = rect.bottom + "px";
                    tooltipNode.style.left = rect.left + "px";
                    tooltipNode.style.right = "";
                    tooltipNode.style.bottom = "";
                }
                else {
                    tooltipNode.style.top = popup.container.offsetTop - tooltipNode.offsetHeight + "px";
                    tooltipNode.style.left = rect.left + "px";
                    tooltipNode.style.right = "";
                    tooltipNode.style.bottom = "";
                }
            }
            else {
                tooltipNode.style.right = window.innerWidth - rect.left + "px";
                tooltipNode.style.left = "";
            }
        }
        else {
            tooltipNode.style.left = (rect.right + 1) + "px";
            tooltipNode.style.right = "";
        }
    };
    Autocomplete.prototype.hideDocTooltip = function () {
        this.tooltipTimer.cancel();
        if (!this.tooltipNode)
            return;
        var el = this.tooltipNode;
        if (!this.editor.isFocused() && document.activeElement == el)
            this.editor.focus();
        this.tooltipNode = null;
        if (el.parentNode)
            el.parentNode.removeChild(el);
    };
    Autocomplete.prototype.onTooltipClick = function (e) {
        var a = e.target;
        while (a && a != this.tooltipNode) {
            if (a.nodeName == "A" && a.href) {
                a.rel = "noreferrer";
                a.target = "_blank";
                break;
            }
            a = a.parentNode;
        }
    };
    Autocomplete.prototype.destroy = function () {
        this.detach();
        if (this.popup) {
            this.popup.destroy();
            var el = this.popup.container;
            if (el && el.parentNode)
                el.parentNode.removeChild(el);
        }
        if (this.editor && this.editor.completer == this) {
            this.editor.off("destroy", destroyCompleter);
            this.editor.completer = null;
        }
        this.inlineRenderer = this.popup = this.editor = null;
    };
    return Autocomplete;
}());
Autocomplete.prototype.commands = {
    "Up": function (editor) { editor.completer.goTo("up"); },
    "Down": function (editor) { editor.completer.goTo("down"); },
    "Ctrl-Up|Ctrl-Home": function (editor) { editor.completer.goTo("start"); },
    "Ctrl-Down|Ctrl-End": function (editor) { editor.completer.goTo("end"); },
    "Esc": function (editor) { editor.completer.detach(); },
    "Return": function (editor) { return editor.completer.insertMatch(); },
    "Shift-Return": function (editor) { editor.completer.insertMatch(null, { deleteSuffix: true }); },
    "Tab": function (editor) {
        var result = editor.completer.insertMatch();
        if (!result && !editor.tabstopManager)
            editor.completer.goTo("down");
        else
            return result;
    },
    "PageUp": function (editor) { editor.completer.popup.gotoPageUp(); },
    "PageDown": function (editor) { editor.completer.popup.gotoPageDown(); }
};
Autocomplete.for = function (editor) {
    if (editor.completer instanceof Autocomplete) {
        return editor.completer;
    }
    if (editor.completer) {
        editor.completer.destroy();
        editor.completer = null;
    }
    if (config.get("sharedPopups")) {
        if (!Autocomplete.$sharedInstance)
            Autocomplete.$sharedInstance = new Autocomplete();
        editor.completer = Autocomplete.$sharedInstance;
    }
    else {
        editor.completer = new Autocomplete();
        editor.once("destroy", destroyCompleter);
    }
    return editor.completer;
};
Autocomplete.startCommand = {
    name: "startAutocomplete",
    exec: function (editor, options) {
        var completer = Autocomplete.for(editor);
        completer.autoInsert = false;
        completer.autoSelect = true;
        completer.autoShown = false;
        completer.showPopup(editor, options);
        completer.cancelContextMenu();
    },
    bindKey: "Ctrl-Space|Ctrl-Shift-Space|Alt-Space"
};
var CompletionProvider = /** @class */ (function () {
    function CompletionProvider(initialPosition) {
        this.initialPosition = initialPosition;
        this.active = true;
    }
    CompletionProvider.prototype.insertByIndex = function (editor, index, options) {
        if (!this.completions || !this.completions.filtered) {
            return false;
        }
        return this.insertMatch(editor, this.completions.filtered[index], options);
    };
    CompletionProvider.prototype.insertMatch = function (editor, data, options) {
        if (!data)
            return false;
        editor.startOperation({ command: { name: "insertMatch" } });
        if (data.completer && data.completer.insertMatch) {
            data.completer.insertMatch(editor, data);
        }
        else {
            if (!this.completions)
                return false;
            var replaceBefore = this.completions.filterText.length;
            var replaceAfter = 0;
            if (data.range && data.range.start.row === data.range.end.row) {
                replaceBefore -= this.initialPosition.prefix.length;
                replaceBefore += this.initialPosition.pos.column - data.range.start.column;
                replaceAfter += data.range.end.column - this.initialPosition.pos.column;
            }
            if (replaceBefore || replaceAfter) {
                var ranges;
                if (editor.selection.getAllRanges) {
                    ranges = editor.selection.getAllRanges();
                }
                else {
                    ranges = [editor.getSelectionRange()];
                }
                for (var i = 0, range; range = ranges[i]; i++) {
                    range.start.column -= replaceBefore;
                    range.end.column += replaceAfter;
                    editor.session.remove(range);
                }
            }
            if (data.snippet) {
                snippetManager.insertSnippet(editor, data.snippet);
            }
            else {
                this.$insertString(editor, data);
            }
            if (data.command && data.command === "startAutocomplete") {
                editor.execCommand(data.command);
            }
        }
        editor.endOperation();
        return true;
    };
    CompletionProvider.prototype.$insertString = function (editor, data) {
        var text = data.value || data;
        editor.execCommand("insertstring", text);
    };
    CompletionProvider.prototype.gatherCompletions = function (editor, callback) {
        var session = editor.getSession();
        var pos = editor.getCursorPosition();
        var prefix = util.getCompletionPrefix(editor);
        var matches = [];
        this.completers = editor.completers;
        var total = editor.completers.length;
        editor.completers.forEach(function (completer, i) {
            completer.getCompletions(editor, session, pos, prefix, function (err, results) {
                if (completer.hideInlinePreview)
                    results = results.map(function (result) {
                        return Object.assign(result, { hideInlinePreview: completer.hideInlinePreview });
                    });
                if (!err && results)
                    matches = matches.concat(results);
                callback(null, {
                    prefix: util.getCompletionPrefix(editor),
                    matches: matches,
                    finished: (--total === 0)
                });
            });
        });
        return true;
    };
    CompletionProvider.prototype.provideCompletions = function (editor, options, callback) {
        var processResults = function (results) {
            var prefix = results.prefix;
            var matches = results.matches;
            this.completions = new FilteredList(matches);
            if (options.exactMatch)
                this.completions.exactMatch = true;
            if (options.ignoreCaption)
                this.completions.ignoreCaption = true;
            this.completions.setFilter(prefix);
            if (results.finished || this.completions.filtered.length)
                callback(null, this.completions, results.finished);
        }.bind(this);
        var isImmediate = true;
        var immediateResults = null;
        this.gatherCompletions(editor, function (err, results) {
            if (!this.active) {
                return;
            }
            if (err) {
                callback(err, [], true);
                this.detach();
            }
            var prefix = results.prefix;
            if (prefix.indexOf(results.prefix) !== 0)
                return;
            if (isImmediate) {
                immediateResults = results;
                return;
            }
            processResults(results);
        }.bind(this));
        isImmediate = false;
        if (immediateResults) {
            var results = immediateResults;
            immediateResults = null;
            processResults(results);
        }
    };
    CompletionProvider.prototype.detach = function () {
        this.active = false;
        this.completers && this.completers.forEach(function (completer) {
            if (typeof completer.cancel === "function") {
                completer.cancel();
            }
        });
    };
    return CompletionProvider;
}());
var FilteredList = /** @class */ (function () {
    function FilteredList(array, filterText) {
        this.all = array;
        this.filtered = array;
        this.filterText = filterText || "";
        this.exactMatch = false;
        this.ignoreCaption = false;
    }
    FilteredList.prototype.setFilter = function (str) {
        if (str.length > this.filterText && str.lastIndexOf(this.filterText, 0) === 0)
            var matches = this.filtered;
        else
            var matches = this.all;
        this.filterText = str;
        matches = this.filterCompletions(matches, this.filterText);
        matches = matches.sort(function (a, b) {
            return b.exactMatch - a.exactMatch || b.$score - a.$score
                || (a.caption || a.value).localeCompare(b.caption || b.value);
        });
        var prev = null;
        matches = matches.filter(function (item) {
            var caption = item.snippet || item.caption || item.value;
            if (caption === prev)
                return false;
            prev = caption;
            return true;
        });
        this.filtered = matches;
    };
    FilteredList.prototype.filterCompletions = function (items, needle) {
        var results = [];
        var upper = needle.toUpperCase();
        var lower = needle.toLowerCase();
        loop: for (var i = 0, item; item = items[i]; i++) {
            var caption = (!this.ignoreCaption && item.caption) || item.value || item.snippet;
            if (!caption)
                continue;
            var lastIndex = -1;
            var matchMask = 0;
            var penalty = 0;
            var index, distance;
            if (this.exactMatch) {
                if (needle !== caption.substr(0, needle.length))
                    continue loop;
            }
            else {
                var fullMatchIndex = caption.toLowerCase().indexOf(lower);
                if (fullMatchIndex > -1) {
                    penalty = fullMatchIndex;
                }
                else {
                    for (var j = 0; j < needle.length; j++) {
                        var i1 = caption.indexOf(lower[j], lastIndex + 1);
                        var i2 = caption.indexOf(upper[j], lastIndex + 1);
                        index = (i1 >= 0) ? ((i2 < 0 || i1 < i2) ? i1 : i2) : i2;
                        if (index < 0)
                            continue loop;
                        distance = index - lastIndex - 1;
                        if (distance > 0) {
                            if (lastIndex === -1)
                                penalty += 10;
                            penalty += distance;
                            matchMask = matchMask | (1 << j);
                        }
                        lastIndex = index;
                    }
                }
            }
            item.matchMask = matchMask;
            item.exactMatch = penalty ? 0 : 1;
            item.$score = (item.score || 0) - penalty;
            results.push(item);
        }
        return results;
    };
    return FilteredList;
}());
exports.Autocomplete = Autocomplete;
exports.CompletionProvider = CompletionProvider;
exports.FilteredList = FilteredList;

});

define("ace/ext/statusbar",["require","exports","module","ace/lib/dom","ace/lib/lang"], function(require, exports, module){"use strict";
var dom = require("../lib/dom");
var lang = require("../lib/lang");
var StatusBar = /** @class */ (function () {
    function StatusBar(editor, parentNode) {
        this.element = dom.createElement("div");
        this.element.className = "ace_status-indicator";
        this.element.style.cssText = "display: inline-block;";
        parentNode.appendChild(this.element);
        var statusUpdate = lang.delayedCall(function () {
            this.updateStatus(editor);
        }.bind(this)).schedule.bind(null, 100);
        editor.on("changeStatus", statusUpdate);
        editor.on("changeSelection", statusUpdate);
        editor.on("keyboardActivity", statusUpdate);
    }
    StatusBar.prototype.updateStatus = function (editor) {
        var status = [];
        function add(str, separator) {
            str && status.push(str, separator || "|");
        }
        add(editor.keyBinding.getStatusText(editor));
        if (editor.commands.recording)
            add("REC");
        var sel = editor.selection;
        var c = sel.lead;
        if (!sel.isEmpty()) {
            var r = editor.getSelectionRange();
            add("(" + (r.end.row - r.start.row) + ":" + (r.end.column - r.start.column) + ")", " ");
        }
        add(c.row + ":" + c.column, " ");
        if (sel.rangeCount)
            add("[" + sel.rangeCount + "]", " ");
        status.pop();
        this.element.textContent = status.join("");
    };
    return StatusBar;
}());
exports.StatusBar = StatusBar;

});

define("ace/ext/emmet",["require","exports","module","ace/keyboard/hash_handler","ace/editor","ace/snippets","ace/range","ace/config","resources","resources","tabStops","resources","utils","actions"], function(require, exports, module){"use strict";
var HashHandler = require("../keyboard/hash_handler").HashHandler;
var Editor = require("../editor").Editor;
var snippetManager = require("../snippets").snippetManager;
var Range = require("../range").Range;
var config = require("../config");
var emmet, emmetPath;
var AceEmmetEditor = /** @class */ (function () {
    function AceEmmetEditor() {
    }
    AceEmmetEditor.prototype.setupContext = function (editor) {
        this.ace = editor;
        this.indentation = editor.session.getTabString();
        if (!emmet)
            emmet = window.emmet;
        var resources = emmet.resources || emmet.require("resources");
        resources.setVariable("indentation", this.indentation);
        this.$syntax = null;
        this.$syntax = this.getSyntax();
    };
    AceEmmetEditor.prototype.getSelectionRange = function () {
        var range = this.ace.getSelectionRange();
        var doc = this.ace.session.doc;
        return {
            start: doc.positionToIndex(range.start),
            end: doc.positionToIndex(range.end)
        };
    };
    AceEmmetEditor.prototype.createSelection = function (start, end) {
        var doc = this.ace.session.doc;
        this.ace.selection.setRange({
            start: doc.indexToPosition(start),
            end: doc.indexToPosition(end)
        });
    };
    AceEmmetEditor.prototype.getCurrentLineRange = function () {
        var ace = this.ace;
        var row = ace.getCursorPosition().row;
        var lineLength = ace.session.getLine(row).length;
        var index = ace.session.doc.positionToIndex({ row: row, column: 0 });
        return {
            start: index,
            end: index + lineLength
        };
    };
    AceEmmetEditor.prototype.getCaretPos = function () {
        var pos = this.ace.getCursorPosition();
        return this.ace.session.doc.positionToIndex(pos);
    };
    AceEmmetEditor.prototype.setCaretPos = function (index) {
        var pos = this.ace.session.doc.indexToPosition(index);
        this.ace.selection.moveToPosition(pos);
    };
    AceEmmetEditor.prototype.getCurrentLine = function () {
        var row = this.ace.getCursorPosition().row;
        return this.ace.session.getLine(row);
    };
    AceEmmetEditor.prototype.replaceContent = function (value, start, end, noIndent) {
        if (end == null)
            end = start == null ? this.getContent().length : start;
        if (start == null)
            start = 0;
        var editor = this.ace;
        var doc = editor.session.doc;
        var range = Range.fromPoints(doc.indexToPosition(start), doc.indexToPosition(end));
        editor.session.remove(range);
        range.end = range.start;
        value = this.$updateTabstops(value);
        snippetManager.insertSnippet(editor, value);
    };
    AceEmmetEditor.prototype.getContent = function () {
        return this.ace.getValue();
    };
    AceEmmetEditor.prototype.getSyntax = function () {
        if (this.$syntax)
            return this.$syntax;
        var syntax = this.ace.session.$modeId.split("/").pop();
        if (syntax == "html" || syntax == "php") {
            var cursor = this.ace.getCursorPosition();
            var state = this.ace.session.getState(cursor.row);
            if (typeof state != "string")
                state = state[0];
            if (state) {
                state = state.split("-");
                if (state.length > 1)
                    syntax = state[0];
                else if (syntax == "php")
                    syntax = "html";
            }
        }
        return syntax;
    };
    AceEmmetEditor.prototype.getProfileName = function () {
        var resources = emmet.resources || emmet.require("resources");
        switch (this.getSyntax()) {
            case "css": return "css";
            case "xml":
            case "xsl":
                return "xml";
            case "html":
                var profile = resources.getVariable("profile");
                if (!profile)
                    profile = this.ace.session.getLines(0, 2).join("").search(/<!DOCTYPE[^>]+XHTML/i) != -1 ? "xhtml" : "html";
                return profile;
            default:
                var mode = this.ace.session.$mode;
                return mode.emmetConfig && mode.emmetConfig.profile || "xhtml";
        }
    };
    AceEmmetEditor.prototype.prompt = function (title) {
        return prompt(title); // eslint-disable-line no-alert
    };
    AceEmmetEditor.prototype.getSelection = function () {
        return this.ace.session.getTextRange();
    };
    AceEmmetEditor.prototype.getFilePath = function () {
        return "";
    };
    AceEmmetEditor.prototype.$updateTabstops = function (value) {
        var base = 1000;
        var zeroBase = 0;
        var lastZero = null;
        var ts = emmet.tabStops || emmet.require('tabStops');
        var resources = emmet.resources || emmet.require("resources");
        var settings = resources.getVocabulary("user");
        var tabstopOptions = {
            tabstop: function (data) {
                var group = parseInt(data.group, 10);
                var isZero = group === 0;
                if (isZero)
                    group = ++zeroBase;
                else
                    group += base;
                var placeholder = data.placeholder;
                if (placeholder) {
                    placeholder = ts.processText(placeholder, tabstopOptions);
                }
                var result = '${' + group + (placeholder ? ':' + placeholder : '') + '}';
                if (isZero) {
                    lastZero = [data.start, result];
                }
                return result;
            },
            escape: function (ch) {
                if (ch == '$')
                    return '\\$';
                if (ch == '\\')
                    return '\\\\';
                return ch;
            }
        };
        value = ts.processText(value, tabstopOptions);
        if (settings.variables['insert_final_tabstop'] && !/\$\{0\}$/.test(value)) {
            value += '${0}';
        }
        else if (lastZero) {
            var common = emmet.utils ? emmet.utils.common : emmet.require('utils');
            value = common.replaceSubstring(value, '${0}', lastZero[0], lastZero[1]);
        }
        return value;
    };
    return AceEmmetEditor;
}());
var keymap = {
    expand_abbreviation: { "mac": "ctrl+alt+e", "win": "alt+e" },
    match_pair_outward: { "mac": "ctrl+d", "win": "ctrl+," },
    match_pair_inward: { "mac": "ctrl+j", "win": "ctrl+shift+0" },
    matching_pair: { "mac": "ctrl+alt+j", "win": "alt+j" },
    next_edit_point: "alt+right",
    prev_edit_point: "alt+left",
    toggle_comment: { "mac": "command+/", "win": "ctrl+/" },
    split_join_tag: { "mac": "shift+command+'", "win": "shift+ctrl+`" },
    remove_tag: { "mac": "command+'", "win": "shift+ctrl+;" },
    evaluate_math_expression: { "mac": "shift+command+y", "win": "shift+ctrl+y" },
    increment_number_by_1: "ctrl+up",
    decrement_number_by_1: "ctrl+down",
    increment_number_by_01: "alt+up",
    decrement_number_by_01: "alt+down",
    increment_number_by_10: { "mac": "alt+command+up", "win": "shift+alt+up" },
    decrement_number_by_10: { "mac": "alt+command+down", "win": "shift+alt+down" },
    select_next_item: { "mac": "shift+command+.", "win": "shift+ctrl+." },
    select_previous_item: { "mac": "shift+command+,", "win": "shift+ctrl+," },
    reflect_css_value: { "mac": "shift+command+r", "win": "shift+ctrl+r" },
    encode_decode_data_url: { "mac": "shift+ctrl+d", "win": "ctrl+'" },
    expand_abbreviation_with_tab: "Tab",
    wrap_with_abbreviation: { "mac": "shift+ctrl+a", "win": "shift+ctrl+a" }
};
var editorProxy = new AceEmmetEditor();
exports.commands = new HashHandler();
exports.runEmmetCommand = function runEmmetCommand(editor) {
    if (this.action == "expand_abbreviation_with_tab") {
        if (!editor.selection.isEmpty())
            return false;
        var pos = editor.selection.lead;
        var token = editor.session.getTokenAt(pos.row, pos.column);
        if (token && /\btag\b/.test(token.type))
            return false;
    }
    try {
        editorProxy.setupContext(editor);
        var actions = emmet.actions || emmet.require("actions");
        if (this.action == "wrap_with_abbreviation") {
            return setTimeout(function () {
                actions.run("wrap_with_abbreviation", editorProxy);
            }, 0);
        }
        var result = actions.run(this.action, editorProxy);
    }
    catch (e) {
        if (!emmet) {
            var loading = exports.load(runEmmetCommand.bind(this, editor));
            if (this.action == "expand_abbreviation_with_tab")
                return false;
            return loading;
        }
        editor._signal("changeStatus", typeof e == "string" ? e : e.message);
        config.warn(e);
        result = false;
    }
    return result;
};
for (var command in keymap) {
    exports.commands.addCommand({
        name: "emmet:" + command,
        action: command,
        bindKey: keymap[command],
        exec: exports.runEmmetCommand,
        multiSelectAction: "forEach"
    });
}
exports.updateCommands = function (editor, enabled) {
    if (enabled) {
        editor.keyBinding.addKeyboardHandler(exports.commands);
    }
    else {
        editor.keyBinding.removeKeyboardHandler(exports.commands);
    }
};
exports.isSupportedMode = function (mode) {
    if (!mode)
        return false;
    if (mode.emmetConfig)
        return true;
    var id = mode.$id || mode;
    return /css|less|scss|sass|stylus|html|php|twig|ejs|handlebars/.test(id);
};
exports.isAvailable = function (editor, command) {
    if (/(evaluate_math_expression|expand_abbreviation)$/.test(command))
        return true;
    var mode = editor.session.$mode;
    var isSupported = exports.isSupportedMode(mode);
    if (isSupported && mode.$modes) {
        try {
            editorProxy.setupContext(editor);
            if (/js|php/.test(editorProxy.getSyntax()))
                isSupported = false;
        }
        catch (e) { }
    }
    return isSupported;
};
var onChangeMode = function (e, target) {
    var editor = target;
    if (!editor)
        return;
    var enabled = exports.isSupportedMode(editor.session.$mode);
    if (e.enableEmmet === false)
        enabled = false;
    if (enabled)
        exports.load();
    exports.updateCommands(editor, enabled);
};
exports.load = function (cb) {
    if (typeof emmetPath !== "string") {
        config.warn("script for emmet-core is not loaded");
        return false;
    }
    config.loadModule(emmetPath, function () {
        emmetPath = null;
        cb && cb();
    });
    return true;
};
exports.AceEmmetEditor = AceEmmetEditor;
config.defineOptions(Editor.prototype, "editor", {
    enableEmmet: {
        set: function (val) {
            this[val ? "on" : "removeListener"]("changeMode", onChangeMode);
            onChangeMode({ enableEmmet: !!val }, this);
        },
        value: true
    }
});
exports.setCore = function (e) {
    if (typeof e == "string")
        emmetPath = e;
    else
        emmet = e;
};

});

define("ace/autocomplete/text_completer",["require","exports","module","ace/range"], function(require, exports, module){var Range = require("../range").Range;
var splitRegex = /[^a-zA-Z_0-9\$\-\u00C0-\u1FFF\u2C00-\uD7FF\w]+/;
function getWordIndex(doc, pos) {
    var textBefore = doc.getTextRange(Range.fromPoints({
        row: 0,
        column: 0
    }, pos));
    return textBefore.split(splitRegex).length - 1;
}
function wordDistance(doc, pos) {
    var prefixPos = getWordIndex(doc, pos);
    var words = doc.getValue().split(splitRegex);
    var wordScores = Object.create(null);
    var currentWord = words[prefixPos];
    words.forEach(function (word, idx) {
        if (!word || word === currentWord)
            return;
        var distance = Math.abs(prefixPos - idx);
        var score = words.length - distance;
        if (wordScores[word]) {
            wordScores[word] = Math.max(score, wordScores[word]);
        }
        else {
            wordScores[word] = score;
        }
    });
    return wordScores;
}
exports.getCompletions = function (editor, session, pos, prefix, callback) {
    var wordScore = wordDistance(session, pos);
    var wordList = Object.keys(wordScore);
    callback(null, wordList.map(function (word) {
        return {
            caption: word,
            value: word,
            score: wordScore[word],
            meta: "local"
        };
    }));
};

});

define("ace/ext/language_tools",["require","exports","module","ace/snippets","ace/autocomplete","ace/config","ace/lib/lang","ace/autocomplete/util","ace/autocomplete/text_completer","ace/editor","ace/config"], function(require, exports, module){"use strict";
var snippetManager = require("../snippets").snippetManager;
var Autocomplete = require("../autocomplete").Autocomplete;
var config = require("../config");
var lang = require("../lib/lang");
var util = require("../autocomplete/util");
var textCompleter = require("../autocomplete/text_completer");
var keyWordCompleter = {
    getCompletions: function (editor, session, pos, prefix, callback) {
        if (session.$mode.completer) {
            return session.$mode.completer.getCompletions(editor, session, pos, prefix, callback);
        }
        var state = editor.session.getState(pos.row);
        var completions = session.$mode.getCompletions(state, session, pos, prefix);
        completions = completions.map(function (el) {
            el.completerId = keyWordCompleter.id;
            return el;
        });
        callback(null, completions);
    },
    id: "keywordCompleter"
};
var transformSnippetTooltip = function (str) {
    var record = {};
    return str.replace(/\${(\d+)(:(.*?))?}/g, function (_, p1, p2, p3) {
        return (record[p1] = p3 || '');
    }).replace(/\$(\d+?)/g, function (_, p1) {
        return record[p1];
    });
};
var snippetCompleter = {
    getCompletions: function (editor, session, pos, prefix, callback) {
        var scopes = [];
        var token = session.getTokenAt(pos.row, pos.column);
        if (token && token.type.match(/(tag-name|tag-open|tag-whitespace|attribute-name|attribute-value)\.xml$/))
            scopes.push('html-tag');
        else
            scopes = snippetManager.getActiveScopes(editor);
        var snippetMap = snippetManager.snippetMap;
        var completions = [];
        scopes.forEach(function (scope) {
            var snippets = snippetMap[scope] || [];
            for (var i = snippets.length; i--;) {
                var s = snippets[i];
                var caption = s.name || s.tabTrigger;
                if (!caption)
                    continue;
                completions.push({
                    caption: caption,
                    snippet: s.content,
                    meta: s.tabTrigger && !s.name ? s.tabTrigger + "\u21E5 " : "snippet",
                    completerId: snippetCompleter.id
                });
            }
        }, this);
        callback(null, completions);
    },
    getDocTooltip: function (item) {
        if (item.snippet && !item.docHTML) {
            item.docHTML = [
                "<b>", lang.escapeHTML(item.caption), "</b>", "<hr></hr>",
                lang.escapeHTML(transformSnippetTooltip(item.snippet))
            ].join("");
        }
    },
    id: "snippetCompleter"
};
var completers = [snippetCompleter, textCompleter, keyWordCompleter];
exports.setCompleters = function (val) {
    completers.length = 0;
    if (val)
        completers.push.apply(completers, val);
};
exports.addCompleter = function (completer) {
    completers.push(completer);
};
exports.textCompleter = textCompleter;
exports.keyWordCompleter = keyWordCompleter;
exports.snippetCompleter = snippetCompleter;
var expandSnippet = {
    name: "expandSnippet",
    exec: function (editor) {
        return snippetManager.expandWithTab(editor);
    },
    bindKey: "Tab"
};
var onChangeMode = function (e, editor) {
    loadSnippetsForMode(editor.session.$mode);
};
var loadSnippetsForMode = function (mode) {
    if (typeof mode == "string")
        mode = config.$modes[mode];
    if (!mode)
        return;
    if (!snippetManager.files)
        snippetManager.files = {};
    loadSnippetFile(mode.$id, mode.snippetFileId);
    if (mode.modes)
        mode.modes.forEach(loadSnippetsForMode);
};
var loadSnippetFile = function (id, snippetFilePath) {
    if (!snippetFilePath || !id || snippetManager.files[id])
        return;
    snippetManager.files[id] = {};
    config.loadModule(snippetFilePath, function (m) {
        if (!m)
            return;
        snippetManager.files[id] = m;
        if (!m.snippets && m.snippetText)
            m.snippets = snippetManager.parseSnippetFile(m.snippetText);
        snippetManager.register(m.snippets || [], m.scope);
        if (m.includeScopes) {
            snippetManager.snippetMap[m.scope].includeScopes = m.includeScopes;
            m.includeScopes.forEach(function (x) {
                loadSnippetsForMode("ace/mode/" + x);
            });
        }
    });
};
var doLiveAutocomplete = function (e) {
    var editor = e.editor;
    var hasCompleter = editor.completer && editor.completer.activated;
    if (e.command.name === "backspace") {
        if (hasCompleter && !util.getCompletionPrefix(editor))
            editor.completer.detach();
    }
    else if (e.command.name === "insertstring" && !hasCompleter) {
        lastExecEvent = e;
        var delay = e.editor.$liveAutocompletionDelay;
        if (delay) {
            liveAutocompleteTimer.delay(delay);
        }
        else {
            showLiveAutocomplete(e);
        }
    }
};
var lastExecEvent;
var liveAutocompleteTimer = lang.delayedCall(function () {
    showLiveAutocomplete(lastExecEvent);
}, 0);
var showLiveAutocomplete = function (e) {
    var editor = e.editor;
    var prefix = util.getCompletionPrefix(editor);
    var triggerAutocomplete = util.triggerAutocomplete(editor);
    if (prefix && prefix.length >= editor.$liveAutocompletionThreshold || triggerAutocomplete) {
        var completer = Autocomplete.for(editor);
        completer.autoShown = true;
        completer.showPopup(editor);
    }
};
var Editor = require("../editor").Editor;
require("../config").defineOptions(Editor.prototype, "editor", {
    enableBasicAutocompletion: {
        set: function (val) {
            if (val) {
                if (!this.completers)
                    this.completers = Array.isArray(val) ? val : completers;
                this.commands.addCommand(Autocomplete.startCommand);
            }
            else {
                this.commands.removeCommand(Autocomplete.startCommand);
            }
        },
        value: false
    },
    enableLiveAutocompletion: {
        set: function (val) {
            if (val) {
                if (!this.completers)
                    this.completers = Array.isArray(val) ? val : completers;
                this.commands.on('afterExec', doLiveAutocomplete);
            }
            else {
                this.commands.off('afterExec', doLiveAutocomplete);
            }
        },
        value: false
    },
    liveAutocompletionDelay: {
        initialValue: 0
    },
    liveAutocompletionThreshold: {
        initialValue: 0
    },
    enableSnippets: {
        set: function (val) {
            if (val) {
                this.commands.addCommand(expandSnippet);
                this.on("changeMode", onChangeMode);
                onChangeMode(null, this);
            }
            else {
                this.commands.removeCommand(expandSnippet);
                this.off("changeMode", onChangeMode);
            }
        },
        value: false
    }
});

});

define("ace/ext/command_bar",["require","exports","module","ace/tooltip","ace/lib/event_emitter","ace/lib/lang","ace/lib/dom","ace/lib/oop","ace/lib/useragent"], function(require, exports, module){var __values = (this && this.__values) || function(o) {
    var s = typeof Symbol === "function" && Symbol.iterator, m = s && o[s], i = 0;
    if (m) return m.call(o);
    if (o && typeof o.length === "number") return {
        next: function () {
            if (o && i >= o.length) o = void 0;
            return { value: o && o[i++], done: !o };
        }
    };
    throw new TypeError(s ? "Object is not iterable." : "Symbol.iterator is not defined.");
};
var Tooltip = require("../tooltip").Tooltip;
var EventEmitter = require("../lib/event_emitter").EventEmitter;
var lang = require("../lib/lang");
var dom = require("../lib/dom");
var oop = require("../lib/oop");
var useragent = require("../lib/useragent");
var BUTTON_CLASS_NAME = 'command_bar_tooltip_button';
var VALUE_CLASS_NAME = 'command_bar_button_value';
var CAPTION_CLASS_NAME = 'command_bar_button_caption';
var KEYBINDING_CLASS_NAME = 'command_bar_keybinding';
var TOOLTIP_CLASS_NAME = 'command_bar_tooltip';
var MORE_OPTIONS_BUTTON_ID = 'MoreOptionsButton';
var defaultDelay = 100;
var defaultMaxElements = 4;
var minPosition = function (posA, posB) {
    if (posB.row > posA.row) {
        return posA;
    }
    else if (posB.row === posA.row && posB.column > posA.column) {
        return posA;
    }
    return posB;
};
var keyDisplayMap = {
    "Ctrl": { mac: "^" },
    "Option": { mac: "⌥" },
    "Command": { mac: "⌘" },
    "Cmd": { mac: "⌘" },
    "Shift": "⇧",
    "Left": "←",
    "Right": "→",
    "Up": "↑",
    "Down": "↓"
};
var CommandBarTooltip = /** @class */ (function () {
    function CommandBarTooltip(parentNode, options) {
        var e_1, _a;
        options = options || {};
        this.parentNode = parentNode;
        this.tooltip = new Tooltip(this.parentNode);
        this.moreOptions = new Tooltip(this.parentNode);
        this.maxElementsOnTooltip = options.maxElementsOnTooltip || defaultMaxElements;
        this.$alwaysShow = options.alwaysShow || false;
        this.eventListeners = {};
        this.elements = {};
        this.commands = {};
        this.tooltipEl = dom.buildDom(['div', { class: TOOLTIP_CLASS_NAME }], this.tooltip.getElement());
        this.moreOptionsEl = dom.buildDom(['div', { class: TOOLTIP_CLASS_NAME + " tooltip_more_options" }], this.moreOptions.getElement());
        this.$showTooltipTimer = lang.delayedCall(this.$showTooltip.bind(this), options.showDelay || defaultDelay);
        this.$hideTooltipTimer = lang.delayedCall(this.$hideTooltip.bind(this), options.hideDelay || defaultDelay);
        this.$tooltipEnter = this.$tooltipEnter.bind(this);
        this.$onMouseMove = this.$onMouseMove.bind(this);
        this.$onChangeScroll = this.$onChangeScroll.bind(this);
        this.$onEditorChangeSession = this.$onEditorChangeSession.bind(this);
        this.$scheduleTooltipForHide = this.$scheduleTooltipForHide.bind(this);
        this.$preventMouseEvent = this.$preventMouseEvent.bind(this);
        try {
            for (var _b = __values(["mousedown", "mouseup", "click"]), _c = _b.next(); !_c.done; _c = _b.next()) {
                var event = _c.value;
                this.tooltip.getElement().addEventListener(event, this.$preventMouseEvent);
                this.moreOptions.getElement().addEventListener(event, this.$preventMouseEvent);
            }
        }
        catch (e_1_1) { e_1 = { error: e_1_1 }; }
        finally {
            try {
                if (_c && !_c.done && (_a = _b.return)) _a.call(_b);
            }
            finally { if (e_1) throw e_1.error; }
        }
    }
    CommandBarTooltip.prototype.registerCommand = function (id, command) {
        var registerForMainTooltip = Object.keys(this.commands).length < this.maxElementsOnTooltip;
        if (!registerForMainTooltip && !this.elements[MORE_OPTIONS_BUTTON_ID]) {
            this.$createCommand(MORE_OPTIONS_BUTTON_ID, {
                name: "···",
                exec: function () {
                    this.$shouldHideMoreOptions = false;
                    this.$setMoreOptionsVisibility(!this.isMoreOptionsShown());
                }.bind(this),
                type: "checkbox",
                getValue: function () {
                    return this.isMoreOptionsShown();
                }.bind(this),
                enabled: true
            }, true);
        }
        this.$createCommand(id, command, registerForMainTooltip);
        if (this.isShown()) {
            this.updatePosition();
        }
    };
    CommandBarTooltip.prototype.isShown = function () {
        return !!this.tooltip && this.tooltip.isOpen;
    };
    CommandBarTooltip.prototype.isMoreOptionsShown = function () {
        return !!this.moreOptions && this.moreOptions.isOpen;
    };
    CommandBarTooltip.prototype.getAlwaysShow = function () {
        return this.$alwaysShow;
    };
    CommandBarTooltip.prototype.setAlwaysShow = function (alwaysShow) {
        this.$alwaysShow = alwaysShow;
        this.$updateOnHoverHandlers(!this.$alwaysShow);
        this._signal("alwaysShow", this.$alwaysShow);
    };
    CommandBarTooltip.prototype.attach = function (editor) {
        if (!editor || (this.isShown() && this.editor === editor)) {
            return;
        }
        this.detach();
        this.editor = editor;
        this.editor.on("changeSession", this.$onEditorChangeSession);
        if (this.editor.session) {
            this.editor.session.on("changeScrollLeft", this.$onChangeScroll);
            this.editor.session.on("changeScrollTop", this.$onChangeScroll);
        }
        if (this.getAlwaysShow()) {
            this.$showTooltip();
        }
        else {
            this.$updateOnHoverHandlers(true);
        }
    };
    CommandBarTooltip.prototype.updatePosition = function () {
        if (!this.editor) {
            return;
        }
        var renderer = this.editor.renderer;
        var ranges;
        if (this.editor.selection.getAllRanges) {
            ranges = this.editor.selection.getAllRanges();
        }
        else {
            ranges = [this.editor.getSelectionRange()];
        }
        if (!ranges.length) {
            return;
        }
        var minPos = minPosition(ranges[0].start, ranges[0].end);
        for (var i = 0, range; range = ranges[i]; i++) {
            minPos = minPosition(minPos, minPosition(range.start, range.end));
        }
        var pos = renderer.$cursorLayer.getPixelPosition(minPos, true);
        var tooltipEl = this.tooltip.getElement();
        var screenWidth = window.innerWidth;
        var screenHeight = window.innerHeight;
        var rect = this.editor.container.getBoundingClientRect();
        pos.top += rect.top - renderer.layerConfig.offset;
        pos.left += rect.left + renderer.gutterWidth - renderer.scrollLeft;
        var cursorVisible = pos.top >= rect.top && pos.top <= rect.bottom &&
            pos.left >= rect.left + renderer.gutterWidth && pos.left <= rect.right;
        if (!cursorVisible && this.isShown()) {
            this.$hideTooltip();
            return;
        }
        else if (cursorVisible && !this.isShown() && this.getAlwaysShow()) {
            this.$showTooltip();
            return;
        }
        var top = pos.top - tooltipEl.offsetHeight;
        var left = Math.min(screenWidth - tooltipEl.offsetWidth, pos.left);
        var tooltipFits = top >= 0 && top + tooltipEl.offsetHeight <= screenHeight &&
            left >= 0 && left + tooltipEl.offsetWidth <= screenWidth;
        if (!tooltipFits) {
            this.$hideTooltip();
            return;
        }
        this.tooltip.setPosition(left, top);
        if (this.isMoreOptionsShown()) {
            top = top + tooltipEl.offsetHeight;
            left = this.elements[MORE_OPTIONS_BUTTON_ID].getBoundingClientRect().left;
            var moreOptionsEl = this.moreOptions.getElement();
            var screenHeight = window.innerHeight;
            if (top + moreOptionsEl.offsetHeight > screenHeight) {
                top -= tooltipEl.offsetHeight + moreOptionsEl.offsetHeight;
            }
            if (left + moreOptionsEl.offsetWidth > screenWidth) {
                left = screenWidth - moreOptionsEl.offsetWidth;
            }
            this.moreOptions.setPosition(left, top);
        }
    };
    CommandBarTooltip.prototype.update = function () {
        Object.keys(this.elements).forEach(this.$updateElement.bind(this));
    };
    CommandBarTooltip.prototype.detach = function () {
        this.tooltip.hide();
        this.moreOptions.hide();
        this.$updateOnHoverHandlers(false);
        if (this.editor) {
            this.editor.off("changeSession", this.$onEditorChangeSession);
            if (this.editor.session) {
                this.editor.session.off("changeScrollLeft", this.$onChangeScroll);
                this.editor.session.off("changeScrollTop", this.$onChangeScroll);
            }
        }
        this.$mouseInTooltip = false;
        this.editor = null;
    };
    CommandBarTooltip.prototype.destroy = function () {
        if (this.tooltip && this.moreOptions) {
            this.detach();
            this.tooltip.destroy();
            this.moreOptions.destroy();
        }
        this.eventListeners = {};
        this.commands = {};
        this.elements = {};
        this.tooltip = this.moreOptions = this.parentNode = null;
    };
    CommandBarTooltip.prototype.$createCommand = function (id, command, forMainTooltip) {
        var parentEl = forMainTooltip ? this.tooltipEl : this.moreOptionsEl;
        var keyParts = [];
        var bindKey = command.bindKey;
        if (bindKey) {
            if (typeof bindKey === 'object') {
                bindKey = useragent.isMac ? bindKey.mac : bindKey.win;
            }
            bindKey = bindKey.split("|")[0];
            keyParts = bindKey.split("-");
            keyParts = keyParts.map(function (key) {
                if (keyDisplayMap[key]) {
                    if (typeof keyDisplayMap[key] === 'string') {
                        return keyDisplayMap[key];
                    }
                    else if (useragent.isMac && keyDisplayMap[key].mac) {
                        return keyDisplayMap[key].mac;
                    }
                }
                return key;
            });
        }
        var buttonNode;
        if (forMainTooltip && command.iconCssClass) {
            buttonNode = [
                'div',
                {
                    class: ["ace_icon_svg", command.iconCssClass].join(" "),
                    "aria-label": command.name + " (" + command.bindKey + ")"
                }
            ];
        }
        else {
            buttonNode = [
                ['div', { class: VALUE_CLASS_NAME }],
                ['div', { class: CAPTION_CLASS_NAME }, command.name]
            ];
            if (keyParts.length) {
                buttonNode.push([
                    'div',
                    { class: KEYBINDING_CLASS_NAME },
                    keyParts.map(function (keyPart) {
                        return ['div', keyPart];
                    })
                ]);
            }
        }
        dom.buildDom(['div', { class: [BUTTON_CLASS_NAME, command.cssClass || ""].join(" "), ref: id }, buttonNode], parentEl, this.elements);
        this.commands[id] = command;
        var eventListener = function (e) {
            if (this.editor) {
                this.editor.focus();
            }
            this.$shouldHideMoreOptions = this.isMoreOptionsShown();
            if (!this.elements[id].disabled && command.exec) {
                command.exec(this.editor);
            }
            if (this.$shouldHideMoreOptions) {
                this.$setMoreOptionsVisibility(false);
            }
            this.update();
            e.preventDefault();
        }.bind(this);
        this.eventListeners[id] = eventListener;
        this.elements[id].addEventListener('click', eventListener.bind(this));
        this.$updateElement(id);
    };
    CommandBarTooltip.prototype.$setMoreOptionsVisibility = function (visible) {
        if (visible) {
            this.moreOptions.setTheme(this.editor.renderer.theme);
            this.moreOptions.setClassName(TOOLTIP_CLASS_NAME + "_wrapper");
            this.moreOptions.show();
            this.update();
            this.updatePosition();
        }
        else {
            this.moreOptions.hide();
        }
    };
    CommandBarTooltip.prototype.$onEditorChangeSession = function (e) {
        if (e.oldSession) {
            e.oldSession.off("changeScrollTop", this.$onChangeScroll);
            e.oldSession.off("changeScrollLeft", this.$onChangeScroll);
        }
        this.detach();
    };
    CommandBarTooltip.prototype.$onChangeScroll = function () {
        if (this.editor.renderer && (this.isShown() || this.getAlwaysShow())) {
            this.editor.renderer.once("afterRender", this.updatePosition.bind(this));
        }
    };
    CommandBarTooltip.prototype.$onMouseMove = function (e) {
        if (this.$mouseInTooltip) {
            return;
        }
        var cursorPosition = this.editor.getCursorPosition();
        var cursorScreenPosition = this.editor.renderer.textToScreenCoordinates(cursorPosition.row, cursorPosition.column);
        var lineHeight = this.editor.renderer.lineHeight;
        var isInCurrentLine = e.clientY >= cursorScreenPosition.pageY && e.clientY < cursorScreenPosition.pageY + lineHeight;
        if (isInCurrentLine) {
            if (!this.isShown() && !this.$showTooltipTimer.isPending()) {
                this.$showTooltipTimer.delay();
            }
            if (this.$hideTooltipTimer.isPending()) {
                this.$hideTooltipTimer.cancel();
            }
        }
        else {
            if (this.isShown() && !this.$hideTooltipTimer.isPending()) {
                this.$hideTooltipTimer.delay();
            }
            if (this.$showTooltipTimer.isPending()) {
                this.$showTooltipTimer.cancel();
            }
        }
    };
    CommandBarTooltip.prototype.$preventMouseEvent = function (e) {
        if (this.editor) {
            this.editor.focus();
        }
        e.preventDefault();
    };
    CommandBarTooltip.prototype.$scheduleTooltipForHide = function () {
        this.$mouseInTooltip = false;
        this.$showTooltipTimer.cancel();
        this.$hideTooltipTimer.delay();
    };
    CommandBarTooltip.prototype.$tooltipEnter = function () {
        this.$mouseInTooltip = true;
        if (this.$showTooltipTimer.isPending()) {
            this.$showTooltipTimer.cancel();
        }
        if (this.$hideTooltipTimer.isPending()) {
            this.$hideTooltipTimer.cancel();
        }
    };
    CommandBarTooltip.prototype.$updateOnHoverHandlers = function (enableHover) {
        var tooltipEl = this.tooltip.getElement();
        var moreOptionsEl = this.moreOptions.getElement();
        if (enableHover) {
            if (this.editor) {
                this.editor.on("mousemove", this.$onMouseMove);
                this.editor.renderer.getMouseEventTarget().addEventListener("mouseout", this.$scheduleTooltipForHide, true);
            }
            tooltipEl.addEventListener('mouseenter', this.$tooltipEnter);
            tooltipEl.addEventListener('mouseleave', this.$scheduleTooltipForHide);
            moreOptionsEl.addEventListener('mouseenter', this.$tooltipEnter);
            moreOptionsEl.addEventListener('mouseleave', this.$scheduleTooltipForHide);
        }
        else {
            if (this.editor) {
                this.editor.off("mousemove", this.$onMouseMove);
                this.editor.renderer.getMouseEventTarget().removeEventListener("mouseout", this.$scheduleTooltipForHide, true);
            }
            tooltipEl.removeEventListener('mouseenter', this.$tooltipEnter);
            tooltipEl.removeEventListener('mouseleave', this.$scheduleTooltipForHide);
            moreOptionsEl.removeEventListener('mouseenter', this.$tooltipEnter);
            moreOptionsEl.removeEventListener('mouseleave', this.$scheduleTooltipForHide);
        }
    };
    CommandBarTooltip.prototype.$showTooltip = function () {
        if (this.isShown()) {
            return;
        }
        this.tooltip.setTheme(this.editor.renderer.theme);
        this.tooltip.setClassName(TOOLTIP_CLASS_NAME + "_wrapper");
        this.tooltip.show();
        this.update();
        this.updatePosition();
        this._signal("show");
    };
    CommandBarTooltip.prototype.$hideTooltip = function () {
        this.$mouseInTooltip = false;
        if (!this.isShown()) {
            return;
        }
        this.moreOptions.hide();
        this.tooltip.hide();
        this._signal("hide");
    };
    CommandBarTooltip.prototype.$updateElement = function (id) {
        var command = this.commands[id];
        if (!command) {
            return;
        }
        var el = this.elements[id];
        var commandEnabled = command.enabled;
        if (typeof commandEnabled === 'function') {
            commandEnabled = commandEnabled(this.editor);
        }
        if (typeof command.getValue === 'function') {
            var value = command.getValue(this.editor);
            if (command.type === 'text') {
                el.textContent = value;
            }
            else if (command.type === 'checkbox') {
                var domCssFn = value ? dom.addCssClass : dom.removeCssClass;
                var isOnTooltip = el.parentElement === this.tooltipEl;
                el.ariaChecked = value;
                if (isOnTooltip) {
                    domCssFn(el, "ace_selected");
                }
                else {
                    el = el.querySelector("." + VALUE_CLASS_NAME);
                    domCssFn(el, "ace_checkmark");
                }
            }
        }
        if (commandEnabled && el.disabled) {
            dom.removeCssClass(el, "ace_disabled");
            el.ariaDisabled = el.disabled = false;
            el.removeAttribute("disabled");
        }
        else if (!commandEnabled && !el.disabled) {
            dom.addCssClass(el, "ace_disabled");
            el.ariaDisabled = el.disabled = true;
            el.setAttribute("disabled", "");
        }
    };
    return CommandBarTooltip;
}());
oop.implement(CommandBarTooltip.prototype, EventEmitter);
dom.importCssString("\n.ace_tooltip.".concat(TOOLTIP_CLASS_NAME, "_wrapper {\n    padding: 0;\n}\n\n.ace_tooltip .").concat(TOOLTIP_CLASS_NAME, " {\n    padding: 1px 5px;\n    display: flex;\n    pointer-events: auto;\n}\n\n.ace_tooltip .").concat(TOOLTIP_CLASS_NAME, ".tooltip_more_options {\n    padding: 1px;\n    flex-direction: column;\n}\n\ndiv.").concat(BUTTON_CLASS_NAME, " {\n    display: inline-flex;\n    cursor: pointer;\n    margin: 1px;\n    border-radius: 2px;\n    padding: 2px 5px;\n    align-items: center;\n}\n\ndiv.").concat(BUTTON_CLASS_NAME, ".ace_selected,\ndiv.").concat(BUTTON_CLASS_NAME, ":hover:not(.ace_disabled) {\n    background-color: rgba(0, 0, 0, 0.1);\n}\n\ndiv.").concat(BUTTON_CLASS_NAME, ".ace_disabled {\n    color: #777;\n    pointer-events: none;\n}\n\ndiv.").concat(BUTTON_CLASS_NAME, " .ace_icon_svg {\n    height: 12px;\n    background-color: #000;\n}\n\ndiv.").concat(BUTTON_CLASS_NAME, ".ace_disabled .ace_icon_svg {\n    background-color: #777;\n}\n\n.").concat(TOOLTIP_CLASS_NAME, ".tooltip_more_options .").concat(BUTTON_CLASS_NAME, " {\n    display: flex;\n}\n\n.").concat(TOOLTIP_CLASS_NAME, ".").concat(VALUE_CLASS_NAME, " {\n    display: none;\n}\n\n.").concat(TOOLTIP_CLASS_NAME, ".tooltip_more_options .").concat(VALUE_CLASS_NAME, " {\n    display: inline-block;\n    width: 12px;\n}\n\n.").concat(CAPTION_CLASS_NAME, " {\n    display: inline-block;\n}\n\n.").concat(KEYBINDING_CLASS_NAME, " {\n    margin: 0 2px;\n    display: inline-block;\n    font-size: 8px;\n}\n\n.").concat(TOOLTIP_CLASS_NAME, ".tooltip_more_options .").concat(KEYBINDING_CLASS_NAME, " {\n    margin-left: auto;\n}\n\n.").concat(KEYBINDING_CLASS_NAME, " div {\n    display: inline-block;\n    min-width: 8px;\n    padding: 2px;\n    margin: 0 1px;\n    border-radius: 2px;\n    background-color: #ccc;\n    text-align: center;\n}\n\n.ace_dark.ace_tooltip .").concat(TOOLTIP_CLASS_NAME, " {\n    background-color: #373737;\n    color: #eee;\n}\n\n.ace_dark div.").concat(BUTTON_CLASS_NAME, ".ace_disabled {\n    color: #979797;\n}\n\n.ace_dark div.").concat(BUTTON_CLASS_NAME, ".ace_selected,\n.ace_dark div.").concat(BUTTON_CLASS_NAME, ":hover:not(.ace_disabled) {\n    background-color: rgba(255, 255, 255, 0.1);\n}\n\n.ace_dark div.").concat(BUTTON_CLASS_NAME, " .ace_icon_svg {\n    background-color: #eee;\n}\n\n.ace_dark div.").concat(BUTTON_CLASS_NAME, ".ace_disabled .ace_icon_svg {\n    background-color: #979797;\n}\n\n.ace_dark .").concat(BUTTON_CLASS_NAME, ".ace_disabled {\n    color: #979797;\n}\n\n.ace_dark .").concat(KEYBINDING_CLASS_NAME, " div {\n    background-color: #575757;\n}\n\n.ace_checkmark::before {\n    content: '\u2713';\n}\n"), "commandbar.css", false);
exports.CommandBarTooltip = CommandBarTooltip;
exports.TOOLTIP_CLASS_NAME = TOOLTIP_CLASS_NAME;
exports.BUTTON_CLASS_NAME = BUTTON_CLASS_NAME;

});

define("ace/ext/inline_autocomplete",["require","exports","module","ace/keyboard/hash_handler","ace/autocomplete/inline","ace/autocomplete","ace/autocomplete","ace/editor","ace/autocomplete/util","ace/lib/dom","ace/lib/lang","ace/ext/command_bar","ace/ext/command_bar","ace/ext/language_tools","ace/ext/language_tools","ace/ext/language_tools","ace/config"], function(require, exports, module){"use strict";
var HashHandler = require("../keyboard/hash_handler").HashHandler;
var AceInline = require("../autocomplete/inline").AceInline;
var FilteredList = require("../autocomplete").FilteredList;
var CompletionProvider = require("../autocomplete").CompletionProvider;
var Editor = require("../editor").Editor;
var util = require("../autocomplete/util");
var dom = require("../lib/dom");
var lang = require("../lib/lang");
var CommandBarTooltip = require("./command_bar").CommandBarTooltip;
var BUTTON_CLASS_NAME = require("./command_bar").BUTTON_CLASS_NAME;
var snippetCompleter = require("./language_tools").snippetCompleter;
var textCompleter = require("./language_tools").textCompleter;
var keyWordCompleter = require("./language_tools").keyWordCompleter;
var destroyCompleter = function (e, editor) {
    editor.completer && editor.completer.destroy();
};
var InlineAutocomplete = /** @class */ (function () {
    function InlineAutocomplete(editor) {
        this.editor = editor;
        this.keyboardHandler = new HashHandler(this.commands);
        this.$index = -1;
        this.blurListener = this.blurListener.bind(this);
        this.changeListener = this.changeListener.bind(this);
        this.changeTimer = lang.delayedCall(function () {
            this.updateCompletions();
        }.bind(this));
    }
    InlineAutocomplete.prototype.getInlineRenderer = function () {
        if (!this.inlineRenderer)
            this.inlineRenderer = new AceInline();
        return this.inlineRenderer;
    };
    InlineAutocomplete.prototype.getInlineTooltip = function () {
        if (!this.inlineTooltip) {
            this.inlineTooltip = InlineAutocomplete.createInlineTooltip(document.body || document.documentElement);
        }
        return this.inlineTooltip;
    };
    InlineAutocomplete.prototype.show = function (options) {
        this.activated = true;
        if (this.editor.completer !== this) {
            if (this.editor.completer)
                this.editor.completer.detach();
            this.editor.completer = this;
        }
        this.editor.on("changeSelection", this.changeListener);
        this.editor.on("blur", this.blurListener);
        this.updateCompletions(options);
    };
    InlineAutocomplete.prototype.$open = function () {
        if (this.editor.textInput.setAriaOptions) {
            this.editor.textInput.setAriaOptions({});
        }
        this.editor.keyBinding.addKeyboardHandler(this.keyboardHandler);
        this.getInlineTooltip().attach(this.editor);
        if (this.$index === -1) {
            this.setIndex(0);
        }
        else {
            this.$showCompletion();
        }
        this.changeTimer.cancel();
    };
    InlineAutocomplete.prototype.insertMatch = function () {
        var result = this.getCompletionProvider().insertByIndex(this.editor, this.$index);
        this.detach();
        return result;
    };
    InlineAutocomplete.prototype.changeListener = function (e) {
        var cursor = this.editor.selection.lead;
        if (cursor.row != this.base.row || cursor.column < this.base.column) {
            this.detach();
        }
        if (this.activated)
            this.changeTimer.schedule();
        else
            this.detach();
    };
    InlineAutocomplete.prototype.blurListener = function (e) {
        this.detach();
    };
    InlineAutocomplete.prototype.goTo = function (where) {
        if (!this.completions || !this.completions.filtered) {
            return;
        }
        var completionLength = this.completions.filtered.length;
        switch (where.toLowerCase()) {
            case "prev":
                this.setIndex((this.$index - 1 + completionLength) % completionLength);
                break;
            case "next":
                this.setIndex((this.$index + 1 + completionLength) % completionLength);
                break;
            case "first":
                this.setIndex(0);
                break;
            case "last":
                this.setIndex(this.completions.filtered.length - 1);
                break;
        }
    };
    InlineAutocomplete.prototype.getLength = function () {
        if (!this.completions || !this.completions.filtered) {
            return 0;
        }
        return this.completions.filtered.length;
    };
    InlineAutocomplete.prototype.getData = function (index) {
        if (index == undefined || index === null) {
            return this.completions.filtered[this.$index];
        }
        else {
            return this.completions.filtered[index];
        }
    };
    InlineAutocomplete.prototype.getIndex = function () {
        return this.$index;
    };
    InlineAutocomplete.prototype.isOpen = function () {
        return this.$index >= 0;
    };
    InlineAutocomplete.prototype.setIndex = function (value) {
        if (!this.completions || !this.completions.filtered) {
            return;
        }
        var newIndex = Math.max(-1, Math.min(this.completions.filtered.length - 1, value));
        if (newIndex !== this.$index) {
            this.$index = newIndex;
            this.$showCompletion();
        }
    };
    InlineAutocomplete.prototype.getCompletionProvider = function (initialPosition) {
        if (!this.completionProvider)
            this.completionProvider = new CompletionProvider(initialPosition);
        return this.completionProvider;
    };
    InlineAutocomplete.prototype.$showCompletion = function () {
        if (!this.getInlineRenderer().show(this.editor, this.completions.filtered[this.$index], this.completions.filterText)) {
            this.getInlineRenderer().hide();
        }
        if (this.inlineTooltip && this.inlineTooltip.isShown()) {
            this.inlineTooltip.update();
        }
    };
    InlineAutocomplete.prototype.$updatePrefix = function () {
        var pos = this.editor.getCursorPosition();
        var prefix = this.editor.session.getTextRange({ start: this.base, end: pos });
        this.completions.setFilter(prefix);
        if (!this.completions.filtered.length)
            return this.detach();
        if (this.completions.filtered.length == 1
            && this.completions.filtered[0].value == prefix
            && !this.completions.filtered[0].snippet)
            return this.detach();
        this.$open(this.editor, prefix);
        return prefix;
    };
    InlineAutocomplete.prototype.updateCompletions = function (options) {
        var prefix = "";
        if (options && options.matches) {
            var pos = this.editor.getSelectionRange().start;
            this.base = this.editor.session.doc.createAnchor(pos.row, pos.column);
            this.base.$insertRight = true;
            this.completions = new FilteredList(options.matches);
            return this.$open(this.editor, "");
        }
        if (this.base && this.completions) {
            prefix = this.$updatePrefix();
        }
        var session = this.editor.getSession();
        var pos = this.editor.getCursorPosition();
        var prefix = util.getCompletionPrefix(this.editor);
        this.base = session.doc.createAnchor(pos.row, pos.column - prefix.length);
        this.base.$insertRight = true;
        var options = {
            exactMatch: true,
            ignoreCaption: true
        };
        this.getCompletionProvider({
            prefix: prefix,
            base: this.base,
            pos: pos
        }).provideCompletions(this.editor, options, function (err, completions, finished) {
            var filtered = completions.filtered;
            var prefix = util.getCompletionPrefix(this.editor);
            if (finished) {
                if (!filtered.length)
                    return this.detach();
                if (filtered.length == 1 && filtered[0].value == prefix && !filtered[0].snippet)
                    return this.detach();
            }
            this.completions = completions;
            this.$open(this.editor, prefix);
        }.bind(this));
    };
    InlineAutocomplete.prototype.detach = function () {
        if (this.editor) {
            this.editor.keyBinding.removeKeyboardHandler(this.keyboardHandler);
            this.editor.off("changeSelection", this.changeListener);
            this.editor.off("blur", this.blurListener);
        }
        this.changeTimer.cancel();
        if (this.inlineTooltip) {
            this.inlineTooltip.detach();
        }
        this.setIndex(-1);
        if (this.completionProvider) {
            this.completionProvider.detach();
        }
        if (this.inlineRenderer && this.inlineRenderer.isOpen()) {
            this.inlineRenderer.hide();
        }
        if (this.base)
            this.base.detach();
        this.activated = false;
        this.completionProvider = this.completions = this.base = null;
    };
    InlineAutocomplete.prototype.destroy = function () {
        this.detach();
        if (this.inlineRenderer)
            this.inlineRenderer.destroy();
        if (this.inlineTooltip)
            this.inlineTooltip.destroy();
        if (this.editor && this.editor.completer == this) {
            this.editor.off("destroy", destroyCompleter);
            this.editor.completer = null;
        }
        this.inlineTooltip = this.editor = this.inlineRenderer = null;
    };
    return InlineAutocomplete;
}());
InlineAutocomplete.prototype.commands = {
    "Previous": {
        bindKey: "Alt-[",
        name: "Previous",
        exec: function (editor) {
            editor.completer.goTo("prev");
        }
    },
    "Next": {
        bindKey: "Alt-]",
        name: "Next",
        exec: function (editor) {
            editor.completer.goTo("next");
        }
    },
    "Accept": {
        bindKey: { win: "Tab|Ctrl-Right", mac: "Tab|Cmd-Right" },
        name: "Accept",
        exec: function (editor) {
            return editor.completer.insertMatch();
        }
    },
    "Close": {
        bindKey: "Esc",
        name: "Close",
        exec: function (editor) {
            editor.completer.detach();
        }
    }
};
InlineAutocomplete.for = function (editor) {
    if (editor.completer instanceof InlineAutocomplete) {
        return editor.completer;
    }
    if (editor.completer) {
        editor.completer.destroy();
        editor.completer = null;
    }
    editor.completer = new InlineAutocomplete(editor);
    editor.once("destroy", destroyCompleter);
    return editor.completer;
};
InlineAutocomplete.startCommand = {
    name: "startInlineAutocomplete",
    exec: function (editor, options) {
        var completer = InlineAutocomplete.for(editor);
        completer.show(options);
    },
    bindKey: { win: "Alt-C", mac: "Option-C" }
};
var completers = [snippetCompleter, textCompleter, keyWordCompleter];
require("../config").defineOptions(Editor.prototype, "editor", {
    enableInlineAutocompletion: {
        set: function (val) {
            if (val) {
                if (!this.completers)
                    this.completers = Array.isArray(val) ? val : completers;
                this.commands.addCommand(InlineAutocomplete.startCommand);
            }
            else {
                this.commands.removeCommand(InlineAutocomplete.startCommand);
            }
        },
        value: false
    }
});
InlineAutocomplete.createInlineTooltip = function (parentEl) {
    var inlineTooltip = new CommandBarTooltip(parentEl);
    inlineTooltip.registerCommand("Previous", Object.assign({}, InlineAutocomplete.prototype.commands["Previous"], {
        enabled: true,
        type: "button",
        iconCssClass: "ace_arrow_rotated"
    }));
    inlineTooltip.registerCommand("Position", {
        enabled: false,
        getValue: function (editor) {
            return editor ? [editor.completer.getIndex() + 1, editor.completer.getLength()].join("/") : "";
        },
        type: "text",
        cssClass: "completion_position"
    });
    inlineTooltip.registerCommand("Next", Object.assign({}, InlineAutocomplete.prototype.commands["Next"], {
        enabled: true,
        type: "button",
        iconCssClass: "ace_arrow"
    }));
    inlineTooltip.registerCommand("Accept", Object.assign({}, InlineAutocomplete.prototype.commands["Accept"], {
        enabled: function (editor) {
            return !!editor && editor.completer.getIndex() >= 0;
        },
        type: "button"
    }));
    inlineTooltip.registerCommand("ShowTooltip", {
        name: "Always Show Tooltip",
        exec: function () {
            inlineTooltip.setAlwaysShow(!inlineTooltip.getAlwaysShow());
        },
        enabled: true,
        getValue: function () {
            return inlineTooltip.getAlwaysShow();
        },
        type: "checkbox"
    });
    return inlineTooltip;
};
dom.importCssString("\n\n.ace_icon_svg.ace_arrow,\n.ace_icon_svg.ace_arrow_rotated {\n    -webkit-mask-image: url(\"data:image/svg+xml;base64,PHN2ZyB3aWR0aD0iMTYiIGhlaWdodD0iMTYiIHZpZXdCb3g9IjAgMCAxNiAxNiIgZmlsbD0ibm9uZSIgeG1sbnM9Imh0dHA6Ly93d3cudzMub3JnLzIwMDAvc3ZnIj48cGF0aCBmaWxsLXJ1bGU9ImV2ZW5vZGQiIGNsaXAtcnVsZT0iZXZlbm9kZCIgZD0iTTUuODM3MDEgMTVMNC41ODc1MSAxMy43MTU1TDEwLjE0NjggOEw0LjU4NzUxIDIuMjg0NDZMNS44MzcwMSAxTDEyLjY0NjUgOEw1LjgzNzAxIDE1WiIgZmlsbD0iYmxhY2siLz48L3N2Zz4=\");\n}\n\n.ace_icon_svg.ace_arrow_rotated {\n    transform: rotate(180deg);\n}\n\ndiv.".concat(BUTTON_CLASS_NAME, ".completion_position {\n    padding: 0;\n}\n"), "inlineautocomplete.css", false);
exports.InlineAutocomplete = InlineAutocomplete;

});

define("ace/ext/beautify",["require","exports","module","ace/token_iterator"], function(require, exports, module){// [WIP]
"use strict";
var TokenIterator = require("../token_iterator").TokenIterator;
function is(token, type) {
    return token.type.lastIndexOf(type + ".xml") > -1;
}
exports.singletonTags = ["area", "base", "br", "col", "command", "embed", "hr", "html", "img", "input", "keygen", "link", "meta", "param", "source", "track", "wbr"];
exports.blockTags = ["article", "aside", "blockquote", "body", "div", "dl", "fieldset", "footer", "form", "head", "header", "html", "nav", "ol", "p", "script", "section", "style", "table", "tbody", "tfoot", "thead", "ul"];
exports.formatOptions = {
    lineBreaksAfterCommasInCurlyBlock: true
};
exports.beautify = function (session) {
    var iterator = new TokenIterator(session, 0, 0);
    var token = iterator.getCurrentToken();
    var tabString = session.getTabString();
    var singletonTags = exports.singletonTags;
    var blockTags = exports.blockTags;
    var formatOptions = exports.formatOptions || {};
    var nextToken;
    var breakBefore = false;
    var spaceBefore = false;
    var spaceAfter = false;
    var code = "";
    var value = "";
    var tagName = "";
    var depth = 0;
    var lastDepth = 0;
    var lastIndent = 0;
    var indent = 0;
    var unindent = 0;
    var roundDepth = 0;
    var curlyDepth = 0;
    var row;
    var curRow = 0;
    var rowsToAdd = 0;
    var rowTokens = [];
    var abort = false;
    var i;
    var indentNextLine = false;
    var inTag = false;
    var inCSS = false;
    var inBlock = false;
    var levels = { 0: 0 };
    var parents = [];
    var caseBody = false;
    var trimNext = function () {
        if (nextToken && nextToken.value && nextToken.type !== 'string.regexp')
            nextToken.value = nextToken.value.replace(/^\s*/, "");
    };
    var trimLine = function () {
        var end = code.length - 1;
        while (true) {
            if (end == 0)
                break;
            if (code[end] !== " ")
                break;
            end = end - 1;
        }
        code = code.slice(0, end + 1);
    };
    var trimCode = function () {
        code = code.trimRight();
        breakBefore = false;
    };
    while (token !== null) {
        curRow = iterator.getCurrentTokenRow();
        rowTokens = iterator.$rowTokens;
        nextToken = iterator.stepForward();
        if (typeof token !== "undefined") {
            value = token.value;
            unindent = 0;
            inCSS = (tagName === "style" || session.$modeId === "ace/mode/css");
            if (is(token, "tag-open")) {
                inTag = true;
                if (nextToken)
                    inBlock = (blockTags.indexOf(nextToken.value) !== -1);
                if (value === "</") {
                    if (inBlock && !breakBefore && rowsToAdd < 1)
                        rowsToAdd++;
                    if (inCSS)
                        rowsToAdd = 1;
                    unindent = 1;
                    inBlock = false;
                }
            }
            else if (is(token, "tag-close")) {
                inTag = false;
            }
            else if (is(token, "comment.start")) {
                inBlock = true;
            }
            else if (is(token, "comment.end")) {
                inBlock = false;
            }
            if (!inTag && !rowsToAdd && token.type === "paren.rparen" && token.value.substr(0, 1) === "}") {
                rowsToAdd++;
            }
            if (curRow !== row) {
                rowsToAdd = curRow;
                if (row)
                    rowsToAdd -= row;
            }
            if (rowsToAdd) {
                trimCode();
                for (; rowsToAdd > 0; rowsToAdd--)
                    code += "\n";
                breakBefore = true;
                if (!is(token, "comment") && !token.type.match(/^(comment|string)$/))
                    value = value.trimLeft();
            }
            if (value) {
                if (token.type === "keyword" && value.match(/^(if|else|elseif|for|foreach|while|switch)$/)) {
                    parents[depth] = value;
                    trimNext();
                    spaceAfter = true;
                    if (value.match(/^(else|elseif)$/)) {
                        if (code.match(/\}[\s]*$/)) {
                            trimCode();
                            spaceBefore = true;
                        }
                    }
                }
                else if (token.type === "paren.lparen") {
                    trimNext();
                    if (value.substr(-1) === "{") {
                        spaceAfter = true;
                        indentNextLine = false;
                        if (!inTag)
                            rowsToAdd = 1;
                    }
                    if (value.substr(0, 1) === "{") {
                        spaceBefore = true;
                        if (code.substr(-1) !== '[' && code.trimRight().substr(-1) === '[') {
                            trimCode();
                            spaceBefore = false;
                        }
                        else if (code.trimRight().substr(-1) === ')') {
                            trimCode();
                        }
                        else {
                            trimLine();
                        }
                    }
                }
                else if (token.type === "paren.rparen") {
                    unindent = 1;
                    if (value.substr(0, 1) === "}") {
                        if (parents[depth - 1] === 'case')
                            unindent++;
                        if (code.trimRight().substr(-1) === '{') {
                            trimCode();
                        }
                        else {
                            spaceBefore = true;
                            if (inCSS)
                                rowsToAdd += 2;
                        }
                    }
                    if (value.substr(0, 1) === "]") {
                        if (code.substr(-1) !== '}' && code.trimRight().substr(-1) === '}') {
                            spaceBefore = false;
                            indent++;
                            trimCode();
                        }
                    }
                    if (value.substr(0, 1) === ")") {
                        if (code.substr(-1) !== '(' && code.trimRight().substr(-1) === '(') {
                            spaceBefore = false;
                            indent++;
                            trimCode();
                        }
                    }
                    trimLine();
                }
                else if ((token.type === "keyword.operator" || token.type === "keyword") && value.match(/^(=|==|===|!=|!==|&&|\|\||and|or|xor|\+=|.=|>|>=|<|<=|=>)$/)) {
                    trimCode();
                    trimNext();
                    spaceBefore = true;
                    spaceAfter = true;
                }
                else if (token.type === "punctuation.operator" && value === ';') {
                    trimCode();
                    trimNext();
                    spaceAfter = true;
                    if (inCSS)
                        rowsToAdd++;
                }
                else if (token.type === "punctuation.operator" && value.match(/^(:|,)$/)) {
                    trimCode();
                    trimNext();
                    if (value.match(/^(,)$/) && curlyDepth > 0 && roundDepth === 0 && formatOptions.lineBreaksAfterCommasInCurlyBlock) {
                        rowsToAdd++;
                    }
                    else {
                        spaceAfter = true;
                        breakBefore = false;
                    }
                }
                else if (token.type === "support.php_tag" && value === "?>" && !breakBefore) {
                    trimCode();
                    spaceBefore = true;
                }
                else if (is(token, "attribute-name") && code.substr(-1).match(/^\s$/)) {
                    spaceBefore = true;
                }
                else if (is(token, "attribute-equals")) {
                    trimLine();
                    trimNext();
                }
                else if (is(token, "tag-close")) {
                    trimLine();
                    if (value === "/>")
                        spaceBefore = true;
                }
                else if (token.type === "keyword" && value.match(/^(case|default)$/)) {
                    if (caseBody)
                        unindent = 1;
                }
                if (breakBefore && !(token.type.match(/^(comment)$/) && !value.substr(0, 1).match(/^[/#]$/)) && !(token.type.match(/^(string)$/) && !value.substr(0, 1).match(/^['"@]$/))) {
                    indent = lastIndent;
                    if (depth > lastDepth) {
                        indent++;
                        for (i = depth; i > lastDepth; i--)
                            levels[i] = indent;
                    }
                    else if (depth < lastDepth)
                        indent = levels[depth];
                    lastDepth = depth;
                    lastIndent = indent;
                    if (unindent)
                        indent -= unindent;
                    if (indentNextLine && !roundDepth) {
                        indent++;
                        indentNextLine = false;
                    }
                    for (i = 0; i < indent; i++)
                        code += tabString;
                }
                if (token.type === "keyword" && value.match(/^(case|default)$/)) {
                    if (caseBody === false) {
                        parents[depth] = value;
                        depth++;
                        caseBody = true;
                    }
                }
                else if (token.type === "keyword" && value.match(/^(break)$/)) {
                    if (parents[depth - 1] && parents[depth - 1].match(/^(case|default)$/)) {
                        depth--;
                        caseBody = false;
                    }
                }
                if (token.type === "paren.lparen") {
                    roundDepth += (value.match(/\(/g) || []).length;
                    curlyDepth += (value.match(/\{/g) || []).length;
                    depth += value.length;
                }
                if (token.type === "keyword" && value.match(/^(if|else|elseif|for|while)$/)) {
                    indentNextLine = true;
                    roundDepth = 0;
                }
                else if (!roundDepth && value.trim() && token.type !== "comment")
                    indentNextLine = false;
                if (token.type === "paren.rparen") {
                    roundDepth -= (value.match(/\)/g) || []).length;
                    curlyDepth -= (value.match(/\}/g) || []).length;
                    for (i = 0; i < value.length; i++) {
                        depth--;
                        if (value.substr(i, 1) === '}' && parents[depth] === 'case') {
                            depth--;
                        }
                    }
                }
                if (token.type == "text")
                    value = value.replace(/\s+$/, " ");
                if (spaceBefore && !breakBefore) {
                    trimLine();
                    if (code.substr(-1) !== "\n")
                        code += " ";
                }
                code += value;
                if (spaceAfter)
                    code += " ";
                breakBefore = false;
                spaceBefore = false;
                spaceAfter = false;
                if ((is(token, "tag-close") && (inBlock || blockTags.indexOf(tagName) !== -1)) || (is(token, "doctype") && value === ">")) {
                    if (inBlock && nextToken && nextToken.value === "</")
                        rowsToAdd = -1;
                    else
                        rowsToAdd = 1;
                }
                if (nextToken && singletonTags.indexOf(nextToken.value) === -1) {
                    if (is(token, "tag-open") && value === "</") {
                        depth--;
                    }
                    else if (is(token, "tag-open") && value === "<") {
                        depth++;
                    }
                    else if (is(token, "tag-close") && value === "/>") {
                        depth--;
                    }
                }
                if (is(token, "tag-name")) {
                    tagName = value;
                }
                row = curRow;
            }
        }
        token = nextToken;
    }
    code = code.trim();
    session.doc.setValue(code);
};
exports.commands = [{
        name: "beautify",
        description: "Format selection (Beautify)",
        exec: function (editor) {
            exports.beautify(editor.session);
        },
        bindKey: "Ctrl-Shift-B"
    }];

});

define("kitchen-sink/demo",["require","exports","module","ace/ext/rtl","ace/multi_select","kitchen-sink/inline_editor","kitchen-sink/dev_util","kitchen-sink/file_drop","ace/config","ace/lib/dom","ace/lib/net","ace/lib/lang","ace/lib/event","ace/theme/textmate","ace/edit_session","ace/undomanager","ace/keyboard/hash_handler","ace/virtual_renderer","ace/editor","ace/range","ace/ext/whitespace","kitchen-sink/doclist","kitchen-sink/layout","kitchen-sink/util","ace/ext/elastic_tabstops_lite","ace/incremental_search","kitchen-sink/token_tooltip","ace/config","ace/config","ace/tooltip","ace/marker_group","ace/worker/worker_client","ace/split","ace/ext/options","ace/autocomplete","ace/ext/statusbar","ace/ext/emmet","ace/placeholder","ace/snippets","ace/ext/language_tools","ace/ext/inline_autocomplete","ace/ext/beautify","ace/keyboard/keybinding","ace/commands/command_manager"], function(require, exports, module) {"use strict";

require("ace/ext/rtl");

require("ace/multi_select");
require("./inline_editor");
var devUtil = require("./dev_util");
require("./file_drop");

var config = require("ace/config");
config.setLoader(function(moduleName, cb) {
    require([moduleName], function(module) {
        cb(null, module);
    });
});

var env = {};

var dom = require("ace/lib/dom");
var net = require("ace/lib/net");
var lang = require("ace/lib/lang");

var event = require("ace/lib/event");
var theme = require("ace/theme/textmate");
var EditSession = require("ace/edit_session").EditSession;
var UndoManager = require("ace/undomanager").UndoManager;

var HashHandler = require("ace/keyboard/hash_handler").HashHandler;

var Renderer = require("ace/virtual_renderer").VirtualRenderer;
var Editor = require("ace/editor").Editor;
var Range = require("ace/range").Range;

var whitespace = require("ace/ext/whitespace");



var doclist = require("./doclist");
var layout = require("./layout");
var util = require("./util");
var saveOption = util.saveOption;

require("ace/ext/elastic_tabstops_lite");
require("ace/incremental_search");

var TokenTooltip = require("./token_tooltip").TokenTooltip;
require("ace/config").defineOptions(Editor.prototype, "editor", {
    showTokenInfo: {
        set: function(val) {
            if (val) {
                this.tokenTooltip = this.tokenTooltip || new TokenTooltip(this);
            }
            else if (this.tokenTooltip) {
                this.tokenTooltip.destroy();
                delete this.tokenTooltip;
            }
        },
        get: function() {
            return !!this.tokenTooltip;
        },
        handlesSet: true
    }
});

require("ace/config").defineOptions(Editor.prototype, "editor", {
    useAceLinters: {
        set: function(val) {
            if (val && !window.languageProvider) {
                loadLanguageProvider(editor);
            }
            else if (val) {
                window.languageProvider.registerEditor(this);
            } else {
            }
        }
    }
});

var {HoverTooltip} = require("ace/tooltip");
var MarkerGroup = require("ace/marker_group").MarkerGroup;
var docTooltip = new HoverTooltip();
function loadLanguageProvider(editor) {
    require([
        "https://mkslanc.github.io/ace-linters/build/ace-linters.js"
    ], function(m) {
        var languageProvider = m.LanguageProvider.fromCdn("https://mkslanc.github.io/ace-linters/build", {
            functionality: {
                hover: true,
                completion: {
                    overwriteCompleters: true
                },
                completionResolve: true,
                format: true,
                documentHighlights: true,
                signatureHelp: false
            }
        });
        window.languageProvider = languageProvider;
        languageProvider.registerEditor(editor);
        if (languageProvider.$descriptionTooltip)
            editor.off("mousemove", languageProvider.$descriptionTooltip.onMouseMove);
        languageProvider.$messageController.$worker.addEventListener("message", function(e) {
            var id = e.data.sessionId.split(".")[0];
            var session = languageProvider.$getSessionLanguageProvider({id: id})?.session;
            if (e.data.type == 6) {
                e.stopPropagation();
                if (session) {
                    showAnnotations(session, e.data.value);
                }
            } else if (e.data.type == 14) {
                if (session) showOccurrenceMarkers(session, e.data.value);
            }
        }, true);
        function showOccurrenceMarkers(session, positions) {
            if (!session.state.occurrenceMarkers) {
                session.state.occurrenceMarkers = new MarkerGroup(session);
            }
            session.state.occurrenceMarkers.setMarkers(positions.map(function(el) {
                var r = el.range;
                return {
                    range: new Range(r.start.line, r.start.character, r.end.line, r.end.character),
                    className: el.kind == 2
                        ? "language_highlight_read"
                        : el.kind == 3
                        ? "language_highlight_write"
                        : "language_highlight_text"
                };
            }));
        }
        function showAnnotations(session, diagnostics) {
            session.clearAnnotations();
            let annotations = diagnostics.map((el) => {
                return {
                    row: el.range.start.line,
                    column: el.range.start.character,
                    text: el.message,
                    type: el.severity === 1 ? "error" : el.severity === 2 ? "warning" : "info"
                };
            });
            if (annotations && annotations.length > 0) {
                session.setAnnotations(annotations);
            }
            
            if (!session.state) session.state = {}
            if (!session.state.diagnosticMarkers) {
                session.state.diagnosticMarkers = new MarkerGroup(session);
            }
            session.state.diagnosticMarkers.setMarkers(diagnostics.map(function(el) {
                var r = el.range;
                return {
                    range: new Range(r.start.line, r.start.character, r.end.line, r.end.character),
                    tooltipText: el.message,
                    className:  "language_highlight_error"
                };
            }));
        };
        
        docTooltip.setDataProvider(function(e, editor) {
            let session = editor.session;
            let docPos = e.getDocumentPosition();

            languageProvider.doHover(session, docPos, function(hover) {
                var errorMarker = session.state?.diagnosticMarkers.getMarkerAtPosition(docPos);

                if (!errorMarker && !hover?.content) return;

                var range = hover?.range || errorMarker?.range;
                range = range ? Range.fromPoints(range.start, range.end) : session.getWordRange(docPos.row, docPos.column);
                var hoverNode = hover && dom.buildDom(["div", {}]);
                if (hoverNode) {
                    hoverNode.innerHTML = languageProvider.getTooltipText(hover);
                };
                
                var domNode = dom.buildDom(["div", {},
                    errorMarker && ["div", {}, errorMarker.tooltipText.trim()],
                    hoverNode
                ]);
                docTooltip.showForRange(editor, range, domNode, e);
            });
        });
        
        docTooltip.addToEditor(editor)
    });
}



var workerModule = require("ace/worker/worker_client");
if (location.href.indexOf("noworker") !== -1) {
    workerModule.WorkerClient = workerModule.UIWorkerClient;
}
var container = document.getElementById("editor-container");
var Split = require("ace/split").Split;
var split = new Split(container, theme, 1);
env.editor = split.getEditor(0);
split.on("focus", function(editor) {
    env.editor = editor;
    updateUIEditorOptions();
});
env.split = split;
window.env = env;


var consoleEl = dom.createElement("div");
container.parentNode.appendChild(consoleEl);
consoleEl.style.cssText = "position:fixed; bottom:1px; right:0;\
border:1px solid #baf; z-index:100";

var cmdLine = new layout.singleLineEditor(consoleEl);
cmdLine.setOption("placeholder", "Enter a command...");
cmdLine.editor = env.editor;
env.editor.cmdLine = cmdLine;

env.editor.showCommandLine = function(val) {
    this.cmdLine.focus();
    if (typeof val == "string")
        this.cmdLine.setValue(val, 1);
};
env.editor.commands.addCommands([{
    name: "snippet",
    bindKey: {win: "Alt-C", mac: "Command-Alt-C"},
    exec: function(editor, needle) {
        if (typeof needle == "object") {
            editor.cmdLine.setValue("snippet ", 1);
            editor.cmdLine.focus();
            return;
        }
        var s = snippetManager.getSnippetByName(needle, editor);
        if (s)
            snippetManager.insertSnippet(editor, s.content);
    },
    readOnly: true
}, {
    name: "focusCommandLine",
    bindKey: "shift-esc|ctrl-`",
    exec: function(editor, needle) { editor.cmdLine.focus(); },
    readOnly: true
}, {
    name: "nextFile",
    bindKey: "Ctrl-tab",
    exec: function(editor) { doclist.cycleOpen(editor, 1); },
    readOnly: true
}, {
    name: "previousFile",
    bindKey: "Ctrl-shift-tab",
    exec: function(editor) { doclist.cycleOpen(editor, -1); },
    readOnly: true
}, {
    name: "execute",
    bindKey: "ctrl+enter",
    exec: function(editor) {
        try {
            var r = window.eval(editor.getCopyText() || editor.getValue());
        } catch(e) {
            r = e;
        }
        editor.cmdLine.setValue(r + "");
    },
    readOnly: true
}, {
    name: "showKeyboardShortcuts",
    bindKey: {win: "Ctrl-Alt-h", mac: "Command-Alt-h"},
    exec: function(editor) {
        config.loadModule("ace/ext/keybinding_menu", function(module) {
            module.init(editor);
            editor.showKeyboardShortcuts();
        });
    }
}, {
    name: "increaseFontSize",
    bindKey: "Ctrl-=|Ctrl-+",
    exec: function(editor) {
        var size = parseInt(editor.getFontSize(), 10) || 12;
        editor.setFontSize(size + 1);
    }
}, {
    name: "decreaseFontSize",
    bindKey: "Ctrl+-|Ctrl-_",
    exec: function(editor) {
        var size = parseInt(editor.getFontSize(), 10) || 12;
        editor.setFontSize(Math.max(size - 1 || 1));
    }
}, {
    name: "resetFontSize",
    bindKey: "Ctrl+0|Ctrl-Numpad0",
    exec: function(editor) {
        editor.setFontSize(12);
    }
}]);


env.editor.commands.addCommands(whitespace.commands);

cmdLine.commands.bindKeys({
    "Shift-Return|Ctrl-Return|Alt-Return": function(cmdLine) { cmdLine.insert("\n"); },
    "Esc|Shift-Esc": function(cmdLine){ cmdLine.editor.focus(); },
    "Return": function(cmdLine){
        var command = cmdLine.getValue().split(/\s+/);
        var editor = cmdLine.editor;
        editor.commands.exec(command[0], editor, command[1]);
        editor.focus();
    }
});

cmdLine.commands.removeCommands(["find", "gotoline", "findall", "replace", "replaceall"]);

var commands = env.editor.commands;
commands.addCommand({
    name: "save",
    bindKey: {win: "Ctrl-S", mac: "Command-S"},
    exec: function(arg) {
        var session = env.editor.session;
        var name = session.name.match(/[^\/]+$/);
        localStorage.setItem(
            "saved_file:" + name,
            session.getValue()
        );
        env.editor.cmdLine.setValue("saved "+ name);
    }
});

commands.addCommand({
    name: "load",
    bindKey: {win: "Ctrl-O", mac: "Command-O"},
    exec: function(arg) {
        var session = env.editor.session;
        var name = session.name.match(/[^\/]+$/);
        var value = localStorage.getItem("saved_file:" + name);
        if (typeof value == "string") {
            session.setValue(value);
            env.editor.cmdLine.setValue("loaded "+ name);
        } else {
            env.editor.cmdLine.setValue("no previuos value saved for "+ name);
        }
    }
});
function handleToggleActivate(target) {
    if (dom.hasCssClass(sidePanelContainer, "closed"))
        onResize(null, false);
    else if (dom.hasCssClass(target, "toggleButton"))
        onResize(null, true);
};
var sidePanelContainer = document.getElementById("sidePanel");
sidePanelContainer.onclick = function(e) {
    handleToggleActivate(e.target);
};
var optionToggle = document.getElementById("optionToggle");
optionToggle.onkeydown = function(e) {
    if (e.code === "Space" || e.code === "Enter") {
        handleToggleActivate(e.target);
    }
};
var consoleHeight = 20;
function onResize(e, closeSidePanel) {
    var left = 280;
    var width = document.documentElement.clientWidth;
    var height = document.documentElement.clientHeight;
    if (closeSidePanel == null)
        closeSidePanel = width < 2 * left;
    if (closeSidePanel) {
        left = 20;
        document.getElementById("optionToggle").setAttribute("aria-label", "Show Options");
    } else
        document.getElementById("optionToggle").setAttribute("aria-label", "Hide Options");
    width -= left;
    container.style.width = width + "px";
    container.style.height = height - consoleHeight + "px";
    container.style.left = left + "px";
    env.split.resize();

    consoleEl.style.width = width + "px";
    consoleEl.style.left = left + "px";
    cmdLine.resize();
    
    sidePanel.style.width = left + "px";
    sidePanel.style.height = height + "px";
    dom.setCssClass(sidePanelContainer, "closed", closeSidePanel);
}

window.onresize = onResize;
onResize();
doclist.history = doclist.docs.map(function(doc) {
    return doc.name;
});
doclist.history.index = 0;
doclist.cycleOpen = function(editor, dir) {
    var h = this.history;
    h.index += dir;
    if (h.index >= h.length)
        h.index = 0;
    else if (h.index <= 0)
        h.index = h.length - 1;
    var s = h[h.index];
    doclist.pickDocument(s);
};
doclist.addToHistory = function(name) {
    var h = this.history;
    var i = h.indexOf(name);
    if (i != h.index) {
        if (i != -1)
            h.splice(i, 1);
        h.index = h.push(name);
    }
};
doclist.pickDocument = function(name) {
    doclist.loadDoc(name, function(session) {
        if (!session)
            return;
        doclist.addToHistory(session.name);
        session = env.split.setSession(session);
        whitespace.detectIndentation(session);
        optionsPanel.render();
        env.editor.focus();
    });
};



var OptionPanel = require("ace/ext/options").OptionPanel;
var optionsPanel = new OptionPanel(env.editor);

var originalAutocompleteCommand = null;

optionsPanel.add({
    Main: {
        Document: {
            type: "select",
            path: "doc",
            items: doclist.all,
            position: -101,
            onchange: doclist.pickDocument,
            getValue: function() {
                return env.editor.session.name || "javascript";
            }
        },
        Split: {
            type: "buttonBar",
            path: "split",
            values: ["None", "Below", "Beside"],
            position: -100,
            onchange: function(value) {
                var sp = env.split;
                if (value == "Below" || value == "Beside") {
                    var newEditor = (sp.getSplits() == 1);
                    sp.setOrientation(value == "Below" ? sp.BELOW : sp.BESIDE);
                    sp.setSplits(2);

                    if (newEditor) {
                        var session = sp.getEditor(0).session;
                        var newSession = sp.setSession(session, 1);
                        newSession.name = session.name;
                    }
                } else {
                    sp.setSplits(1);
                }
            },
            getValue: function() {
                var sp = env.split;
                return sp.getSplits() == 1
                    ? "None"
                    : sp.getOrientation() == sp.BELOW
                    ? "Below"
                    : "Beside";
            }
        }
    },
    More: {
        "RTL": {
            path: "rtl",
            position: 900
        },
        "Line based RTL switching": {
            path: "rtlText",
            position: 900
        },
        "Show token info": {
            path: "showTokenInfo",
            position: 2000
        },
        "Inline preview for autocomplete": {
            path: "inlineEnabledForAutocomplete",
            position: 2000,
            onchange: function(value) {
                var Autocomplete = require("ace/autocomplete").Autocomplete;
                if (value && !originalAutocompleteCommand) {
                    originalAutocompleteCommand = Autocomplete.startCommand.exec;
                    Autocomplete.startCommand.exec = function(editor) {
                        var autocomplete = Autocomplete.for(editor);
                        autocomplete.inlineEnabled = true;
                        originalAutocompleteCommand(...arguments);
                    }
                } else if (!value) {
                    var autocomplete = Autocomplete.for(editor);
                    autocomplete.destroy();
                    Autocomplete.startCommand.exec = originalAutocompleteCommand;
                    originalAutocompleteCommand = null;
                }
            },
            getValue: function() {
                return !!originalAutocompleteCommand;
            }
        },
        "Use Ace Linters": {
            position: 3000,
            path: "useAceLinters"
        },
        "Show Textarea Position": devUtil.textPositionDebugger,
        "Text Input Debugger": devUtil.textInputDebugger,
    }
});

var optionsPanelContainer = document.getElementById("optionsPanel");
optionsPanel.render();
optionsPanelContainer.insertBefore(optionsPanel.container, optionsPanelContainer.firstChild);
optionsPanel.container.style.width = "80%";
optionsPanel.on("setOption", function(e) {
    util.saveOption(e.name, e.value);
});

function updateUIEditorOptions() {
    optionsPanel.editor = env.editor;
    optionsPanel.render();
}

optionsPanel.setOption("doc", util.getOption("doc") || "JavaScript");
for (var i in optionsPanel.options) {
    var value = util.getOption(i);
    if (value != undefined) {
        if ((i == "mode" || i == "theme") && !/[/]/.test(value))
            value = "ace/" + i + "/" + value;
        optionsPanel.setOption(i, value);
    }
}


function synchroniseScrolling() {
    var s1 = env.split.$editors[0].session;
    var s2 = env.split.$editors[1].session;
    s1.on('changeScrollTop', function(pos) {s2.setScrollTop(pos)});
    s2.on('changeScrollTop', function(pos) {s1.setScrollTop(pos)});
    s1.on('changeScrollLeft', function(pos) {s2.setScrollLeft(pos)});
    s2.on('changeScrollLeft', function(pos) {s1.setScrollLeft(pos)});
}

var StatusBar = require("ace/ext/statusbar").StatusBar;
new StatusBar(env.editor, cmdLine.container);


var Emmet = require("ace/ext/emmet");
net.loadScript("https://cloud9ide.github.io/emmet-core/emmet.js", function() {
    Emmet.setCore(window.emmet);
    env.editor.setOption("enableEmmet", true);
});

require("ace/placeholder").PlaceHolder;

var snippetManager = require("ace/snippets").snippetManager;

env.editSnippets = function() {
    var sp = env.split;
    if (sp.getSplits() == 2) {
        sp.setSplits(1);
        return;
    }
    sp.setSplits(1);
    sp.setSplits(2);
    sp.setOrientation(sp.BESIDE);
    var editor = sp.$editors[1];
    var id = sp.$editors[0].session.$mode.$id || "";
    var m = snippetManager.files[id];
    if (!doclist["snippets/" + id]) {
        var text = m.snippetText;
        var s = doclist.initDoc(text, "", {});
        s.setMode("ace/mode/snippets");
        doclist["snippets/" + id] = s;
    }
    editor.on("blur", function() {
        m.snippetText = editor.getValue();
        snippetManager.unregister(m.snippets);
        m.snippets = snippetManager.parseSnippetFile(m.snippetText, m.scope);
        snippetManager.register(m.snippets);
    });
    sp.$editors[0].once("changeMode", function() {
        sp.setSplits(1);
    });
    editor.setSession(doclist["snippets/" + id], 1);
    editor.focus();
};

optionsPanelContainer.insertBefore(
    dom.buildDom(["div", {style: "text-align:right;width: 80%"},
        ["div", {}, 
            ["button", {onclick: env.editSnippets}, "Edit Snippets"]],
        ["div", {}, 
            ["button", {onclick: function() {
                var info = navigator.platform + "\n" + navigator.userAgent;
                if (env.editor.getValue() == info)
                    return env.editor.undo();
                env.editor.setValue(info, -1);
                env.editor.setOption("wrap", 80);
            }}, "Show Browser Info"]],
        devUtil.getUI(),
        ["div", {},
            "Open Dialog ",
            ["button",  {onclick: openTestDialog.bind(null, false)}, "Scale"],
            ["button",  {onclick: openTestDialog.bind(null, true)}, "Height"]
        ]
    ]),
    optionsPanelContainer.children[1]
);

function openTestDialog(animateHeight) {
    if (window.dialogEditor) 
        window.dialogEditor.destroy();
    var editor = ace.edit(null, {
        value: "test editor", 
        mode: "ace/mode/javascript"
    });
    window.dialogEditor = editor;

    var dialog = dom.buildDom(["div", {
        style: "transition: all 1s; position: fixed; z-index: 100000;"
          + "background: darkblue; border: solid 1px black; display: flex; flex-direction: column"
        }, 
        ["div", {}, "test dialog"],
        editor.container
    ], document.body);
    editor.container.style.flex = "1";
    if (animateHeight) {
        dialog.style.width = "0vw";
        dialog.style.height = "0vh";
        dialog.style.left = "20vw";
        dialog.style.top = "20vh";
        setTimeout(function() {            
            dialog.style.width = "80vw";
            dialog.style.height = "80vh";
            dialog.style.left = "10vw";
            dialog.style.top = "10vh";
        }, 0);
        
    } else {
        dialog.style.width = "80vw";
        dialog.style.height = "80vh";
        dialog.style.left = "10vw";
        dialog.style.top = "10vh";
        dialog.style.transform = "scale(0)";
        setTimeout(function() {
            dialog.style.transform = "scale(1)"
        }, 0);
    }
    function close(e) {
        if (!e || !dialog.contains(e.target)) {
            if (animateHeight) {
                dialog.style.width = "0vw";
                dialog.style.height = "0vh";
                dialog.style.left = "80vw";
                dialog.style.top = "80vh";
            } else {
                dialog.style.transform = "scale(0)"
            }
            window.removeEventListener("mousedown", close);
            dialog.addEventListener("transitionend", function() {
                dialog.remove();
                editor.destroy();
            });
        }
    }
    window.addEventListener("mousedown", close);
    editor.focus()
    editor.commands.bindKey("Esc", function() { close(); });
}


require("ace/ext/language_tools");
require("ace/ext/inline_autocomplete");
env.editor.setOptions({
    enableBasicAutocompletion: true,
    enableInlineAutocompletion: true,
    enableSnippets: true
});

var beautify = require("ace/ext/beautify");
env.editor.commands.addCommands(beautify.commands);

var KeyBinding = require("ace/keyboard/keybinding").KeyBinding;
var CommandManager = require("ace/commands/command_manager").CommandManager;
var commandManager = new CommandManager();
var kb = new KeyBinding({
    commands: commandManager,
    fake: true
});
event.addCommandKeyListener(document.documentElement, kb.onCommandKey.bind(kb));
event.addListener(document.documentElement, "keyup", function(e) {
    if (e.keyCode === 18) // do not trigger browser menu on windows
        e.preventDefault();
});
commandManager.addCommands([{
    name: "window-left",
    bindKey: {win: "cmd-alt-left", mac: "ctrl-cmd-left"},
    exec: function() {
        moveFocus();
    }
}, {
    name: "window-right",
    bindKey: {win: "cmd-alt-right", mac: "ctrl-cmd-right"},
    exec: function() {
        moveFocus();
    }
}, {
    name: "window-up",
    bindKey: {win: "cmd-alt-up", mac: "ctrl-cmd-up"},
    exec: function() {
        moveFocus();
    }
}, {
    name: "window-down",
    bindKey: {win: "cmd-alt-down", mac: "ctrl-cmd-down"},
    exec: function() {
        moveFocus();
    }
}]);

function moveFocus() {
    var el = document.activeElement;
    if (el == env.editor.textInput.getElement())
        env.editor.cmdLine.focus();    
    else
        env.editor.focus();
}

});                (function() {
                    window.require(["kitchen-sink/demo"], function(m) {
                        if (typeof module == "object" && typeof exports == "object" && module) {
                            module.exports = m;
                        }
                    });
                })();
            