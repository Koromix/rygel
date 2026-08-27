var __create = Object.create;
var __defProp = Object.defineProperty;
var __getOwnPropDesc = Object.getOwnPropertyDescriptor;
var __getOwnPropNames = Object.getOwnPropertyNames;
var __getProtoOf = Object.getPrototypeOf;
var __hasOwnProp = Object.prototype.hasOwnProperty;
var __commonJS = (cb, mod) => function __require() {
  try {
    return mod || (0, cb[__getOwnPropNames(cb)[0]])((mod = { exports: {} }).exports, mod), mod.exports;
  } catch (e) {
    throw mod = 0, e;
  }
};
var __copyProps = (to, from, except, desc) => {
  if (from && typeof from === "object" || typeof from === "function") {
    for (let key of __getOwnPropNames(from))
      if (!__hasOwnProp.call(to, key) && key !== except)
        __defProp(to, key, { get: () => from[key], enumerable: !(desc = __getOwnPropDesc(from, key)) || desc.enumerable });
  }
  return to;
};
var __toESM = (mod, isNodeMode, target) => (target = mod != null ? __create(__getProtoOf(mod)) : {}, __copyProps(
  // If the importer is in node compatibility mode or this is not an ESM
  // file that has been converted to a CommonJS file using a Babel-
  // compatible transform (i.e. "__esModule" has not been set), then set
  // "default" to the CommonJS "module.exports" for node compatibility.
  isNodeMode || !mod || !mod.__esModule ? __defProp(target, "default", { value: mod, enumerable: true }) : target,
  mod
));

// repo/build/highlight.js
var require_highlight = __commonJS({
  "repo/build/highlight.js"(exports, module) {
    var hljs = (function() {
      "use strict";
      function deepFreeze(obj) {
        if (obj instanceof Map) {
          obj.clear = obj.delete = obj.set = function() {
            throw new Error("map is read-only");
          };
        } else if (obj instanceof Set) {
          obj.add = obj.clear = obj.delete = function() {
            throw new Error("set is read-only");
          };
        }
        Object.freeze(obj);
        Object.getOwnPropertyNames(obj).forEach((name) => {
          const prop = obj[name];
          const type = typeof prop;
          if ((type === "object" || type === "function") && !Object.isFrozen(prop)) {
            deepFreeze(prop);
          }
        });
        return obj;
      }
      class Response {
        /**
         * @param {CompiledMode} mode
         */
        constructor(mode) {
          if (mode.data === void 0) mode.data = {};
          this.data = mode.data;
          this.isMatchIgnored = false;
        }
        ignoreMatch() {
          this.isMatchIgnored = true;
        }
      }
      function escapeHTML(value) {
        return value.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/"/g, "&quot;").replace(/'/g, "&#x27;");
      }
      function inherit$1(original, ...objects) {
        const result = /* @__PURE__ */ Object.create(null);
        for (const key in original) {
          result[key] = original[key];
        }
        objects.forEach(function(obj) {
          for (const key in obj) {
            result[key] = obj[key];
          }
        });
        return (
          /** @type {T} */
          result
        );
      }
      const SPAN_CLOSE = "</span>";
      const emitsWrappingTags = (node) => {
        return !!node.scope;
      };
      const scopeToCSSClass = (name, { prefix }) => {
        if (name.startsWith("language:")) {
          return name.replace("language:", "language-");
        }
        if (name.includes(".")) {
          const pieces = name.split(".");
          return [
            `${prefix}${pieces.shift()}`,
            ...pieces.map((x, i) => `${x}${"_".repeat(i + 1)}`)
          ].join(" ");
        }
        return `${prefix}${name}`;
      };
      class HTMLRenderer {
        /**
         * Creates a new HTMLRenderer
         *
         * @param {Tree} parseTree - the parse tree (must support `walk` API)
         * @param {{classPrefix: string}} options
         */
        constructor(parseTree, options) {
          this.buffer = "";
          this.classPrefix = options.classPrefix;
          parseTree.walk(this);
        }
        /**
         * Adds texts to the output stream
         *
         * @param {string} text */
        addText(text) {
          this.buffer += escapeHTML(text);
        }
        /**
         * Adds a node open to the output stream (if needed)
         *
         * @param {Node} node */
        openNode(node) {
          if (!emitsWrappingTags(node)) return;
          const className = scopeToCSSClass(
            node.scope,
            { prefix: this.classPrefix }
          );
          this.span(className);
        }
        /**
         * Adds a node close to the output stream (if needed)
         *
         * @param {Node} node */
        closeNode(node) {
          if (!emitsWrappingTags(node)) return;
          this.buffer += SPAN_CLOSE;
        }
        /**
         * returns the accumulated buffer
        */
        value() {
          return this.buffer;
        }
        // helpers
        /**
         * Builds a span element
         *
         * @param {string} className */
        span(className) {
          this.buffer += `<span class="${className}">`;
        }
      }
      const newNode = (opts = {}) => {
        const result = { children: [] };
        Object.assign(result, opts);
        return result;
      };
      class TokenTree {
        constructor() {
          this.rootNode = newNode();
          this.stack = [this.rootNode];
        }
        get top() {
          return this.stack[this.stack.length - 1];
        }
        get root() {
          return this.rootNode;
        }
        /** @param {Node} node */
        add(node) {
          this.top.children.push(node);
        }
        /** @param {string} scope */
        openNode(scope) {
          const node = newNode({ scope });
          this.add(node);
          this.stack.push(node);
        }
        closeNode() {
          if (this.stack.length > 1) {
            return this.stack.pop();
          }
          return void 0;
        }
        closeAllNodes() {
          while (this.closeNode()) ;
        }
        toJSON() {
          return JSON.stringify(this.rootNode, null, 4);
        }
        /**
         * @typedef { import("./html_renderer").Renderer } Renderer
         * @param {Renderer} builder
         */
        walk(builder) {
          return this.constructor._walk(builder, this.rootNode);
        }
        /**
         * @param {Renderer} builder
         * @param {Node} node
         */
        static _walk(builder, node) {
          if (typeof node === "string") {
            builder.addText(node);
          } else if (node.children) {
            builder.openNode(node);
            node.children.forEach((child) => this._walk(builder, child));
            builder.closeNode(node);
          }
          return builder;
        }
        /**
         * @param {Node} node
         */
        static _collapse(node) {
          if (typeof node === "string") return;
          if (!node.children) return;
          if (node.children.every((el) => typeof el === "string")) {
            node.children = [node.children.join("")];
          } else {
            node.children.forEach((child) => {
              TokenTree._collapse(child);
            });
          }
        }
      }
      class TokenTreeEmitter extends TokenTree {
        /**
         * @param {*} options
         */
        constructor(options) {
          super();
          this.options = options;
        }
        /**
         * @param {string} text
         */
        addText(text) {
          if (text === "") {
            return;
          }
          this.add(text);
        }
        /** @param {string} scope */
        startScope(scope) {
          this.openNode(scope);
        }
        endScope() {
          this.closeNode();
        }
        /**
         * @param {Emitter & {root: DataNode}} emitter
         * @param {string} name
         */
        __addSublanguage(emitter, name) {
          const node = emitter.root;
          if (name) node.scope = `language:${name}`;
          this.add(node);
        }
        toHTML() {
          const renderer = new HTMLRenderer(this, this.options);
          return renderer.value();
        }
        finalize() {
          this.closeAllNodes();
          return true;
        }
      }
      function source(re) {
        if (!re) return null;
        if (typeof re === "string") return re;
        return re.source;
      }
      function lookahead(re) {
        return concat("(?=", re, ")");
      }
      function anyNumberOfTimes(re) {
        return concat("(?:", re, ")*");
      }
      function optional(re) {
        return concat("(?:", re, ")?");
      }
      function concat(...args) {
        const joined = args.map((x) => source(x)).join("");
        return joined;
      }
      function stripOptionsFromArgs(args) {
        const opts = args[args.length - 1];
        if (typeof opts === "object" && opts.constructor === Object) {
          args.splice(args.length - 1, 1);
          return opts;
        } else {
          return {};
        }
      }
      function either(...args) {
        const opts = stripOptionsFromArgs(args);
        const joined = "(" + (opts.capture ? "" : "?:") + args.map((x) => source(x)).join("|") + ")";
        return joined;
      }
      function countMatchGroups(re) {
        return new RegExp(re.toString() + "|").exec("").length - 1;
      }
      function startsWith(re, lexeme) {
        const match = re && re.exec(lexeme);
        return match && match.index === 0;
      }
      const BACKREF_RE = new RegExp(either(
        /\[(?:[^\\\]]|\\.)*\]/,
        // a character class, inside which ( and \ lose their meaning
        /\(\?<(?![=!])[^>]+>/,
        // a named capture group `(?<name>` (not a lookbehind `(?<=` / `(?<!`)
        /\(\?'[^']+'/,
        // a named capture group `(?'name'`
        /\(\??/,
        // an opening parenthesis, capturing or non-capturing / lookahead
        /\\([1-9][0-9]*)/,
        // a backreference like `\1`
        /\\./
        // any other escape sequence
      ));
      function _rewriteBackreferences(regexps, { joinWith }) {
        let numCaptures = 0;
        return regexps.map((regex) => {
          numCaptures += 1;
          const offset = numCaptures;
          let re = source(regex);
          let out = "";
          while (re.length > 0) {
            const match = BACKREF_RE.exec(re);
            if (!match) {
              out += re;
              break;
            }
            out += re.substring(0, match.index);
            re = re.substring(match.index + match[0].length);
            if (match[0][0] === "\\" && match[1]) {
              out += "\\" + String(Number(match[1]) + offset);
            } else {
              out += match[0];
              if (match[0] === "(" || /^\(\?[<']/.test(match[0])) {
                numCaptures++;
              }
            }
          }
          return out;
        }).map((re) => `(${re})`).join(joinWith);
      }
      const MATCH_NOTHING_RE = /\b\B/;
      const IDENT_RE$1 = "[a-zA-Z]\\w*";
      const UNDERSCORE_IDENT_RE = "[a-zA-Z_]\\w*";
      const NUMBER_RE = "\\b\\d+(\\.\\d+)?";
      const C_NUMBER_RE = "(-?)(\\b0[xX][a-fA-F0-9]+|(\\b\\d+(\\.\\d*)?|\\.\\d+)([eE][-+]?\\d+)?)";
      const BINARY_NUMBER_RE = "\\b(0b[01]+)";
      const RE_STARTERS_RE = "!|!=|!==|%|%=|&|&&|&=|\\*|\\*=|\\+|\\+=|,|-|-=|/=|/|:|;|<<|<<=|<=|<|===|==|=|>>>=|>>=|>=|>>>|>>|>|\\?|\\[|\\{|\\(|\\^|\\^=|\\||\\|=|\\|\\||~";
      const SHEBANG = (opts = {}) => {
        const beginShebang = /^#![ ]*\//;
        if (opts.binary) {
          opts.begin = concat(
            beginShebang,
            /.*\b/,
            opts.binary,
            /\b.*/
          );
        }
        return inherit$1({
          scope: "meta",
          begin: beginShebang,
          end: /$/,
          relevance: 0,
          /** @type {ModeCallback} */
          "on:begin": (m, resp) => {
            if (m.index !== 0) resp.ignoreMatch();
          }
        }, opts);
      };
      const BACKSLASH_ESCAPE = {
        begin: "\\\\[\\s\\S]",
        relevance: 0
      };
      const APOS_STRING_MODE = {
        scope: "string",
        begin: "'",
        end: "'",
        illegal: "\\n",
        contains: [BACKSLASH_ESCAPE]
      };
      const QUOTE_STRING_MODE = {
        scope: "string",
        begin: '"',
        end: '"',
        illegal: "\\n",
        contains: [BACKSLASH_ESCAPE]
      };
      const PHRASAL_WORDS_MODE = {
        begin: /\b(a|an|the|are|I'm|isn't|don't|doesn't|won't|but|just|should|pretty|simply|enough|gonna|going|wtf|so|such|will|you|your|they|like|more)\b/
      };
      const COMMENT = function(begin, end, modeOptions = {}) {
        const mode = inherit$1(
          {
            scope: "comment",
            begin,
            end,
            contains: []
          },
          modeOptions
        );
        mode.contains.push({
          scope: "doctag",
          // hack to avoid the space from being included. the space is necessary to
          // match here to prevent the plain text rule below from gobbling up doctags
          begin: "[ ]*(?=(TODO|FIXME|NOTE|BUG|OPTIMIZE|HACK|XXX):)",
          end: /(TODO|FIXME|NOTE|BUG|OPTIMIZE|HACK|XXX):/,
          excludeBegin: true,
          relevance: 0
        });
        const ENGLISH_WORD = either(
          // list of common 1 and 2 letter words in English
          "I",
          "a",
          "is",
          "so",
          "us",
          "to",
          "at",
          "if",
          "in",
          "it",
          "on",
          // note: this is not an exhaustive list of contractions, just popular ones
          /[A-Za-z]+['](d|ve|re|ll|t|s|n)/,
          // contractions - can't we'd they're let's, etc
          /[A-Za-z]+[-][a-z]+/,
          // `no-way`, etc.
          /[A-Za-z][a-z]{2,}/
          // allow capitalized words at beginning of sentences
        );
        mode.contains.push(
          {
            // TODO: how to include ", (, ) without breaking grammars that use these for
            // comment delimiters?
            // begin: /[ ]+([()"]?([A-Za-z'-]{3,}|is|a|I|so|us|[tT][oO]|at|if|in|it|on)[.]?[()":]?([.][ ]|[ ]|\))){3}/
            // ---
            // this tries to find sequences of 3 english words in a row (without any
            // "programming" type syntax) this gives us a strong signal that we've
            // TRULY found a comment - vs perhaps scanning with the wrong language.
            // It's possible to find something that LOOKS like the start of the
            // comment - but then if there is no readable text - good chance it is a
            // false match and not a comment.
            //
            // for a visual example please see:
            // https://github.com/highlightjs/highlight.js/issues/2827
            begin: concat(
              /[ ]+/,
              // necessary to prevent us gobbling up doctags like /* @author Bob Mcgill */
              "(",
              ENGLISH_WORD,
              /[.]?[:]?([.][ ]|[ ])/,
              "){3}"
            )
            // look for 3 words in a row
          }
        );
        return mode;
      };
      const C_LINE_COMMENT_MODE = COMMENT("//", "$");
      const C_BLOCK_COMMENT_MODE = COMMENT("/\\*", "\\*/");
      const HASH_COMMENT_MODE = COMMENT("#", "$");
      const NUMBER_MODE = {
        scope: "number",
        begin: NUMBER_RE,
        relevance: 0
      };
      const C_NUMBER_MODE = {
        scope: "number",
        begin: C_NUMBER_RE,
        relevance: 0
      };
      const BINARY_NUMBER_MODE = {
        scope: "number",
        begin: BINARY_NUMBER_RE,
        relevance: 0
      };
      const REGEXP_MODE = {
        scope: "regexp",
        begin: /\/(?=[^/\n]*\/)/,
        end: /\/[gimuy]*/,
        contains: [
          BACKSLASH_ESCAPE,
          {
            begin: /\[/,
            end: /\]/,
            relevance: 0,
            contains: [BACKSLASH_ESCAPE]
          }
        ]
      };
      const TITLE_MODE = {
        scope: "title",
        begin: IDENT_RE$1,
        relevance: 0
      };
      const UNDERSCORE_TITLE_MODE = {
        scope: "title",
        begin: UNDERSCORE_IDENT_RE,
        relevance: 0
      };
      const METHOD_GUARD = {
        // excludes method names from keyword processing
        begin: "\\.\\s*" + UNDERSCORE_IDENT_RE,
        relevance: 0
      };
      const END_SAME_AS_BEGIN = function(mode) {
        return Object.assign(
          mode,
          {
            /** @type {ModeCallback} */
            "on:begin": (m, resp) => {
              resp.data._beginMatch = m[1];
            },
            /** @type {ModeCallback} */
            "on:end": (m, resp) => {
              if (resp.data._beginMatch !== m[1]) resp.ignoreMatch();
            }
          }
        );
      };
      var MODES$1 = /* @__PURE__ */ Object.freeze({
        __proto__: null,
        APOS_STRING_MODE,
        BACKSLASH_ESCAPE,
        BINARY_NUMBER_MODE,
        BINARY_NUMBER_RE,
        COMMENT,
        C_BLOCK_COMMENT_MODE,
        C_LINE_COMMENT_MODE,
        C_NUMBER_MODE,
        C_NUMBER_RE,
        END_SAME_AS_BEGIN,
        HASH_COMMENT_MODE,
        IDENT_RE: IDENT_RE$1,
        MATCH_NOTHING_RE,
        METHOD_GUARD,
        NUMBER_MODE,
        NUMBER_RE,
        PHRASAL_WORDS_MODE,
        QUOTE_STRING_MODE,
        REGEXP_MODE,
        RE_STARTERS_RE,
        SHEBANG,
        TITLE_MODE,
        UNDERSCORE_IDENT_RE,
        UNDERSCORE_TITLE_MODE
      });
      function skipIfHasPrecedingDot(match, response) {
        const before = match.input[match.index - 1];
        if (before === ".") {
          response.ignoreMatch();
        }
      }
      function scopeClassName(mode, _parent) {
        if (mode.className !== void 0) {
          mode.scope = mode.className;
          delete mode.className;
        }
      }
      function beginKeywords(mode, parent) {
        if (!parent) return;
        if (!mode.beginKeywords) return;
        mode.begin = "\\b(" + mode.beginKeywords.split(" ").join("|") + ")(?!\\.)(?=\\b|\\s)";
        mode.__beforeBegin = skipIfHasPrecedingDot;
        mode.keywords = mode.keywords || mode.beginKeywords;
        delete mode.beginKeywords;
        if (mode.relevance === void 0) mode.relevance = 0;
      }
      function compileIllegal(mode, _parent) {
        if (!Array.isArray(mode.illegal)) return;
        mode.illegal = either(...mode.illegal);
      }
      function compileMatch(mode, _parent) {
        if (!mode.match) return;
        if (mode.begin || mode.end) throw new Error("begin & end are not supported with match");
        mode.begin = mode.match;
        delete mode.match;
      }
      function compileRelevance(mode, _parent) {
        if (mode.relevance === void 0) mode.relevance = 1;
      }
      const beforeMatchExt = (mode, parent) => {
        if (!mode.beforeMatch) return;
        if (mode.starts) throw new Error("beforeMatch cannot be used with starts");
        const originalMode = Object.assign({}, mode);
        Object.keys(mode).forEach((key) => {
          delete mode[key];
        });
        mode.keywords = originalMode.keywords;
        mode.begin = concat(originalMode.beforeMatch, lookahead(originalMode.begin));
        mode.starts = {
          relevance: 0,
          contains: [
            Object.assign(originalMode, { endsParent: true })
          ]
        };
        mode.relevance = 0;
        delete originalMode.beforeMatch;
      };
      const COMMON_KEYWORDS = [
        "of",
        "and",
        "for",
        "in",
        "not",
        "or",
        "if",
        "then",
        "parent",
        // common variable name
        "list",
        // common variable name
        "value"
        // common variable name
      ];
      const DEFAULT_KEYWORD_SCOPE = "keyword";
      function compileKeywords(rawKeywords, caseInsensitive, scopeName = DEFAULT_KEYWORD_SCOPE) {
        const compiledKeywords = /* @__PURE__ */ Object.create(null);
        if (typeof rawKeywords === "string") {
          compileList(scopeName, rawKeywords.split(" "));
        } else if (Array.isArray(rawKeywords)) {
          compileList(scopeName, rawKeywords);
        } else {
          Object.keys(rawKeywords).forEach(function(scopeName2) {
            Object.assign(
              compiledKeywords,
              compileKeywords(rawKeywords[scopeName2], caseInsensitive, scopeName2)
            );
          });
        }
        return compiledKeywords;
        function compileList(scopeName2, keywordList) {
          if (caseInsensitive) {
            keywordList = keywordList.map((x) => x.toLowerCase());
          }
          keywordList.forEach(function(keyword) {
            const pair = keyword.split("|");
            compiledKeywords[pair[0]] = [scopeName2, scoreForKeyword(pair[0], pair[1])];
          });
        }
      }
      function scoreForKeyword(keyword, providedScore) {
        if (providedScore) {
          return Number(providedScore);
        }
        return commonKeyword(keyword) ? 0 : 1;
      }
      function commonKeyword(keyword) {
        return COMMON_KEYWORDS.includes(keyword.toLowerCase());
      }
      const seenDeprecations = {};
      const error = (message) => {
        console.error(message);
      };
      const warn = (message, ...args) => {
        console.log(`WARN: ${message}`, ...args);
      };
      const deprecated = (version2, message) => {
        if (seenDeprecations[`${version2}/${message}`]) return;
        console.log(`Deprecated as of ${version2}. ${message}`);
        seenDeprecations[`${version2}/${message}`] = true;
      };
      const MultiClassError = new Error();
      function remapScopeNames(mode, regexes, { key }) {
        let offset = 0;
        const scopeNames = mode[key];
        const emit = {};
        const positions = {};
        for (let i = 1; i <= regexes.length; i++) {
          positions[i + offset] = scopeNames[i];
          emit[i + offset] = true;
          offset += countMatchGroups(regexes[i - 1]);
        }
        mode[key] = positions;
        mode[key]._emit = emit;
        mode[key]._multi = true;
      }
      function beginMultiClass(mode) {
        if (!Array.isArray(mode.begin)) return;
        if (mode.skip || mode.excludeBegin || mode.returnBegin) {
          error("skip, excludeBegin, returnBegin not compatible with beginScope: {}");
          throw MultiClassError;
        }
        if (typeof mode.beginScope !== "object" || mode.beginScope === null) {
          error("beginScope must be object");
          throw MultiClassError;
        }
        remapScopeNames(mode, mode.begin, { key: "beginScope" });
        mode.begin = _rewriteBackreferences(mode.begin, { joinWith: "" });
      }
      function endMultiClass(mode) {
        if (!Array.isArray(mode.end)) return;
        if (mode.skip || mode.excludeEnd || mode.returnEnd) {
          error("skip, excludeEnd, returnEnd not compatible with endScope: {}");
          throw MultiClassError;
        }
        if (typeof mode.endScope !== "object" || mode.endScope === null) {
          error("endScope must be object");
          throw MultiClassError;
        }
        remapScopeNames(mode, mode.end, { key: "endScope" });
        mode.end = _rewriteBackreferences(mode.end, { joinWith: "" });
      }
      function scopeSugar(mode) {
        if (mode.scope && typeof mode.scope === "object" && mode.scope !== null) {
          mode.beginScope = mode.scope;
          delete mode.scope;
        }
      }
      function MultiClass(mode) {
        scopeSugar(mode);
        if (typeof mode.beginScope === "string") {
          mode.beginScope = { _wrap: mode.beginScope };
        }
        if (typeof mode.endScope === "string") {
          mode.endScope = { _wrap: mode.endScope };
        }
        beginMultiClass(mode);
        endMultiClass(mode);
      }
      function compileLanguage(language) {
        function langRe(value, global) {
          return new RegExp(
            source(value),
            "m" + (language.case_insensitive ? "i" : "") + (language.unicodeRegex ? "u" : "") + (global ? "g" : "")
          );
        }
        class MultiRegex {
          constructor() {
            this.matchIndexes = {};
            this.regexes = [];
            this.matchAt = 1;
            this.position = 0;
          }
          // @ts-ignore
          addRule(re, opts) {
            opts.position = this.position++;
            this.matchIndexes[this.matchAt] = opts;
            this.regexes.push([opts, re]);
            this.matchAt += countMatchGroups(re) + 1;
          }
          compile() {
            if (this.regexes.length === 0) {
              this.exec = () => null;
            }
            const terminators = this.regexes.map((el) => el[1]);
            this.matcherRe = langRe(_rewriteBackreferences(terminators, { joinWith: "|" }), true);
            this.lastIndex = 0;
          }
          /** @param {string} s */
          exec(s) {
            this.matcherRe.lastIndex = this.lastIndex;
            const match = this.matcherRe.exec(s);
            if (!match) {
              return null;
            }
            const i = match.findIndex((el, i2) => i2 > 0 && el !== void 0);
            const matchData = this.matchIndexes[i];
            match.splice(0, i);
            return Object.assign(match, matchData);
          }
        }
        class ResumableMultiRegex {
          constructor() {
            this.rules = [];
            this.multiRegexes = [];
            this.count = 0;
            this.lastIndex = 0;
            this.regexIndex = 0;
          }
          // @ts-ignore
          getMatcher(index) {
            if (this.multiRegexes[index]) return this.multiRegexes[index];
            const matcher = new MultiRegex();
            this.rules.slice(index).forEach(([re, opts]) => matcher.addRule(re, opts));
            matcher.compile();
            this.multiRegexes[index] = matcher;
            return matcher;
          }
          resumingScanAtSamePosition() {
            return this.regexIndex !== 0;
          }
          considerAll() {
            this.regexIndex = 0;
          }
          // @ts-ignore
          addRule(re, opts) {
            this.rules.push([re, opts]);
            if (opts.type === "begin") this.count++;
          }
          /** @param {string} s */
          exec(s) {
            const m = this.getMatcher(this.regexIndex);
            m.lastIndex = this.lastIndex;
            let result = m.exec(s);
            if (this.resumingScanAtSamePosition()) {
              if (result && result.index === this.lastIndex) ;
              else {
                const m2 = this.getMatcher(0);
                m2.lastIndex = this.lastIndex + 1;
                result = m2.exec(s);
              }
            }
            if (result) {
              this.regexIndex += result.position + 1;
              if (this.regexIndex === this.count) {
                this.considerAll();
              }
            }
            return result;
          }
        }
        function buildModeRegex(mode) {
          const mm = new ResumableMultiRegex();
          mode.contains.forEach((term) => mm.addRule(term.begin, { rule: term, type: "begin" }));
          if (mode.terminatorEnd) {
            mm.addRule(mode.terminatorEnd, { type: "end" });
          }
          if (mode.illegal) {
            mm.addRule(mode.illegal, { type: "illegal" });
          }
          return mm;
        }
        function compileMode(mode, parent) {
          const cmode = (
            /** @type CompiledMode */
            mode
          );
          if (mode.isCompiled) return cmode;
          [
            scopeClassName,
            // do this early so compiler extensions generally don't have to worry about
            // the distinction between match/begin
            compileMatch,
            MultiClass,
            beforeMatchExt
          ].forEach((ext) => ext(mode, parent));
          language.compilerExtensions.forEach((ext) => ext(mode, parent));
          mode.__beforeBegin = null;
          [
            beginKeywords,
            // do this later so compiler extensions that come earlier have access to the
            // raw array if they wanted to perhaps manipulate it, etc.
            compileIllegal,
            // default to 1 relevance if not specified
            compileRelevance
          ].forEach((ext) => ext(mode, parent));
          mode.isCompiled = true;
          let keywordPattern = null;
          if (typeof mode.keywords === "object" && mode.keywords.$pattern) {
            mode.keywords = Object.assign({}, mode.keywords);
            keywordPattern = mode.keywords.$pattern;
            delete mode.keywords.$pattern;
          }
          keywordPattern = keywordPattern || /\w+/;
          if (mode.keywords) {
            mode.keywords = compileKeywords(mode.keywords, language.case_insensitive);
          }
          cmode.keywordPatternRe = langRe(keywordPattern, true);
          if (parent) {
            if (!mode.begin) mode.begin = /\B|\b/;
            cmode.beginRe = langRe(cmode.begin);
            if (!mode.end && !mode.endsWithParent) mode.end = /\B|\b/;
            if (mode.end) cmode.endRe = langRe(cmode.end);
            cmode.terminatorEnd = source(cmode.end) || "";
            if (mode.endsWithParent && parent.terminatorEnd) {
              cmode.terminatorEnd += (mode.end ? "|" : "") + parent.terminatorEnd;
            }
          }
          if (mode.illegal) cmode.illegalRe = langRe(
            /** @type {RegExp | string} */
            mode.illegal
          );
          if (!mode.contains) mode.contains = [];
          mode.contains = [].concat(...mode.contains.map(function(c2) {
            return expandOrCloneMode(c2 === "self" ? mode : c2);
          }));
          mode.contains.forEach(function(c2) {
            compileMode(
              /** @type Mode */
              c2,
              cmode
            );
          });
          if (mode.starts) {
            compileMode(mode.starts, parent);
          }
          cmode.matcher = buildModeRegex(cmode);
          return cmode;
        }
        if (!language.compilerExtensions) language.compilerExtensions = [];
        if (language.contains && language.contains.includes("self")) {
          throw new Error("ERR: contains `self` is not supported at the top-level of a language.  See documentation.");
        }
        language.classNameAliases = inherit$1(language.classNameAliases || {});
        return compileMode(
          /** @type Mode */
          language
        );
      }
      function dependencyOnParent(mode) {
        if (!mode) return false;
        return mode.endsWithParent || dependencyOnParent(mode.starts);
      }
      function expandOrCloneMode(mode) {
        if (mode.variants && !mode.cachedVariants) {
          mode.cachedVariants = mode.variants.map(function(variant) {
            return inherit$1(mode, { variants: null }, variant);
          });
        }
        if (mode.cachedVariants) {
          return mode.cachedVariants;
        }
        if (dependencyOnParent(mode)) {
          return inherit$1(mode, { starts: mode.starts ? inherit$1(mode.starts) : null });
        }
        if (Object.isFrozen(mode)) {
          return inherit$1(mode);
        }
        return mode;
      }
      var version = "11.12.0";
      class HTMLInjectionError extends Error {
        constructor(reason, html) {
          super(reason);
          this.name = "HTMLInjectionError";
          this.html = html;
        }
      }
      const escape = escapeHTML;
      const inherit = inherit$1;
      const NO_MATCH = /* @__PURE__ */ Symbol("nomatch");
      const MAX_KEYWORD_HITS = 7;
      const HLJS = function(hljs3) {
        const languages = /* @__PURE__ */ Object.create(null);
        const aliases = /* @__PURE__ */ Object.create(null);
        const plugins = [];
        let SAFE_MODE = true;
        const LANGUAGE_NOT_FOUND = "Could not find the language '{}', did you forget to load/include a language module?";
        const PLAINTEXT_LANGUAGE = { disableAutodetect: true, name: "Plain text", contains: [] };
        let options = {
          ignoreUnescapedHTML: false,
          throwUnescapedHTML: false,
          noHighlightRe: /^(no-?highlight)$/i,
          languageDetectRe: /\blang(?:uage)?-([\w-]+)\b/i,
          classPrefix: "hljs-",
          cssSelector: "pre code",
          languages: null,
          // beta configuration options, subject to change, welcome to discuss
          // https://github.com/highlightjs/highlight.js/issues/1086
          __emitter: TokenTreeEmitter
        };
        function shouldNotHighlight(languageName) {
          return options.noHighlightRe.test(languageName);
        }
        function blockLanguage(block) {
          let classes = block.className + " ";
          classes += block.parentNode ? block.parentNode.className : "";
          const match = options.languageDetectRe.exec(classes);
          if (match) {
            const language = getLanguage(match[1]);
            if (!language) {
              warn(LANGUAGE_NOT_FOUND.replace("{}", match[1]));
              warn("Falling back to no-highlight mode for this block.", block);
            }
            return language ? match[1] : "no-highlight";
          }
          return classes.split(/\s+/).find((_class) => shouldNotHighlight(_class) || getLanguage(_class));
        }
        function highlight3(codeOrLanguageName, optionsOrCode, ignoreIllegals) {
          let code = "";
          let languageName = "";
          if (typeof optionsOrCode === "object") {
            code = codeOrLanguageName;
            ignoreIllegals = optionsOrCode.ignoreIllegals;
            languageName = optionsOrCode.language;
          } else {
            deprecated("10.7.0", "highlight(lang, code, ...args) has been deprecated.");
            deprecated("10.7.0", "Please use highlight(code, options) instead.\nhttps://github.com/highlightjs/highlight.js/issues/2277");
            languageName = codeOrLanguageName;
            code = optionsOrCode;
          }
          if (ignoreIllegals === void 0) {
            ignoreIllegals = true;
          }
          const context = {
            code,
            language: languageName
          };
          fire("before:highlight", context);
          const result = context.result ? context.result : _highlight(context.language, context.code, ignoreIllegals);
          result.code = context.code;
          fire("after:highlight", result);
          return result;
        }
        function _highlight(languageName, codeToHighlight, ignoreIllegals, continuation) {
          const keywordHits = /* @__PURE__ */ Object.create(null);
          function keywordData(mode, matchText) {
            return mode.keywords[matchText];
          }
          function processKeywords() {
            if (!top.keywords) {
              emitter.addText(modeBuffer);
              return;
            }
            let lastIndex = 0;
            top.keywordPatternRe.lastIndex = 0;
            let match = top.keywordPatternRe.exec(modeBuffer);
            let buf = "";
            while (match) {
              buf += modeBuffer.substring(lastIndex, match.index);
              const word = language.case_insensitive ? match[0].toLowerCase() : match[0];
              const data = keywordData(top, word);
              if (data) {
                const [kind, keywordRelevance] = data;
                emitter.addText(buf);
                buf = "";
                keywordHits[word] = (keywordHits[word] || 0) + 1;
                if (keywordHits[word] <= MAX_KEYWORD_HITS) relevance += keywordRelevance;
                if (kind.startsWith("_")) {
                  buf += match[0];
                } else {
                  const cssClass = language.classNameAliases[kind] || kind;
                  emitKeyword(match[0], cssClass);
                }
              } else {
                buf += match[0];
              }
              lastIndex = top.keywordPatternRe.lastIndex;
              match = top.keywordPatternRe.exec(modeBuffer);
            }
            buf += modeBuffer.substring(lastIndex);
            emitter.addText(buf);
          }
          function processSubLanguage() {
            if (modeBuffer === "") return;
            let result2 = null;
            if (typeof top.subLanguage === "string") {
              if (!languages[top.subLanguage]) {
                emitter.addText(modeBuffer);
                return;
              }
              result2 = _highlight(top.subLanguage, modeBuffer, true, continuations[top.subLanguage]);
              continuations[top.subLanguage] = /** @type {CompiledMode} */
              result2._top;
            } else {
              result2 = highlightAuto2(modeBuffer, top.subLanguage.length ? top.subLanguage : null);
            }
            if (top.relevance > 0) {
              relevance += result2.relevance;
            }
            emitter.__addSublanguage(result2._emitter, result2.language);
          }
          function processBuffer() {
            if (top.subLanguage != null) {
              processSubLanguage();
            } else {
              processKeywords();
            }
            modeBuffer = "";
          }
          function emitKeyword(keyword, scope) {
            if (keyword === "") return;
            emitter.startScope(scope);
            emitter.addText(keyword);
            emitter.endScope();
          }
          function emitMultiClass(scope, match) {
            let i = 1;
            const max = match.length - 1;
            while (i <= max) {
              if (!scope._emit[i]) {
                i++;
                continue;
              }
              const klass = language.classNameAliases[scope[i]] || scope[i];
              const text = match[i];
              if (klass) {
                emitKeyword(text, klass);
              } else {
                modeBuffer = text;
                processKeywords();
                modeBuffer = "";
              }
              i++;
            }
          }
          function startNewMode(mode, match) {
            if (mode.scope && typeof mode.scope === "string") {
              emitter.openNode(language.classNameAliases[mode.scope] || mode.scope);
            }
            if (mode.beginScope) {
              if (mode.beginScope._wrap) {
                emitKeyword(modeBuffer, language.classNameAliases[mode.beginScope._wrap] || mode.beginScope._wrap);
                modeBuffer = "";
              } else if (mode.beginScope._multi) {
                emitMultiClass(mode.beginScope, match);
                modeBuffer = "";
              }
            }
            top = Object.create(mode, { parent: { value: top } });
            return top;
          }
          function endOfMode(mode, match, matchPlusRemainder) {
            let matched = startsWith(mode.endRe, matchPlusRemainder);
            if (matched) {
              if (mode["on:end"]) {
                const resp = new Response(mode);
                mode["on:end"](match, resp);
                if (resp.isMatchIgnored) matched = false;
              }
              if (matched) {
                while (mode.endsParent && mode.parent) {
                  mode = mode.parent;
                }
                return mode;
              }
            }
            if (mode.endsWithParent) {
              return endOfMode(mode.parent, match, matchPlusRemainder);
            }
          }
          function doIgnore(lexeme) {
            if (top.matcher.regexIndex === 0) {
              modeBuffer += lexeme[0];
              return 1;
            } else {
              resumeScanAtSamePosition = true;
              return 0;
            }
          }
          function doBeginMatch(match) {
            const lexeme = match[0];
            const newMode = match.rule;
            const resp = new Response(newMode);
            const beforeCallbacks = [newMode.__beforeBegin, newMode["on:begin"]];
            for (const cb of beforeCallbacks) {
              if (!cb) continue;
              cb(match, resp);
              if (resp.isMatchIgnored) return doIgnore(lexeme);
            }
            if (newMode.skip) {
              modeBuffer += lexeme;
            } else {
              if (newMode.excludeBegin) {
                modeBuffer += lexeme;
              }
              processBuffer();
              if (!newMode.returnBegin && !newMode.excludeBegin) {
                modeBuffer = lexeme;
              }
            }
            startNewMode(newMode, match);
            return newMode.returnBegin ? 0 : lexeme.length;
          }
          function doEndMatch(match) {
            const lexeme = match[0];
            const matchPlusRemainder = codeToHighlight.substring(match.index);
            const endMode = endOfMode(top, match, matchPlusRemainder);
            if (!endMode) {
              return NO_MATCH;
            }
            const origin = top;
            if (top.endScope && top.endScope._wrap) {
              processBuffer();
              emitKeyword(lexeme, top.endScope._wrap);
            } else if (top.endScope && top.endScope._multi) {
              processBuffer();
              emitMultiClass(top.endScope, match);
            } else if (origin.skip) {
              modeBuffer += lexeme;
            } else {
              if (!(origin.returnEnd || origin.excludeEnd)) {
                modeBuffer += lexeme;
              }
              processBuffer();
              if (origin.excludeEnd) {
                modeBuffer = lexeme;
              }
            }
            do {
              if (top.scope) {
                emitter.closeNode();
              }
              if (!top.skip && !top.subLanguage) {
                relevance += top.relevance;
              }
              top = top.parent;
            } while (top !== endMode.parent);
            if (endMode.starts) {
              startNewMode(endMode.starts, match);
            }
            return origin.returnEnd ? 0 : lexeme.length;
          }
          function processContinuations() {
            const list = [];
            for (let current = top; current !== language; current = current.parent) {
              if (current.scope) {
                list.unshift(current.scope);
              }
            }
            list.forEach((item) => emitter.openNode(item));
          }
          let lastMatch = {};
          function processLexeme(textBeforeMatch, match) {
            const lexeme = match && match[0];
            modeBuffer += textBeforeMatch;
            if (lexeme == null) {
              processBuffer();
              return 0;
            }
            if (lastMatch.type === "begin" && match.type === "end" && lastMatch.index === match.index && lexeme === "") {
              modeBuffer += codeToHighlight.slice(match.index, match.index + 1);
              if (!SAFE_MODE) {
                const err = new Error(`0 width match regex (${languageName})`);
                err.languageName = languageName;
                err.badRule = lastMatch.rule;
                throw err;
              }
              return 1;
            }
            lastMatch = match;
            if (match.type === "begin") {
              return doBeginMatch(match);
            } else if (match.type === "illegal" && !ignoreIllegals) {
              const err = new Error('Illegal lexeme "' + lexeme + '" for mode "' + (top.scope || "<unnamed>") + '"');
              err.mode = top;
              throw err;
            } else if (match.type === "end") {
              const processed = doEndMatch(match);
              if (processed !== NO_MATCH) {
                return processed;
              }
            }
            if (match.type === "illegal" && lexeme === "") {
              if (match.index === codeToHighlight.length) ;
              else {
                modeBuffer += "\n";
              }
              return 1;
            }
            if (iterations > 1e5 && iterations > match.index * 3) {
              const err = new Error("potential infinite loop, way more iterations than matches");
              throw err;
            }
            modeBuffer += lexeme;
            return lexeme.length;
          }
          const language = getLanguage(languageName);
          if (!language) {
            error(LANGUAGE_NOT_FOUND.replace("{}", languageName));
            throw new Error('Unknown language: "' + languageName + '"');
          }
          const md = compileLanguage(language);
          let result = "";
          let top = continuation || md;
          const continuations = {};
          const emitter = new options.__emitter(options);
          processContinuations();
          let modeBuffer = "";
          let relevance = 0;
          let index = 0;
          let iterations = 0;
          let resumeScanAtSamePosition = false;
          try {
            if (!language.__emitTokens) {
              top.matcher.considerAll();
              for (; ; ) {
                iterations++;
                if (resumeScanAtSamePosition) {
                  resumeScanAtSamePosition = false;
                } else {
                  top.matcher.considerAll();
                }
                top.matcher.lastIndex = index;
                const match = top.matcher.exec(codeToHighlight);
                if (!match) break;
                const beforeMatch = codeToHighlight.substring(index, match.index);
                const processedCount = processLexeme(beforeMatch, match);
                index = match.index + processedCount;
              }
              processLexeme(codeToHighlight.substring(index));
            } else {
              language.__emitTokens(codeToHighlight, emitter);
            }
            emitter.finalize();
            result = emitter.toHTML();
            return {
              language: languageName,
              value: result,
              relevance,
              illegal: false,
              _emitter: emitter,
              _top: top
            };
          } catch (err) {
            if (err.message && err.message.includes("Illegal")) {
              return {
                language: languageName,
                value: escape(codeToHighlight),
                illegal: true,
                relevance: 0,
                _illegalBy: {
                  message: err.message,
                  index,
                  context: codeToHighlight.slice(index - 100, index + 100),
                  mode: err.mode,
                  resultSoFar: result
                },
                _emitter: emitter
              };
            } else if (SAFE_MODE) {
              return {
                language: languageName,
                value: escape(codeToHighlight),
                illegal: false,
                relevance: 0,
                errorRaised: err,
                _emitter: emitter,
                _top: top
              };
            } else {
              throw err;
            }
          }
        }
        function justTextHighlightResult(code) {
          const result = {
            value: escape(code),
            illegal: false,
            relevance: 0,
            _top: PLAINTEXT_LANGUAGE,
            _emitter: new options.__emitter(options)
          };
          result._emitter.addText(code);
          return result;
        }
        function highlightAuto2(code, languageSubset) {
          languageSubset = languageSubset || options.languages || Object.keys(languages);
          const plaintext2 = justTextHighlightResult(code);
          const results = languageSubset.filter(getLanguage).filter(autoDetection).map(
            (name) => _highlight(name, code, false)
          );
          results.unshift(plaintext2);
          const sorted = results.sort((a, b) => {
            if (a.relevance !== b.relevance) return b.relevance - a.relevance;
            if (a.language && b.language) {
              if (getLanguage(a.language).supersetOf === b.language) {
                return 1;
              } else if (getLanguage(b.language).supersetOf === a.language) {
                return -1;
              }
            }
            return 0;
          });
          const [best, secondBest] = sorted;
          const result = best;
          result.secondBest = secondBest;
          return result;
        }
        function updateClassName(element, currentLang, resultLang) {
          const language = currentLang && aliases[currentLang] || resultLang;
          element.classList.add("hljs");
          element.classList.add(`language-${language}`);
        }
        function highlightElement2(element) {
          let node = null;
          const language = blockLanguage(element);
          if (shouldNotHighlight(language)) return;
          fire(
            "before:highlightElement",
            { el: element, language }
          );
          if (element.dataset.highlighted) {
            console.log("Element previously highlighted. To highlight again, first unset `dataset.highlighted`.", element);
            return;
          }
          if (element.children.length > 0) {
            if (!options.ignoreUnescapedHTML) {
              console.warn("One of your code blocks includes unescaped HTML. This is a potentially serious security risk.");
              console.warn("https://github.com/highlightjs/highlight.js/wiki/security");
              console.warn("The element with unescaped HTML:");
              console.warn(element);
            }
            if (options.throwUnescapedHTML) {
              const err = new HTMLInjectionError(
                "One of your code blocks includes unescaped HTML.",
                element.innerHTML
              );
              throw err;
            }
          }
          node = element;
          const text = node.textContent;
          const result = language ? highlight3(text, { language, ignoreIllegals: true }) : highlightAuto2(text);
          element.innerHTML = result.value;
          element.dataset.highlighted = "yes";
          updateClassName(element, language, result.language);
          element.result = {
            language: result.language,
            // TODO: remove with version 11.0
            re: result.relevance,
            relevance: result.relevance
          };
          if (result.secondBest) {
            element.secondBest = {
              language: result.secondBest.language,
              relevance: result.secondBest.relevance
            };
          }
          fire("after:highlightElement", { el: element, result, text });
        }
        function configure(userOptions) {
          options = inherit(options, userOptions);
        }
        const initHighlighting = () => {
          highlightAll2();
          deprecated("10.6.0", "initHighlighting() deprecated.  Use highlightAll() now.");
        };
        function initHighlightingOnLoad() {
          highlightAll2();
          deprecated("10.6.0", "initHighlightingOnLoad() deprecated.  Use highlightAll() now.");
        }
        let wantsHighlight = false;
        function highlightAll2() {
          function boot() {
            highlightAll2();
          }
          if (document.readyState === "loading") {
            if (!wantsHighlight) {
              window.addEventListener("DOMContentLoaded", boot, false);
            }
            wantsHighlight = true;
            return;
          }
          const blocks = document.querySelectorAll(options.cssSelector);
          blocks.forEach(highlightElement2);
        }
        function registerLanguage2(languageName, languageDefinition) {
          let lang = null;
          try {
            lang = languageDefinition(hljs3);
          } catch (error$1) {
            error("Language definition for '{}' could not be registered.".replace("{}", languageName));
            if (!SAFE_MODE) {
              throw error$1;
            } else {
              error(error$1);
            }
            lang = PLAINTEXT_LANGUAGE;
          }
          if (!lang.name) lang.name = languageName;
          languages[languageName] = lang;
          lang.rawDefinition = languageDefinition.bind(null, hljs3);
          if (lang.aliases) {
            registerAliases(lang.aliases, { languageName });
          }
        }
        function unregisterLanguage(languageName) {
          delete languages[languageName];
          for (const alias of Object.keys(aliases)) {
            if (aliases[alias] === languageName) {
              delete aliases[alias];
            }
          }
        }
        function listLanguages() {
          return Object.keys(languages);
        }
        function getLanguage(name) {
          name = (name || "").toLowerCase();
          return languages[name] || languages[aliases[name]];
        }
        function registerAliases(aliasList, { languageName }) {
          if (typeof aliasList === "string") {
            aliasList = [aliasList];
          }
          aliasList.forEach((alias) => {
            aliases[alias.toLowerCase()] = languageName;
          });
        }
        function autoDetection(name) {
          const lang = getLanguage(name);
          return lang && !lang.disableAutodetect;
        }
        function upgradePluginAPI(plugin) {
          if (plugin["before:highlightBlock"] && !plugin["before:highlightElement"]) {
            plugin["before:highlightElement"] = (data) => {
              plugin["before:highlightBlock"](
                Object.assign({ block: data.el }, data)
              );
            };
          }
          if (plugin["after:highlightBlock"] && !plugin["after:highlightElement"]) {
            plugin["after:highlightElement"] = (data) => {
              plugin["after:highlightBlock"](
                Object.assign({ block: data.el }, data)
              );
            };
          }
        }
        function addPlugin2(plugin) {
          upgradePluginAPI(plugin);
          plugins.push(plugin);
        }
        function removePlugin(plugin) {
          const index = plugins.indexOf(plugin);
          if (index !== -1) {
            plugins.splice(index, 1);
          }
        }
        function fire(event, args) {
          const cb = event;
          plugins.forEach(function(plugin) {
            if (plugin[cb]) {
              plugin[cb](args);
            }
          });
        }
        function deprecateHighlightBlock(el) {
          deprecated("10.7.0", "highlightBlock will be removed entirely in v12.0");
          deprecated("10.7.0", "Please use highlightElement now.");
          return highlightElement2(el);
        }
        Object.assign(hljs3, {
          highlight: highlight3,
          highlightAuto: highlightAuto2,
          highlightAll: highlightAll2,
          highlightElement: highlightElement2,
          // TODO: Remove with v12 API
          highlightBlock: deprecateHighlightBlock,
          configure,
          initHighlighting,
          initHighlightingOnLoad,
          registerLanguage: registerLanguage2,
          unregisterLanguage,
          listLanguages,
          getLanguage,
          registerAliases,
          autoDetection,
          inherit,
          addPlugin: addPlugin2,
          removePlugin
        });
        hljs3.debugMode = function() {
          SAFE_MODE = false;
        };
        hljs3.safeMode = function() {
          SAFE_MODE = true;
        };
        hljs3.versionString = version;
        hljs3.regex = {
          concat,
          lookahead,
          either,
          optional,
          anyNumberOfTimes
        };
        for (const key in MODES$1) {
          if (typeof MODES$1[key] === "object") {
            deepFreeze(MODES$1[key]);
          }
        }
        Object.assign(hljs3, MODES$1);
        return hljs3;
      };
      const highlight2 = HLJS({});
      highlight2.newInstance = () => HLJS({});
      function ruby(hljs3) {
        const regex = hljs3.regex;
        const RUBY_METHOD_RE = "([a-zA-Z_]\\w*[!?=]?|[-+~]@|<<|>>|=~|===?|<=>|[<>]=?|\\*\\*|[-/+%^&*~`|]|\\[\\]=?)";
        const CLASS_NAME_RE = regex.either(
          /\b([A-Z]+[a-z0-9]+)+/,
          // ends in caps
          /\b([A-Z]+[a-z0-9]+)+[A-Z]+/
        );
        const CLASS_NAME_WITH_NAMESPACE_RE = regex.concat(CLASS_NAME_RE, /(::\w+)*/);
        const PSEUDO_KWS = [
          "include",
          "extend",
          "prepend",
          "public",
          "private",
          "protected",
          "raise",
          "throw"
        ];
        const RUBY_KEYWORDS = {
          "variable.constant": [
            "__FILE__",
            "__LINE__",
            "__ENCODING__"
          ],
          "variable.language": [
            "self",
            "super"
          ],
          keyword: [
            "alias",
            "and",
            "begin",
            "BEGIN",
            "break",
            "case",
            "class",
            "defined",
            "do",
            "else",
            "elsif",
            "end",
            "END",
            "ensure",
            "for",
            "if",
            "in",
            "module",
            "next",
            "not",
            "or",
            "redo",
            "require",
            "rescue",
            "retry",
            "return",
            "then",
            "undef",
            "unless",
            "until",
            "when",
            "while",
            "yield",
            ...PSEUDO_KWS
          ],
          built_in: [
            "proc",
            "lambda",
            "attr_accessor",
            "attr_reader",
            "attr_writer",
            "define_method",
            "private_constant",
            "module_function"
          ],
          literal: [
            "true",
            "false",
            "nil"
          ]
        };
        const YARDOCTAG = {
          className: "doctag",
          begin: "@[A-Za-z]+"
        };
        const IRB_OBJECT = {
          begin: "#<",
          end: ">"
        };
        const COMMENT_MODES = [
          hljs3.COMMENT(
            "#",
            "$",
            { contains: [YARDOCTAG] }
          ),
          hljs3.COMMENT(
            "^=begin",
            "^=end",
            {
              contains: [YARDOCTAG],
              relevance: 10
            }
          ),
          hljs3.COMMENT("^__END__", hljs3.MATCH_NOTHING_RE)
        ];
        const SUBST = {
          className: "subst",
          begin: /#\{/,
          end: /\}/,
          keywords: RUBY_KEYWORDS
        };
        const STRING = {
          className: "string",
          contains: [
            hljs3.BACKSLASH_ESCAPE,
            SUBST
          ],
          variants: [
            {
              begin: /'/,
              end: /'/
            },
            {
              begin: /"/,
              end: /"/
            },
            {
              begin: /`/,
              end: /`/
            },
            {
              begin: /%[qQwWx]?\(/,
              end: /\)/
            },
            {
              begin: /%[qQwWx]?\[/,
              end: /\]/
            },
            {
              begin: /%[qQwWx]?\{/,
              end: /\}/
            },
            {
              begin: /%[qQwWx]?</,
              end: />/
            },
            {
              begin: /%[qQwWx]?\//,
              end: /\//
            },
            {
              begin: /%[qQwWx]?%/,
              end: /%/
            },
            {
              begin: /%[qQwWx]?-/,
              end: /-/
            },
            {
              begin: /%[qQwWx]?\|/,
              end: /\|/
            },
            // in the following expressions, \B in the beginning suppresses recognition of ?-sequences
            // where ? is the last character of a preceding identifier, as in: `func?4`
            { begin: /\B\?(\\\d{1,3})/ },
            { begin: /\B\?(\\x[A-Fa-f0-9]{1,2})/ },
            { begin: /\B\?(\\u\{?[A-Fa-f0-9]{1,6}\}?)/ },
            { begin: /\B\?(\\M-\\C-|\\M-\\c|\\c\\M-|\\M-|\\C-\\M-)[\x20-\x7e]/ },
            { begin: /\B\?\\(c|C-)[\x20-\x7e]/ },
            { begin: /\B\?\\?\S/ },
            // heredocs
            {
              // this guard makes sure that we have an entire heredoc and not a false
              // positive (auto-detect, etc.)
              begin: regex.concat(
                /<<[-~]?'?/,
                regex.lookahead(/(\w+)(?=\W)[^\n]*\n(?:[^\n]*\n)*?\s*\1\b/)
              ),
              contains: [
                hljs3.END_SAME_AS_BEGIN({
                  begin: /(\w+)/,
                  end: /(\w+)/,
                  contains: [
                    hljs3.BACKSLASH_ESCAPE,
                    SUBST
                  ]
                })
              ]
            }
          ]
        };
        const decimal = "[1-9](_?[0-9])*|0";
        const digits = "[0-9](_?[0-9])*";
        const NUMBER = {
          className: "number",
          relevance: 0,
          variants: [
            // decimal integer/float, optionally exponential or rational, optionally imaginary
            { begin: `\\b(${decimal})(\\.(${digits}))?([eE][+-]?(${digits})|r)?i?\\b` },
            // explicit decimal/binary/octal/hexadecimal integer,
            // optionally rational and/or imaginary
            { begin: "\\b0[dD][0-9](_?[0-9])*r?i?\\b" },
            { begin: "\\b0[bB][0-1](_?[0-1])*r?i?\\b" },
            { begin: "\\b0[oO][0-7](_?[0-7])*r?i?\\b" },
            { begin: "\\b0[xX][0-9a-fA-F](_?[0-9a-fA-F])*r?i?\\b" },
            // 0-prefixed implicit octal integer, optionally rational and/or imaginary
            { begin: "\\b0(_?[0-7])+r?i?\\b" }
          ]
        };
        const PARAMS = {
          variants: [
            {
              match: /\(\)/
            },
            {
              className: "params",
              begin: /\(/,
              end: /(?=\))/,
              excludeBegin: true,
              endsParent: true,
              keywords: RUBY_KEYWORDS
            }
          ]
        };
        const INCLUDE_EXTEND = {
          match: [
            /(include|extend)\s+/,
            CLASS_NAME_WITH_NAMESPACE_RE
          ],
          scope: {
            2: "title.class"
          },
          keywords: RUBY_KEYWORDS
        };
        const CLASS_DEFINITION = {
          variants: [
            {
              match: [
                /class\s+/,
                CLASS_NAME_WITH_NAMESPACE_RE,
                /\s+<\s+/,
                CLASS_NAME_WITH_NAMESPACE_RE
              ]
            },
            {
              match: [
                /\b(class|module)\s+/,
                CLASS_NAME_WITH_NAMESPACE_RE
              ]
            }
          ],
          scope: {
            2: "title.class",
            4: "title.class.inherited"
          },
          keywords: RUBY_KEYWORDS
        };
        const UPPER_CASE_CONSTANT = {
          relevance: 0,
          match: /\b[A-Z][A-Z_0-9]+\b/,
          className: "variable.constant"
        };
        const METHOD_DEFINITION = {
          match: [
            /def/,
            /\s+/,
            RUBY_METHOD_RE
          ],
          scope: {
            1: "keyword",
            3: "title.function"
          },
          contains: [
            PARAMS
          ]
        };
        const OBJECT_CREATION = {
          relevance: 0,
          match: [
            CLASS_NAME_WITH_NAMESPACE_RE,
            /\.new[. (]/
          ],
          scope: {
            1: "title.class"
          }
        };
        const CLASS_REFERENCE = {
          relevance: 0,
          match: CLASS_NAME_RE,
          scope: "title.class"
        };
        const RUBY_DEFAULT_CONTAINS = [
          STRING,
          CLASS_DEFINITION,
          INCLUDE_EXTEND,
          OBJECT_CREATION,
          UPPER_CASE_CONSTANT,
          CLASS_REFERENCE,
          METHOD_DEFINITION,
          {
            // swallow the scope resolution operator so `::` is not read as a symbol
            begin: "::"
          },
          {
            className: "symbol",
            begin: hljs3.UNDERSCORE_IDENT_RE + "(!|\\?)?:",
            relevance: 0
          },
          {
            className: "symbol",
            begin: ":(?!\\s)",
            contains: [
              STRING,
              { begin: RUBY_METHOD_RE }
            ],
            relevance: 0
          },
          NUMBER,
          {
            // negative-look forward attempts to prevent false matches like:
            // @ident@ or $ident$ that might indicate this is not ruby at all
            className: "variable",
            begin: `(\\$\\W)|((\\$|@@?)(\\w+))(?=[^@$?])(?![A-Za-z])(?![@$?'])`
          },
          {
            className: "params",
            begin: /\|(?!=)/,
            end: /\|/,
            excludeBegin: true,
            excludeEnd: true,
            relevance: 0,
            // this could be a lot of things (in other languages) other than params
            keywords: RUBY_KEYWORDS
          },
          {
            // regexp container
            begin: "(" + hljs3.RE_STARTERS_RE + "|unless)\\s*",
            keywords: "unless",
            contains: [
              {
                className: "regexp",
                contains: [
                  hljs3.BACKSLASH_ESCAPE,
                  SUBST
                ],
                illegal: /\n/,
                variants: [
                  {
                    begin: "/",
                    end: "/[a-z]*"
                  },
                  {
                    begin: /%r\{/,
                    end: /\}[a-z]*/
                  },
                  {
                    begin: "%r\\(",
                    end: "\\)[a-z]*"
                  },
                  {
                    begin: "%r!",
                    end: "![a-z]*"
                  },
                  {
                    begin: "%r\\[",
                    end: "\\][a-z]*"
                  }
                ]
              }
            ].concat(IRB_OBJECT, COMMENT_MODES),
            relevance: 0
          }
        ].concat(IRB_OBJECT, COMMENT_MODES);
        SUBST.contains = RUBY_DEFAULT_CONTAINS;
        PARAMS.contains = RUBY_DEFAULT_CONTAINS;
        const SIMPLE_PROMPT = "[>?]>";
        const DEFAULT_PROMPT = "[\\w#]+\\(\\w+\\):\\d+:\\d+[>*]";
        const RVM_PROMPT = "(\\w+-)?\\d+\\.\\d+\\.\\d+(p\\d+)?[^\\d][^>]+>";
        const IRB_DEFAULT = [
          {
            begin: /^\s*=>/,
            starts: {
              end: "$",
              contains: RUBY_DEFAULT_CONTAINS
            }
          },
          {
            className: "meta.prompt",
            begin: "^(" + SIMPLE_PROMPT + "|" + DEFAULT_PROMPT + "|" + RVM_PROMPT + ")(?=[ ])",
            starts: {
              end: "$",
              keywords: RUBY_KEYWORDS,
              contains: RUBY_DEFAULT_CONTAINS
            }
          }
        ];
        COMMENT_MODES.unshift(IRB_OBJECT);
        return {
          name: "Ruby",
          aliases: [
            "rb",
            "gemspec",
            "podspec",
            "thor",
            "irb"
          ],
          keywords: RUBY_KEYWORDS,
          illegal: /\/\*/,
          contains: [hljs3.SHEBANG({ binary: "ruby" })].concat(IRB_DEFAULT).concat(COMMENT_MODES).concat(RUBY_DEFAULT_CONTAINS)
        };
      }
      function yaml(hljs3) {
        const LITERALS2 = "true false yes no null";
        const URI_CHARACTERS = "[\\w#;/?:@&=+$,.~*'()[\\]]+";
        const KEY = {
          className: "attr",
          variants: [
            // added brackets support and special char support
            { begin: /[\w*@][\w*@ :()\./-]*:(?=[ \t]|$)/ },
            {
              // double quoted keys - with brackets and special char support
              begin: /"[\w*@][\w*@ :()\./-]*":(?=[ \t]|$)/
            },
            {
              // single quoted keys - with brackets and special char support
              begin: /'[\w*@][\w*@ :()\./-]*':(?=[ \t]|$)/
            }
          ]
        };
        const TEMPLATE_VARIABLES = {
          className: "template-variable",
          variants: [
            {
              // jinja templates Ansible
              begin: /\{\{/,
              end: /\}\}/
            },
            {
              // Ruby i18n
              begin: /%\{/,
              end: /\}/
            }
          ]
        };
        const SINGLE_QUOTE_STRING = {
          className: "string",
          relevance: 0,
          begin: /'/,
          end: /'/,
          contains: [
            {
              match: /''/,
              scope: "char.escape",
              relevance: 0
            }
          ]
        };
        const STRING = {
          className: "string",
          relevance: 0,
          variants: [
            {
              begin: /"/,
              end: /"/
            },
            { begin: /\S+/ }
          ],
          contains: [
            hljs3.BACKSLASH_ESCAPE,
            TEMPLATE_VARIABLES
          ]
        };
        const CONTAINER_STRING = hljs3.inherit(STRING, { variants: [
          {
            begin: /'/,
            end: /'/,
            contains: [
              {
                begin: /''/,
                relevance: 0
              }
            ]
          },
          {
            begin: /"/,
            end: /"/
          },
          { begin: /[^\s,{}[\]]+/ }
        ] });
        const DATE_RE = "[0-9]{4}(-[0-9][0-9]){0,2}";
        const TIME_RE = "([Tt \\t][0-9][0-9]?(:[0-9][0-9]){2})?";
        const FRACTION_RE = "(\\.[0-9]*)?";
        const ZONE_RE = "([ \\t])*(Z|[-+][0-9][0-9]?(:[0-9][0-9])?)?";
        const TIMESTAMP = {
          className: "number",
          begin: "\\b" + DATE_RE + TIME_RE + FRACTION_RE + ZONE_RE + "\\b"
        };
        const VALUE_CONTAINER = {
          end: ",",
          endsWithParent: true,
          excludeEnd: true,
          keywords: LITERALS2,
          relevance: 0
        };
        const OBJECT = {
          begin: /\{/,
          end: /\}/,
          contains: [VALUE_CONTAINER],
          illegal: "\\n",
          relevance: 0
        };
        const ARRAY = {
          begin: "\\[",
          end: "\\]",
          contains: [VALUE_CONTAINER],
          illegal: "\\n",
          relevance: 0
        };
        const MODES2 = [
          KEY,
          {
            className: "meta",
            begin: "^---\\s*$",
            relevance: 10
          },
          {
            // multi line string
            // Blocks start with a | or > followed by a newline
            //
            // Indentation of subsequent lines must be the same to
            // be considered part of the block
            className: "string",
            begin: "[\\|>]([1-9]?[+-])?[ ]*\\n( +)[^ ][^\\n]*\\n(\\2[^\\n]+\\n?)*"
          },
          {
            // Ruby/Rails erb
            begin: "<%[%=-]?",
            end: "[%-]?%>",
            subLanguage: "ruby",
            excludeBegin: true,
            excludeEnd: true,
            relevance: 0
          },
          {
            // named tags
            className: "type",
            begin: "!\\w+!" + URI_CHARACTERS
          },
          // https://yaml.org/spec/1.2/spec.html#id2784064
          {
            // verbatim tags
            className: "type",
            begin: "!<" + URI_CHARACTERS + ">"
          },
          {
            // primary tags
            className: "type",
            begin: "!" + URI_CHARACTERS
          },
          {
            // secondary tags
            className: "type",
            begin: "!!" + URI_CHARACTERS
          },
          {
            // fragment id &ref
            className: "meta",
            begin: "&" + hljs3.UNDERSCORE_IDENT_RE + "$"
          },
          {
            // fragment reference *ref
            className: "meta",
            begin: "\\*" + hljs3.UNDERSCORE_IDENT_RE + "$"
          },
          {
            // array listing
            className: "bullet",
            // TODO: remove |$ hack when we have proper look-ahead support
            begin: "-(?=[ ]|$)",
            relevance: 0
          },
          hljs3.HASH_COMMENT_MODE,
          {
            beginKeywords: LITERALS2,
            keywords: { literal: LITERALS2 }
          },
          TIMESTAMP,
          // numbers are any valid C-style number that
          // sit isolated from other words
          {
            className: "number",
            begin: hljs3.C_NUMBER_RE + "\\b",
            relevance: 0
          },
          OBJECT,
          ARRAY,
          SINGLE_QUOTE_STRING,
          STRING
        ];
        const VALUE_MODES = [...MODES2];
        VALUE_MODES.pop();
        VALUE_MODES.push(CONTAINER_STRING);
        VALUE_CONTAINER.contains = VALUE_MODES;
        return {
          name: "YAML",
          case_insensitive: true,
          aliases: ["yml"],
          contains: MODES2
        };
      }
      function xml(hljs3) {
        const regex = hljs3.regex;
        const TAG_NAME_RE = regex.concat(/[\p{L}_]/u, regex.optional(/[\p{L}0-9_.-]*:/u), /[\p{L}0-9_.-]*/u);
        const XML_IDENT_RE = /[\p{L}0-9._:-]+/u;
        const XML_ENTITIES = {
          className: "symbol",
          begin: /&[a-z]+;|&#[0-9]+;|&#x[a-f0-9]+;/
        };
        const XML_META_KEYWORDS = {
          begin: /\s/,
          contains: [
            {
              className: "keyword",
              begin: /#?[a-z_][a-z1-9_-]+/,
              illegal: /\n/
            }
          ]
        };
        const XML_META_PAR_KEYWORDS = hljs3.inherit(XML_META_KEYWORDS, {
          begin: /\(/,
          end: /\)/
        });
        const APOS_META_STRING_MODE = hljs3.inherit(hljs3.APOS_STRING_MODE, { className: "string" });
        const QUOTE_META_STRING_MODE = hljs3.inherit(hljs3.QUOTE_STRING_MODE, { className: "string" });
        const TAG_INTERNALS = {
          endsWithParent: true,
          illegal: /</,
          relevance: 0,
          contains: [
            {
              className: "attr",
              begin: XML_IDENT_RE,
              relevance: 0
            },
            {
              begin: /=\s*/,
              relevance: 0,
              contains: [
                {
                  className: "string",
                  endsParent: true,
                  variants: [
                    {
                      begin: /"/,
                      end: /"/,
                      contains: [XML_ENTITIES]
                    },
                    {
                      begin: /'/,
                      end: /'/,
                      contains: [XML_ENTITIES]
                    },
                    { begin: /[^\s"'=<>`]+/ }
                  ]
                }
              ]
            }
          ]
        };
        return {
          name: "HTML, XML",
          aliases: [
            "html",
            "xhtml",
            "rss",
            "atom",
            "xjb",
            "xsd",
            "xsl",
            "plist",
            "wsf",
            "svg"
          ],
          case_insensitive: true,
          unicodeRegex: true,
          contains: [
            {
              className: "meta",
              begin: /<![a-z]/,
              end: />/,
              relevance: 10,
              contains: [
                XML_META_KEYWORDS,
                QUOTE_META_STRING_MODE,
                APOS_META_STRING_MODE,
                XML_META_PAR_KEYWORDS,
                {
                  begin: /\[/,
                  end: /\]/,
                  contains: [
                    {
                      className: "meta",
                      begin: /<![a-z]/,
                      end: />/,
                      contains: [
                        XML_META_KEYWORDS,
                        XML_META_PAR_KEYWORDS,
                        QUOTE_META_STRING_MODE,
                        APOS_META_STRING_MODE
                      ]
                    }
                  ]
                }
              ]
            },
            hljs3.COMMENT(
              /<!--/,
              /-->/,
              { relevance: 10 }
            ),
            {
              begin: /<!\[CDATA\[/,
              end: /\]\]>/,
              relevance: 10
            },
            XML_ENTITIES,
            // xml processing instructions
            {
              className: "meta",
              end: /\?>/,
              variants: [
                {
                  begin: /<\?xml/,
                  relevance: 10,
                  contains: [
                    QUOTE_META_STRING_MODE
                  ]
                },
                {
                  begin: /<\?[a-z][a-z0-9]+/
                }
              ]
            },
            {
              className: "tag",
              /*
              The lookahead pattern (?=...) ensures that 'begin' only matches
              '<style' as a single word, followed by a whitespace or an
              ending bracket.
              */
              begin: /<style(?=\s|>)/,
              end: />/,
              keywords: { name: "style" },
              contains: [TAG_INTERNALS],
              starts: {
                end: /<\/style>/,
                returnEnd: true,
                subLanguage: "css"
              }
            },
            {
              className: "tag",
              // See the comment in the <style tag about the lookahead pattern
              begin: /<script(?=\s|>)/,
              end: />/,
              keywords: { name: "script" },
              contains: [TAG_INTERNALS],
              starts: {
                end: /<\/script>/,
                returnEnd: true,
                subLanguage: "javascript"
              }
            },
            // we need this for now for jSX
            {
              className: "tag",
              begin: /<>|<\/>/
            },
            // open tag
            {
              className: "tag",
              begin: regex.concat(
                /</,
                regex.lookahead(regex.concat(
                  TAG_NAME_RE,
                  // <tag/>
                  // <tag>
                  // <tag ...
                  regex.either(/\/>/, />/, /\s/)
                ))
              ),
              end: /\/?>/,
              contains: [
                {
                  className: "name",
                  begin: TAG_NAME_RE,
                  relevance: 0,
                  starts: TAG_INTERNALS
                }
              ]
            },
            // close tag
            {
              className: "tag",
              begin: regex.concat(
                /<\//,
                regex.lookahead(regex.concat(
                  TAG_NAME_RE,
                  />/
                ))
              ),
              contains: [
                {
                  className: "name",
                  begin: TAG_NAME_RE,
                  relevance: 0
                },
                {
                  begin: />/,
                  relevance: 0,
                  endsParent: true
                }
              ]
            }
          ]
        };
      }
      function wasm(hljs3) {
        hljs3.regex;
        const BLOCK_COMMENT = hljs3.COMMENT(/\(;/, /;\)/);
        BLOCK_COMMENT.contains.push("self");
        const LINE_COMMENT = hljs3.COMMENT(/;;/, /$/);
        const KWS = [
          "anyfunc",
          "block",
          "br",
          "br_if",
          "br_table",
          "call",
          "call_indirect",
          "data",
          "drop",
          "elem",
          "else",
          "end",
          "export",
          "func",
          "global.get",
          "global.set",
          "local.get",
          "local.set",
          "local.tee",
          "get_global",
          "get_local",
          "global",
          "if",
          "import",
          "local",
          "loop",
          "memory",
          "memory.grow",
          "memory.size",
          "module",
          "mut",
          "nop",
          "offset",
          "param",
          "result",
          "return",
          "select",
          "set_global",
          "set_local",
          "start",
          "table",
          "tee_local",
          "then",
          "type",
          "unreachable"
        ];
        const FUNCTION_REFERENCE = {
          begin: [
            /(?:func|call|call_indirect)/,
            /\s+/,
            /\$[^\s)]+/
          ],
          className: {
            1: "keyword",
            3: "title.function"
          }
        };
        const ARGUMENT = {
          className: "variable",
          begin: /\$[\w_]+/
        };
        const PARENS = {
          match: /(\((?!;)|\))+/,
          className: "punctuation",
          relevance: 0
        };
        const NUMBER = {
          className: "number",
          relevance: 0,
          // borrowed from Prism, TODO: split out into variants
          match: /[+-]?\b(?:\d(?:_?\d)*(?:\.\d(?:_?\d)*)?(?:[eE][+-]?\d(?:_?\d)*)?|0x[\da-fA-F](?:_?[\da-fA-F])*(?:\.[\da-fA-F](?:_?[\da-fA-D])*)?(?:[pP][+-]?\d(?:_?\d)*)?)\b|\binf\b|\bnan(?::0x[\da-fA-F](?:_?[\da-fA-D])*)?\b/
        };
        const TYPE = {
          // look-ahead prevents us from gobbling up opcodes
          match: /(i32|i64|f32|f64)(?!\.)/,
          className: "type"
        };
        const MATH_OPERATIONS = {
          className: "keyword",
          // borrowed from Prism, TODO: split out into variants
          match: /\b(f32|f64|i32|i64)(?:\.(?:abs|add|and|ceil|clz|const|convert_[su]\/i(?:32|64)|copysign|ctz|demote\/f64|div(?:_[su])?|eqz?|extend_[su]\/i32|floor|ge(?:_[su])?|gt(?:_[su])?|le(?:_[su])?|load(?:(?:8|16|32)_[su])?|lt(?:_[su])?|max|min|mul|nearest|neg?|or|popcnt|promote\/f32|reinterpret\/[fi](?:32|64)|rem_[su]|rot[lr]|shl|shr_[su]|store(?:8|16|32)?|sqrt|sub|trunc(?:_[su]\/f(?:32|64))?|wrap\/i64|xor))\b/
        };
        const OFFSET_ALIGN = {
          match: [
            /(?:offset|align)/,
            /\s*/,
            /=/
          ],
          className: {
            1: "keyword",
            3: "operator"
          }
        };
        return {
          name: "WebAssembly",
          keywords: {
            $pattern: /[\w.]+/,
            keyword: KWS
          },
          contains: [
            LINE_COMMENT,
            BLOCK_COMMENT,
            OFFSET_ALIGN,
            ARGUMENT,
            PARENS,
            FUNCTION_REFERENCE,
            hljs3.QUOTE_STRING_MODE,
            TYPE,
            MATH_OPERATIONS,
            NUMBER
          ]
        };
      }
      function vbnet(hljs3) {
        const regex = hljs3.regex;
        const CHARACTER = {
          className: "string",
          begin: /"(""|[^/n])"C\b/
        };
        const STRING = {
          className: "string",
          begin: /"/,
          end: /"/,
          illegal: /\n/,
          contains: [
            {
              // double quote escape
              begin: /""/
            }
          ]
        };
        const MM_DD_YYYY = /\d{1,2}\/\d{1,2}\/\d{4}/;
        const YYYY_MM_DD = /\d{4}-\d{1,2}-\d{1,2}/;
        const TIME_12H = /(\d|1[012])(:\d+){0,2} *(AM|PM)/;
        const TIME_24H = /\d{1,2}(:\d{1,2}){1,2}/;
        const DATE = {
          className: "literal",
          variants: [
            {
              // #YYYY-MM-DD# (ISO-Date) or #M/D/YYYY# (US-Date)
              begin: regex.concat(/# */, regex.either(YYYY_MM_DD, MM_DD_YYYY), / *#/)
            },
            {
              // #H:mm[:ss]# (24h Time)
              begin: regex.concat(/# */, TIME_24H, / *#/)
            },
            {
              // #h[:mm[:ss]] A# (12h Time)
              begin: regex.concat(/# */, TIME_12H, / *#/)
            },
            {
              // date plus time
              begin: regex.concat(
                /# */,
                regex.either(YYYY_MM_DD, MM_DD_YYYY),
                / +/,
                regex.either(TIME_12H, TIME_24H),
                / *#/
              )
            }
          ]
        };
        const NUMBER = {
          className: "number",
          relevance: 0,
          variants: [
            {
              // Float
              begin: /\b\d[\d_]*((\.[\d_]+(E[+-]?[\d_]+)?)|(E[+-]?[\d_]+))[RFD@!#]?/
            },
            {
              // Integer (base 10)
              begin: /\b\d[\d_]*((U?[SIL])|[%&])?/
            },
            {
              // Integer (base 16)
              begin: /&H[\dA-F_]+((U?[SIL])|[%&])?/
            },
            {
              // Integer (base 8)
              begin: /&O[0-7_]+((U?[SIL])|[%&])?/
            },
            {
              // Integer (base 2)
              begin: /&B[01_]+((U?[SIL])|[%&])?/
            }
          ]
        };
        const LABEL = {
          className: "label",
          begin: /^\w+:/
        };
        const DOC_COMMENT = hljs3.COMMENT(/'''/, /$/, { contains: [
          {
            className: "doctag",
            begin: /<\/?/,
            end: />/
          }
        ] });
        const COMMENT2 = hljs3.COMMENT(null, /$/, { variants: [
          { begin: /'/ },
          {
            // TODO: Use multi-class for leading spaces
            begin: /([\t ]|^)REM(?=\s)/
          }
        ] });
        const DIRECTIVES = {
          className: "meta",
          // TODO: Use multi-class for indentation once available
          begin: /[\t ]*#(const|disable|else|elseif|enable|end|externalsource|if|region)\b/,
          end: /$/,
          keywords: { keyword: "const disable else elseif enable end externalsource if region then" },
          contains: [COMMENT2]
        };
        return {
          name: "Visual Basic .NET",
          aliases: ["vb"],
          case_insensitive: true,
          classNameAliases: { label: "symbol" },
          keywords: {
            keyword: "addhandler alias aggregate ansi as async assembly auto binary by byref byval call case catch class compare const continue custom declare default delegate dim distinct do each equals else elseif end enum erase error event exit explicit finally for friend from function get global goto group handles if implements imports in inherits interface into iterator join key let lib loop me mid module mustinherit mustoverride mybase myclass namespace narrowing new next notinheritable notoverridable of off on operator option optional order overloads overridable overrides paramarray partial preserve private property protected public raiseevent readonly redim removehandler resume return select set shadows shared skip static step stop structure strict sub synclock take text then throw to try unicode until using when where while widening with withevents writeonly yield",
            built_in: (
              // Operators https://docs.microsoft.com/dotnet/visual-basic/language-reference/operators
              "addressof and andalso await directcast gettype getxmlnamespace is isfalse isnot istrue like mod nameof new not or orelse trycast typeof xor cbool cbyte cchar cdate cdbl cdec cint clng cobj csbyte cshort csng cstr cuint culng cushort"
            ),
            type: (
              // Data types https://docs.microsoft.com/dotnet/visual-basic/language-reference/data-types
              "boolean byte char date decimal double integer long object sbyte short single string uinteger ulong ushort"
            ),
            literal: "true false nothing"
          },
          illegal: "//|\\{|\\}|endif|gosub|variant|wend|^\\$ ",
          contains: [
            CHARACTER,
            STRING,
            DATE,
            NUMBER,
            LABEL,
            DOC_COMMENT,
            COMMENT2,
            DIRECTIVES
          ]
        };
      }
      const IDENT_RE = "[A-Za-z$_][0-9A-Za-z$_]*";
      const EXTENDED_NUMBER_RE = "([-+]?)(\\b0[xX][a-fA-F0-9]+|(\\b\\d+(\\.\\d*)?|\\.\\d+)([eE][-+]?\\d+)?)|NaN|[-+]?Infinity";
      const EXTENDED_NUMBER_MODE = {
        scope: "number",
        match: EXTENDED_NUMBER_RE,
        relevance: 0
      };
      const KEYWORDS = [
        "as",
        // for exports
        "in",
        "of",
        "if",
        "for",
        "while",
        "finally",
        "var",
        "new",
        "function",
        "do",
        "return",
        "void",
        "else",
        "break",
        "catch",
        "instanceof",
        "with",
        "throw",
        "case",
        "default",
        "try",
        "switch",
        "continue",
        "typeof",
        "delete",
        "let",
        "yield",
        "const",
        "class",
        // JS handles these with a special rule
        // "get",
        // "set",
        "debugger",
        "async",
        "await",
        "static",
        "import",
        "from",
        "export",
        "extends",
        // It's reached stage 3, which is "recommended for implementation":
        "using"
      ];
      const LITERALS = [
        "true",
        "false",
        "null",
        "undefined",
        "NaN",
        "Infinity"
      ];
      const TYPES = [
        // Fundamental objects
        "Object",
        "Function",
        "Boolean",
        "Symbol",
        // numbers and dates
        "Math",
        "Date",
        "Number",
        "BigInt",
        // text
        "String",
        "RegExp",
        // Indexed collections
        "Array",
        "Float32Array",
        "Float64Array",
        "Int8Array",
        "Uint8Array",
        "Uint8ClampedArray",
        "Int16Array",
        "Int32Array",
        "Uint16Array",
        "Uint32Array",
        "BigInt64Array",
        "BigUint64Array",
        // Keyed collections
        "Set",
        "Map",
        "WeakSet",
        "WeakMap",
        // Structured data
        "ArrayBuffer",
        "SharedArrayBuffer",
        "Atomics",
        "DataView",
        "JSON",
        // Control abstraction objects
        "Promise",
        "Generator",
        "GeneratorFunction",
        "AsyncFunction",
        // Reflection
        "Reflect",
        "Proxy",
        // Internationalization
        "Intl",
        // WebAssembly
        "WebAssembly"
      ];
      const ERROR_TYPES = [
        "Error",
        "EvalError",
        "InternalError",
        "RangeError",
        "ReferenceError",
        "SyntaxError",
        "TypeError",
        "URIError"
      ];
      const BUILT_IN_GLOBALS = [
        "setInterval",
        "setTimeout",
        "clearInterval",
        "clearTimeout",
        "require",
        "exports",
        "eval",
        "isFinite",
        "isNaN",
        "parseFloat",
        "parseInt",
        "decodeURI",
        "decodeURIComponent",
        "encodeURI",
        "encodeURIComponent",
        "escape",
        "unescape"
      ];
      const BUILT_IN_VARIABLES = [
        "arguments",
        "this",
        "super",
        "console",
        "window",
        "document",
        "localStorage",
        "sessionStorage",
        "module",
        "self",
        "global"
        // Node.js
      ];
      const BUILT_INS = [].concat(
        BUILT_IN_GLOBALS,
        TYPES,
        ERROR_TYPES
      );
      function javascript(hljs3) {
        const regex = hljs3.regex;
        const hasClosingTag = (match, { after }) => {
          const tag = "</" + match[0].slice(1);
          const pos = match.input.indexOf(tag, after);
          return pos !== -1;
        };
        const IDENT_RE$12 = IDENT_RE;
        const FRAGMENT = {
          begin: "<>",
          end: "</>"
        };
        const XML_SELF_CLOSING = /<[A-Za-z0-9\\._:-]+\s*\/>/;
        const XML_TAG = {
          begin: /<[A-Za-z0-9\\._:-]+/,
          end: /\/[A-Za-z0-9\\._:-]+>|\/>/,
          /**
           * @param {RegExpMatchArray} match
           * @param {CallbackResponse} response
           */
          isTrulyOpeningTag: (match, response) => {
            const afterMatchIndex = match[0].length + match.index;
            const nextChar = match.input[afterMatchIndex];
            if (
              // HTML should not include another raw `<` inside a tag
              // nested type?
              // `<Array<Array<number>>`, etc.
              nextChar === "<" || // the , gives away that this is not HTML
              // `<T, A extends keyof T, V>`
              nextChar === ","
            ) {
              response.ignoreMatch();
              return;
            }
            if (nextChar === ">") {
              if (!hasClosingTag(match, { after: afterMatchIndex })) {
                response.ignoreMatch();
              }
            }
            let m;
            const afterMatch = match.input.substring(afterMatchIndex);
            if (m = afterMatch.match(/^\s*=/)) {
              response.ignoreMatch();
              return;
            }
            if (m = afterMatch.match(/^\s+extends\s+/)) {
              if (m.index === 0) {
                response.ignoreMatch();
                return;
              }
            }
          }
        };
        const KEYWORDS$1 = {
          $pattern: IDENT_RE,
          keyword: KEYWORDS,
          literal: LITERALS,
          built_in: BUILT_INS,
          "variable.language": BUILT_IN_VARIABLES
        };
        const decimalDigits2 = "[0-9](_?[0-9])*";
        const frac2 = `\\.(${decimalDigits2})`;
        const decimalInteger = `0|[1-9](_?[0-9])*|0[0-7]*[89][0-9]*`;
        const NUMBER = {
          className: "number",
          variants: [
            // DecimalLiteral
            { begin: `(\\b(${decimalInteger})((${frac2})|\\.)?|(${frac2}))[eE][+-]?(${decimalDigits2})\\b` },
            { begin: `\\b(${decimalInteger})\\b((${frac2})\\b|\\.)?|(${frac2})\\b` },
            // DecimalBigIntegerLiteral
            { begin: `\\b(0|[1-9](_?[0-9])*)n\\b` },
            // NonDecimalIntegerLiteral
            { begin: "\\b0[xX][0-9a-fA-F](_?[0-9a-fA-F])*n?\\b" },
            { begin: "\\b0[bB][0-1](_?[0-1])*n?\\b" },
            { begin: "\\b0[oO][0-7](_?[0-7])*n?\\b" },
            // LegacyOctalIntegerLiteral (does not include underscore separators)
            // https://tc39.es/ecma262/#sec-additional-syntax-numeric-literals
            { begin: "\\b0[0-7]+n?\\b" }
          ],
          relevance: 0
        };
        const SUBST = {
          className: "subst",
          begin: "\\$\\{",
          end: "\\}",
          keywords: KEYWORDS$1,
          contains: []
          // defined later
        };
        const HTML_TEMPLATE = {
          begin: ".?html`",
          end: "",
          starts: {
            end: "`",
            returnEnd: false,
            contains: [
              hljs3.BACKSLASH_ESCAPE,
              SUBST
            ],
            subLanguage: "xml"
          }
        };
        const CSS_TEMPLATE = {
          begin: ".?css`",
          end: "",
          starts: {
            end: "`",
            returnEnd: false,
            contains: [
              hljs3.BACKSLASH_ESCAPE,
              SUBST
            ],
            subLanguage: "css"
          }
        };
        const GRAPHQL_TEMPLATE = {
          begin: ".?gql`",
          end: "",
          starts: {
            end: "`",
            returnEnd: false,
            contains: [
              hljs3.BACKSLASH_ESCAPE,
              SUBST
            ],
            subLanguage: "graphql"
          }
        };
        const TEMPLATE_STRING = {
          className: "string",
          begin: "`",
          end: "`",
          contains: [
            hljs3.BACKSLASH_ESCAPE,
            SUBST
          ]
        };
        const JSDOC_COMMENT = hljs3.COMMENT(
          /\/\*\*(?!\/)/,
          "\\*/",
          {
            relevance: 0,
            contains: [
              {
                begin: "(?=@[A-Za-z]+)",
                relevance: 0,
                contains: [
                  {
                    className: "doctag",
                    begin: "@[A-Za-z]+"
                  },
                  {
                    className: "type",
                    begin: "\\{",
                    end: "\\}",
                    excludeEnd: true,
                    excludeBegin: true,
                    relevance: 0
                  },
                  {
                    className: "variable",
                    begin: IDENT_RE$12 + "(?=\\s*(-)|$)",
                    endsParent: true,
                    relevance: 0
                  },
                  // eat spaces (not newlines) so we can find
                  // types or variables
                  {
                    begin: /(?=[^\n])\s/,
                    relevance: 0
                  }
                ]
              }
            ]
          }
        );
        const COMMENT2 = {
          className: "comment",
          variants: [
            JSDOC_COMMENT,
            hljs3.C_BLOCK_COMMENT_MODE,
            hljs3.C_LINE_COMMENT_MODE
          ]
        };
        const SUBST_INTERNALS = [
          hljs3.APOS_STRING_MODE,
          hljs3.QUOTE_STRING_MODE,
          HTML_TEMPLATE,
          CSS_TEMPLATE,
          GRAPHQL_TEMPLATE,
          TEMPLATE_STRING,
          // Skip numbers when they are part of a variable name
          { match: /\$\d+/ },
          NUMBER
          // This is intentional:
          // See https://github.com/highlightjs/highlight.js/issues/3288
          // hljs.REGEXP_MODE
        ];
        SUBST.contains = SUBST_INTERNALS.concat({
          // we need to pair up {} inside our subst to prevent
          // it from ending too early by matching another }
          begin: /\{/,
          end: /\}/,
          keywords: KEYWORDS$1,
          contains: [
            "self"
          ].concat(SUBST_INTERNALS)
        });
        const SUBST_AND_COMMENTS = [].concat(COMMENT2, SUBST.contains);
        const PARAMS_CONTAINS = SUBST_AND_COMMENTS.concat([
          // eat recursive parens in sub expressions
          {
            begin: /(\s*)\(/,
            end: /\)/,
            keywords: KEYWORDS$1,
            contains: ["self"].concat(SUBST_AND_COMMENTS)
          }
        ]);
        const PARAMS = {
          className: "params",
          // convert this to negative lookbehind in v12
          begin: /(\s*)\(/,
          // to match the parms with
          end: /\)/,
          excludeBegin: true,
          excludeEnd: true,
          keywords: KEYWORDS$1,
          contains: PARAMS_CONTAINS
        };
        const CLASS_OR_EXTENDS = {
          variants: [
            // class Car extends vehicle
            {
              match: [
                /class/,
                /\s+/,
                IDENT_RE$12,
                /\s+/,
                /extends/,
                /\s+/,
                regex.concat(IDENT_RE$12, "(", regex.concat(/\./, IDENT_RE$12), ")*")
              ],
              scope: {
                1: "keyword",
                3: "title.class",
                5: "keyword",
                7: "title.class.inherited"
              }
            },
            // class Car
            {
              match: [
                /class/,
                /\s+/,
                IDENT_RE$12
              ],
              scope: {
                1: "keyword",
                3: "title.class"
              }
            }
          ]
        };
        const CLASS_REFERENCE = {
          relevance: 0,
          match: regex.either(
            // Hard coded exceptions
            /\bJSON/,
            // Float32Array, OutT
            /\b[A-Z][a-z]+([A-Z][a-z]*|\d)*/,
            // CSSFactory, CSSFactoryT
            /\b[A-Z]{2,}([A-Z][a-z]+|\d)+([A-Z][a-z]*)*/,
            // FPs, FPsT
            /\b[A-Z]{2,}[a-z]+([A-Z][a-z]+|\d)*([A-Z][a-z]*)*/
            // P
            // single letters are not highlighted
            // BLAH
            // this will be flagged as a UPPER_CASE_CONSTANT instead
          ),
          className: "title.class",
          keywords: {
            _: [
              // se we still get relevance credit for JS library classes
              ...TYPES,
              ...ERROR_TYPES
            ]
          }
        };
        const USE_STRICT = {
          label: "use_strict",
          className: "meta",
          relevance: 10,
          begin: /^\s*['"]use (strict|asm)['"]/
        };
        const FUNCTION_DEFINITION = {
          variants: [
            {
              match: [
                /function/,
                /\s+/,
                IDENT_RE$12,
                /(?=\s*\()/
              ]
            },
            // anonymous function
            {
              match: [
                /function/,
                /\s*(?=\()/
              ]
            }
          ],
          className: {
            1: "keyword",
            3: "title.function"
          },
          label: "func.def",
          contains: [PARAMS],
          illegal: /%/
        };
        const UPPER_CASE_CONSTANT = {
          relevance: 0,
          match: /\b[A-Z][A-Z_0-9]+\b/,
          className: "variable.constant"
        };
        function noneOf(list) {
          return regex.concat("(?!", list.join("|"), ")");
        }
        const FUNCTION_CALL = {
          match: regex.concat(
            /\b/,
            noneOf([
              ...BUILT_IN_GLOBALS,
              "super",
              "import",
              "await"
            ].map((x) => `${x}\\s*\\(`)),
            IDENT_RE$12,
            regex.lookahead(/\s*\(/)
          ),
          className: "title.function",
          relevance: 0
        };
        const PROPERTY_ACCESS = {
          begin: regex.concat(/\./, regex.lookahead(
            regex.concat(IDENT_RE$12, /(?![0-9A-Za-z$_(])/)
          )),
          end: IDENT_RE$12,
          excludeBegin: true,
          keywords: "prototype",
          className: "property",
          relevance: 0
        };
        const GETTER_OR_SETTER = {
          match: [
            /get|set/,
            /\s+/,
            IDENT_RE$12,
            /(?=\()/
          ],
          className: {
            1: "keyword",
            3: "title.function"
          },
          contains: [
            {
              // eat to avoid empty params
              begin: /\(\)/
            },
            PARAMS
          ]
        };
        const FUNC_LEAD_IN_RE = "(\\([^()]*(\\([^()]*(\\([^()]*\\)[^()]*)*\\)[^()]*)*\\)|" + hljs3.UNDERSCORE_IDENT_RE + ")\\s*=>";
        const FUNCTION_VARIABLE = {
          match: [
            /const|var|let/,
            /\s+/,
            IDENT_RE$12,
            /\s*/,
            /=\s*/,
            /(async\s*)?/,
            // async is optional
            regex.lookahead(FUNC_LEAD_IN_RE)
          ],
          keywords: "async",
          className: {
            1: "keyword",
            3: "title.function"
          },
          contains: [
            PARAMS
          ]
        };
        return {
          name: "JavaScript",
          aliases: ["js", "jsx", "mjs", "cjs"],
          keywords: KEYWORDS$1,
          // this will be extended by TypeScript
          exports: { PARAMS_CONTAINS, CLASS_REFERENCE },
          illegal: /#(?![$_A-Za-z])/,
          contains: [
            hljs3.SHEBANG({
              label: "shebang",
              binary: "node",
              relevance: 5
            }),
            USE_STRICT,
            hljs3.APOS_STRING_MODE,
            hljs3.QUOTE_STRING_MODE,
            HTML_TEMPLATE,
            CSS_TEMPLATE,
            GRAPHQL_TEMPLATE,
            TEMPLATE_STRING,
            COMMENT2,
            // Skip numbers when they are part of a variable name
            { match: /\$\d+/ },
            NUMBER,
            CLASS_REFERENCE,
            {
              scope: "attr",
              match: IDENT_RE$12 + regex.lookahead(":"),
              relevance: 0
            },
            FUNCTION_VARIABLE,
            {
              // "value" container
              begin: "(" + hljs3.RE_STARTERS_RE + "|\\b(case|return|throw)\\b)\\s*",
              keywords: "return throw case",
              relevance: 0,
              contains: [
                COMMENT2,
                hljs3.REGEXP_MODE,
                {
                  className: "function",
                  // we have to count the parens to make sure we actually have the
                  // correct bounding ( ) before the =>.  There could be any number of
                  // sub-expressions inside also surrounded by parens.
                  begin: FUNC_LEAD_IN_RE,
                  returnBegin: true,
                  end: "\\s*=>",
                  contains: [
                    {
                      className: "params",
                      variants: [
                        {
                          begin: hljs3.UNDERSCORE_IDENT_RE,
                          relevance: 0
                        },
                        {
                          className: null,
                          begin: /\(\s*\)/,
                          skip: true
                        },
                        {
                          begin: /(\s*)\(/,
                          end: /\)/,
                          excludeBegin: true,
                          excludeEnd: true,
                          keywords: KEYWORDS$1,
                          contains: PARAMS_CONTAINS
                        }
                      ]
                    }
                  ]
                },
                {
                  // could be a comma delimited list of params to a function call
                  begin: /,/,
                  relevance: 0
                },
                {
                  match: /\s+/,
                  relevance: 0
                },
                {
                  // JSX
                  variants: [
                    { begin: FRAGMENT.begin, end: FRAGMENT.end },
                    { match: XML_SELF_CLOSING },
                    {
                      begin: XML_TAG.begin,
                      // we carefully check the opening tag to see if it truly
                      // is a tag and not a false positive
                      "on:begin": XML_TAG.isTrulyOpeningTag,
                      end: XML_TAG.end
                    }
                  ],
                  subLanguage: "xml",
                  contains: [
                    {
                      begin: XML_TAG.begin,
                      end: XML_TAG.end,
                      skip: true,
                      contains: ["self"]
                    }
                  ]
                }
              ]
            },
            FUNCTION_DEFINITION,
            {
              // prevent this from getting swallowed up by function
              // since they appear "function like"
              beginKeywords: "while if switch catch for"
            },
            {
              // we have to count the parens to make sure we actually have the correct
              // bounding ( ).  There could be any number of sub-expressions inside
              // also surrounded by parens.
              begin: "\\b(?!function)" + hljs3.UNDERSCORE_IDENT_RE + "\\([^()]*(\\([^()]*(\\([^()]*\\)[^()]*)*\\)[^()]*)*\\)\\s*\\{",
              // end parens
              returnBegin: true,
              label: "func.def",
              contains: [
                PARAMS,
                hljs3.inherit(hljs3.TITLE_MODE, { begin: IDENT_RE$12, className: "title.function" })
              ]
            },
            // catch ... so it won't trigger the property rule below
            {
              match: /\.\.\./,
              relevance: 0
            },
            PROPERTY_ACCESS,
            // hack: prevents detection of keywords in some circumstances
            // .keyword()
            // $keyword = x
            {
              match: "\\$" + IDENT_RE$12,
              relevance: 0
            },
            {
              match: [/\bconstructor(?=\s*\()/],
              className: { 1: "title.function" },
              contains: [PARAMS]
            },
            FUNCTION_CALL,
            UPPER_CASE_CONSTANT,
            CLASS_OR_EXTENDS,
            GETTER_OR_SETTER,
            {
              match: /\$[(.]/
              // relevance booster for a pattern common to JS libs: `$(something)` and `$.something`
            }
          ]
        };
      }
      function typescript(hljs3) {
        const regex = hljs3.regex;
        const tsLanguage = javascript(hljs3);
        const IDENT_RE$12 = IDENT_RE;
        const TYPES2 = [
          "any",
          "void",
          "number",
          "boolean",
          "string",
          "object",
          "never",
          "symbol",
          "bigint",
          "unknown"
        ];
        const NAMESPACE = {
          begin: [
            /namespace/,
            /\s+/,
            hljs3.IDENT_RE
          ],
          beginScope: {
            1: "keyword",
            3: "title.class"
          }
        };
        const INTERFACE = {
          beginKeywords: "interface",
          end: /\{/,
          excludeEnd: true,
          keywords: {
            keyword: "interface extends",
            built_in: TYPES2
          },
          contains: [tsLanguage.exports.CLASS_REFERENCE]
        };
        const USE_STRICT = {
          className: "meta",
          relevance: 10,
          begin: /^\s*['"]use strict['"]/
        };
        const TS_SPECIFIC_KEYWORDS = [
          "type",
          // "namespace",
          "interface",
          "public",
          "private",
          "protected",
          "implements",
          "declare",
          "abstract",
          "readonly",
          "enum",
          "override",
          "satisfies"
        ];
        const KEYWORDS$1 = {
          $pattern: IDENT_RE,
          keyword: KEYWORDS.concat(TS_SPECIFIC_KEYWORDS),
          literal: LITERALS,
          built_in: BUILT_INS.concat(TYPES2),
          "variable.language": BUILT_IN_VARIABLES
        };
        const DECORATOR = {
          className: "meta",
          begin: "@" + IDENT_RE$12
        };
        const swapMode = (mode, label, replacement) => {
          const indx = mode.contains.findIndex((m) => m.label === label);
          if (indx === -1) {
            throw new Error("can not find mode to replace");
          }
          mode.contains.splice(indx, 1, replacement);
        };
        Object.assign(tsLanguage.keywords, KEYWORDS$1);
        tsLanguage.exports.PARAMS_CONTAINS.push(DECORATOR);
        const ATTRIBUTE_HIGHLIGHT = tsLanguage.contains.find((c2) => c2.scope === "attr");
        const OPTIONAL_KEY_OR_ARGUMENT = Object.assign(
          {},
          ATTRIBUTE_HIGHLIGHT,
          { match: regex.concat(IDENT_RE$12, regex.lookahead(/\s*\?:/)) }
        );
        tsLanguage.exports.PARAMS_CONTAINS.push([
          tsLanguage.exports.CLASS_REFERENCE,
          // class reference for highlighting the params types
          ATTRIBUTE_HIGHLIGHT,
          // highlight the params key
          OPTIONAL_KEY_OR_ARGUMENT
          // Added for optional property assignment highlighting
        ]);
        tsLanguage.contains = tsLanguage.contains.concat([
          DECORATOR,
          NAMESPACE,
          INTERFACE,
          OPTIONAL_KEY_OR_ARGUMENT
          // Added for optional property assignment highlighting
        ]);
        swapMode(tsLanguage, "shebang", hljs3.SHEBANG());
        swapMode(tsLanguage, "use_strict", USE_STRICT);
        const functionDeclaration = tsLanguage.contains.find((m) => m.label === "func.def");
        functionDeclaration.relevance = 0;
        Object.assign(tsLanguage, {
          name: "TypeScript",
          aliases: [
            "ts",
            "tsx",
            "mts",
            "cts"
          ]
        });
        return tsLanguage;
      }
      const keywordWrapper = (keyword) => concat(
        /\b/,
        keyword,
        /\w$/.test(keyword) ? /\b/ : /\B/
      );
      const dotKeywords = [
        "Protocol",
        // contextual
        "Type"
        // contextual
      ].map(keywordWrapper);
      const optionalDotKeywords = [
        "init",
        "self"
      ].map(keywordWrapper);
      const keywordTypes = [
        "Any",
        "Self"
      ];
      const keywords = [
        // strings below will be fed into the regular `keywords` engine while regex
        // will result in additional modes being created to scan for those keywords to
        // avoid conflicts with other rules
        "actor",
        "any",
        // contextual
        "associatedtype",
        "async",
        "await",
        /as\?/,
        // operator
        /as!/,
        // operator
        "as",
        // operator
        "borrowing",
        // contextual
        "break",
        "case",
        "catch",
        "class",
        "consume",
        // contextual
        "consuming",
        // contextual
        "continue",
        "convenience",
        // contextual
        "copy",
        // contextual
        "default",
        "defer",
        "deinit",
        "didSet",
        // contextual
        "distributed",
        "do",
        "dynamic",
        // contextual
        "each",
        "else",
        "enum",
        "extension",
        "fallthrough",
        /fileprivate\(set\)/,
        "fileprivate",
        "final",
        // contextual
        "for",
        "func",
        "get",
        // contextual
        "guard",
        "if",
        "import",
        "indirect",
        // contextual
        "infix",
        // contextual
        /init\?/,
        /init!/,
        "inout",
        /internal\(set\)/,
        "internal",
        "in",
        "is",
        // operator
        "isolated",
        // contextual
        "nonisolated",
        // contextual
        "lazy",
        // contextual
        "let",
        "macro",
        "mutating",
        // contextual
        "nonmutating",
        // contextual
        /open\(set\)/,
        // contextual
        "open",
        // contextual
        "operator",
        "optional",
        // contextual
        "override",
        // contextual
        "package",
        "postfix",
        // contextual
        "precedencegroup",
        "prefix",
        // contextual
        /private\(set\)/,
        "private",
        "protocol",
        /public\(set\)/,
        "public",
        "repeat",
        "required",
        // contextual
        "rethrows",
        "return",
        "set",
        // contextual
        "some",
        // contextual
        "static",
        "struct",
        "subscript",
        "super",
        "switch",
        "throws",
        "throw",
        /try\?/,
        // operator
        /try!/,
        // operator
        "try",
        // operator
        "typealias",
        /unowned\(safe\)/,
        // contextual
        /unowned\(unsafe\)/,
        // contextual
        "unowned",
        // contextual
        "var",
        "weak",
        // contextual
        "where",
        "while",
        "willSet"
        // contextual
      ];
      const literals = [
        "false",
        "nil",
        "true"
      ];
      const precedencegroupKeywords = [
        "assignment",
        "associativity",
        "higherThan",
        "left",
        "lowerThan",
        "none",
        "right"
      ];
      const numberSignKeywords = [
        "#colorLiteral",
        "#column",
        "#dsohandle",
        "#else",
        "#elseif",
        "#endif",
        "#error",
        "#file",
        "#fileID",
        "#fileLiteral",
        "#filePath",
        "#function",
        "#if",
        "#imageLiteral",
        "#keyPath",
        "#line",
        "#selector",
        "#sourceLocation",
        "#warning"
      ];
      const builtIns$1 = [
        "abs",
        "all",
        "any",
        "assert",
        "assertionFailure",
        "debugPrint",
        "dump",
        "fatalError",
        "getVaList",
        "isKnownUniquelyReferenced",
        "max",
        "min",
        "numericCast",
        "pointwiseMax",
        "pointwiseMin",
        "precondition",
        "preconditionFailure",
        "print",
        "readLine",
        "repeatElement",
        "sequence",
        "stride",
        "swap",
        "swift_unboxFromSwiftValueWithType",
        "transcode",
        "type",
        "unsafeBitCast",
        "unsafeDowncast",
        "withExtendedLifetime",
        "withUnsafeMutablePointer",
        "withUnsafePointer",
        "withVaList",
        "withoutActuallyEscaping",
        "zip"
      ];
      const operatorHead = either(
        /[/=\-+!*%<>&|^~?]/,
        /[\u00A1-\u00A7]/,
        /[\u00A9\u00AB]/,
        /[\u00AC\u00AE]/,
        /[\u00B0\u00B1]/,
        /[\u00B6\u00BB\u00BF\u00D7\u00F7]/,
        /[\u2016-\u2017]/,
        /[\u2020-\u2027]/,
        /[\u2030-\u203E]/,
        /[\u2041-\u2053]/,
        /[\u2055-\u205E]/,
        /[\u2190-\u23FF]/,
        /[\u2500-\u2775]/,
        /[\u2794-\u2BFF]/,
        /[\u2E00-\u2E7F]/,
        /[\u3001-\u3003]/,
        /[\u3008-\u3020]/,
        /[\u3030]/
      );
      const operatorCharacter = either(
        operatorHead,
        /[\u0300-\u036F]/,
        /[\u1DC0-\u1DFF]/,
        /[\u20D0-\u20FF]/,
        /[\uFE00-\uFE0F]/,
        /[\uFE20-\uFE2F]/
        // TODO: The following characters are also allowed, but the regex isn't supported yet.
        // /[\u{E0100}-\u{E01EF}]/u
      );
      const operator = concat(operatorHead, operatorCharacter, "*");
      const identifierHead = either(
        /[a-zA-Z_]/,
        /[\u00A8\u00AA\u00AD\u00AF\u00B2-\u00B5\u00B7-\u00BA]/,
        /[\u00BC-\u00BE\u00C0-\u00D6\u00D8-\u00F6\u00F8-\u00FF]/,
        /[\u0100-\u02FF\u0370-\u167F\u1681-\u180D\u180F-\u1DBF]/,
        /[\u1E00-\u1FFF]/,
        /[\u200B-\u200D\u202A-\u202E\u203F-\u2040\u2054\u2060-\u206F]/,
        /[\u2070-\u20CF\u2100-\u218F\u2460-\u24FF\u2776-\u2793]/,
        /[\u2C00-\u2DFF\u2E80-\u2FFF]/,
        /[\u3004-\u3007\u3021-\u302F\u3031-\u303F\u3040-\uD7FF]/,
        /[\uF900-\uFD3D\uFD40-\uFDCF\uFDF0-\uFE1F\uFE30-\uFE44]/,
        /[\uFE47-\uFEFE\uFF00-\uFFFD]/
        // Should be /[\uFE47-\uFFFD]/, but we have to exclude FEFF.
        // The following characters are also allowed, but the regexes aren't supported yet.
        // /[\u{10000}-\u{1FFFD}\u{20000-\u{2FFFD}\u{30000}-\u{3FFFD}\u{40000}-\u{4FFFD}]/u,
        // /[\u{50000}-\u{5FFFD}\u{60000-\u{6FFFD}\u{70000}-\u{7FFFD}\u{80000}-\u{8FFFD}]/u,
        // /[\u{90000}-\u{9FFFD}\u{A0000-\u{AFFFD}\u{B0000}-\u{BFFFD}\u{C0000}-\u{CFFFD}]/u,
        // /[\u{D0000}-\u{DFFFD}\u{E0000-\u{EFFFD}]/u
      );
      const identifierCharacter = either(
        identifierHead,
        /\d/,
        /[\u0300-\u036F\u1DC0-\u1DFF\u20D0-\u20FF\uFE20-\uFE2F]/
      );
      const identifier = concat(identifierHead, identifierCharacter, "*");
      const typeIdentifier = concat(/[A-Z]/, identifierCharacter, "*");
      const keywordAttributes = [
        "attached",
        "autoclosure",
        concat(/convention\(/, either("swift", "block", "c"), /\)/),
        "discardableResult",
        "dynamicCallable",
        "dynamicMemberLookup",
        "escaping",
        "freestanding",
        "frozen",
        "GKInspectable",
        "IBAction",
        "IBDesignable",
        "IBInspectable",
        "IBOutlet",
        "IBSegueAction",
        "inlinable",
        "main",
        "nonobjc",
        "NSApplicationMain",
        "NSCopying",
        "NSManaged",
        concat(/objc\(/, identifier, /\)/),
        "objc",
        "objcMembers",
        "propertyWrapper",
        "requires_stored_property_inits",
        "resultBuilder",
        "Sendable",
        "testable",
        "UIApplicationMain",
        "unchecked",
        "unknown",
        "usableFromInline",
        "warn_unqualified_access"
      ];
      const availabilityKeywords = [
        "iOS",
        "iOSApplicationExtension",
        "macOS",
        "macOSApplicationExtension",
        "macCatalyst",
        "macCatalystApplicationExtension",
        "watchOS",
        "watchOSApplicationExtension",
        "tvOS",
        "tvOSApplicationExtension",
        "swift"
      ];
      function swift(hljs3) {
        const WHITESPACE = {
          match: /\s+/,
          relevance: 0
        };
        const BLOCK_COMMENT = hljs3.COMMENT(
          "/\\*",
          "\\*/",
          { contains: ["self"] }
        );
        const COMMENTS = [
          hljs3.C_LINE_COMMENT_MODE,
          BLOCK_COMMENT
        ];
        const DOT_KEYWORD = {
          match: [
            /\./,
            either(...dotKeywords, ...optionalDotKeywords)
          ],
          className: { 2: "keyword" }
        };
        const KEYWORD_GUARD = {
          // Consume .keyword to prevent highlighting properties and methods as keywords.
          match: concat(/\./, either(...keywords)),
          relevance: 0
        };
        const PLAIN_KEYWORDS = keywords.filter((kw) => typeof kw === "string").concat(["_|0"]);
        const REGEX_KEYWORDS = keywords.filter((kw) => typeof kw !== "string").concat(keywordTypes).map(keywordWrapper);
        const KEYWORD = { variants: [
          {
            className: "keyword",
            match: either(...REGEX_KEYWORDS, ...optionalDotKeywords)
          }
        ] };
        const KEYWORDS2 = {
          $pattern: either(
            /\b\w+/,
            // regular keywords
            /#\w+/
            // number keywords
          ),
          keyword: PLAIN_KEYWORDS.concat(numberSignKeywords),
          literal: literals
        };
        const KEYWORD_MODES = [
          DOT_KEYWORD,
          KEYWORD_GUARD,
          KEYWORD
        ];
        const BUILT_IN_GUARD = {
          // Consume .built_in to prevent highlighting properties and methods.
          match: concat(/\./, either(...builtIns$1)),
          relevance: 0
        };
        const BUILT_IN = {
          className: "built_in",
          match: concat(/\b/, either(...builtIns$1), /(?=\()/)
        };
        const BUILT_INS2 = [
          BUILT_IN_GUARD,
          BUILT_IN
        ];
        const OPERATOR_GUARD = {
          // Prevent -> from being highlighting as an operator.
          match: /->/,
          relevance: 0
        };
        const OPERATOR = {
          className: "operator",
          relevance: 0,
          variants: [
            { match: operator },
            {
              // dot-operator: only operators that start with a dot are allowed to use dots as
              // characters (..., ...<, .*, etc). So there rule here is: a dot followed by one or more
              // characters that may also include dots.
              match: `\\.(\\.|${operatorCharacter})+`
            }
          ]
        };
        const OPERATORS = [
          OPERATOR_GUARD,
          OPERATOR
        ];
        const decimalDigits2 = "([0-9]_*)+";
        const hexDigits2 = "([0-9a-fA-F]_*)+";
        const NUMBER = {
          className: "number",
          relevance: 0,
          variants: [
            // decimal floating-point-literal (subsumes decimal-literal)
            { match: `\\b(${decimalDigits2})(\\.(${decimalDigits2}))?([eE][+-]?(${decimalDigits2}))?\\b` },
            // hexadecimal floating-point-literal (subsumes hexadecimal-literal)
            { match: `\\b0x(${hexDigits2})(\\.(${hexDigits2}))?([pP][+-]?(${decimalDigits2}))?\\b` },
            // octal-literal
            { match: /\b0o([0-7]_*)+\b/ },
            // binary-literal
            { match: /\b0b([01]_*)+\b/ }
          ]
        };
        const ESCAPED_CHARACTER = (rawDelimiter = "") => ({
          className: "subst",
          variants: [
            { match: concat(/\\/, rawDelimiter, /[0\\tnr"']/) },
            { match: concat(/\\/, rawDelimiter, /u\{[0-9a-fA-F]{1,8}\}/) }
          ]
        });
        const ESCAPED_NEWLINE = (rawDelimiter = "") => ({
          className: "subst",
          match: concat(/\\/, rawDelimiter, /[\t ]*(?:[\r\n]|\r\n)/)
        });
        const INTERPOLATION = (rawDelimiter = "") => ({
          className: "subst",
          label: "interpol",
          begin: concat(/\\/, rawDelimiter, /\(/),
          end: /\)/
        });
        const MULTILINE_STRING = (rawDelimiter = "") => ({
          begin: concat(rawDelimiter, /"""/),
          end: concat(/"""/, rawDelimiter),
          contains: [
            ESCAPED_CHARACTER(rawDelimiter),
            ESCAPED_NEWLINE(rawDelimiter),
            INTERPOLATION(rawDelimiter)
          ]
        });
        const SINGLE_LINE_STRING = (rawDelimiter = "") => ({
          begin: concat(rawDelimiter, /"/),
          end: concat(/"/, rawDelimiter),
          contains: [
            ESCAPED_CHARACTER(rawDelimiter),
            INTERPOLATION(rawDelimiter)
          ]
        });
        const STRING = {
          className: "string",
          variants: [
            MULTILINE_STRING(),
            MULTILINE_STRING("#"),
            MULTILINE_STRING("##"),
            MULTILINE_STRING("###"),
            SINGLE_LINE_STRING(),
            SINGLE_LINE_STRING("#"),
            SINGLE_LINE_STRING("##"),
            SINGLE_LINE_STRING("###")
          ]
        };
        const REGEXP_CONTENTS = [
          hljs3.BACKSLASH_ESCAPE,
          {
            begin: /\[/,
            end: /\]/,
            relevance: 0,
            contains: [hljs3.BACKSLASH_ESCAPE]
          }
        ];
        const BARE_REGEXP_LITERAL = {
          begin: /\/[^\s](?=[^/\n]*\/)/,
          end: /\//,
          contains: REGEXP_CONTENTS
        };
        const EXTENDED_REGEXP_LITERAL = (rawDelimiter) => {
          const begin = concat(rawDelimiter, /\//);
          const end = concat(/\//, rawDelimiter);
          return {
            begin,
            end,
            contains: [
              ...REGEXP_CONTENTS,
              {
                scope: "comment",
                begin: `#(?!.*${end})`,
                end: /$/
              }
            ]
          };
        };
        const REGEXP = {
          scope: "regexp",
          variants: [
            EXTENDED_REGEXP_LITERAL("###"),
            EXTENDED_REGEXP_LITERAL("##"),
            EXTENDED_REGEXP_LITERAL("#"),
            BARE_REGEXP_LITERAL
          ]
        };
        const QUOTED_IDENTIFIER = { match: concat(/`/, identifier, /`/) };
        const IMPLICIT_PARAMETER = {
          className: "variable",
          match: /\$\d+/
        };
        const PROPERTY_WRAPPER_PROJECTION = {
          className: "variable",
          match: `\\$${identifierCharacter}+`
        };
        const IDENTIFIERS = [
          QUOTED_IDENTIFIER,
          IMPLICIT_PARAMETER,
          PROPERTY_WRAPPER_PROJECTION
        ];
        const AVAILABLE_ATTRIBUTE = {
          match: /(@|#(un)?)available/,
          scope: "keyword",
          starts: { contains: [
            {
              begin: /\(/,
              end: /\)/,
              keywords: availabilityKeywords,
              contains: [
                ...OPERATORS,
                NUMBER,
                STRING
              ]
            }
          ] }
        };
        const KEYWORD_ATTRIBUTE = {
          scope: "keyword",
          match: concat(/@/, either(...keywordAttributes), lookahead(either(/\(/, /\s+/)))
        };
        const USER_DEFINED_ATTRIBUTE = {
          scope: "meta",
          match: concat(/@/, identifier)
        };
        const ATTRIBUTES2 = [
          AVAILABLE_ATTRIBUTE,
          KEYWORD_ATTRIBUTE,
          USER_DEFINED_ATTRIBUTE
        ];
        const TYPE = {
          match: lookahead(/\b[A-Z]/),
          relevance: 0,
          contains: [
            {
              // Common Apple frameworks, for relevance boost
              className: "type",
              match: concat(/(AV|CA|CF|CG|CI|CL|CM|CN|CT|MK|MP|MTK|MTL|NS|SCN|SK|UI|WK|XC)/, identifierCharacter, "+")
            },
            {
              // Type identifier
              className: "type",
              match: typeIdentifier,
              relevance: 0
            },
            {
              // Optional type
              match: /[?!]+/,
              relevance: 0
            },
            {
              // Variadic parameter
              match: /\.\.\./,
              relevance: 0
            },
            {
              // Protocol composition
              match: concat(/\s+&\s+/, lookahead(typeIdentifier)),
              relevance: 0
            }
          ]
        };
        const GENERIC_ARGUMENTS = {
          begin: /</,
          end: />/,
          keywords: KEYWORDS2,
          contains: [
            ...COMMENTS,
            ...KEYWORD_MODES,
            ...ATTRIBUTES2,
            OPERATOR_GUARD,
            TYPE
          ]
        };
        TYPE.contains.push(GENERIC_ARGUMENTS);
        const TUPLE_ELEMENT_NAME = {
          match: concat(identifier, /\s*:/),
          keywords: "_|0",
          relevance: 0
        };
        const TUPLE = {
          begin: /\(/,
          end: /\)/,
          relevance: 0,
          keywords: KEYWORDS2,
          contains: [
            "self",
            TUPLE_ELEMENT_NAME,
            ...COMMENTS,
            REGEXP,
            ...KEYWORD_MODES,
            ...BUILT_INS2,
            ...OPERATORS,
            NUMBER,
            STRING,
            ...IDENTIFIERS,
            ...ATTRIBUTES2,
            TYPE
          ]
        };
        const GENERIC_PARAMETERS = {
          begin: /</,
          end: />/,
          keywords: "repeat each",
          contains: [
            ...COMMENTS,
            TYPE
          ]
        };
        const FUNCTION_PARAMETER_NAME = {
          begin: either(
            lookahead(concat(identifier, /\s*:/)),
            lookahead(concat(identifier, /\s+/, identifier, /\s*:/))
          ),
          end: /:/,
          relevance: 0,
          contains: [
            {
              className: "keyword",
              match: /\b_\b/
            },
            {
              className: "params",
              match: identifier
            }
          ]
        };
        const FUNCTION_PARAMETERS = {
          begin: /\(/,
          end: /\)/,
          keywords: KEYWORDS2,
          contains: [
            FUNCTION_PARAMETER_NAME,
            ...COMMENTS,
            ...KEYWORD_MODES,
            ...OPERATORS,
            NUMBER,
            STRING,
            ...ATTRIBUTES2,
            TYPE,
            TUPLE
          ],
          endsParent: true,
          illegal: /["']/
        };
        const FUNCTION_OR_MACRO = {
          match: [
            /(func|macro)/,
            /\s+/,
            either(QUOTED_IDENTIFIER.match, identifier, operator)
          ],
          className: {
            1: "keyword",
            3: "title.function"
          },
          contains: [
            GENERIC_PARAMETERS,
            FUNCTION_PARAMETERS,
            WHITESPACE
          ],
          illegal: [
            /\[/,
            /%/
          ]
        };
        const INIT_SUBSCRIPT = {
          match: [
            /\b(?:subscript|init[?!]?)/,
            /\s*(?=[<(])/
          ],
          className: { 1: "keyword" },
          contains: [
            GENERIC_PARAMETERS,
            FUNCTION_PARAMETERS,
            WHITESPACE
          ],
          illegal: /\[|%/
        };
        const OPERATOR_DECLARATION = {
          match: [
            /operator/,
            /\s+/,
            operator
          ],
          className: {
            1: "keyword",
            3: "title"
          }
        };
        const PRECEDENCEGROUP = {
          begin: [
            /precedencegroup/,
            /\s+/,
            typeIdentifier
          ],
          className: {
            1: "keyword",
            3: "title"
          },
          contains: [TYPE],
          keywords: [
            ...precedencegroupKeywords,
            ...literals
          ],
          end: /}/
        };
        const CLASS_FUNC_DECLARATION = {
          match: [
            /class\b/,
            /\s+/,
            /func\b/,
            /\s+/,
            /\b[A-Za-z_][A-Za-z0-9_]*\b/
          ],
          scope: {
            1: "keyword",
            3: "keyword",
            5: "title.function"
          }
        };
        const CLASS_VAR_DECLARATION = {
          match: [
            /class\b/,
            /\s+/,
            /var\b/
          ],
          scope: {
            1: "keyword",
            3: "keyword"
          }
        };
        const TYPE_DECLARATION = {
          begin: [
            /(struct|protocol|class|extension|enum|actor)/,
            /\s+/,
            identifier,
            /\s*/
          ],
          beginScope: {
            1: "keyword",
            3: "title.class"
          },
          keywords: KEYWORDS2,
          contains: [
            GENERIC_PARAMETERS,
            ...KEYWORD_MODES,
            {
              begin: /:/,
              end: /\{/,
              keywords: KEYWORDS2,
              contains: [
                {
                  scope: "title.class.inherited",
                  match: typeIdentifier
                },
                ...KEYWORD_MODES
              ],
              relevance: 0
            }
          ]
        };
        for (const variant of STRING.variants) {
          const interpolation = variant.contains.find((mode) => mode.label === "interpol");
          interpolation.keywords = KEYWORDS2;
          const submodes = [
            ...KEYWORD_MODES,
            ...BUILT_INS2,
            ...OPERATORS,
            NUMBER,
            STRING,
            ...IDENTIFIERS
          ];
          interpolation.contains = [
            ...submodes,
            {
              begin: /\(/,
              end: /\)/,
              contains: [
                "self",
                ...submodes
              ]
            }
          ];
        }
        return {
          name: "Swift",
          keywords: KEYWORDS2,
          contains: [
            ...COMMENTS,
            FUNCTION_OR_MACRO,
            INIT_SUBSCRIPT,
            CLASS_FUNC_DECLARATION,
            CLASS_VAR_DECLARATION,
            TYPE_DECLARATION,
            OPERATOR_DECLARATION,
            PRECEDENCEGROUP,
            {
              beginKeywords: "import",
              end: /$/,
              contains: [...COMMENTS],
              relevance: 0
            },
            REGEXP,
            ...KEYWORD_MODES,
            ...BUILT_INS2,
            ...OPERATORS,
            NUMBER,
            STRING,
            ...IDENTIFIERS,
            ...ATTRIBUTES2,
            TYPE,
            TUPLE
          ]
        };
      }
      function sql(hljs3) {
        const regex = hljs3.regex;
        const COMMENT_MODE = hljs3.COMMENT("--", "$");
        const STRING = {
          scope: "string",
          variants: [
            {
              begin: /'/,
              end: /'/,
              contains: [{ match: /''/ }]
            }
          ]
        };
        const QUOTED_IDENTIFIER = {
          begin: /"/,
          end: /"/,
          contains: [{ match: /""/ }]
        };
        const LITERALS2 = [
          "true",
          "false",
          // Not sure it's correct to call NULL literal, and clauses like IS [NOT] NULL look strange that way.
          // "null",
          "unknown"
        ];
        const MULTI_WORD_TYPES = [
          "double precision",
          "large object",
          "with timezone",
          "without timezone"
        ];
        const TYPES2 = [
          "bigint",
          "binary",
          "blob",
          "boolean",
          "char",
          "character",
          "clob",
          "date",
          "dec",
          "decfloat",
          "decimal",
          "float",
          "int",
          "integer",
          "interval",
          "nchar",
          "nclob",
          "national",
          "numeric",
          "real",
          "row",
          "smallint",
          "time",
          "timestamp",
          "varchar",
          "varying",
          // modifier (character varying)
          "varbinary"
        ];
        const NON_RESERVED_WORDS = [
          "add",
          "asc",
          "collation",
          "desc",
          "final",
          "first",
          "last",
          "view"
        ];
        const RESERVED_WORDS = [
          "abs",
          "acos",
          "all",
          "allocate",
          "alter",
          "and",
          "any",
          "are",
          "array",
          "array_agg",
          "array_max_cardinality",
          "as",
          "asensitive",
          "asin",
          "asymmetric",
          "at",
          "atan",
          "atomic",
          "authorization",
          "avg",
          "begin",
          "begin_frame",
          "begin_partition",
          "between",
          "bigint",
          "binary",
          "blob",
          "boolean",
          "both",
          "by",
          "call",
          "called",
          "cardinality",
          "cascaded",
          "case",
          "cast",
          "ceil",
          "ceiling",
          "char",
          "char_length",
          "character",
          "character_length",
          "check",
          "classifier",
          "clob",
          "close",
          "coalesce",
          "collate",
          "collect",
          "column",
          "commit",
          "condition",
          "connect",
          "constraint",
          "contains",
          "convert",
          "copy",
          "corr",
          "corresponding",
          "cos",
          "cosh",
          "count",
          "covar_pop",
          "covar_samp",
          "create",
          "cross",
          "cube",
          "cume_dist",
          "current",
          "current_catalog",
          "current_date",
          "current_default_transform_group",
          "current_path",
          "current_role",
          "current_row",
          "current_schema",
          "current_time",
          "current_timestamp",
          "current_path",
          "current_role",
          "current_transform_group_for_type",
          "current_user",
          "cursor",
          "cycle",
          "date",
          "day",
          "deallocate",
          "dec",
          "decimal",
          "decfloat",
          "declare",
          "default",
          "define",
          "delete",
          "dense_rank",
          "deref",
          "describe",
          "deterministic",
          "disconnect",
          "distinct",
          "double",
          "drop",
          "dynamic",
          "each",
          "element",
          "else",
          "empty",
          "end",
          "end_frame",
          "end_partition",
          "end-exec",
          "equals",
          "escape",
          "every",
          "except",
          "exec",
          "execute",
          "exists",
          "exp",
          "external",
          "extract",
          "false",
          "fetch",
          "filter",
          "first_value",
          "float",
          "floor",
          "for",
          "foreign",
          "frame_row",
          "free",
          "from",
          "full",
          "function",
          "fusion",
          "get",
          "global",
          "grant",
          "group",
          "grouping",
          "groups",
          "having",
          "hold",
          "hour",
          "identity",
          "in",
          "indicator",
          "initial",
          "inner",
          "inout",
          "insensitive",
          "insert",
          "int",
          "integer",
          "intersect",
          "intersection",
          "interval",
          "into",
          "is",
          "join",
          "json_array",
          "json_arrayagg",
          "json_exists",
          "json_object",
          "json_objectagg",
          "json_query",
          "json_table",
          "json_table_primitive",
          "json_value",
          "lag",
          "language",
          "large",
          "last_value",
          "lateral",
          "lead",
          "leading",
          "left",
          "like",
          "like_regex",
          "listagg",
          "ln",
          "local",
          "localtime",
          "localtimestamp",
          "log",
          "log10",
          "lower",
          "match",
          "match_number",
          "match_recognize",
          "matches",
          "max",
          "member",
          "merge",
          "method",
          "min",
          "minute",
          "mod",
          "modifies",
          "module",
          "month",
          "multiset",
          "national",
          "natural",
          "nchar",
          "nclob",
          "new",
          "no",
          "none",
          "normalize",
          "not",
          "nth_value",
          "ntile",
          "null",
          "nullif",
          "numeric",
          "octet_length",
          "occurrences_regex",
          "of",
          "offset",
          "old",
          "omit",
          "on",
          "one",
          "only",
          "open",
          "or",
          "order",
          "out",
          "outer",
          "over",
          "overlaps",
          "overlay",
          "parameter",
          "partition",
          "pattern",
          "per",
          "percent",
          "percent_rank",
          "percentile_cont",
          "percentile_disc",
          "period",
          "portion",
          "position",
          "position_regex",
          "power",
          "precedes",
          "precision",
          "prepare",
          "primary",
          "procedure",
          "ptf",
          "range",
          "rank",
          "reads",
          "real",
          "recursive",
          "ref",
          "references",
          "referencing",
          "regr_avgx",
          "regr_avgy",
          "regr_count",
          "regr_intercept",
          "regr_r2",
          "regr_slope",
          "regr_sxx",
          "regr_sxy",
          "regr_syy",
          "release",
          "result",
          "return",
          "returns",
          "revoke",
          "right",
          "rollback",
          "rollup",
          "row",
          "row_number",
          "rows",
          "running",
          "savepoint",
          "scope",
          "scroll",
          "search",
          "second",
          "seek",
          "select",
          "sensitive",
          "session_user",
          "set",
          "show",
          "similar",
          "sin",
          "sinh",
          "skip",
          "smallint",
          "some",
          "specific",
          "specifictype",
          "sql",
          "sqlexception",
          "sqlstate",
          "sqlwarning",
          "sqrt",
          "start",
          "static",
          "stddev_pop",
          "stddev_samp",
          "submultiset",
          "subset",
          "substring",
          "substring_regex",
          "succeeds",
          "sum",
          "symmetric",
          "system",
          "system_time",
          "system_user",
          "table",
          "tablesample",
          "tan",
          "tanh",
          "then",
          "time",
          "timestamp",
          "timezone_hour",
          "timezone_minute",
          "to",
          "trailing",
          "translate",
          "translate_regex",
          "translation",
          "treat",
          "trigger",
          "trim",
          "trim_array",
          "true",
          "truncate",
          "uescape",
          "union",
          "unique",
          "unknown",
          "unnest",
          "update",
          "upper",
          "user",
          "using",
          "value",
          "values",
          "value_of",
          "var_pop",
          "var_samp",
          "varbinary",
          "varchar",
          "varying",
          "versioning",
          "when",
          "whenever",
          "where",
          "width_bucket",
          "window",
          "with",
          "within",
          "without",
          "year"
        ];
        const RESERVED_FUNCTIONS = [
          "abs",
          "acos",
          "array_agg",
          "asin",
          "atan",
          "avg",
          "cast",
          "ceil",
          "ceiling",
          "coalesce",
          "corr",
          "cos",
          "cosh",
          "count",
          "covar_pop",
          "covar_samp",
          "cume_dist",
          "dense_rank",
          "deref",
          "element",
          "exp",
          "extract",
          "first_value",
          "floor",
          "json_array",
          "json_arrayagg",
          "json_exists",
          "json_object",
          "json_objectagg",
          "json_query",
          "json_table",
          "json_table_primitive",
          "json_value",
          "lag",
          "last_value",
          "lead",
          "listagg",
          "ln",
          "log",
          "log10",
          "lower",
          "max",
          "min",
          "mod",
          "nth_value",
          "ntile",
          "nullif",
          "percent_rank",
          "percentile_cont",
          "percentile_disc",
          "position",
          "position_regex",
          "power",
          "rank",
          "regr_avgx",
          "regr_avgy",
          "regr_count",
          "regr_intercept",
          "regr_r2",
          "regr_slope",
          "regr_sxx",
          "regr_sxy",
          "regr_syy",
          "row_number",
          "sin",
          "sinh",
          "sqrt",
          "stddev_pop",
          "stddev_samp",
          "substring",
          "substring_regex",
          "sum",
          "tan",
          "tanh",
          "translate",
          "translate_regex",
          "treat",
          "trim",
          "trim_array",
          "unnest",
          "upper",
          "value_of",
          "var_pop",
          "var_samp",
          "width_bucket"
        ];
        const POSSIBLE_WITHOUT_PARENS = [
          "current_catalog",
          "current_date",
          "current_default_transform_group",
          "current_path",
          "current_role",
          "current_schema",
          "current_transform_group_for_type",
          "current_user",
          "session_user",
          "system_time",
          "system_user",
          "current_time",
          "localtime",
          "current_timestamp",
          "localtimestamp"
        ];
        const COMBOS = [
          "create table",
          "insert into",
          "primary key",
          "foreign key",
          "not null",
          "alter table",
          "add constraint",
          "grouping sets",
          "on overflow",
          "character set",
          "respect nulls",
          "ignore nulls",
          "nulls first",
          "nulls last",
          "depth first",
          "breadth first"
        ];
        const FUNCTIONS = RESERVED_FUNCTIONS;
        const KEYWORDS2 = [
          ...RESERVED_WORDS,
          ...NON_RESERVED_WORDS
        ].filter((keyword) => {
          return !RESERVED_FUNCTIONS.includes(keyword);
        });
        const VARIABLE = {
          scope: "variable",
          match: /@[a-z0-9][a-z0-9_]*/
        };
        const OPERATOR = {
          scope: "operator",
          match: /[-+*/=%^~]|&&?|\|\|?|!=?|<(?:=>?|<|>)?|>[>=]?/,
          relevance: 0
        };
        const FUNCTION_CALL = {
          match: regex.concat(/\b/, regex.either(...FUNCTIONS), /\s*\(/),
          relevance: 0,
          keywords: { built_in: FUNCTIONS }
        };
        function kws_to_regex(list) {
          return regex.concat(
            /\b/,
            regex.either(...list.map((kw) => {
              return kw.replace(/\s+/, "\\s+");
            })),
            /\b/
          );
        }
        const MULTI_WORD_KEYWORDS = {
          scope: "keyword",
          match: kws_to_regex(COMBOS),
          relevance: 0
        };
        function reduceRelevancy(list, {
          exceptions,
          when
        } = {}) {
          const qualifyFn = when;
          exceptions = exceptions || [];
          return list.map((item) => {
            if (item.match(/\|\d+$/) || exceptions.includes(item)) {
              return item;
            } else if (qualifyFn(item)) {
              return `${item}|0`;
            } else {
              return item;
            }
          });
        }
        return {
          name: "SQL",
          case_insensitive: true,
          // does not include {} or HTML tags `</`
          illegal: /[{}]|<\//,
          keywords: {
            $pattern: /\b[\w\.]+/,
            keyword: reduceRelevancy(KEYWORDS2, { when: (x) => x.length < 3 }),
            literal: LITERALS2,
            type: TYPES2,
            built_in: POSSIBLE_WITHOUT_PARENS
          },
          contains: [
            {
              scope: "type",
              match: kws_to_regex(MULTI_WORD_TYPES)
            },
            MULTI_WORD_KEYWORDS,
            FUNCTION_CALL,
            VARIABLE,
            STRING,
            QUOTED_IDENTIFIER,
            hljs3.C_NUMBER_MODE,
            hljs3.C_BLOCK_COMMENT_MODE,
            COMMENT_MODE,
            OPERATOR
          ]
        };
      }
      function bash(hljs3) {
        const regex = hljs3.regex;
        const VAR = {};
        const BRACED_VAR = {
          begin: /\$\{/,
          end: /\}/,
          contains: [
            "self",
            {
              begin: /:-/,
              contains: [VAR]
            }
            // default values
          ]
        };
        Object.assign(VAR, {
          className: "variable",
          variants: [
            { begin: regex.concat(
              /\$[\w\d#@][\w\d_]*/,
              // negative look-ahead tries to avoid matching patterns that are not
              // Perl at all like $ident$, @ident@, etc.
              `(?![\\w\\d])(?![$])`
            ) },
            BRACED_VAR
          ]
        });
        const SUBST = {
          className: "subst",
          begin: /\$\(/,
          end: /\)/,
          contains: [hljs3.BACKSLASH_ESCAPE]
        };
        const COMMENT2 = hljs3.inherit(
          hljs3.COMMENT(),
          {
            match: [
              /(^|\s)/,
              /#.*$/
            ],
            scope: {
              2: "comment"
            }
          }
        );
        const HERE_DOC = {
          begin: /<<-?\s*(?=\w+)/,
          starts: { contains: [
            hljs3.END_SAME_AS_BEGIN({
              begin: /(\w+)/,
              end: /(\w+)/,
              className: "string"
            })
          ] }
        };
        const QUOTE_STRING = {
          className: "string",
          begin: /"/,
          end: /"/,
          contains: [
            hljs3.BACKSLASH_ESCAPE,
            VAR,
            SUBST
          ]
        };
        SUBST.contains.push(QUOTE_STRING);
        const ESCAPED_QUOTE = {
          match: /\\"/
        };
        const APOS_STRING = {
          className: "string",
          begin: /'/,
          end: /'/
        };
        const ESCAPED_APOS = {
          match: /\\'/
        };
        const ARITHMETIC = {
          begin: /\$?\(\(/,
          end: /\)\)/,
          contains: [
            {
              begin: /\d+#[0-9a-f]+/,
              className: "number"
            },
            hljs3.NUMBER_MODE,
            VAR
          ]
        };
        const SH_LIKE_SHELLS = [
          "fish",
          "bash",
          "zsh",
          "sh",
          "csh",
          "ksh",
          "tcsh",
          "dash",
          "scsh"
        ];
        const KNOWN_SHEBANG = hljs3.SHEBANG({
          binary: `(${SH_LIKE_SHELLS.join("|")})`,
          relevance: 10
        });
        const FUNCTION = {
          className: "function",
          begin: /\w[\w\d_]*\s*\(\s*\)\s*\{/,
          returnBegin: true,
          contains: [hljs3.inherit(hljs3.TITLE_MODE, { begin: /\w[\w\d_]*/ })],
          relevance: 0
        };
        const KEYWORDS2 = [
          "if",
          "then",
          "else",
          "elif",
          "fi",
          "time",
          "for",
          "while",
          "until",
          "in",
          "do",
          "done",
          "case",
          "esac",
          "coproc",
          "function",
          "select"
        ];
        const LITERALS2 = [
          "true",
          "false"
        ];
        const PATH_MODE = { match: /(\/[a-z._-]+)+/ };
        const SHELL_BUILT_INS = [
          "break",
          "cd",
          "continue",
          "eval",
          "exec",
          "exit",
          "export",
          "getopts",
          "hash",
          "pwd",
          "readonly",
          "return",
          "shift",
          "test",
          "times",
          "trap",
          "umask",
          "unset"
        ];
        const BASH_BUILT_INS = [
          "alias",
          "bind",
          "builtin",
          "caller",
          "command",
          "declare",
          "echo",
          "enable",
          "help",
          "let",
          "local",
          "logout",
          "mapfile",
          "printf",
          "read",
          "readarray",
          "source",
          "sudo",
          "type",
          "typeset",
          "ulimit",
          "unalias"
        ];
        const ZSH_BUILT_INS = [
          "autoload",
          "bg",
          "bindkey",
          "bye",
          "cap",
          "chdir",
          "clone",
          "comparguments",
          "compcall",
          "compctl",
          "compdescribe",
          "compfiles",
          "compgroups",
          "compquote",
          "comptags",
          "comptry",
          "compvalues",
          "dirs",
          "disable",
          "disown",
          "echotc",
          "echoti",
          "emulate",
          "fc",
          "fg",
          "float",
          "functions",
          "getcap",
          "getln",
          "history",
          "integer",
          "jobs",
          "kill",
          "limit",
          "log",
          "noglob",
          "popd",
          "print",
          "pushd",
          "pushln",
          "rehash",
          "sched",
          "setcap",
          "setopt",
          "stat",
          "suspend",
          "ttyctl",
          "unfunction",
          "unhash",
          "unlimit",
          "unsetopt",
          "vared",
          "wait",
          "whence",
          "where",
          "which",
          "zcompile",
          "zformat",
          "zftp",
          "zle",
          "zmodload",
          "zparseopts",
          "zprof",
          "zpty",
          "zregexparse",
          "zsocket",
          "zstyle",
          "ztcp"
        ];
        const GNU_CORE_UTILS = [
          "chcon",
          "chgrp",
          "chown",
          "chmod",
          "cp",
          "dd",
          "df",
          "dir",
          "dircolors",
          "ln",
          "ls",
          "mkdir",
          "mkfifo",
          "mknod",
          "mktemp",
          "mv",
          "realpath",
          "rm",
          "rmdir",
          "shred",
          "sync",
          "touch",
          "truncate",
          "vdir",
          "b2sum",
          "base32",
          "base64",
          "cat",
          "cksum",
          "comm",
          "csplit",
          "cut",
          "expand",
          "fmt",
          "fold",
          "head",
          "join",
          "md5sum",
          "nl",
          "numfmt",
          "od",
          "paste",
          "ptx",
          "pr",
          "sha1sum",
          "sha224sum",
          "sha256sum",
          "sha384sum",
          "sha512sum",
          "shuf",
          "sort",
          "split",
          "sum",
          "tac",
          "tail",
          "tr",
          "tsort",
          "unexpand",
          "uniq",
          "wc",
          "arch",
          "basename",
          "chroot",
          "date",
          "dirname",
          "du",
          "echo",
          "env",
          "expr",
          "factor",
          // "false", // keyword literal already
          "groups",
          "hostid",
          "id",
          "link",
          "logname",
          "nice",
          "nohup",
          "nproc",
          "pathchk",
          "pinky",
          "printenv",
          "printf",
          "pwd",
          "readlink",
          "runcon",
          "seq",
          "sleep",
          "stat",
          "stdbuf",
          "stty",
          "tee",
          "test",
          "timeout",
          // "true", // keyword literal already
          "tty",
          "uname",
          "unlink",
          "uptime",
          "users",
          "who",
          "whoami",
          "yes"
        ];
        return {
          name: "Bash",
          aliases: [
            "sh",
            "zsh"
          ],
          keywords: {
            $pattern: /\b[a-z][a-z0-9._-]+\b/,
            keyword: KEYWORDS2,
            literal: LITERALS2,
            built_in: [
              ...SHELL_BUILT_INS,
              ...BASH_BUILT_INS,
              // Shell modifiers
              "set",
              "shopt",
              ...ZSH_BUILT_INS,
              ...GNU_CORE_UTILS
            ]
          },
          contains: [
            KNOWN_SHEBANG,
            // to catch known shells and boost relevancy
            hljs3.SHEBANG(),
            // to catch unknown shells but still highlight the shebang
            FUNCTION,
            ARITHMETIC,
            COMMENT2,
            HERE_DOC,
            PATH_MODE,
            QUOTE_STRING,
            ESCAPED_QUOTE,
            APOS_STRING,
            ESCAPED_APOS,
            VAR
          ]
        };
      }
      function shell(hljs3) {
        return {
          name: "Shell Session",
          aliases: [
            "console",
            "shellsession"
          ],
          contains: [
            {
              className: "meta.prompt",
              // We cannot add \s (spaces) in the regular expression otherwise it will be too broad and produce unexpected result.
              // For instance, in the following example, it would match "echo /path/to/home >" as a prompt:
              // echo /path/to/home > t.exe
              begin: /^\s{0,3}[./~\w\d[\]()@-]*[>%$#][ ]?/,
              starts: {
                end: /[^\\](?=\s*$)/,
                subLanguage: "bash"
              }
            }
          ]
        };
      }
      const MODES = (hljs3) => {
        return {
          IMPORTANT: {
            scope: "meta",
            begin: "!important"
          },
          BLOCK_COMMENT: hljs3.C_BLOCK_COMMENT_MODE,
          HEXCOLOR: {
            scope: "number",
            begin: /#(([0-9a-fA-F]{3,4})|(([0-9a-fA-F]{2}){3,4}))\b/
          },
          UNICODE_RANGE: {
            scope: "number",
            begin: /\b[Uu]\+[0-9A-Fa-f][0-9A-Fa-f?]{0,5}(-[0-9A-Fa-f][0-9A-Fa-f]{0,5})?/
          },
          FUNCTION_DISPATCH: {
            className: "built_in",
            begin: /[\w-]+(?=\()/
          },
          ATTRIBUTE_SELECTOR_MODE: {
            scope: "selector-attr",
            begin: /\[/,
            end: /\]/,
            illegal: "$",
            contains: [
              hljs3.APOS_STRING_MODE,
              hljs3.QUOTE_STRING_MODE
            ]
          },
          CSS_NUMBER_MODE: {
            scope: "number",
            begin: hljs3.NUMBER_RE + "(%|em|ex|ch|rem|vw|vh|vmin|vmax|cm|mm|in|pt|pc|px|deg|grad|rad|turn|s|ms|Hz|kHz|dpi|dpcm|dppx)?",
            relevance: 0
          },
          CSS_VARIABLE: {
            className: "attr",
            begin: /--[A-Za-z_][A-Za-z0-9_-]*/
          }
        };
      };
      const HTML_TAGS = [
        "a",
        "abbr",
        "address",
        "article",
        "aside",
        "audio",
        "b",
        "blockquote",
        "body",
        "button",
        "canvas",
        "caption",
        "cite",
        "code",
        "dd",
        "del",
        "details",
        "dfn",
        "div",
        "dl",
        "dt",
        "em",
        "fieldset",
        "figcaption",
        "figure",
        "footer",
        "form",
        "h1",
        "h2",
        "h3",
        "h4",
        "h5",
        "h6",
        "header",
        "hgroup",
        "html",
        "i",
        "iframe",
        "img",
        "input",
        "ins",
        "kbd",
        "label",
        "legend",
        "li",
        "main",
        "mark",
        "menu",
        "nav",
        "object",
        "ol",
        "optgroup",
        "option",
        "p",
        "picture",
        "q",
        "quote",
        "samp",
        "section",
        "select",
        "source",
        "span",
        "strong",
        "summary",
        "sup",
        "table",
        "tbody",
        "td",
        "textarea",
        "tfoot",
        "th",
        "thead",
        "time",
        "tr",
        "ul",
        "var",
        "video"
      ];
      const SVG_TAGS = [
        "defs",
        "g",
        "marker",
        "mask",
        "pattern",
        "svg",
        "switch",
        "symbol",
        "feBlend",
        "feColorMatrix",
        "feComponentTransfer",
        "feComposite",
        "feConvolveMatrix",
        "feDiffuseLighting",
        "feDisplacementMap",
        "feFlood",
        "feGaussianBlur",
        "feImage",
        "feMerge",
        "feMorphology",
        "feOffset",
        "feSpecularLighting",
        "feTile",
        "feTurbulence",
        "linearGradient",
        "radialGradient",
        "stop",
        "circle",
        "ellipse",
        "image",
        "line",
        "path",
        "polygon",
        "polyline",
        "rect",
        "text",
        "use",
        "textPath",
        "tspan",
        "foreignObject",
        "clipPath"
      ];
      const TAGS = [
        ...HTML_TAGS,
        ...SVG_TAGS
      ];
      const MEDIA_FEATURES = [
        "any-hover",
        "any-pointer",
        "aspect-ratio",
        "color",
        "color-gamut",
        "color-index",
        "device-aspect-ratio",
        "device-height",
        "device-width",
        "display-mode",
        "forced-colors",
        "grid",
        "height",
        "hover",
        "inverted-colors",
        "monochrome",
        "orientation",
        "overflow-block",
        "overflow-inline",
        "pointer",
        "prefers-color-scheme",
        "prefers-contrast",
        "prefers-reduced-motion",
        "prefers-reduced-transparency",
        "resolution",
        "scan",
        "scripting",
        "update",
        "width",
        // TODO: find a better solution?
        "min-width",
        "max-width",
        "min-height",
        "max-height"
      ].sort().reverse();
      const PSEUDO_CLASSES = [
        "active",
        "any-link",
        "blank",
        "checked",
        "current",
        "default",
        "defined",
        "dir",
        // dir()
        "disabled",
        "drop",
        "empty",
        "enabled",
        "first",
        "first-child",
        "first-of-type",
        "fullscreen",
        "future",
        "focus",
        "focus-visible",
        "focus-within",
        "has",
        // has()
        "host",
        // host or host()
        "host-context",
        // host-context()
        "hover",
        "indeterminate",
        "in-range",
        "invalid",
        "is",
        // is()
        "lang",
        // lang()
        "last-child",
        "last-of-type",
        "left",
        "link",
        "local-link",
        "not",
        // not()
        "nth-child",
        // nth-child()
        "nth-col",
        // nth-col()
        "nth-last-child",
        // nth-last-child()
        "nth-last-col",
        // nth-last-col()
        "nth-last-of-type",
        //nth-last-of-type()
        "nth-of-type",
        //nth-of-type()
        "only-child",
        "only-of-type",
        "optional",
        "out-of-range",
        "past",
        "placeholder-shown",
        "read-only",
        "read-write",
        "required",
        "right",
        "root",
        "scope",
        "target",
        "target-within",
        "user-invalid",
        "valid",
        "visited",
        "where"
        // where()
      ].sort().reverse();
      const PSEUDO_ELEMENTS = [
        "after",
        "backdrop",
        "before",
        "cue",
        "cue-region",
        "first-letter",
        "first-line",
        "grammar-error",
        "marker",
        "part",
        "placeholder",
        "selection",
        "slotted",
        "spelling-error"
      ].sort().reverse();
      const ATTRIBUTES = [
        "accent-color",
        "align-content",
        "align-items",
        "align-self",
        "alignment-baseline",
        "all",
        "anchor-name",
        "animation",
        "animation-composition",
        "animation-delay",
        "animation-direction",
        "animation-duration",
        "animation-fill-mode",
        "animation-iteration-count",
        "animation-name",
        "animation-play-state",
        "animation-range",
        "animation-range-end",
        "animation-range-start",
        "animation-timeline",
        "animation-timing-function",
        "appearance",
        "aspect-ratio",
        "backdrop-filter",
        "backface-visibility",
        "background",
        "background-attachment",
        "background-blend-mode",
        "background-clip",
        "background-color",
        "background-image",
        "background-origin",
        "background-position",
        "background-position-x",
        "background-position-y",
        "background-repeat",
        "background-size",
        "baseline-shift",
        "block-size",
        "border",
        "border-block",
        "border-block-color",
        "border-block-end",
        "border-block-end-color",
        "border-block-end-style",
        "border-block-end-width",
        "border-block-start",
        "border-block-start-color",
        "border-block-start-style",
        "border-block-start-width",
        "border-block-style",
        "border-block-width",
        "border-bottom",
        "border-bottom-color",
        "border-bottom-left-radius",
        "border-bottom-right-radius",
        "border-bottom-style",
        "border-bottom-width",
        "border-collapse",
        "border-color",
        "border-end-end-radius",
        "border-end-start-radius",
        "border-image",
        "border-image-outset",
        "border-image-repeat",
        "border-image-slice",
        "border-image-source",
        "border-image-width",
        "border-inline",
        "border-inline-color",
        "border-inline-end",
        "border-inline-end-color",
        "border-inline-end-style",
        "border-inline-end-width",
        "border-inline-start",
        "border-inline-start-color",
        "border-inline-start-style",
        "border-inline-start-width",
        "border-inline-style",
        "border-inline-width",
        "border-left",
        "border-left-color",
        "border-left-style",
        "border-left-width",
        "border-radius",
        "border-right",
        "border-right-color",
        "border-right-style",
        "border-right-width",
        "border-spacing",
        "border-start-end-radius",
        "border-start-start-radius",
        "border-style",
        "border-top",
        "border-top-color",
        "border-top-left-radius",
        "border-top-right-radius",
        "border-top-style",
        "border-top-width",
        "border-width",
        "bottom",
        "box-align",
        "box-decoration-break",
        "box-direction",
        "box-flex",
        "box-flex-group",
        "box-lines",
        "box-ordinal-group",
        "box-orient",
        "box-pack",
        "box-shadow",
        "box-sizing",
        "break-after",
        "break-before",
        "break-inside",
        "caption-side",
        "caret-color",
        "clear",
        "clip",
        "clip-path",
        "clip-rule",
        "color",
        "color-interpolation",
        "color-interpolation-filters",
        "color-profile",
        "color-rendering",
        "color-scheme",
        "column-count",
        "column-fill",
        "column-gap",
        "column-rule",
        "column-rule-color",
        "column-rule-style",
        "column-rule-width",
        "column-span",
        "column-width",
        "columns",
        "contain",
        "contain-intrinsic-block-size",
        "contain-intrinsic-height",
        "contain-intrinsic-inline-size",
        "contain-intrinsic-size",
        "contain-intrinsic-width",
        "container",
        "container-name",
        "container-type",
        "content",
        "content-visibility",
        "corner-bottom-left-shape",
        "corner-bottom-right-shape",
        "corner-shape",
        "corner-top-left-shape",
        "corner-top-right-shape",
        "counter-increment",
        "counter-reset",
        "counter-set",
        "cue",
        "cue-after",
        "cue-before",
        "cursor",
        "cx",
        "cy",
        "direction",
        "display",
        "dominant-baseline",
        "empty-cells",
        "enable-background",
        "field-sizing",
        "fill",
        "fill-opacity",
        "fill-rule",
        "filter",
        "flex",
        "flex-basis",
        "flex-direction",
        "flex-flow",
        "flex-grow",
        "flex-shrink",
        "flex-wrap",
        "float",
        "flood-color",
        "flood-opacity",
        "flow",
        "font",
        "font-display",
        "font-family",
        "font-feature-settings",
        "font-kerning",
        "font-language-override",
        "font-optical-sizing",
        "font-palette",
        "font-size",
        "font-size-adjust",
        "font-smooth",
        "font-smoothing",
        "font-stretch",
        "font-style",
        "font-synthesis",
        "font-synthesis-position",
        "font-synthesis-small-caps",
        "font-synthesis-style",
        "font-synthesis-weight",
        "font-variant",
        "font-variant-alternates",
        "font-variant-caps",
        "font-variant-east-asian",
        "font-variant-emoji",
        "font-variant-ligatures",
        "font-variant-numeric",
        "font-variant-position",
        "font-variation-settings",
        "font-weight",
        "forced-color-adjust",
        "gap",
        "glyph-orientation-horizontal",
        "glyph-orientation-vertical",
        "grid",
        "grid-area",
        "grid-auto-columns",
        "grid-auto-flow",
        "grid-auto-rows",
        "grid-column",
        "grid-column-end",
        "grid-column-start",
        "grid-gap",
        "grid-row",
        "grid-row-end",
        "grid-row-start",
        "grid-template",
        "grid-template-areas",
        "grid-template-columns",
        "grid-template-rows",
        "hanging-punctuation",
        "height",
        "hyphenate-character",
        "hyphenate-limit-chars",
        "hyphens",
        "icon",
        "image-orientation",
        "image-rendering",
        "image-resolution",
        "ime-mode",
        "initial-letter",
        "initial-letter-align",
        "inline-size",
        "inset",
        "inset-area",
        "inset-block",
        "inset-block-end",
        "inset-block-start",
        "inset-inline",
        "inset-inline-end",
        "inset-inline-start",
        "isolation",
        "justify-content",
        "justify-items",
        "justify-self",
        "kerning",
        "left",
        "letter-spacing",
        "lighting-color",
        "line-break",
        "line-height",
        "line-height-step",
        "list-style",
        "list-style-image",
        "list-style-position",
        "list-style-type",
        "margin",
        "margin-block",
        "margin-block-end",
        "margin-block-start",
        "margin-bottom",
        "margin-inline",
        "margin-inline-end",
        "margin-inline-start",
        "margin-left",
        "margin-right",
        "margin-top",
        "margin-trim",
        "marker",
        "marker-end",
        "marker-mid",
        "marker-start",
        "marks",
        "mask",
        "mask-border",
        "mask-border-mode",
        "mask-border-outset",
        "mask-border-repeat",
        "mask-border-slice",
        "mask-border-source",
        "mask-border-width",
        "mask-clip",
        "mask-composite",
        "mask-image",
        "mask-mode",
        "mask-origin",
        "mask-position",
        "mask-repeat",
        "mask-size",
        "mask-type",
        "masonry-auto-flow",
        "math-depth",
        "math-shift",
        "math-style",
        "max-block-size",
        "max-height",
        "max-inline-size",
        "max-width",
        "min-block-size",
        "min-height",
        "min-inline-size",
        "min-width",
        "mix-blend-mode",
        "nav-down",
        "nav-index",
        "nav-left",
        "nav-right",
        "nav-up",
        "none",
        "normal",
        "object-fit",
        "object-position",
        "offset",
        "offset-anchor",
        "offset-distance",
        "offset-path",
        "offset-position",
        "offset-rotate",
        "opacity",
        "order",
        "orphans",
        "outline",
        "outline-color",
        "outline-offset",
        "outline-style",
        "outline-width",
        "overflow",
        "overflow-anchor",
        "overflow-block",
        "overflow-clip-margin",
        "overflow-inline",
        "overflow-wrap",
        "overflow-x",
        "overflow-y",
        "overlay",
        "overscroll-behavior",
        "overscroll-behavior-block",
        "overscroll-behavior-inline",
        "overscroll-behavior-x",
        "overscroll-behavior-y",
        "padding",
        "padding-block",
        "padding-block-end",
        "padding-block-start",
        "padding-bottom",
        "padding-inline",
        "padding-inline-end",
        "padding-inline-start",
        "padding-left",
        "padding-right",
        "padding-top",
        "page",
        "page-break-after",
        "page-break-before",
        "page-break-inside",
        "paint-order",
        "pause",
        "pause-after",
        "pause-before",
        "perspective",
        "perspective-origin",
        "place-content",
        "place-items",
        "place-self",
        "pointer-events",
        "position",
        "position-anchor",
        "position-visibility",
        "print-color-adjust",
        "quotes",
        "r",
        "resize",
        "rest",
        "rest-after",
        "rest-before",
        "right",
        "rotate",
        "row-gap",
        "ruby-align",
        "ruby-position",
        "scale",
        "scroll-behavior",
        "scroll-margin",
        "scroll-margin-block",
        "scroll-margin-block-end",
        "scroll-margin-block-start",
        "scroll-margin-bottom",
        "scroll-margin-inline",
        "scroll-margin-inline-end",
        "scroll-margin-inline-start",
        "scroll-margin-left",
        "scroll-margin-right",
        "scroll-margin-top",
        "scroll-padding",
        "scroll-padding-block",
        "scroll-padding-block-end",
        "scroll-padding-block-start",
        "scroll-padding-bottom",
        "scroll-padding-inline",
        "scroll-padding-inline-end",
        "scroll-padding-inline-start",
        "scroll-padding-left",
        "scroll-padding-right",
        "scroll-padding-top",
        "scroll-snap-align",
        "scroll-snap-stop",
        "scroll-snap-type",
        "scroll-timeline",
        "scroll-timeline-axis",
        "scroll-timeline-name",
        "scrollbar-color",
        "scrollbar-gutter",
        "scrollbar-width",
        "shape-image-threshold",
        "shape-margin",
        "shape-outside",
        "shape-rendering",
        "speak",
        "speak-as",
        "src",
        // @font-face
        "stop-color",
        "stop-opacity",
        "stroke",
        "stroke-dasharray",
        "stroke-dashoffset",
        "stroke-linecap",
        "stroke-linejoin",
        "stroke-miterlimit",
        "stroke-opacity",
        "stroke-width",
        "tab-size",
        "table-layout",
        "text-align",
        "text-align-all",
        "text-align-last",
        "text-anchor",
        "text-combine-upright",
        "text-decoration",
        "text-decoration-color",
        "text-decoration-line",
        "text-decoration-skip",
        "text-decoration-skip-ink",
        "text-decoration-style",
        "text-decoration-thickness",
        "text-emphasis",
        "text-emphasis-color",
        "text-emphasis-position",
        "text-emphasis-style",
        "text-indent",
        "text-justify",
        "text-orientation",
        "text-overflow",
        "text-rendering",
        "text-shadow",
        "text-size-adjust",
        "text-transform",
        "text-underline-offset",
        "text-underline-position",
        "text-wrap",
        "text-wrap-mode",
        "text-wrap-style",
        "timeline-scope",
        "top",
        "touch-action",
        "transform",
        "transform-box",
        "transform-origin",
        "transform-style",
        "transition",
        "transition-behavior",
        "transition-delay",
        "transition-duration",
        "transition-property",
        "transition-timing-function",
        "translate",
        "unicode-bidi",
        "unicode-range",
        "user-modify",
        "user-select",
        "vector-effect",
        "vertical-align",
        "view-timeline",
        "view-timeline-axis",
        "view-timeline-inset",
        "view-timeline-name",
        "view-transition-name",
        "visibility",
        "voice-balance",
        "voice-duration",
        "voice-family",
        "voice-pitch",
        "voice-range",
        "voice-rate",
        "voice-stress",
        "voice-volume",
        "white-space",
        "white-space-collapse",
        "widows",
        "width",
        "will-change",
        "word-break",
        "word-spacing",
        "word-wrap",
        "writing-mode",
        "x",
        "y",
        "z-index",
        "zoom"
      ].sort().reverse();
      const PSEUDO_SELECTORS = PSEUDO_CLASSES.concat(PSEUDO_ELEMENTS).sort().reverse();
      function scss(hljs3) {
        const modes = MODES(hljs3);
        const PSEUDO_ELEMENTS$1 = PSEUDO_ELEMENTS;
        const PSEUDO_CLASSES$1 = PSEUDO_CLASSES;
        const AT_IDENTIFIER = "@[a-z-]+";
        const AT_MODIFIERS = "and or not only";
        const IDENT_RE2 = "[a-zA-Z-][a-zA-Z0-9_-]*";
        const VARIABLE = {
          className: "variable",
          begin: "(\\$" + IDENT_RE2 + ")\\b",
          relevance: 0
        };
        return {
          name: "SCSS",
          case_insensitive: true,
          illegal: "[=/|']",
          contains: [
            hljs3.C_LINE_COMMENT_MODE,
            hljs3.C_BLOCK_COMMENT_MODE,
            // to recognize keyframe 40% etc which are outside the scope of our
            // attribute value mode
            modes.CSS_NUMBER_MODE,
            {
              className: "selector-id",
              begin: "#[A-Za-z0-9_-]+",
              relevance: 0
            },
            {
              className: "selector-class",
              begin: "\\.[A-Za-z0-9_-]+",
              relevance: 0
            },
            modes.ATTRIBUTE_SELECTOR_MODE,
            {
              className: "selector-tag",
              begin: "\\b(" + TAGS.join("|") + ")\\b",
              // was there, before, but why?
              relevance: 0
            },
            {
              className: "selector-pseudo",
              begin: ":(" + PSEUDO_CLASSES$1.join("|") + ")"
            },
            {
              className: "selector-pseudo",
              begin: ":(:)?(" + PSEUDO_ELEMENTS$1.join("|") + ")"
            },
            VARIABLE,
            {
              // pseudo-selector params
              begin: /\(/,
              end: /\)/,
              contains: [modes.CSS_NUMBER_MODE]
            },
            modes.CSS_VARIABLE,
            {
              className: "attribute",
              begin: "\\b(" + ATTRIBUTES.join("|") + ")\\b"
            },
            { begin: "\\b(whitespace|wait|w-resize|visible|vertical-text|vertical-ideographic|uppercase|upper-roman|upper-alpha|underline|transparent|top|thin|thick|text|text-top|text-bottom|tb-rl|table-header-group|table-footer-group|sw-resize|super|strict|static|square|solid|small-caps|separate|se-resize|scroll|s-resize|rtl|row-resize|ridge|right|repeat|repeat-y|repeat-x|relative|progress|pointer|overline|outside|outset|oblique|nowrap|not-allowed|normal|none|nw-resize|no-repeat|no-drop|newspaper|ne-resize|n-resize|move|middle|medium|ltr|lr-tb|lowercase|lower-roman|lower-alpha|loose|list-item|line|line-through|line-edge|lighter|left|keep-all|justify|italic|inter-word|inter-ideograph|inside|inset|inline|inline-block|inherit|inactive|ideograph-space|ideograph-parenthesis|ideograph-numeric|ideograph-alpha|horizontal|hidden|help|hand|groove|fixed|ellipsis|e-resize|double|dotted|distribute|distribute-space|distribute-letter|distribute-all-lines|disc|disabled|default|decimal|dashed|crosshair|collapse|col-resize|circle|char|center|capitalize|break-word|break-all|bottom|both|bolder|bold|block|bidi-override|below|baseline|auto|always|all-scroll|absolute|table|table-cell)\\b" },
            {
              begin: /:/,
              end: /[;}{]/,
              relevance: 0,
              contains: [
                modes.BLOCK_COMMENT,
                VARIABLE,
                modes.HEXCOLOR,
                modes.CSS_NUMBER_MODE,
                modes.UNICODE_RANGE,
                hljs3.QUOTE_STRING_MODE,
                hljs3.APOS_STRING_MODE,
                modes.IMPORTANT,
                modes.FUNCTION_DISPATCH
              ]
            },
            // matching these here allows us to treat them more like regular CSS
            // rules so everything between the {} gets regular rule highlighting,
            // which is what we want for page and font-face
            {
              begin: "@(page|font-face)",
              keywords: {
                $pattern: AT_IDENTIFIER,
                keyword: "@page @font-face"
              }
            },
            {
              begin: "@",
              end: "[{;]",
              returnBegin: true,
              keywords: {
                $pattern: /[a-z-]+/,
                keyword: AT_MODIFIERS,
                attribute: MEDIA_FEATURES.join(" ")
              },
              contains: [
                {
                  begin: AT_IDENTIFIER,
                  className: "keyword"
                },
                {
                  begin: /[a-z-]+(?=:)/,
                  className: "attribute"
                },
                VARIABLE,
                hljs3.QUOTE_STRING_MODE,
                hljs3.APOS_STRING_MODE,
                modes.HEXCOLOR,
                modes.CSS_NUMBER_MODE
              ]
            },
            modes.FUNCTION_DISPATCH
          ]
        };
      }
      function rust(hljs3) {
        const regex = hljs3.regex;
        const RAW_IDENTIFIER = /(r#)?/;
        const UNDERSCORE_IDENT_RE2 = regex.concat(RAW_IDENTIFIER, hljs3.UNDERSCORE_IDENT_RE);
        const IDENT_RE2 = regex.concat(RAW_IDENTIFIER, hljs3.IDENT_RE);
        const FUNCTION_INVOKE = {
          scope: "title.function.invoke",
          relevance: 0,
          begin: regex.concat(
            /\b/,
            /(?!(?:let|for|while|if|else|match)\b)/,
            IDENT_RE2,
            regex.lookahead(/\s*\(/)
          )
        };
        const NUMBER_SUFFIX = "([ui](8|16|32|64|128|size)|f(16|32|64|128))?";
        const KEYWORDS2 = [
          "abstract",
          "as",
          "async",
          "await",
          "become",
          "box",
          "break",
          "const",
          "continue",
          "crate",
          "do",
          "dyn",
          "else",
          "enum",
          "extern",
          "false",
          "final",
          "fn",
          "for",
          "if",
          "impl",
          "in",
          "let",
          "loop",
          "macro",
          "match",
          "mod",
          "move",
          "mut",
          "override",
          "priv",
          "pub",
          "raw",
          "ref",
          "return",
          "self",
          "Self",
          "static",
          "struct",
          "super",
          "trait",
          "true",
          "try",
          "type",
          "typeof",
          "union",
          "unsafe",
          "unsized",
          "use",
          "virtual",
          "where",
          "while",
          "yield"
        ];
        const LITERALS2 = [
          "true",
          "false",
          "Some",
          "None",
          "Ok",
          "Err"
        ];
        const BUILTINS = [
          // functions
          "drop ",
          // traits
          "Copy",
          "Send",
          "Sized",
          "Sync",
          "Drop",
          "Fn",
          "FnMut",
          "FnOnce",
          "ToOwned",
          "Clone",
          "Debug",
          "PartialEq",
          "PartialOrd",
          "Eq",
          "Ord",
          "AsRef",
          "AsMut",
          "Into",
          "From",
          "Default",
          "Iterator",
          "Extend",
          "IntoIterator",
          "DoubleEndedIterator",
          "ExactSizeIterator",
          "SliceConcatExt",
          "ToString",
          // macros
          "assert!",
          "assert_eq!",
          "bitflags!",
          "bytes!",
          "cfg!",
          "col!",
          "concat!",
          "concat_idents!",
          "debug_assert!",
          "debug_assert_eq!",
          "env!",
          "eprintln!",
          "panic!",
          "file!",
          "format!",
          "format_args!",
          "include_bytes!",
          "include_str!",
          "line!",
          "local_data_key!",
          "module_path!",
          "option_env!",
          "print!",
          "println!",
          "select!",
          "stringify!",
          "try!",
          "unimplemented!",
          "unreachable!",
          "vec!",
          "write!",
          "writeln!",
          "macro_rules!",
          "assert_ne!",
          "debug_assert_ne!"
        ];
        const TYPES2 = [
          "i8",
          "i16",
          "i32",
          "i64",
          "i128",
          "isize",
          "u8",
          "u16",
          "u32",
          "u64",
          "u128",
          "usize",
          "f16",
          "f32",
          "f64",
          "f128",
          "str",
          "char",
          "bool",
          "Box",
          "Option",
          "Result",
          "String",
          "Vec"
        ];
        return {
          name: "Rust",
          aliases: ["rs"],
          keywords: {
            $pattern: hljs3.IDENT_RE + "!?",
            type: TYPES2,
            keyword: KEYWORDS2,
            literal: LITERALS2,
            built_in: BUILTINS
          },
          illegal: "</",
          contains: [
            hljs3.C_LINE_COMMENT_MODE,
            hljs3.COMMENT("/\\*", "\\*/", { contains: ["self"] }),
            hljs3.inherit(hljs3.QUOTE_STRING_MODE, {
              begin: /b?"/,
              illegal: null
            }),
            {
              scope: "symbol",
              // negative lookahead to avoid matching `'`
              begin: /'[a-zA-Z_][a-zA-Z0-9_]*(?!')/
            },
            {
              scope: "string",
              variants: [
                { begin: /b?r(#*)"(.|\n)*?"\1(?!#)/ },
                {
                  begin: /b?'/,
                  end: /'/,
                  contains: [
                    {
                      scope: "char.escape",
                      match: /\\('|"|\\|\w|x\w{2}|u\w{4}|U\w{8})/
                    }
                  ]
                }
              ]
            },
            {
              scope: "number",
              variants: [
                { begin: "\\b0b([01_]+)" + NUMBER_SUFFIX },
                { begin: "\\b0o([0-7_]+)" + NUMBER_SUFFIX },
                { begin: "\\b0x([A-Fa-f0-9_]+)" + NUMBER_SUFFIX },
                { begin: "\\b(\\d[\\d_]*(\\.[0-9_]+)?([eE][+-]?[0-9_]+)?)" + NUMBER_SUFFIX }
              ],
              relevance: 0
            },
            {
              begin: [
                /\bsafe/,
                /\s+/,
                /extern/
              ],
              scope: {
                1: "keyword",
                3: "keyword"
              }
            },
            {
              begin: [
                /fn/,
                /\s+/,
                UNDERSCORE_IDENT_RE2
              ],
              scope: {
                1: "keyword",
                3: "title.function"
              }
            },
            {
              scope: "meta",
              begin: "#!?\\[",
              end: "\\]",
              contains: [
                {
                  scope: "string",
                  begin: /"/,
                  end: /"/,
                  contains: [
                    hljs3.BACKSLASH_ESCAPE
                  ]
                }
              ]
            },
            {
              begin: [
                /let/,
                /\s+/,
                /(?:mut\s+)?/,
                UNDERSCORE_IDENT_RE2
              ],
              scope: {
                1: "keyword",
                3: "keyword",
                4: "variable"
              }
            },
            // must come before impl/for rule later
            {
              begin: [
                /for/,
                /\s+/,
                UNDERSCORE_IDENT_RE2,
                /\s+/,
                /in/
              ],
              scope: {
                1: "keyword",
                3: "variable",
                5: "keyword"
              }
            },
            {
              begin: [
                /type/,
                /\s+/,
                UNDERSCORE_IDENT_RE2
              ],
              scope: {
                1: "keyword",
                3: "title.class"
              }
            },
            {
              begin: [
                /(?:trait|enum|struct|union|impl|for)/,
                /\s+/,
                UNDERSCORE_IDENT_RE2
              ],
              scope: {
                1: "keyword",
                3: "title.class"
              }
            },
            {
              begin: hljs3.IDENT_RE + "::",
              keywords: {
                keyword: "Self",
                built_in: BUILTINS,
                type: TYPES2
              }
            },
            {
              scope: "punctuation",
              begin: "->"
            },
            FUNCTION_INVOKE
          ]
        };
      }
      function r(hljs3) {
        const regex = hljs3.regex;
        const IDENT_RE2 = /(?:(?:[a-zA-Z]|\.[._a-zA-Z])[._a-zA-Z0-9]*)|\.(?!\d)/;
        const NUMBER_TYPES_RE = regex.either(
          // Special case: only hexadecimal binary powers can contain fractions
          /0[xX][0-9a-fA-F]+\.[0-9a-fA-F]*[pP][+-]?\d+i?/,
          // Hexadecimal numbers without fraction and optional binary power
          /0[xX][0-9a-fA-F]+(?:[pP][+-]?\d+)?[Li]?/,
          // Decimal numbers
          /(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?[Li]?/
        );
        const OPERATORS_RE = /[=!<>:]=|\|\||&&|:::?|<-|<<-|->>|->|\|>|[-+*\/?!$&|:<=>@^~]|\*\*/;
        const PUNCTUATION_RE = regex.either(
          /[()]/,
          /[{}]/,
          /\[\[/,
          /[[\]]/,
          /\\/,
          /,/
        );
        return {
          name: "R",
          keywords: {
            $pattern: IDENT_RE2,
            keyword: "function if in break next repeat else for while",
            literal: "NULL NA TRUE FALSE Inf NaN NA_integer_|10 NA_real_|10 NA_character_|10 NA_complex_|10",
            built_in: (
              // Builtin constants
              "LETTERS letters month.abb month.name pi T F abs acos acosh all any anyNA Arg as.call as.character as.complex as.double as.environment as.integer as.logical as.null.default as.numeric as.raw asin asinh atan atanh attr attributes baseenv browser c call ceiling class Conj cos cosh cospi cummax cummin cumprod cumsum digamma dim dimnames emptyenv exp expression floor forceAndCall gamma gc.time globalenv Im interactive invisible is.array is.atomic is.call is.character is.complex is.double is.environment is.expression is.finite is.function is.infinite is.integer is.language is.list is.logical is.matrix is.na is.name is.nan is.null is.numeric is.object is.pairlist is.raw is.recursive is.single is.symbol lazyLoadDBfetch length lgamma list log max min missing Mod names nargs nzchar oldClass on.exit pos.to.env proc.time prod quote range Re rep retracemem return round seq_along seq_len seq.int sign signif sin sinh sinpi sqrt standardGeneric substitute sum switch tan tanh tanpi tracemem trigamma trunc unclass untracemem UseMethod xtfrm"
            )
          },
          contains: [
            // Roxygen comments
            hljs3.COMMENT(
              /#'/,
              /$/,
              { contains: [
                {
                  // Handle `@examples` separately to cause all subsequent code
                  // until the next `@`-tag on its own line to be kept as-is,
                  // preventing highlighting. This code is example R code, so nested
                  // doctags shouldn’t be treated as such. See
                  // `test/markup/r/roxygen.txt` for an example.
                  scope: "doctag",
                  match: /@examples/,
                  starts: {
                    end: regex.lookahead(regex.either(
                      // end if another doc comment
                      /\n^#'\s*(?=@[a-zA-Z]+)/,
                      // or a line with no comment
                      /\n^(?!#')/
                    )),
                    endsParent: true
                  }
                },
                {
                  // Handle `@param` to highlight the parameter name following
                  // after.
                  scope: "doctag",
                  begin: "@param",
                  end: /$/,
                  contains: [
                    {
                      scope: "variable",
                      variants: [
                        { match: IDENT_RE2 },
                        { match: /`(?:\\.|[^`\\])+`/ }
                      ],
                      endsParent: true
                    }
                  ]
                },
                {
                  scope: "doctag",
                  match: /@[a-zA-Z]+/
                },
                {
                  scope: "keyword",
                  match: /\\[a-zA-Z]+/
                }
              ] }
            ),
            hljs3.HASH_COMMENT_MODE,
            {
              scope: "string",
              contains: [hljs3.BACKSLASH_ESCAPE],
              variants: [
                hljs3.END_SAME_AS_BEGIN({
                  begin: /[rR]"(-*)\(/,
                  end: /\)(-*)"/
                }),
                hljs3.END_SAME_AS_BEGIN({
                  begin: /[rR]"(-*)\{/,
                  end: /\}(-*)"/
                }),
                hljs3.END_SAME_AS_BEGIN({
                  begin: /[rR]"(-*)\[/,
                  end: /\](-*)"/
                }),
                hljs3.END_SAME_AS_BEGIN({
                  begin: /[rR]'(-*)\(/,
                  end: /\)(-*)'/
                }),
                hljs3.END_SAME_AS_BEGIN({
                  begin: /[rR]'(-*)\{/,
                  end: /\}(-*)'/
                }),
                hljs3.END_SAME_AS_BEGIN({
                  begin: /[rR]'(-*)\[/,
                  end: /\](-*)'/
                }),
                {
                  begin: '"',
                  end: '"',
                  relevance: 0
                },
                {
                  begin: "'",
                  end: "'",
                  relevance: 0
                }
              ]
            },
            // Matching numbers immediately following punctuation and operators is
            // tricky since we need to look at the character ahead of a number to
            // ensure the number is not part of an identifier, and we cannot use
            // negative look-behind assertions. So instead we explicitly handle all
            // possible combinations of (operator|punctuation), number.
            // TODO: replace with negative look-behind when available
            // { begin: /(?<![a-zA-Z0-9._])0[xX][0-9a-fA-F]+\.[0-9a-fA-F]*[pP][+-]?\d+i?/ },
            // { begin: /(?<![a-zA-Z0-9._])0[xX][0-9a-fA-F]+([pP][+-]?\d+)?[Li]?/ },
            // { begin: /(?<![a-zA-Z0-9._])(\d+(\.\d*)?|\.\d+)([eE][+-]?\d+)?[Li]?/ }
            {
              relevance: 0,
              variants: [
                {
                  scope: {
                    1: "operator",
                    2: "number"
                  },
                  match: [
                    OPERATORS_RE,
                    NUMBER_TYPES_RE
                  ]
                },
                {
                  scope: {
                    1: "operator",
                    2: "number"
                  },
                  match: [
                    /%[^%]*%/,
                    NUMBER_TYPES_RE
                  ]
                },
                {
                  scope: {
                    1: "punctuation",
                    2: "number"
                  },
                  match: [
                    PUNCTUATION_RE,
                    NUMBER_TYPES_RE
                  ]
                },
                {
                  scope: { 2: "number" },
                  match: [
                    /[^a-zA-Z0-9._]|^/,
                    // not part of an identifier, or start of document
                    NUMBER_TYPES_RE
                  ]
                }
              ]
            },
            // Operators/punctuation when they're not directly followed by numbers
            {
              // Relevance boost for the most common assignment form.
              scope: { 3: "operator" },
              match: [
                IDENT_RE2,
                /\s+/,
                /<-/,
                /\s+/
              ]
            },
            {
              scope: "operator",
              relevance: 0,
              variants: [
                { match: OPERATORS_RE },
                { match: /%[^%]*%/ }
              ]
            },
            {
              scope: "punctuation",
              relevance: 0,
              match: PUNCTUATION_RE
            },
            {
              // Escaped identifier
              begin: "`",
              end: "`",
              contains: [{ begin: /\\./ }]
            }
          ]
        };
      }
      function python(hljs3) {
        const regex = hljs3.regex;
        const IDENT_RE2 = /[\p{XID_Start}_]\p{XID_Continue}*/u;
        const RESERVED_WORDS = [
          "and",
          "as",
          "assert",
          "async",
          "await",
          "break",
          "case",
          "class",
          "continue",
          "def",
          "del",
          "elif",
          "else",
          "except",
          "finally",
          "for",
          "from",
          "global",
          "if",
          "import",
          "in",
          "is",
          "lambda",
          "lazy",
          "match",
          "nonlocal|10",
          "not",
          "or",
          "pass",
          "raise",
          "return",
          "try",
          "while",
          "with",
          "yield"
        ];
        const BUILT_INS2 = [
          "__import__",
          "abs",
          "aiter",
          "all",
          "anext",
          "any",
          "ascii",
          "bin",
          "bool",
          "breakpoint",
          "bytearray",
          "bytes",
          "callable",
          "chr",
          "classmethod",
          "compile",
          "complex",
          "delattr",
          "dict",
          "dir",
          "divmod",
          "enumerate",
          "eval",
          "exec",
          "filter",
          "float",
          "format",
          "frozendict",
          "frozenset",
          "getattr",
          "globals",
          "hasattr",
          "hash",
          "help",
          "hex",
          "id",
          "input",
          "int",
          "isinstance",
          "issubclass",
          "iter",
          "len",
          "list",
          "locals",
          "map",
          "max",
          "memoryview",
          "min",
          "next",
          "object",
          "oct",
          "open",
          "ord",
          "pow",
          "print",
          "property",
          "range",
          "repr",
          "reversed",
          "round",
          "sentinel",
          "set",
          "setattr",
          "slice",
          "sorted",
          "staticmethod",
          "str",
          "sum",
          "super",
          "tuple",
          "type",
          "vars",
          "zip"
        ];
        const LITERALS2 = [
          "__debug__",
          "Ellipsis",
          "False",
          "None",
          "NotImplemented",
          "True"
        ];
        const TYPES2 = [
          "Any",
          "Callable",
          "Coroutine",
          "Dict",
          "List",
          "Literal",
          "Generic",
          "Optional",
          "Sequence",
          "Set",
          "Tuple",
          "Type",
          "Union"
        ];
        const KEYWORDS2 = {
          $pattern: /[A-Za-z]\w+|__\w+__/,
          keyword: RESERVED_WORDS,
          built_in: BUILT_INS2,
          literal: LITERALS2,
          type: TYPES2
        };
        const PROMPT = {
          className: "meta",
          begin: /^(>>>|\.\.\.) /
        };
        const SUBST = {
          className: "subst",
          begin: /\{/,
          end: /\}/,
          keywords: KEYWORDS2,
          illegal: /#/
        };
        const LITERAL_BRACKET = {
          begin: /\{\{/,
          relevance: 0
        };
        const STRING = {
          className: "string",
          contains: [hljs3.BACKSLASH_ESCAPE],
          variants: [
            {
              begin: /([uU]|[bB]|[rR]|[bB][rR]|[rR][bB])?'''/,
              end: /'''/,
              contains: [
                hljs3.BACKSLASH_ESCAPE,
                PROMPT
              ],
              relevance: 10
            },
            {
              begin: /([uU]|[bB]|[rR]|[bB][rR]|[rR][bB])?"""/,
              end: /"""/,
              contains: [
                hljs3.BACKSLASH_ESCAPE,
                PROMPT
              ],
              relevance: 10
            },
            {
              begin: /([fFtT][rR]|[rR][fFtT]|[fFtT])'''/,
              end: /'''/,
              contains: [
                hljs3.BACKSLASH_ESCAPE,
                PROMPT,
                LITERAL_BRACKET,
                SUBST
              ]
            },
            {
              begin: /([fFtT][rR]|[rR][fFtT]|[fFtT])"""/,
              end: /"""/,
              contains: [
                hljs3.BACKSLASH_ESCAPE,
                PROMPT,
                LITERAL_BRACKET,
                SUBST
              ]
            },
            {
              begin: /([uU]|[rR])'/,
              end: /'/,
              relevance: 10
            },
            {
              begin: /([uU]|[rR])"/,
              end: /"/,
              relevance: 10
            },
            {
              begin: /([bB]|[bB][rR]|[rR][bB])'/,
              end: /'/
            },
            {
              begin: /([bB]|[bB][rR]|[rR][bB])"/,
              end: /"/
            },
            {
              begin: /([fFtT][rR]|[rR][fFtT]|[fFtT])'/,
              end: /'/,
              contains: [
                hljs3.BACKSLASH_ESCAPE,
                LITERAL_BRACKET,
                SUBST
              ]
            },
            {
              begin: /([fFtT][rR]|[rR][fFtT]|[fFtT])"/,
              end: /"/,
              contains: [
                hljs3.BACKSLASH_ESCAPE,
                LITERAL_BRACKET,
                SUBST
              ]
            },
            hljs3.APOS_STRING_MODE,
            hljs3.QUOTE_STRING_MODE
          ]
        };
        const digitpart = "[0-9](_?[0-9])*";
        const pointfloat = `(\\b(${digitpart}))?\\.(${digitpart})|\\b(${digitpart})\\.`;
        const lookahead2 = `\\b|${RESERVED_WORDS.join("|")}`;
        const NUMBER = {
          className: "number",
          relevance: 0,
          variants: [
            // exponentfloat, pointfloat
            // https://docs.python.org/3.9/reference/lexical_analysis.html#floating-point-literals
            // optionally imaginary
            // https://docs.python.org/3.9/reference/lexical_analysis.html#imaginary-literals
            // Note: no leading \b because floats can start with a decimal point
            // and we don't want to mishandle e.g. `fn(.5)`,
            // no trailing \b for pointfloat because it can end with a decimal point
            // and we don't want to mishandle e.g. `0..hex()`; this should be safe
            // because both MUST contain a decimal point and so cannot be confused with
            // the interior part of an identifier
            {
              begin: `(\\b(${digitpart})|(${pointfloat}))[eE][+-]?(${digitpart})[jJ]?(?=${lookahead2})`
            },
            {
              begin: `(${pointfloat})[jJ]?`
            },
            // decinteger, bininteger, octinteger, hexinteger
            // https://docs.python.org/3.9/reference/lexical_analysis.html#integer-literals
            // optionally "long" in Python 2
            // https://docs.python.org/2.7/reference/lexical_analysis.html#integer-and-long-integer-literals
            // decinteger is optionally imaginary
            // https://docs.python.org/3.9/reference/lexical_analysis.html#imaginary-literals
            {
              begin: `\\b([1-9](_?[0-9])*|0+(_?0)*)[lLjJ]?(?=${lookahead2})`
            },
            {
              begin: `\\b0[bB](_?[01])+[lL]?(?=${lookahead2})`
            },
            {
              begin: `\\b0[oO](_?[0-7])+[lL]?(?=${lookahead2})`
            },
            {
              begin: `\\b0[xX](_?[0-9a-fA-F])+[lL]?(?=${lookahead2})`
            },
            // imagnumber (digitpart-based)
            // https://docs.python.org/3.9/reference/lexical_analysis.html#imaginary-literals
            {
              begin: `\\b(${digitpart})[jJ](?=${lookahead2})`
            }
          ]
        };
        const COMMENT_TYPE = {
          className: "comment",
          begin: regex.lookahead(/# type:/),
          end: /$/,
          keywords: KEYWORDS2,
          contains: [
            {
              // prevent keywords from coloring `type`
              begin: /# type:/
            },
            // comment within a datatype comment includes no keywords
            {
              begin: /#/,
              end: /\b\B/,
              endsWithParent: true
            }
          ]
        };
        const PARAMS = {
          className: "params",
          variants: [
            // Exclude params in functions without params
            {
              className: "",
              begin: /\(\s*\)/,
              skip: true
            },
            {
              begin: /\(/,
              end: /\)/,
              excludeBegin: true,
              excludeEnd: true,
              keywords: KEYWORDS2,
              contains: [
                "self",
                PROMPT,
                NUMBER,
                STRING,
                hljs3.HASH_COMMENT_MODE
              ]
            }
          ]
        };
        SUBST.contains = [
          STRING,
          NUMBER,
          PROMPT
        ];
        return {
          name: "Python",
          aliases: [
            "py",
            "gyp",
            "ipython"
          ],
          unicodeRegex: true,
          keywords: KEYWORDS2,
          illegal: /(<\/|\?)|=>/,
          contains: [
            PROMPT,
            NUMBER,
            {
              // very common convention
              scope: "variable.language",
              match: /\bself\b/
            },
            {
              // eat "if" prior to string so that it won't accidentally be
              // labeled as an f-string
              beginKeywords: "if",
              relevance: 0
            },
            { match: /\bor\b/, scope: "keyword" },
            STRING,
            COMMENT_TYPE,
            hljs3.HASH_COMMENT_MODE,
            {
              match: [
                /\bdef/,
                /\s+/,
                IDENT_RE2
              ],
              scope: {
                1: "keyword",
                3: "title.function"
              },
              contains: [PARAMS]
            },
            {
              variants: [
                {
                  match: [
                    /\bclass/,
                    /\s+/,
                    IDENT_RE2,
                    /\s*/,
                    /\(\s*/,
                    IDENT_RE2,
                    /\s*\)/
                  ]
                },
                {
                  match: [
                    /\bclass/,
                    /\s+/,
                    IDENT_RE2
                  ]
                }
              ],
              scope: {
                1: "keyword",
                3: "title.class",
                6: "title.class.inherited"
              }
            },
            {
              className: "meta",
              begin: /^[\t ]*@/,
              end: /(?=#)|$/,
              contains: [
                NUMBER,
                PARAMS,
                STRING
              ]
            }
          ]
        };
      }
      function pythonRepl(hljs3) {
        return {
          aliases: ["pycon"],
          contains: [
            {
              className: "meta.prompt",
              starts: {
                // a space separates the REPL prefix from the actual code
                // this is purely for cleaner HTML output
                end: / |$/,
                starts: {
                  end: "$",
                  subLanguage: "python"
                }
              },
              variants: [
                { begin: /^>>>(?=[ ]|$)/ },
                { begin: /^\.\.\.(?=[ ]|$)/ }
              ]
            }
          ]
        };
      }
      function plaintext(hljs3) {
        return {
          name: "Plain text",
          aliases: [
            "text",
            "txt"
          ],
          disableAutodetect: true
        };
      }
      function php(hljs3) {
        const regex = hljs3.regex;
        const NOT_PERL_ETC = /(?![A-Za-z0-9])(?![$])/;
        const IDENT_RE2 = regex.concat(
          /[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*/,
          NOT_PERL_ETC
        );
        const PASCAL_CASE_CLASS_NAME_RE = regex.concat(
          /(\\?[A-Z][a-z0-9_\x7f-\xff]+|\\?[A-Z]+(?=[A-Z][a-z0-9_\x7f-\xff])){1,}/,
          NOT_PERL_ETC
        );
        const UPCASE_NAME_RE = regex.concat(
          /[A-Z]+/,
          NOT_PERL_ETC
        );
        const VARIABLE = {
          scope: "variable",
          match: "\\$+" + IDENT_RE2
        };
        const PREPROCESSOR = {
          scope: "meta",
          variants: [
            { begin: /<\?php/, relevance: 10 },
            // boost for obvious PHP
            { begin: /<\?=/ },
            // less relevant per PSR-1 which says not to use short-tags
            { begin: /<\?/, relevance: 0.1 },
            { begin: /\?>/ }
            // end php tag
          ]
        };
        const SUBST = {
          scope: "subst",
          variants: [
            { begin: /\$\w+/ },
            {
              begin: /\{\$/,
              end: /\}/
            }
          ]
        };
        const SINGLE_QUOTED = hljs3.inherit(hljs3.APOS_STRING_MODE, { illegal: null });
        const DOUBLE_QUOTED = hljs3.inherit(hljs3.QUOTE_STRING_MODE, {
          illegal: null,
          contains: hljs3.QUOTE_STRING_MODE.contains.concat(SUBST)
        });
        const HEREDOC = {
          begin: /<<<[ \t]*(?:(\w+)|"(\w+)")\n/,
          end: /[ \t]*(\w+)\b/,
          contains: hljs3.QUOTE_STRING_MODE.contains.concat(SUBST),
          "on:begin": (m, resp) => {
            resp.data._beginMatch = m[1] || m[2];
          },
          "on:end": (m, resp) => {
            if (resp.data._beginMatch !== m[1]) resp.ignoreMatch();
          }
        };
        const NOWDOC = hljs3.END_SAME_AS_BEGIN({
          begin: /<<<[ \t]*'(\w+)'\n/,
          end: /[ \t]*(\w+)\b/
        });
        const WHITESPACE = "[ 	\n]";
        const STRING = {
          scope: "string",
          variants: [
            DOUBLE_QUOTED,
            SINGLE_QUOTED,
            HEREDOC,
            NOWDOC
          ]
        };
        const NUMBER = {
          scope: "number",
          variants: [
            { begin: `\\b0[bB][01]+(?:_[01]+)*\\b` },
            // Binary w/ underscore support
            { begin: `\\b0[oO][0-7]+(?:_[0-7]+)*\\b` },
            // Octals w/ underscore support
            { begin: `\\b0[xX][\\da-fA-F]+(?:_[\\da-fA-F]+)*\\b` },
            // Hex w/ underscore support
            // Decimals w/ underscore support, with optional fragments and scientific exponent (e) suffix.
            { begin: `(?:\\b\\d+(?:_\\d+)*(\\.(?:\\d+(?:_\\d+)*))?|\\B\\.\\d+)(?:[eE][+-]?\\d+)?` }
          ],
          relevance: 0
        };
        const LITERALS2 = [
          "false",
          "null",
          "true"
        ];
        const KWS = [
          // Magic constants:
          // <https://www.php.net/manual/en/language.constants.predefined.php>
          "__CLASS__",
          "__DIR__",
          "__FILE__",
          "__FUNCTION__",
          "__COMPILER_HALT_OFFSET__",
          "__LINE__",
          "__METHOD__",
          "__NAMESPACE__",
          "__TRAIT__",
          // Function that look like language construct or language construct that look like function:
          // List of keywords that may not require parenthesis
          "die",
          "echo",
          "exit",
          "include",
          "include_once",
          "print",
          "require",
          "require_once",
          // These are not language construct (function) but operate on the currently-executing function and can access the current symbol table
          // 'compact extract func_get_arg func_get_args func_num_args get_called_class get_parent_class ' +
          // Other keywords:
          // <https://www.php.net/manual/en/reserved.php>
          // <https://www.php.net/manual/en/language.types.type-juggling.php>
          "array",
          "abstract",
          "and",
          "as",
          "binary",
          "bool",
          "boolean",
          "break",
          "callable",
          "case",
          "catch",
          "class",
          "clone",
          "const",
          "continue",
          "declare",
          "default",
          "do",
          "double",
          "else",
          "elseif",
          "empty",
          "enddeclare",
          "endfor",
          "endforeach",
          "endif",
          "endswitch",
          "endwhile",
          "enum",
          "eval",
          "extends",
          "final",
          "finally",
          "float",
          "for",
          "foreach",
          "from",
          "global",
          "goto",
          "if",
          "implements",
          "instanceof",
          "insteadof",
          "int",
          "integer",
          "interface",
          "isset",
          "iterable",
          "list",
          "match|0",
          "mixed",
          "new",
          "never",
          "object",
          "or",
          "private",
          "protected",
          "public",
          "readonly",
          "real",
          "return",
          "string",
          "switch",
          "throw",
          "trait",
          "try",
          "unset",
          "use",
          "var",
          "void",
          "while",
          "xor",
          "yield"
        ];
        const BUILT_INS2 = [
          // Standard PHP library:
          // <https://www.php.net/manual/en/book.spl.php>
          "Error|0",
          "AppendIterator",
          "ArgumentCountError",
          "ArithmeticError",
          "ArrayIterator",
          "ArrayObject",
          "AssertionError",
          "BadFunctionCallException",
          "BadMethodCallException",
          "CachingIterator",
          "CallbackFilterIterator",
          "CompileError",
          "Countable",
          "DirectoryIterator",
          "DivisionByZeroError",
          "DomainException",
          "EmptyIterator",
          "ErrorException",
          "Exception",
          "FilesystemIterator",
          "FilterIterator",
          "GlobIterator",
          "InfiniteIterator",
          "InvalidArgumentException",
          "IteratorIterator",
          "LengthException",
          "LimitIterator",
          "LogicException",
          "MultipleIterator",
          "NoRewindIterator",
          "OutOfBoundsException",
          "OutOfRangeException",
          "OuterIterator",
          "OverflowException",
          "ParentIterator",
          "ParseError",
          "RangeException",
          "RecursiveArrayIterator",
          "RecursiveCachingIterator",
          "RecursiveCallbackFilterIterator",
          "RecursiveDirectoryIterator",
          "RecursiveFilterIterator",
          "RecursiveIterator",
          "RecursiveIteratorIterator",
          "RecursiveRegexIterator",
          "RecursiveTreeIterator",
          "RegexIterator",
          "RuntimeException",
          "SeekableIterator",
          "SplDoublyLinkedList",
          "SplFileInfo",
          "SplFileObject",
          "SplFixedArray",
          "SplHeap",
          "SplMaxHeap",
          "SplMinHeap",
          "SplObjectStorage",
          "SplObserver",
          "SplPriorityQueue",
          "SplQueue",
          "SplStack",
          "SplSubject",
          "SplTempFileObject",
          "TypeError",
          "UnderflowException",
          "UnexpectedValueException",
          "UnhandledMatchError",
          // Reserved interfaces:
          // <https://www.php.net/manual/en/reserved.interfaces.php>
          "ArrayAccess",
          "BackedEnum",
          "Closure",
          "Fiber",
          "Generator",
          "Iterator",
          "IteratorAggregate",
          "Serializable",
          "Stringable",
          "Throwable",
          "Traversable",
          "UnitEnum",
          "WeakReference",
          "WeakMap",
          // Reserved classes:
          // <https://www.php.net/manual/en/reserved.classes.php>
          "Directory",
          "__PHP_Incomplete_Class",
          "parent",
          "php_user_filter",
          "self",
          "static",
          "stdClass"
        ];
        const dualCase = (items) => {
          const result = [];
          items.forEach((item) => {
            result.push(item);
            if (item.toLowerCase() === item) {
              result.push(item.toUpperCase());
            } else {
              result.push(item.toLowerCase());
            }
          });
          return result;
        };
        const KEYWORDS2 = {
          keyword: KWS,
          literal: dualCase(LITERALS2),
          built_in: BUILT_INS2
        };
        const normalizeKeywords = (items) => {
          return items.map((item) => {
            return item.replace(/\|\d+$/, "");
          });
        };
        const CONSTRUCTOR_CALL = { variants: [
          {
            match: [
              /new/,
              regex.concat(WHITESPACE, "+"),
              // to prevent built ins from being confused as the class constructor call
              regex.concat("(?!", normalizeKeywords(BUILT_INS2).join("\\b|"), "\\b)"),
              PASCAL_CASE_CLASS_NAME_RE
            ],
            scope: {
              1: "keyword",
              4: "title.class"
            }
          }
        ] };
        const CONSTANT_REFERENCE = regex.concat(IDENT_RE2, "\\b(?!\\()");
        const LEFT_AND_RIGHT_SIDE_OF_DOUBLE_COLON = { variants: [
          {
            match: [
              regex.concat(
                /::/,
                regex.lookahead(/(?!class\b)/)
              ),
              CONSTANT_REFERENCE
            ],
            scope: { 2: "variable.constant" }
          },
          {
            match: [
              /::/,
              /class/
            ],
            scope: { 2: "variable.language" }
          },
          {
            match: [
              PASCAL_CASE_CLASS_NAME_RE,
              regex.concat(
                /::/,
                regex.lookahead(/(?!class\b)/)
              ),
              CONSTANT_REFERENCE
            ],
            scope: {
              1: "title.class",
              3: "variable.constant"
            }
          },
          {
            match: [
              PASCAL_CASE_CLASS_NAME_RE,
              regex.concat(
                "::",
                regex.lookahead(/(?!class\b)/)
              )
            ],
            scope: { 1: "title.class" }
          },
          {
            match: [
              PASCAL_CASE_CLASS_NAME_RE,
              /::/,
              /class/
            ],
            scope: {
              1: "title.class",
              3: "variable.language"
            }
          }
        ] };
        const NAMED_ARGUMENT = {
          scope: "attr",
          match: regex.concat(IDENT_RE2, regex.lookahead(":"), regex.lookahead(/(?!::)/))
        };
        const PARAMS_MODE = {
          relevance: 0,
          begin: /\(/,
          end: /\)/,
          keywords: KEYWORDS2,
          contains: [
            NAMED_ARGUMENT,
            VARIABLE,
            LEFT_AND_RIGHT_SIDE_OF_DOUBLE_COLON,
            hljs3.C_BLOCK_COMMENT_MODE,
            hljs3.C_LINE_COMMENT_MODE,
            hljs3.HASH_COMMENT_MODE,
            STRING,
            NUMBER,
            CONSTRUCTOR_CALL
          ]
        };
        const FUNCTION_INVOKE = {
          relevance: 0,
          match: [
            /\b/,
            // to prevent keywords from being confused as the function title
            regex.concat("(?!fn\\b|function\\b|", normalizeKeywords(KWS).join("\\b|"), "|", normalizeKeywords(BUILT_INS2).join("\\b|"), "\\b)"),
            IDENT_RE2,
            regex.concat(WHITESPACE, "*"),
            regex.lookahead(/(?=\()/)
          ],
          scope: { 3: "title.function.invoke" },
          contains: [PARAMS_MODE]
        };
        PARAMS_MODE.contains.push(FUNCTION_INVOKE);
        const ATTRIBUTE_CONTAINS = [
          NAMED_ARGUMENT,
          LEFT_AND_RIGHT_SIDE_OF_DOUBLE_COLON,
          hljs3.C_BLOCK_COMMENT_MODE,
          hljs3.C_LINE_COMMENT_MODE,
          hljs3.HASH_COMMENT_MODE,
          STRING,
          NUMBER,
          CONSTRUCTOR_CALL
        ];
        const ATTRIBUTES2 = {
          begin: regex.concat(
            /#\[\s*\\?/,
            regex.either(
              PASCAL_CASE_CLASS_NAME_RE,
              UPCASE_NAME_RE
            )
          ),
          beginScope: "meta",
          end: /]/,
          endScope: "meta",
          keywords: {
            literal: LITERALS2,
            keyword: [
              "new",
              "array"
            ]
          },
          contains: [
            {
              begin: /\[/,
              end: /]/,
              keywords: {
                literal: LITERALS2,
                keyword: [
                  "new",
                  "array"
                ]
              },
              contains: [
                "self",
                ...ATTRIBUTE_CONTAINS
              ]
            },
            ...ATTRIBUTE_CONTAINS,
            {
              scope: "meta",
              variants: [
                { match: PASCAL_CASE_CLASS_NAME_RE },
                { match: UPCASE_NAME_RE }
              ]
            }
          ]
        };
        return {
          case_insensitive: false,
          keywords: KEYWORDS2,
          contains: [
            ATTRIBUTES2,
            hljs3.HASH_COMMENT_MODE,
            hljs3.COMMENT("//", "$"),
            hljs3.COMMENT(
              "/\\*",
              "\\*/",
              { contains: [
                {
                  scope: "doctag",
                  match: "@[A-Za-z]+"
                }
              ] }
            ),
            {
              match: /__halt_compiler\(\);/,
              keywords: "__halt_compiler",
              starts: {
                scope: "comment",
                end: hljs3.MATCH_NOTHING_RE,
                contains: [
                  {
                    match: /\?>/,
                    scope: "meta",
                    endsParent: true
                  }
                ]
              }
            },
            PREPROCESSOR,
            {
              scope: "variable.language",
              match: /\$this\b/
            },
            VARIABLE,
            FUNCTION_INVOKE,
            LEFT_AND_RIGHT_SIDE_OF_DOUBLE_COLON,
            {
              match: [
                /const/,
                /\s/,
                IDENT_RE2
              ],
              scope: {
                1: "keyword",
                3: "variable.constant"
              }
            },
            CONSTRUCTOR_CALL,
            {
              scope: "function",
              relevance: 0,
              beginKeywords: "fn function",
              end: /[;{]/,
              excludeEnd: true,
              illegal: "[$%\\[]",
              contains: [
                { beginKeywords: "use" },
                hljs3.UNDERSCORE_TITLE_MODE,
                {
                  begin: "=>",
                  // No markup, just a relevance booster
                  endsParent: true
                },
                {
                  scope: "params",
                  begin: "\\(",
                  end: "\\)",
                  excludeBegin: true,
                  excludeEnd: true,
                  keywords: KEYWORDS2,
                  contains: [
                    "self",
                    ATTRIBUTES2,
                    VARIABLE,
                    LEFT_AND_RIGHT_SIDE_OF_DOUBLE_COLON,
                    hljs3.C_BLOCK_COMMENT_MODE,
                    hljs3.C_LINE_COMMENT_MODE,
                    hljs3.HASH_COMMENT_MODE,
                    STRING,
                    NUMBER
                  ]
                }
              ]
            },
            {
              scope: "class",
              variants: [
                {
                  beginKeywords: "enum",
                  illegal: /[($"]/
                },
                {
                  beginKeywords: "class interface trait",
                  illegal: /[:($"]/
                }
              ],
              relevance: 0,
              end: /\{/,
              excludeEnd: true,
              contains: [
                { beginKeywords: "extends implements" },
                hljs3.UNDERSCORE_TITLE_MODE
              ]
            },
            // both use and namespace still use "old style" rules (vs multi-match)
            // because the namespace name can include `\` and we still want each
            // element to be treated as its own *individual* title
            {
              beginKeywords: "namespace",
              relevance: 0,
              end: ";",
              illegal: /[.']/,
              contains: [hljs3.inherit(hljs3.UNDERSCORE_TITLE_MODE, { scope: "title.class" })]
            },
            {
              beginKeywords: "use",
              relevance: 0,
              end: ";",
              contains: [
                // TODO: title.function vs title.class
                {
                  match: /\b(as|const|function)\b/,
                  scope: "keyword"
                },
                // TODO: could be title.class or title.function
                hljs3.UNDERSCORE_TITLE_MODE
              ]
            },
            STRING,
            NUMBER
          ]
        };
      }
      function phpTemplate(hljs3) {
        return {
          name: "PHP template",
          subLanguage: "xml",
          contains: [
            {
              begin: /<\?(php|=)?/,
              end: /\?>/,
              subLanguage: "php",
              contains: [
                // We don't want the php closing tag ?> to close the PHP block when
                // inside any of the following blocks:
                {
                  begin: "/\\*",
                  end: "\\*/",
                  skip: true
                },
                {
                  begin: 'b"',
                  end: '"',
                  skip: true
                },
                {
                  begin: "b'",
                  end: "'",
                  skip: true
                },
                hljs3.inherit(hljs3.APOS_STRING_MODE, {
                  illegal: null,
                  className: null,
                  contains: null,
                  skip: true
                }),
                hljs3.inherit(hljs3.QUOTE_STRING_MODE, {
                  illegal: null,
                  className: null,
                  contains: null,
                  skip: true
                })
              ]
            }
          ]
        };
      }
      function perl(hljs3) {
        const regex = hljs3.regex;
        const KEYWORDS2 = [
          "abs",
          "accept",
          "alarm",
          "and",
          "atan2",
          "bind",
          "binmode",
          "bless",
          "break",
          "caller",
          "chdir",
          "chmod",
          "chomp",
          "chop",
          "chown",
          "chr",
          "chroot",
          "class",
          "close",
          "closedir",
          "connect",
          "continue",
          "cos",
          "crypt",
          "dbmclose",
          "dbmopen",
          "defined",
          "delete",
          "die",
          "do",
          "dump",
          "each",
          "else",
          "elsif",
          "endgrent",
          "endhostent",
          "endnetent",
          "endprotoent",
          "endpwent",
          "endservent",
          "eof",
          "eval",
          "exec",
          "exists",
          "exit",
          "exp",
          "fcntl",
          "field",
          "fileno",
          "flock",
          "for",
          "foreach",
          "fork",
          "format",
          "formline",
          "getc",
          "getgrent",
          "getgrgid",
          "getgrnam",
          "gethostbyaddr",
          "gethostbyname",
          "gethostent",
          "getlogin",
          "getnetbyaddr",
          "getnetbyname",
          "getnetent",
          "getpeername",
          "getpgrp",
          "getpriority",
          "getprotobyname",
          "getprotobynumber",
          "getprotoent",
          "getpwent",
          "getpwnam",
          "getpwuid",
          "getservbyname",
          "getservbyport",
          "getservent",
          "getsockname",
          "getsockopt",
          "given",
          "glob",
          "gmtime",
          "goto",
          "grep",
          "gt",
          "hex",
          "if",
          "index",
          "int",
          "ioctl",
          "join",
          "keys",
          "kill",
          "last",
          "lc",
          "lcfirst",
          "length",
          "link",
          "listen",
          "local",
          "localtime",
          "log",
          "lstat",
          "lt",
          "ma",
          "map",
          "method",
          "mkdir",
          "msgctl",
          "msgget",
          "msgrcv",
          "msgsnd",
          "my",
          "ne",
          "next",
          "no",
          "not",
          "oct",
          "open",
          "opendir",
          "or",
          "ord",
          "our",
          "pack",
          "package",
          "pipe",
          "pop",
          "pos",
          "print",
          "printf",
          "prototype",
          "push",
          "q|0",
          "qq",
          "quotemeta",
          "qw",
          "qx",
          "rand",
          "read",
          "readdir",
          "readline",
          "readlink",
          "readpipe",
          "recv",
          "redo",
          "ref",
          "rename",
          "require",
          "reset",
          "return",
          "reverse",
          "rewinddir",
          "rindex",
          "rmdir",
          "say",
          "scalar",
          "seek",
          "seekdir",
          "select",
          "semctl",
          "semget",
          "semop",
          "send",
          "setgrent",
          "sethostent",
          "setnetent",
          "setpgrp",
          "setpriority",
          "setprotoent",
          "setpwent",
          "setservent",
          "setsockopt",
          "shift",
          "shmctl",
          "shmget",
          "shmread",
          "shmwrite",
          "shutdown",
          "sin",
          "sleep",
          "socket",
          "socketpair",
          "sort",
          "splice",
          "split",
          "sprintf",
          "sqrt",
          "srand",
          "stat",
          "state",
          "study",
          "sub",
          "substr",
          "symlink",
          "syscall",
          "sysopen",
          "sysread",
          "sysseek",
          "system",
          "syswrite",
          "tell",
          "telldir",
          "tie",
          "tied",
          "time",
          "times",
          "tr",
          "truncate",
          "uc",
          "ucfirst",
          "umask",
          "undef",
          "unless",
          "unlink",
          "unpack",
          "unshift",
          "untie",
          "until",
          "use",
          "utime",
          "values",
          "vec",
          "wait",
          "waitpid",
          "wantarray",
          "warn",
          "when",
          "while",
          "write",
          "x|0",
          "xor",
          "y|0"
        ];
        const REGEX_MODIFIERS = /[dualxmsipngr]{0,12}/;
        const PERL_KEYWORDS = {
          $pattern: /[\w.]+/,
          keyword: KEYWORDS2.join(" ")
        };
        const SUBST = {
          className: "subst",
          begin: "[$@]\\{",
          end: "\\}",
          keywords: PERL_KEYWORDS
        };
        const METHOD = {
          begin: /->\{/,
          end: /\}/
          // contains defined later
        };
        const ATTR = {
          scope: "attr",
          match: /\s+:\s*\w+(\s*\(.*?\))?/
        };
        const VAR = {
          scope: "variable",
          variants: [
            { begin: /\$\d/ },
            {
              begin: regex.concat(
                /[$%@](?!")(\^\w\b|#\w+(::\w+)*|\{\w+\}|\w+(::\w*)*)/,
                // negative look-ahead tries to avoid matching patterns that are not
                // Perl at all like $ident$, @ident@, etc.
                `(?![A-Za-z])(?![@$%])`
              )
            },
            {
              // Only $= is a special Perl variable and one can't declare @= or %=.
              begin: /[$%@](?!")[^\s\w{=]|\$=/,
              relevance: 0
            }
          ],
          contains: [ATTR]
        };
        const NUMBER = {
          className: "number",
          variants: [
            // decimal numbers:
            // include the case where a number starts with a dot (eg. .9), and
            // the leading 0? avoids mixing the first and second match on 0.x cases
            { match: /0?\.[0-9][0-9_]+\b/ },
            // include the special versioned number (eg. v5.38)
            { match: /\bv?(0|[1-9][0-9_]*(\.[0-9_]+)?|[1-9][0-9_]*)\b/ },
            // non-decimal numbers:
            { match: /\b0[0-7][0-7_]*\b/ },
            { match: /\b0x[0-9a-fA-F][0-9a-fA-F_]*\b/ },
            { match: /\b0b[0-1][0-1_]*\b/ }
          ],
          relevance: 0
        };
        const STRING_CONTAINS = [
          hljs3.BACKSLASH_ESCAPE,
          SUBST,
          VAR
        ];
        const REGEX_DELIMS = [
          /!/,
          /\//,
          /\|/,
          /\?/,
          /'/,
          /"/,
          // valid but infrequent and weird
          /#/
          // valid but infrequent and weird
        ];
        const PAIRED_DOUBLE_RE = (prefix, open, close = "\\1") => {
          const middle = close === "\\1" ? close : regex.concat(close, open);
          return regex.concat(
            regex.concat("(?:", prefix, ")"),
            open,
            /(?:\\.|[^\\\/])*?/,
            middle,
            /(?:\\.|[^\\\/])*?/,
            close,
            REGEX_MODIFIERS
          );
        };
        const PAIRED_RE = (prefix, open, close) => {
          return regex.concat(
            regex.concat("(?:", prefix, ")"),
            open,
            /(?:\\.|[^\\\/])*?/,
            close,
            REGEX_MODIFIERS
          );
        };
        const PERL_DEFAULT_CONTAINS = [
          VAR,
          hljs3.HASH_COMMENT_MODE,
          hljs3.COMMENT(
            /^=\w/,
            /=cut/,
            { endsWithParent: true }
          ),
          METHOD,
          {
            className: "string",
            contains: STRING_CONTAINS,
            variants: [
              {
                begin: "q[qwxr]?\\s*\\(",
                end: "\\)",
                relevance: 5
              },
              {
                begin: "q[qwxr]?\\s*\\[",
                end: "\\]",
                relevance: 5
              },
              {
                begin: "q[qwxr]?\\s*\\{",
                end: "\\}",
                relevance: 5
              },
              {
                begin: "q[qwxr]?\\s*\\|",
                end: "\\|",
                relevance: 5
              },
              {
                begin: "q[qwxr]?\\s*<",
                end: ">",
                relevance: 5
              },
              {
                begin: "qw\\s+q",
                end: "q",
                relevance: 5
              },
              {
                begin: "'",
                end: "'",
                contains: [hljs3.BACKSLASH_ESCAPE]
              },
              {
                begin: '"',
                end: '"'
              },
              {
                begin: "`",
                end: "`",
                contains: [hljs3.BACKSLASH_ESCAPE]
              },
              {
                begin: /\{\w+\}/,
                relevance: 0
              },
              {
                begin: "-?\\w+\\s*=>",
                relevance: 0
              }
            ]
          },
          NUMBER,
          {
            // regexp container
            begin: "(\\/\\/|" + hljs3.RE_STARTERS_RE + "|\\b(split|return|print|reverse|grep)\\b)\\s*",
            keywords: "split return print reverse grep",
            relevance: 0,
            contains: [
              hljs3.HASH_COMMENT_MODE,
              {
                className: "regexp",
                variants: [
                  // allow matching common delimiters
                  { begin: PAIRED_DOUBLE_RE("s|tr|y", regex.either(...REGEX_DELIMS, { capture: true })) },
                  // and then paired delmis
                  { begin: PAIRED_DOUBLE_RE("s|tr|y", "\\(", "\\)") },
                  { begin: PAIRED_DOUBLE_RE("s|tr|y", "\\[", "\\]") },
                  { begin: PAIRED_DOUBLE_RE("s|tr|y", "\\{", "\\}") }
                ],
                relevance: 2
              },
              {
                className: "regexp",
                variants: [
                  {
                    // could be a comment in many languages so do not count
                    // as relevant
                    begin: /(m|qr)\/\//,
                    relevance: 0
                  },
                  // prefix is optional with /regex/
                  { begin: PAIRED_RE("(?:m|qr)?", /\//, /\//) },
                  // allow matching common delimiters
                  { begin: PAIRED_RE("m|qr", regex.either(...REGEX_DELIMS, { capture: true }), /\1/) },
                  // allow common paired delmins
                  { begin: PAIRED_RE("m|qr", /\(/, /\)/) },
                  { begin: PAIRED_RE("m|qr", /\[/, /\]/) },
                  { begin: PAIRED_RE("m|qr", /\{/, /\}/) }
                ]
              }
            ]
          },
          {
            className: "function",
            beginKeywords: "sub method",
            end: "(\\s*\\(.*?\\))?[;{]",
            excludeEnd: true,
            relevance: 5,
            contains: [hljs3.TITLE_MODE, ATTR]
          },
          {
            className: "class",
            beginKeywords: "class",
            end: "[;{]",
            excludeEnd: true,
            relevance: 5,
            contains: [hljs3.TITLE_MODE, ATTR, NUMBER]
          },
          {
            begin: "-\\w\\b",
            relevance: 0
          },
          {
            begin: "^__DATA__$",
            end: "^__END__$",
            subLanguage: "mojolicious",
            contains: [
              {
                begin: "^@@.*",
                end: "$",
                className: "comment"
              }
            ]
          }
        ];
        SUBST.contains = PERL_DEFAULT_CONTAINS;
        METHOD.contains = PERL_DEFAULT_CONTAINS;
        return {
          name: "Perl",
          aliases: [
            "pl",
            "pm"
          ],
          keywords: PERL_KEYWORDS,
          contains: PERL_DEFAULT_CONTAINS
        };
      }
      function objectivec(hljs3) {
        const API_CLASS = {
          className: "built_in",
          begin: "\\b(AV|CA|CF|CG|CI|CL|CM|CN|CT|MK|MP|MTK|MTL|NS|SCN|SK|UI|WK|XC)\\w+"
        };
        const IDENTIFIER_RE = /[a-zA-Z@][a-zA-Z0-9_]*/;
        const TYPES2 = [
          "int",
          "float",
          "char",
          "unsigned",
          "signed",
          "short",
          "long",
          "double",
          "wchar_t",
          "unichar",
          "void",
          "bool",
          "BOOL",
          "id|0",
          "_Bool"
        ];
        const KWS = [
          "while",
          "export",
          "sizeof",
          "typedef",
          "const",
          "struct",
          "for",
          "union",
          "volatile",
          "static",
          "mutable",
          "if",
          "do",
          "return",
          "goto",
          "enum",
          "else",
          "break",
          "extern",
          "asm",
          "case",
          "default",
          "register",
          "explicit",
          "typename",
          "switch",
          "continue",
          "inline",
          "readonly",
          "assign",
          "readwrite",
          "self",
          "@synchronized",
          "id",
          "typeof",
          "nonatomic",
          "IBOutlet",
          "IBAction",
          "strong",
          "weak",
          "copy",
          "in",
          "out",
          "inout",
          "bycopy",
          "byref",
          "oneway",
          "__strong",
          "__weak",
          "__block",
          "__autoreleasing",
          "@private",
          "@protected",
          "@public",
          "@try",
          "@property",
          "@end",
          "@throw",
          "@catch",
          "@finally",
          "@autoreleasepool",
          "@synthesize",
          "@dynamic",
          "@selector",
          "@optional",
          "@required",
          "@encode",
          "@package",
          "@import",
          "@defs",
          "@compatibility_alias",
          "__bridge",
          "__bridge_transfer",
          "__bridge_retained",
          "__bridge_retain",
          "__covariant",
          "__contravariant",
          "__kindof",
          "_Nonnull",
          "_Nullable",
          "_Null_unspecified",
          "__FUNCTION__",
          "__PRETTY_FUNCTION__",
          "__attribute__",
          "getter",
          "setter",
          "retain",
          "unsafe_unretained",
          "nonnull",
          "nullable",
          "null_unspecified",
          "null_resettable",
          "class",
          "instancetype",
          "NS_DESIGNATED_INITIALIZER",
          "NS_UNAVAILABLE",
          "NS_REQUIRES_SUPER",
          "NS_RETURNS_INNER_POINTER",
          "NS_INLINE",
          "NS_AVAILABLE",
          "NS_DEPRECATED",
          "NS_ENUM",
          "NS_OPTIONS",
          "NS_SWIFT_UNAVAILABLE",
          "NS_ASSUME_NONNULL_BEGIN",
          "NS_ASSUME_NONNULL_END",
          "NS_REFINED_FOR_SWIFT",
          "NS_SWIFT_NAME",
          "NS_SWIFT_NOTHROW",
          "NS_DURING",
          "NS_HANDLER",
          "NS_ENDHANDLER",
          "NS_VALUERETURN",
          "NS_VOIDRETURN"
        ];
        const LITERALS2 = [
          "false",
          "true",
          "FALSE",
          "TRUE",
          "nil",
          "YES",
          "NO",
          "NULL"
        ];
        const BUILT_INS2 = [
          "dispatch_once_t",
          "dispatch_queue_t",
          "dispatch_sync",
          "dispatch_async",
          "dispatch_once"
        ];
        const KEYWORDS2 = {
          "variable.language": [
            "this",
            "super"
          ],
          $pattern: IDENTIFIER_RE,
          keyword: KWS,
          literal: LITERALS2,
          built_in: BUILT_INS2,
          type: TYPES2
        };
        const CLASS_KEYWORDS = {
          $pattern: IDENTIFIER_RE,
          keyword: [
            "@interface",
            "@class",
            "@protocol",
            "@implementation"
          ]
        };
        return {
          name: "Objective-C",
          aliases: [
            "mm",
            "objc",
            "obj-c",
            "obj-c++",
            "objective-c++"
          ],
          keywords: KEYWORDS2,
          illegal: "</",
          contains: [
            API_CLASS,
            hljs3.C_LINE_COMMENT_MODE,
            hljs3.C_BLOCK_COMMENT_MODE,
            hljs3.C_NUMBER_MODE,
            hljs3.QUOTE_STRING_MODE,
            hljs3.APOS_STRING_MODE,
            {
              className: "string",
              variants: [
                {
                  begin: '@"',
                  end: '"',
                  illegal: "\\n",
                  contains: [hljs3.BACKSLASH_ESCAPE]
                }
              ]
            },
            {
              className: "meta",
              begin: /#\s*[a-z]+\b/,
              end: /$/,
              keywords: { keyword: "if else elif endif define undef warning error line pragma ifdef ifndef include" },
              contains: [
                {
                  begin: /\\\n/,
                  relevance: 0
                },
                hljs3.inherit(hljs3.QUOTE_STRING_MODE, { className: "string" }),
                {
                  className: "string",
                  begin: /<.*?>/,
                  end: /$/,
                  illegal: "\\n"
                },
                hljs3.C_LINE_COMMENT_MODE,
                hljs3.C_BLOCK_COMMENT_MODE
              ]
            },
            {
              className: "class",
              begin: "(" + CLASS_KEYWORDS.keyword.join("|") + ")\\b",
              end: /(\{|$)/,
              excludeEnd: true,
              keywords: CLASS_KEYWORDS,
              contains: [hljs3.UNDERSCORE_TITLE_MODE]
            },
            {
              begin: "\\." + hljs3.UNDERSCORE_IDENT_RE,
              relevance: 0
            }
          ]
        };
      }
      function markdown(hljs3) {
        const regex = hljs3.regex;
        const INLINE_HTML = {
          begin: /<\/?[A-Za-z_]/,
          end: ">",
          subLanguage: "xml",
          relevance: 0
        };
        const HORIZONTAL_RULE = { match: /^ {0,3}([-*_])[ \t]*(?:\1[ \t]*){2,}$/ };
        const CODE = {
          className: "code",
          variants: [
            // TODO: fix to allow these to work with sublanguage also
            { begin: "(`{3,})[^`](.|\\n)*?\\1`*[ ]*" },
            { begin: "(~{3,})[^~](.|\\n)*?\\1~*[ ]*" },
            // needed to allow markdown as a sublanguage to work
            {
              begin: "```",
              end: "```+[ ]*$"
            },
            {
              begin: "~~~",
              end: "~~~+[ ]*$"
            },
            { begin: "`.+?`" },
            {
              begin: "(?=^( {4}|\\t))",
              // use contains to gobble up multiple lines to allow the block to be whatever size
              // but only have a single open/close tag vs one per line
              contains: [
                {
                  begin: "^( {4}|\\t)",
                  end: "(\\n)$"
                }
              ],
              relevance: 0
            }
          ]
        };
        const LIST = {
          className: "bullet",
          begin: "^[ 	]*([*+-]|(\\d+\\.))(?=\\s+)",
          end: "\\s+",
          excludeEnd: true
        };
        const LINK_REFERENCE = {
          begin: /^\[[^\n]+\]:/,
          returnBegin: true,
          contains: [
            {
              className: "symbol",
              begin: /\[/,
              end: /\]/,
              excludeBegin: true,
              excludeEnd: true
            },
            {
              className: "link",
              begin: /:\s*/,
              end: /$/,
              excludeBegin: true
            }
          ]
        };
        const URL_SCHEME = /[A-Za-z][A-Za-z0-9+.-]*/;
        const LINK = {
          variants: [
            // too much like nested array access in so many languages
            // to have any real relevance
            {
              begin: /\[.+?\]\[.*?\]/,
              relevance: 0
            },
            // popular internet URLs
            {
              begin: /\[.+?\]\(((data|javascript|mailto):|(?:http|ftp)s?:\/\/).*?\)/,
              relevance: 2
            },
            {
              begin: regex.concat(/\[.+?\]\(/, URL_SCHEME, /:\/\/.*?\)/),
              relevance: 2
            },
            // relative urls
            {
              begin: /\[.+?\]\([./?&#].*?\)/,
              relevance: 1
            },
            // whatever else, lower relevance (might not be a link at all)
            {
              begin: /\[.*?\]\(.*?\)/,
              relevance: 0
            }
          ],
          returnBegin: true,
          contains: [
            {
              // empty strings for alt or link text
              match: /\[(?=\])/
            },
            {
              className: "string",
              relevance: 0,
              begin: "\\[",
              end: "\\]",
              excludeBegin: true,
              returnEnd: true
            },
            {
              className: "link",
              relevance: 0,
              begin: "\\]\\(",
              end: "\\)",
              excludeBegin: true,
              excludeEnd: true
            },
            {
              className: "symbol",
              relevance: 0,
              begin: "\\]\\[",
              end: "\\]",
              excludeBegin: true,
              excludeEnd: true
            }
          ]
        };
        const BOLD = {
          className: "strong",
          contains: [],
          // defined later
          variants: [
            {
              begin: /_{2}(?!\s)/,
              end: /_{2}/
            },
            {
              begin: /\*{2}(?!\s)/,
              end: /\*{2}/
            }
          ]
        };
        const ITALIC = {
          className: "emphasis",
          contains: [],
          // defined later
          variants: [
            {
              begin: /\*(?![*\s])/,
              end: /\*/
            },
            {
              begin: /_(?![_\s])/,
              end: /_/,
              relevance: 0
            }
          ]
        };
        const BOLD_WITHOUT_ITALIC = hljs3.inherit(BOLD, { contains: [] });
        const ITALIC_WITHOUT_BOLD = hljs3.inherit(ITALIC, { contains: [] });
        BOLD.contains.push(ITALIC_WITHOUT_BOLD);
        ITALIC.contains.push(BOLD_WITHOUT_ITALIC);
        let CONTAINABLE = [
          INLINE_HTML,
          LINK
        ];
        [
          BOLD,
          ITALIC,
          BOLD_WITHOUT_ITALIC,
          ITALIC_WITHOUT_BOLD
        ].forEach((m) => {
          m.contains = m.contains.concat(CONTAINABLE);
        });
        CONTAINABLE = CONTAINABLE.concat(BOLD, ITALIC);
        const HEADER = {
          className: "section",
          variants: [
            {
              begin: "^#{1,6}",
              end: "$",
              contains: CONTAINABLE
            },
            {
              begin: "(?=^.+?\\n[=-]{2,}$)",
              contains: [
                { begin: "^[=-]*$" },
                {
                  begin: "^",
                  end: "\\n",
                  contains: CONTAINABLE
                }
              ]
            }
          ]
        };
        const BLOCKQUOTE = {
          className: "quote",
          begin: "^>\\s+",
          contains: CONTAINABLE,
          end: "$"
        };
        const ENTITY = {
          //https://spec.commonmark.org/0.31.2/#entity-references
          scope: "literal",
          match: /&([a-zA-Z0-9]+|#[0-9]{1,7}|#[Xx][0-9a-fA-F]{1,6});/
        };
        return {
          name: "Markdown",
          aliases: [
            "md",
            "mkdown",
            "mkd"
          ],
          contains: [
            HEADER,
            INLINE_HTML,
            LIST,
            // must come before BOLD/ITALIC so that a `***` or `___` thematic break
            // isn't mistaken for the start of bold text
            HORIZONTAL_RULE,
            BOLD,
            ITALIC,
            BLOCKQUOTE,
            CODE,
            LINK,
            LINK_REFERENCE,
            ENTITY
          ]
        };
      }
      function makefile(hljs3) {
        const VARIABLE = {
          className: "variable",
          variants: [
            {
              begin: "\\$\\(" + hljs3.UNDERSCORE_IDENT_RE + "\\)",
              contains: [hljs3.BACKSLASH_ESCAPE]
            },
            { begin: /\$[@%<?\^\+\*]/ }
          ]
        };
        const QUOTE_STRING = {
          className: "string",
          begin: /"/,
          end: /"/,
          contains: [
            hljs3.BACKSLASH_ESCAPE,
            VARIABLE
          ]
        };
        const FUNC = {
          className: "variable",
          begin: /\$\([\w-]+\s/,
          end: /\)/,
          keywords: { built_in: "subst patsubst strip findstring filter filter-out sort word wordlist firstword lastword dir notdir suffix basename addsuffix addprefix join wildcard realpath abspath error warning shell origin flavor foreach if or and call eval file value" },
          contains: [
            VARIABLE,
            QUOTE_STRING
            // Added QUOTE_STRING as they can be a part of functions
          ]
        };
        const ASSIGNMENT = { begin: "^" + hljs3.UNDERSCORE_IDENT_RE + "\\s*(?=[:+?]?=)" };
        const META = {
          className: "meta",
          begin: /^\.PHONY:/,
          end: /$/,
          keywords: {
            $pattern: /[\.\w]+/,
            keyword: ".PHONY"
          }
        };
        const TARGET = {
          className: "section",
          begin: /^[^\s]+:/,
          end: /$/,
          contains: [VARIABLE]
        };
        return {
          name: "Makefile",
          aliases: [
            "mk",
            "mak",
            "make"
          ],
          keywords: {
            $pattern: /[\w-]+/,
            keyword: "define endef undefine ifdef ifndef ifeq ifneq else endif include -include sinclude override export unexport private vpath"
          },
          contains: [
            hljs3.HASH_COMMENT_MODE,
            VARIABLE,
            QUOTE_STRING,
            FUNC,
            ASSIGNMENT,
            META,
            TARGET
          ]
        };
      }
      function lua(hljs3) {
        const OPENING_LONG_BRACKET = "\\[=*\\[";
        const CLOSING_LONG_BRACKET = "\\]=*\\]";
        const LONG_BRACKETS = {
          begin: OPENING_LONG_BRACKET,
          end: CLOSING_LONG_BRACKET,
          contains: ["self"]
        };
        const COMMENTS = [
          hljs3.COMMENT("--(?!" + OPENING_LONG_BRACKET + ")", "$"),
          hljs3.COMMENT(
            "--" + OPENING_LONG_BRACKET,
            CLOSING_LONG_BRACKET,
            {
              contains: [LONG_BRACKETS],
              relevance: 10
            }
          )
        ];
        return {
          name: "Lua",
          aliases: ["pluto"],
          keywords: {
            $pattern: hljs3.UNDERSCORE_IDENT_RE,
            literal: "true false nil",
            keyword: "and break do else elseif end for goto if in local global not or repeat return then until while",
            built_in: (
              // Metatags and globals:
              "_G _ENV _VERSION __index __newindex __mode __call __metatable __tostring __len __gc __add __sub __mul __div __mod __pow __concat __unm __eq __lt __le assert collectgarbage dofile error getfenv getmetatable ipairs load loadfile loadstring module next pairs pcall print rawequal rawget rawset require select setfenv setmetatable tonumber tostring type unpack xpcall arg self coroutine resume yield status wrap create running debug getupvalue debug sethook getmetatable gethook setmetatable setlocal traceback setfenv getinfo setupvalue getlocal getregistry getfenv io lines write close flush open output type read stderr stdin input stdout popen tmpfile math log max acos huge ldexp pi cos tanh pow deg tan cosh sinh random randomseed frexp ceil floor rad abs sqrt modf asin min mod fmod log10 atan2 exp sin atan os exit setlocale date getenv difftime remove time clock tmpname rename execute package preload loadlib loaded loaders cpath config path seeall string sub upper len gfind rep find match char dump gmatch reverse byte format gsub lower table setn insert getn foreachi maxn foreach concat sort remove"
            )
          },
          contains: COMMENTS.concat([
            {
              className: "function",
              beginKeywords: "function",
              end: "\\)",
              contains: [
                hljs3.inherit(hljs3.TITLE_MODE, { begin: "([_a-zA-Z]\\w*\\.)*([_a-zA-Z]\\w*:)?[_a-zA-Z]\\w*" }),
                {
                  className: "params",
                  begin: "\\(",
                  endsWithParent: true,
                  contains: COMMENTS
                }
              ].concat(COMMENTS)
            },
            hljs3.C_NUMBER_MODE,
            hljs3.APOS_STRING_MODE,
            hljs3.QUOTE_STRING_MODE,
            {
              className: "string",
              begin: OPENING_LONG_BRACKET,
              end: CLOSING_LONG_BRACKET,
              contains: [LONG_BRACKETS],
              relevance: 5
            }
          ])
        };
      }
      function less(hljs3) {
        const modes = MODES(hljs3);
        const PSEUDO_SELECTORS$1 = PSEUDO_SELECTORS;
        const AT_MODIFIERS = "and or not only";
        const IDENT_RE2 = "[\\w-]+";
        const INTERP_IDENT_RE = "(" + IDENT_RE2 + "|@\\{" + IDENT_RE2 + "\\})";
        const RULES = [];
        const VALUE_MODES = [];
        const STRING_MODE = function(c2) {
          return {
            // Less strings are not multiline (also include '~' for more consistent coloring of "escaped" strings)
            className: "string",
            begin: "~?" + c2 + ".*?" + c2
          };
        };
        const IDENT_MODE = function(name, begin, relevance) {
          return {
            className: name,
            begin,
            relevance
          };
        };
        const AT_KEYWORDS = {
          $pattern: /[a-z-]+/,
          keyword: AT_MODIFIERS,
          attribute: MEDIA_FEATURES.join(" ")
        };
        const PARENS_MODE = {
          // used only to properly balance nested parens inside mixin call, def. arg list
          begin: "\\(",
          end: "\\)",
          contains: VALUE_MODES,
          keywords: AT_KEYWORDS,
          relevance: 0
        };
        VALUE_MODES.push(
          hljs3.C_LINE_COMMENT_MODE,
          hljs3.C_BLOCK_COMMENT_MODE,
          STRING_MODE("'"),
          STRING_MODE('"'),
          modes.CSS_NUMBER_MODE,
          // fixme: it does not include dot for numbers like .5em :(
          {
            begin: "(url|data-uri)\\(",
            starts: {
              className: "string",
              end: "[\\)\\n]",
              excludeEnd: true
            }
          },
          modes.UNICODE_RANGE,
          modes.HEXCOLOR,
          PARENS_MODE,
          IDENT_MODE("variable", "@@?" + IDENT_RE2, 10),
          IDENT_MODE("variable", "@\\{" + IDENT_RE2 + "\\}"),
          IDENT_MODE("built_in", "~?`[^`]*?`"),
          // inline javascript (or whatever host language) *multiline* string
          {
            // @media features (it’s here to not duplicate things in AT_RULE_MODE with extra PARENS_MODE overriding):
            className: "attribute",
            begin: IDENT_RE2 + "\\s*:",
            end: ":",
            returnBegin: true,
            excludeEnd: true
          },
          modes.IMPORTANT,
          { beginKeywords: "and not" },
          modes.FUNCTION_DISPATCH
        );
        const VALUE_WITH_RULESETS = VALUE_MODES.concat({
          begin: /\{/,
          end: /\}/,
          contains: RULES
        });
        const MIXIN_GUARD_MODE = {
          beginKeywords: "when",
          endsWithParent: true,
          contains: [{ beginKeywords: "and not" }].concat(VALUE_MODES)
          // using this form to override VALUE’s 'function' match
        };
        const RULE_MODE = {
          begin: INTERP_IDENT_RE + "\\s*:",
          returnBegin: true,
          end: /[;}]/,
          relevance: 0,
          contains: [
            { begin: /-(webkit|moz|ms|o)-/ },
            modes.CSS_VARIABLE,
            {
              className: "attribute",
              begin: "\\b(" + ATTRIBUTES.join("|") + ")\\b",
              end: /(?=:)/,
              starts: {
                endsWithParent: true,
                illegal: "[<=$]",
                relevance: 0,
                contains: VALUE_MODES
              }
            }
          ]
        };
        const AT_RULE_MODE = {
          className: "keyword",
          begin: "@(import|media|charset|font-face|(-[a-z]+-)?keyframes|supports|document|namespace|page|viewport|host)\\b",
          starts: {
            end: "[;{}]",
            keywords: AT_KEYWORDS,
            returnEnd: true,
            contains: VALUE_MODES,
            relevance: 0
          }
        };
        const VAR_RULE_MODE = {
          className: "variable",
          variants: [
            // using more strict pattern for higher relevance to increase chances of Less detection.
            // this is *the only* Less specific statement used in most of the sources, so...
            // (we’ll still often loose to the css-parser unless there's '//' comment,
            // simply because 1 variable just can't beat 99 properties :)
            {
              begin: "@" + IDENT_RE2 + "\\s*:",
              relevance: 15
            },
            { begin: "@" + IDENT_RE2 }
          ],
          starts: {
            end: "[;}]",
            returnEnd: true,
            contains: VALUE_WITH_RULESETS
          }
        };
        const SELECTOR_MODE = {
          // first parse unambiguous selectors (i.e. those not starting with tag)
          // then fall into the scary lookahead-discriminator variant.
          // this mode also handles mixin definitions and calls
          variants: [
            {
              begin: "[\\.#:&\\[>]",
              end: "[;{}]"
              // mixin calls end with ';'
            },
            {
              begin: INTERP_IDENT_RE,
              end: /\{/
            }
          ],
          returnBegin: true,
          returnEnd: true,
          illegal: `[<='$"]`,
          relevance: 0,
          contains: [
            hljs3.C_LINE_COMMENT_MODE,
            hljs3.C_BLOCK_COMMENT_MODE,
            MIXIN_GUARD_MODE,
            IDENT_MODE("keyword", "all\\b"),
            IDENT_MODE("variable", "@\\{" + IDENT_RE2 + "\\}"),
            // otherwise it’s identified as tag
            {
              begin: "\\b(" + TAGS.join("|") + ")\\b",
              className: "selector-tag"
            },
            modes.CSS_NUMBER_MODE,
            IDENT_MODE("selector-tag", INTERP_IDENT_RE, 0),
            IDENT_MODE("selector-id", "#" + INTERP_IDENT_RE),
            IDENT_MODE("selector-class", "\\." + INTERP_IDENT_RE, 0),
            IDENT_MODE("selector-tag", "&", 0),
            modes.ATTRIBUTE_SELECTOR_MODE,
            {
              className: "selector-pseudo",
              begin: ":(" + PSEUDO_CLASSES.join("|") + ")"
            },
            {
              className: "selector-pseudo",
              begin: ":(:)?(" + PSEUDO_ELEMENTS.join("|") + ")"
            },
            {
              begin: /\(/,
              end: /\)/,
              relevance: 0,
              contains: VALUE_WITH_RULESETS
            },
            // argument list of parametric mixins
            { begin: "!important" },
            // eat !important after mixin call or it will be colored as tag
            modes.FUNCTION_DISPATCH
          ]
        };
        const PSEUDO_SELECTOR_MODE = {
          begin: IDENT_RE2 + `:(:)?(${PSEUDO_SELECTORS$1.join("|")})`,
          returnBegin: true,
          contains: [SELECTOR_MODE]
        };
        RULES.push(
          hljs3.C_LINE_COMMENT_MODE,
          hljs3.C_BLOCK_COMMENT_MODE,
          AT_RULE_MODE,
          VAR_RULE_MODE,
          PSEUDO_SELECTOR_MODE,
          RULE_MODE,
          SELECTOR_MODE,
          MIXIN_GUARD_MODE,
          modes.FUNCTION_DISPATCH
        );
        return {
          name: "Less",
          case_insensitive: true,
          illegal: `[=>'/<($"]`,
          contains: RULES
        };
      }
      var decimalDigits = "[0-9](_*[0-9])*";
      var frac = `\\.(${decimalDigits})`;
      var hexDigits = "[0-9a-fA-F](_*[0-9a-fA-F])*";
      var NUMERIC = {
        className: "number",
        variants: [
          // DecimalFloatingPointLiteral
          // including ExponentPart
          { begin: `(\\b(${decimalDigits})((${frac})|\\.)?|(${frac}))[eE][+-]?(${decimalDigits})[fFdD]?\\b` },
          // excluding ExponentPart
          { begin: `\\b(${decimalDigits})((${frac})[fFdD]?\\b|\\.([fFdD]\\b)?)` },
          { begin: `(${frac})[fFdD]?\\b` },
          { begin: `\\b(${decimalDigits})[fFdD]\\b` },
          // HexadecimalFloatingPointLiteral
          { begin: `\\b0[xX]((${hexDigits})\\.?|(${hexDigits})?\\.(${hexDigits}))[pP][+-]?(${decimalDigits})[fFdD]?\\b` },
          // DecimalIntegerLiteral
          { begin: "\\b(0|[1-9](_*[0-9])*)[lL]?\\b" },
          // HexIntegerLiteral
          { begin: `\\b0[xX](${hexDigits})[lL]?\\b` },
          // OctalIntegerLiteral
          { begin: "\\b0(_*[0-7])*[lL]?\\b" },
          // BinaryIntegerLiteral
          { begin: "\\b0[bB][01](_*[01])*[lL]?\\b" }
        ],
        relevance: 0
      };
      function kotlin(hljs3) {
        const KEYWORDS2 = {
          keyword: "abstract as val var vararg get set class object open private protected public noinline crossinline dynamic final enum if else do while for when throw try catch finally import package is in fun override companion reified inline lateinit init interface annotation data sealed internal infix operator out by constructor super tailrec where const inner suspend typealias external expect actual",
          built_in: "Byte Short Char Int Long Boolean Float Double Void Unit Nothing",
          literal: "true false null"
        };
        const KEYWORDS_WITH_LABEL = {
          className: "keyword",
          begin: /\b(break|continue|return|this)\b/,
          starts: { contains: [
            {
              className: "symbol",
              begin: /@\w+/
            }
          ] }
        };
        const LABEL = {
          className: "symbol",
          begin: hljs3.UNDERSCORE_IDENT_RE + "@"
        };
        const SUBST = {
          className: "subst",
          begin: /\$\{/,
          end: /\}/,
          contains: [hljs3.C_NUMBER_MODE]
        };
        const VARIABLE = {
          className: "variable",
          begin: "\\$" + hljs3.UNDERSCORE_IDENT_RE
        };
        const STRING = {
          className: "string",
          variants: [
            {
              begin: '"""',
              end: '"""(?=[^"])',
              contains: [
                VARIABLE,
                SUBST
              ]
            },
            // Can't use built-in modes easily, as we want to use STRING in the meta
            // context as 'meta-string' and there's no syntax to remove explicitly set
            // classNames in built-in modes.
            {
              begin: "'",
              end: "'",
              illegal: /\n/,
              contains: [hljs3.BACKSLASH_ESCAPE]
            },
            {
              begin: '"',
              end: '"',
              illegal: /\n/,
              contains: [
                hljs3.BACKSLASH_ESCAPE,
                VARIABLE,
                SUBST
              ]
            }
          ]
        };
        SUBST.contains.push(STRING);
        const ANNOTATION_USE_SITE = {
          className: "meta",
          begin: "@(?:file|property|field|get|set|receiver|param|setparam|delegate)\\s*:(?:\\s*" + hljs3.UNDERSCORE_IDENT_RE + ")?"
        };
        const ANNOTATION = {
          className: "meta",
          begin: "@" + hljs3.UNDERSCORE_IDENT_RE,
          contains: [
            {
              begin: /\(/,
              end: /\)/,
              contains: [
                hljs3.inherit(STRING, { className: "string" }),
                "self"
              ]
            }
          ]
        };
        const KOTLIN_NUMBER_MODE = NUMERIC;
        const KOTLIN_NESTED_COMMENT = hljs3.COMMENT(
          "/\\*",
          "\\*/",
          { contains: [hljs3.C_BLOCK_COMMENT_MODE] }
        );
        const KOTLIN_PAREN_TYPE = { variants: [
          {
            className: "type",
            begin: hljs3.UNDERSCORE_IDENT_RE
          },
          {
            begin: /\(/,
            end: /\)/,
            contains: []
            // defined later
          }
        ] };
        const KOTLIN_PAREN_TYPE2 = KOTLIN_PAREN_TYPE;
        KOTLIN_PAREN_TYPE2.variants[1].contains = [KOTLIN_PAREN_TYPE];
        KOTLIN_PAREN_TYPE.variants[1].contains = [KOTLIN_PAREN_TYPE2];
        return {
          name: "Kotlin",
          aliases: [
            "kt",
            "kts",
            "ktm",
            "ktx"
          ],
          keywords: KEYWORDS2,
          contains: [
            hljs3.COMMENT(
              "/\\*\\*",
              "\\*/",
              {
                relevance: 0,
                contains: [
                  {
                    className: "doctag",
                    begin: "@[A-Za-z]+"
                  }
                ]
              }
            ),
            hljs3.C_LINE_COMMENT_MODE,
            KOTLIN_NESTED_COMMENT,
            KEYWORDS_WITH_LABEL,
            LABEL,
            ANNOTATION_USE_SITE,
            ANNOTATION,
            {
              className: "function",
              beginKeywords: "fun",
              end: "[(]|$",
              returnBegin: true,
              excludeEnd: true,
              keywords: KEYWORDS2,
              relevance: 5,
              contains: [
                {
                  begin: hljs3.UNDERSCORE_IDENT_RE + "\\s*\\(",
                  returnBegin: true,
                  relevance: 0,
                  contains: [hljs3.UNDERSCORE_TITLE_MODE]
                },
                {
                  className: "type",
                  begin: /</,
                  end: />/,
                  keywords: "reified",
                  relevance: 0
                },
                {
                  className: "params",
                  begin: /\(/,
                  end: /\)/,
                  endsParent: true,
                  keywords: KEYWORDS2,
                  relevance: 0,
                  contains: [
                    {
                      begin: /:/,
                      end: /[=,\/]/,
                      endsWithParent: true,
                      contains: [
                        KOTLIN_PAREN_TYPE,
                        hljs3.C_LINE_COMMENT_MODE,
                        KOTLIN_NESTED_COMMENT
                      ],
                      relevance: 0
                    },
                    hljs3.C_LINE_COMMENT_MODE,
                    KOTLIN_NESTED_COMMENT,
                    ANNOTATION_USE_SITE,
                    ANNOTATION,
                    STRING,
                    hljs3.C_NUMBER_MODE
                  ]
                },
                KOTLIN_NESTED_COMMENT
              ]
            },
            {
              begin: [
                /class|interface|trait/,
                /\s+/,
                hljs3.UNDERSCORE_IDENT_RE
              ],
              beginScope: {
                3: "title.class"
              },
              keywords: "class interface trait",
              end: /[:\{(]|$/,
              excludeEnd: true,
              illegal: "extends implements",
              contains: [
                { beginKeywords: "public protected internal private constructor" },
                hljs3.UNDERSCORE_TITLE_MODE,
                {
                  className: "type",
                  begin: /</,
                  end: />/,
                  excludeBegin: true,
                  excludeEnd: true,
                  relevance: 0
                },
                {
                  className: "type",
                  begin: /[,:]\s*/,
                  end: /[<\(,){\s]|$/,
                  excludeBegin: true,
                  returnEnd: true
                },
                ANNOTATION_USE_SITE,
                ANNOTATION
              ]
            },
            STRING,
            {
              className: "meta",
              begin: "^#!/usr/bin/env",
              end: "$",
              illegal: "\n"
            },
            KOTLIN_NUMBER_MODE
          ]
        };
      }
      function json(hljs3) {
        const ATTRIBUTE = {
          className: "attr",
          begin: /(("(\\.|[^\\"\r\n])*")|('(\\.|[^\\'\r\n])*'))(?=\s*:)/,
          relevance: 1.01
        };
        const PUNCTUATION = {
          match: /[{}[\],:]/,
          className: "punctuation",
          relevance: 0
        };
        const LITERALS2 = [
          "true",
          "false",
          "null"
        ];
        const LITERALS_MODE = {
          scope: "literal",
          beginKeywords: LITERALS2.join(" ")
        };
        return {
          name: "JSON",
          aliases: ["jsonc", "json5"],
          keywords: {
            literal: LITERALS2
          },
          contains: [
            ATTRIBUTE,
            PUNCTUATION,
            hljs3.APOS_STRING_MODE,
            hljs3.QUOTE_STRING_MODE,
            LITERALS_MODE,
            EXTENDED_NUMBER_MODE,
            hljs3.C_LINE_COMMENT_MODE,
            hljs3.C_BLOCK_COMMENT_MODE
          ],
          illegal: "\\S"
        };
      }
      function recurRegex(re, substitution, depth) {
        if (depth === -1) return "";
        return re.replace(substitution, (_) => {
          return recurRegex(re, substitution, depth - 1);
        });
      }
      function java(hljs3) {
        const regex = hljs3.regex;
        const JAVA_IDENT_RE = "[\xC0-\u02B8a-zA-Z_$][\xC0-\u02B8a-zA-Z_$0-9]*";
        const ARRAY_BRACKETS_OPTIONAL_RE = "(?:(?:\\s*\\[\\s*])+)?";
        const SIMPLE_TYPE_RE = JAVA_IDENT_RE + "<@@@>" + ARRAY_BRACKETS_OPTIONAL_RE;
        const WILDCARD_TYPE_RE = "\\?(?:\\s+(?:extends|super)\\s+" + SIMPLE_TYPE_RE + ")?";
        const TYPE_ARG_RE = "(?:" + WILDCARD_TYPE_RE + "|" + SIMPLE_TYPE_RE + ")";
        const TYPE_ARGS_OPTIONAL_RE = recurRegex(
          "(?:\\s*<\\s*" + TYPE_ARG_RE + "(?:\\s*,\\s*" + TYPE_ARG_RE + ")*\\s*>)?",
          /<@@@>/g,
          2
        );
        const MAIN_KEYWORDS = [
          "synchronized",
          "abstract",
          "private",
          "var",
          "static",
          "if",
          "const ",
          "for",
          "while",
          "strictfp",
          "finally",
          "protected",
          "import",
          "native",
          "final",
          "void",
          "enum",
          "else",
          "break",
          "transient",
          "catch",
          "instanceof",
          "volatile",
          "case",
          "assert",
          "package",
          "default",
          "public",
          "try",
          "switch",
          "continue",
          "throws",
          "protected",
          "public",
          "private",
          "module",
          "requires",
          "exports",
          "do",
          "sealed",
          "yield",
          "permits",
          "goto",
          "when"
        ];
        const BUILT_INS2 = [
          "super",
          "this"
        ];
        const LITERALS2 = [
          "false",
          "true",
          "null"
        ];
        const TYPES2 = [
          "char",
          "boolean",
          "long",
          "float",
          "int",
          "byte",
          "short",
          "double"
        ];
        const KEYWORDS2 = {
          keyword: MAIN_KEYWORDS,
          literal: LITERALS2,
          type: TYPES2,
          built_in: BUILT_INS2
        };
        const ANNOTATION = {
          className: "meta",
          begin: "@" + JAVA_IDENT_RE,
          contains: [
            {
              begin: /\(/,
              end: /\)/,
              contains: ["self"]
              // allow nested () inside our annotation
            }
          ]
        };
        const PARAMS = {
          className: "params",
          begin: /\(/,
          end: /\)/,
          keywords: KEYWORDS2,
          relevance: 0,
          contains: [hljs3.C_BLOCK_COMMENT_MODE],
          endsParent: true
        };
        return {
          name: "Java",
          aliases: ["jsp"],
          keywords: KEYWORDS2,
          illegal: /<\/|#/,
          contains: [
            hljs3.COMMENT(
              "/\\*\\*",
              "\\*/",
              {
                relevance: 0,
                contains: [
                  {
                    // eat up @'s in emails to prevent them to be recognized as doctags
                    begin: /\w+@/,
                    relevance: 0
                  },
                  {
                    className: "doctag",
                    begin: "@[A-Za-z]+"
                  }
                ]
              }
            ),
            // relevance boost
            {
              begin: /import java\.[a-z]+\./,
              keywords: "import",
              relevance: 2
            },
            hljs3.C_LINE_COMMENT_MODE,
            hljs3.C_BLOCK_COMMENT_MODE,
            {
              begin: /"""/,
              end: /"""/,
              className: "string",
              contains: [hljs3.BACKSLASH_ESCAPE]
            },
            hljs3.APOS_STRING_MODE,
            hljs3.QUOTE_STRING_MODE,
            {
              match: [
                /\b(?:class|interface|enum|extends|implements|new)/,
                /\s+/,
                JAVA_IDENT_RE
              ],
              className: {
                1: "keyword",
                3: "title.class"
              }
            },
            {
              // Exceptions for hyphenated keywords
              match: /non-sealed/,
              scope: "keyword"
            },
            {
              // Expression keywords prevent keyword-led expressions from being
              // recognized as variable or method declarations.
              beginKeywords: "new throw return else yield assert",
              relevance: 0
            },
            {
              begin: [
                JAVA_IDENT_RE,
                regex.concat(TYPE_ARGS_OPTIONAL_RE, ARRAY_BRACKETS_OPTIONAL_RE, /\s+/),
                JAVA_IDENT_RE,
                ARRAY_BRACKETS_OPTIONAL_RE,
                /\s*/,
                /=(?!=)/
              ],
              className: {
                1: "type",
                3: "variable",
                6: "operator"
              }
            },
            {
              begin: [
                /record/,
                /\s+/,
                JAVA_IDENT_RE
              ],
              className: {
                1: "keyword",
                3: "title.class"
              },
              contains: [
                PARAMS,
                hljs3.C_LINE_COMMENT_MODE,
                hljs3.C_BLOCK_COMMENT_MODE
              ]
            },
            {
              begin: [
                JAVA_IDENT_RE,
                regex.concat(TYPE_ARGS_OPTIONAL_RE, ARRAY_BRACKETS_OPTIONAL_RE, /\s+/),
                JAVA_IDENT_RE,
                /\s*(?=\()/
              ],
              className: {
                1: "type",
                3: "title.function"
              },
              keywords: KEYWORDS2,
              contains: [
                {
                  className: "params",
                  begin: /\(/,
                  end: /\)/,
                  keywords: KEYWORDS2,
                  relevance: 0,
                  contains: [
                    ANNOTATION,
                    hljs3.APOS_STRING_MODE,
                    hljs3.QUOTE_STRING_MODE,
                    NUMERIC,
                    hljs3.C_BLOCK_COMMENT_MODE
                  ]
                },
                hljs3.C_LINE_COMMENT_MODE,
                hljs3.C_BLOCK_COMMENT_MODE
              ]
            },
            NUMERIC,
            ANNOTATION
          ]
        };
      }
      function ini(hljs3) {
        const regex = hljs3.regex;
        const NUMBERS = {
          className: "number",
          relevance: 0,
          variants: [
            { begin: /([+-]+)?[\d]+_[\d_]+/ },
            { begin: hljs3.NUMBER_RE }
          ]
        };
        const COMMENTS = hljs3.COMMENT();
        COMMENTS.variants = [
          {
            begin: /;/,
            end: /$/
          },
          {
            begin: /#/,
            end: /$/
          }
        ];
        const VARIABLES = {
          className: "variable",
          variants: [
            { begin: /\$[\w\d"][\w\d_]*/ },
            { begin: /\$\{(.*?)\}/ }
          ]
        };
        const LITERALS2 = {
          className: "literal",
          begin: /\bon|off|true|false|yes|no\b/
        };
        const STRINGS = {
          className: "string",
          contains: [hljs3.BACKSLASH_ESCAPE],
          variants: [
            {
              begin: "'''",
              end: "'''",
              relevance: 10
            },
            {
              begin: '"""',
              end: '"""',
              relevance: 10
            },
            {
              begin: '"',
              end: '"'
            },
            {
              begin: "'",
              end: "'"
            }
          ]
        };
        const ARRAY = {
          begin: /\[/,
          end: /\]/,
          contains: [
            COMMENTS,
            LITERALS2,
            VARIABLES,
            STRINGS,
            NUMBERS,
            "self"
          ],
          relevance: 0
        };
        const BARE_KEY = /[A-Za-z0-9_-]+/;
        const QUOTED_KEY_DOUBLE_QUOTE = /"(\\"|[^"])*"/;
        const QUOTED_KEY_SINGLE_QUOTE = /'[^']*'/;
        const ANY_KEY = regex.either(
          BARE_KEY,
          QUOTED_KEY_DOUBLE_QUOTE,
          QUOTED_KEY_SINGLE_QUOTE
        );
        const DOTTED_KEY = regex.concat(
          ANY_KEY,
          "(\\s*\\.\\s*",
          ANY_KEY,
          ")*",
          regex.lookahead(/\s*=\s*[^#\s]/)
        );
        return {
          name: "TOML, also INI",
          aliases: ["toml"],
          case_insensitive: true,
          illegal: /\S/,
          contains: [
            COMMENTS,
            {
              className: "section",
              begin: /\[+/,
              end: /\]+/
            },
            {
              begin: DOTTED_KEY,
              className: "attr",
              starts: {
                end: /$/,
                contains: [
                  COMMENTS,
                  ARRAY,
                  LITERALS2,
                  VARIABLES,
                  STRINGS,
                  NUMBERS
                ]
              }
            }
          ]
        };
      }
      function graphql(hljs3) {
        const regex = hljs3.regex;
        const GQL_NAME = /[_A-Za-z][_0-9A-Za-z]*/;
        return {
          name: "GraphQL",
          aliases: ["gql"],
          case_insensitive: true,
          disableAutodetect: false,
          keywords: {
            keyword: [
              "query",
              "mutation",
              "subscription",
              "type",
              "input",
              "schema",
              "directive",
              "interface",
              "union",
              "scalar",
              "fragment",
              "enum",
              "on"
            ],
            literal: [
              "true",
              "false",
              "null"
            ]
          },
          contains: [
            hljs3.HASH_COMMENT_MODE,
            hljs3.QUOTE_STRING_MODE,
            hljs3.NUMBER_MODE,
            {
              scope: "punctuation",
              match: /[.]{3}/,
              relevance: 0
            },
            {
              scope: "punctuation",
              begin: /[\!\(\)\:\=\[\]\{\|\}]{1}/,
              relevance: 0
            },
            {
              scope: "variable",
              begin: /\$/,
              end: /\W/,
              excludeEnd: true,
              relevance: 0
            },
            {
              scope: "meta",
              match: /@\w+/,
              excludeEnd: true
            },
            {
              scope: "symbol",
              begin: regex.concat(GQL_NAME, regex.lookahead(/\s*:/)),
              relevance: 0
            }
          ],
          illegal: [
            /[;<']/,
            /BEGIN/
          ]
        };
      }
      function go(hljs3) {
        const LITERALS2 = [
          "true",
          "false",
          "iota",
          "nil"
        ];
        const BUILT_INS2 = [
          "append",
          "cap",
          "close",
          "complex",
          "copy",
          "imag",
          "len",
          "make",
          "new",
          "panic",
          "print",
          "println",
          "real",
          "recover",
          "delete"
        ];
        const TYPES2 = [
          "bool",
          "byte",
          "complex64",
          "complex128",
          "error",
          "float32",
          "float64",
          "int8",
          "int16",
          "int32",
          "int64",
          "string",
          "uint8",
          "uint16",
          "uint32",
          "uint64",
          "int",
          "uint",
          "uintptr",
          "rune"
        ];
        const KWS = [
          "break",
          "case",
          "chan",
          "const",
          "continue",
          "default",
          "defer",
          "else",
          "fallthrough",
          "for",
          "func",
          "go",
          "goto",
          "if",
          "import",
          "interface",
          "map",
          "package",
          "range",
          "return",
          "select",
          "struct",
          "switch",
          "type",
          "var"
        ];
        const KEYWORDS2 = {
          keyword: KWS,
          type: TYPES2,
          literal: LITERALS2,
          built_in: BUILT_INS2
        };
        return {
          name: "Go",
          aliases: ["golang"],
          keywords: KEYWORDS2,
          illegal: "</",
          contains: [
            hljs3.C_LINE_COMMENT_MODE,
            hljs3.C_BLOCK_COMMENT_MODE,
            {
              className: "string",
              variants: [
                hljs3.QUOTE_STRING_MODE,
                hljs3.APOS_STRING_MODE,
                {
                  begin: "`",
                  end: "`"
                }
              ]
            },
            {
              className: "number",
              variants: [
                {
                  match: /-?\b0[xX]\.[a-fA-F0-9](_?[a-fA-F0-9])*[pP][+-]?\d(_?\d)*i?/,
                  // hex without a present digit before . (making a digit afterwards required)
                  relevance: 0
                },
                {
                  match: /-?\b0[xX](_?[a-fA-F0-9])+((\.([a-fA-F0-9](_?[a-fA-F0-9])*)?)?[pP][+-]?\d(_?\d)*)?i?/,
                  // hex with a present digit before . (making a digit afterwards optional)
                  relevance: 0
                },
                {
                  match: /-?\b0[oO](_?[0-7])*i?/,
                  // leading 0o octal
                  relevance: 0
                },
                {
                  match: /-?\b0[bB](_?[01])*i?/,
                  // leading 0b binary
                  relevance: 0
                },
                {
                  match: /-?\.\d(_?\d)*([eE][+-]?\d(_?\d)*)?i?/,
                  // decimal without a present digit before . (making a digit afterwards required)
                  relevance: 0
                },
                {
                  match: /-?\b\d(_?\d)*(\.(\d(_?\d)*)?)?([eE][+-]?\d(_?\d)*)?i?/,
                  // decimal with a present digit before . (making a digit afterwards optional)
                  relevance: 0
                }
              ]
            },
            {
              begin: /:=/
              // relevance booster
            },
            {
              className: "function",
              beginKeywords: "func",
              end: "\\s*(\\{|$)",
              excludeEnd: true,
              contains: [
                hljs3.TITLE_MODE,
                {
                  className: "params",
                  begin: /\(/,
                  end: /\)/,
                  endsParent: true,
                  keywords: KEYWORDS2,
                  illegal: /["']/
                }
              ]
            }
          ]
        };
      }
      function diff(hljs3) {
        const regex = hljs3.regex;
        return {
          name: "Diff",
          aliases: ["patch"],
          contains: [
            {
              className: "meta",
              relevance: 10,
              match: regex.either(
                /^@@ +-\d+,\d+ +\+\d+,\d+ +@@/,
                // @@ -1,2 +1,2 @@
                /^@@ +-\d+ +\+\d+,\d+ +@@/,
                // @@ -1 +1,2 @@
                /^@@ +-\d+,\d+ +\+\d+ +@@/,
                // @@ -1,2 +1 @@
                /^@@ +-\d+ +\+\d+ +@@/,
                // @@ -1 +1 @@
                /^\*\*\* +\d+,\d+ +\*\*\*\*$/,
                /^--- +\d+,\d+ +----$/
              )
            },
            {
              className: "comment",
              variants: [
                {
                  begin: regex.either(
                    /Index: /,
                    /^index/,
                    /={3,}/,
                    /^-{3}/,
                    /^\*{3} /,
                    /^\+{3}/,
                    /^diff --git/
                  ),
                  end: /$/
                },
                { match: /^\*{15}$/ }
              ]
            },
            {
              className: "addition",
              begin: /^\+/,
              end: /$/
            },
            {
              className: "deletion",
              begin: /^-/,
              end: /$/
            },
            {
              className: "addition",
              begin: /^!/,
              end: /$/
            }
          ]
        };
      }
      function css(hljs3) {
        const regex = hljs3.regex;
        const modes = MODES(hljs3);
        const VENDOR_PREFIX = { begin: /-(webkit|moz|ms|o)-(?=[a-z])/ };
        const AT_MODIFIERS = "and or not only";
        const AT_PROPERTY_RE = /@-?\w[\w]*(-\w+)*/;
        const IDENT_RE2 = "[a-zA-Z-][a-zA-Z0-9_-]*";
        const STRINGS = [
          hljs3.APOS_STRING_MODE,
          hljs3.QUOTE_STRING_MODE
        ];
        return {
          name: "CSS",
          case_insensitive: true,
          illegal: /[=|'\$]/,
          keywords: { keyframePosition: "from to" },
          classNameAliases: {
            // for visual continuity with `tag {}` and because we
            // don't have a great class for this?
            keyframePosition: "selector-tag"
          },
          contains: [
            modes.BLOCK_COMMENT,
            VENDOR_PREFIX,
            // to recognize keyframe 40% etc which are outside the scope of our
            // attribute value mode
            modes.CSS_NUMBER_MODE,
            {
              className: "selector-id",
              begin: /#[A-Za-z0-9_-]+/,
              relevance: 0
            },
            {
              className: "selector-class",
              begin: "\\." + IDENT_RE2,
              relevance: 0
            },
            modes.ATTRIBUTE_SELECTOR_MODE,
            {
              className: "selector-pseudo",
              variants: [
                { begin: ":(" + PSEUDO_CLASSES.join("|") + ")" },
                { begin: ":(:)?(" + PSEUDO_ELEMENTS.join("|") + ")" }
              ]
            },
            // we may actually need this (12/2020)
            // { // pseudo-selector params
            //   begin: /\(/,
            //   end: /\)/,
            //   contains: [ hljs.CSS_NUMBER_MODE ]
            // },
            modes.CSS_VARIABLE,
            {
              className: "attribute",
              begin: "\\b(" + ATTRIBUTES.join("|") + ")\\b"
            },
            // attribute values
            {
              begin: /:/,
              end: /[;}{]/,
              contains: [
                modes.BLOCK_COMMENT,
                modes.HEXCOLOR,
                modes.IMPORTANT,
                modes.CSS_NUMBER_MODE,
                modes.UNICODE_RANGE,
                ...STRINGS,
                // needed to highlight these as strings and to avoid issues with
                // illegal characters that might be inside urls that would trigger the
                // languages illegal stack
                {
                  begin: /(url|data-uri)\(/,
                  end: /\)/,
                  relevance: 0,
                  // from keywords
                  keywords: { built_in: "url data-uri" },
                  contains: [
                    ...STRINGS,
                    {
                      className: "string",
                      // any character other than `)` as in `url()` will be the start
                      // of a string, which ends with `)` (from the parent mode)
                      begin: /[^)]/,
                      endsWithParent: true,
                      excludeEnd: true
                    }
                  ]
                },
                modes.FUNCTION_DISPATCH
              ]
            },
            {
              begin: regex.lookahead(/@/),
              end: "[{;]",
              relevance: 0,
              illegal: /:/,
              // break on Less variables @var: ...
              contains: [
                {
                  className: "keyword",
                  begin: AT_PROPERTY_RE
                },
                {
                  begin: /\s/,
                  endsWithParent: true,
                  excludeEnd: true,
                  relevance: 0,
                  keywords: {
                    $pattern: /[a-z-]+/,
                    keyword: AT_MODIFIERS,
                    attribute: MEDIA_FEATURES.join(" ")
                  },
                  contains: [
                    {
                      begin: /[a-z-]+(?=:)/,
                      className: "attribute"
                    },
                    ...STRINGS,
                    modes.CSS_NUMBER_MODE
                  ]
                }
              ]
            },
            {
              className: "selector-tag",
              begin: "\\b(" + TAGS.join("|") + ")\\b"
            }
          ]
        };
      }
      function csharp(hljs3) {
        const BUILT_IN_KEYWORDS = [
          "bool",
          "byte",
          "char",
          "decimal",
          "delegate",
          "double",
          "dynamic",
          "enum",
          "float",
          "int",
          "long",
          "nint",
          "nuint",
          "object",
          "sbyte",
          "short",
          "string",
          "ulong",
          "uint",
          "ushort"
        ];
        const FUNCTION_MODIFIERS = [
          "public",
          "private",
          "protected",
          "static",
          "internal",
          "protected",
          "abstract",
          "async",
          "extern",
          "override",
          "unsafe",
          "virtual",
          "new",
          "sealed",
          "partial"
        ];
        const LITERAL_KEYWORDS = [
          "default",
          "false",
          "null",
          "true"
        ];
        const NORMAL_KEYWORDS = [
          "abstract",
          "as",
          "base",
          "break",
          "case",
          "catch",
          "class",
          "const",
          "continue",
          "do",
          "else",
          "event",
          "explicit",
          "extern",
          "finally",
          "fixed",
          "for",
          "foreach",
          "goto",
          "if",
          "implicit",
          "in",
          "interface",
          "internal",
          "is",
          "lock",
          "namespace",
          "new",
          "operator",
          "out",
          "override",
          "params",
          "private",
          "protected",
          "public",
          "readonly",
          "record",
          "ref",
          "return",
          "scoped",
          "sealed",
          "sizeof",
          "stackalloc",
          "static",
          "struct",
          "switch",
          "this",
          "throw",
          "try",
          "typeof",
          "unchecked",
          "unsafe",
          "using",
          "virtual",
          "void",
          "volatile",
          "while"
        ];
        const CONTEXTUAL_KEYWORDS = [
          "add",
          "alias",
          "and",
          "ascending",
          "args",
          "async",
          "await",
          "by",
          "descending",
          "dynamic",
          "equals",
          "file",
          "from",
          "get",
          "global",
          "group",
          "init",
          "into",
          "join",
          "let",
          "nameof",
          "not",
          "notnull",
          "on",
          "or",
          "orderby",
          "partial",
          "record",
          "remove",
          "required",
          "scoped",
          "select",
          "set",
          "unmanaged",
          "value|0",
          "var",
          "when",
          "where",
          "with",
          "yield"
        ];
        const KEYWORDS2 = {
          keyword: NORMAL_KEYWORDS.concat(CONTEXTUAL_KEYWORDS),
          built_in: BUILT_IN_KEYWORDS,
          literal: LITERAL_KEYWORDS
        };
        const TITLE_MODE2 = hljs3.inherit(hljs3.TITLE_MODE, { begin: "[a-zA-Z](\\.?\\w)*" });
        const DIGITS = "\\d(_*\\d)*";
        const INTEGER_SUFFIX = "([uU][lL]?|[lL][uU]?)?";
        const REAL_SUFFIX = "([fFdDmM]|[uU][lL]?|[lL][uU]?)?";
        const NUMBERS = {
          className: "number",
          variants: [
            { begin: "\\b0[bB]_*[01](_*[01])*" + INTEGER_SUFFIX },
            { begin: "(-?)\\b0[xX]_*[a-fA-F0-9](_*[a-fA-F0-9])*" + INTEGER_SUFFIX },
            { begin: "(-?)(\\b" + DIGITS + "(\\.(" + DIGITS + ")?)?|\\." + DIGITS + ")([eE][-+]?" + DIGITS + ")?" + REAL_SUFFIX }
          ],
          relevance: 0
        };
        const RAW_STRING = {
          className: "string",
          begin: /"""("*)(?!")(.|\n)*?"""\1/,
          relevance: 1
        };
        const VERBATIM_STRING = {
          className: "string",
          begin: '@"',
          end: '"',
          contains: [{ begin: '""' }]
        };
        const VERBATIM_STRING_NO_LF = hljs3.inherit(VERBATIM_STRING, { illegal: /\n/ });
        const SUBST = {
          className: "subst",
          begin: /\{/,
          end: /\}/,
          keywords: KEYWORDS2
        };
        const SUBST_NO_LF = hljs3.inherit(SUBST, { illegal: /\n/ });
        const INTERPOLATED_STRING = {
          className: "string",
          begin: /\$"/,
          end: '"',
          illegal: /\n/,
          contains: [
            { begin: /\{\{/ },
            { begin: /\}\}/ },
            hljs3.BACKSLASH_ESCAPE,
            SUBST_NO_LF
          ]
        };
        const INTERPOLATED_VERBATIM_STRING = {
          className: "string",
          begin: /\$@"/,
          end: '"',
          contains: [
            { begin: /\{\{/ },
            { begin: /\}\}/ },
            { begin: '""' },
            SUBST
          ]
        };
        const INTERPOLATED_VERBATIM_STRING_NO_LF = hljs3.inherit(INTERPOLATED_VERBATIM_STRING, {
          illegal: /\n/,
          contains: [
            { begin: /\{\{/ },
            { begin: /\}\}/ },
            { begin: '""' },
            SUBST_NO_LF
          ]
        });
        SUBST.contains = [
          INTERPOLATED_VERBATIM_STRING,
          INTERPOLATED_STRING,
          VERBATIM_STRING,
          hljs3.APOS_STRING_MODE,
          hljs3.QUOTE_STRING_MODE,
          NUMBERS,
          hljs3.C_BLOCK_COMMENT_MODE
        ];
        SUBST_NO_LF.contains = [
          INTERPOLATED_VERBATIM_STRING_NO_LF,
          INTERPOLATED_STRING,
          VERBATIM_STRING_NO_LF,
          hljs3.APOS_STRING_MODE,
          hljs3.QUOTE_STRING_MODE,
          NUMBERS,
          hljs3.inherit(hljs3.C_BLOCK_COMMENT_MODE, { illegal: /\n/ })
        ];
        const STRING = { variants: [
          RAW_STRING,
          INTERPOLATED_VERBATIM_STRING,
          INTERPOLATED_STRING,
          VERBATIM_STRING,
          hljs3.APOS_STRING_MODE,
          hljs3.QUOTE_STRING_MODE
        ] };
        const GENERIC_MODIFIER = {
          begin: "<",
          end: ">",
          contains: [
            { beginKeywords: "in out" },
            TITLE_MODE2
          ]
        };
        const TYPE_IDENT_RE = hljs3.IDENT_RE + "(<" + hljs3.IDENT_RE + "(\\s*,\\s*" + hljs3.IDENT_RE + ")*>)?(\\[\\])?";
        const AT_IDENTIFIER = {
          // prevents expressions like `@class` from incorrect flagging
          // `class` as a keyword
          begin: "@" + hljs3.IDENT_RE,
          relevance: 0
        };
        return {
          name: "C#",
          aliases: [
            "cs",
            "c#"
          ],
          keywords: KEYWORDS2,
          illegal: /::/,
          contains: [
            hljs3.COMMENT(
              "///",
              "$",
              {
                returnBegin: true,
                contains: [
                  {
                    className: "doctag",
                    variants: [
                      {
                        begin: "///",
                        relevance: 0
                      },
                      { begin: "<!--|-->" },
                      {
                        begin: "</?",
                        end: ">"
                      }
                    ]
                  }
                ]
              }
            ),
            hljs3.C_LINE_COMMENT_MODE,
            hljs3.C_BLOCK_COMMENT_MODE,
            {
              className: "meta",
              begin: "#",
              end: "$",
              keywords: { keyword: "if else elif endif define undef warning error line region endregion pragma checksum" }
            },
            STRING,
            NUMBERS,
            {
              beginKeywords: "class interface",
              relevance: 0,
              end: /[{;=]/,
              illegal: /[^\s:,]/,
              contains: [
                { beginKeywords: "where class" },
                TITLE_MODE2,
                GENERIC_MODIFIER,
                hljs3.C_LINE_COMMENT_MODE,
                hljs3.C_BLOCK_COMMENT_MODE
              ]
            },
            {
              beginKeywords: "namespace",
              relevance: 0,
              end: /[{;=]/,
              illegal: /[^\s:]/,
              contains: [
                TITLE_MODE2,
                hljs3.C_LINE_COMMENT_MODE,
                hljs3.C_BLOCK_COMMENT_MODE
              ]
            },
            {
              beginKeywords: "record",
              relevance: 0,
              end: /[{;=]/,
              illegal: /[^\s:]/,
              contains: [
                TITLE_MODE2,
                GENERIC_MODIFIER,
                hljs3.C_LINE_COMMENT_MODE,
                hljs3.C_BLOCK_COMMENT_MODE
              ]
            },
            {
              // [Attributes("")]
              className: "meta",
              begin: "^\\s*\\[(?=[\\w])",
              excludeBegin: true,
              end: "\\]",
              excludeEnd: true,
              contains: [
                {
                  className: "string",
                  begin: /"/,
                  end: /"/
                }
              ]
            },
            {
              // Expression keywords prevent 'keyword Name(...)' from being
              // recognized as a function definition
              beginKeywords: "new return throw await else",
              relevance: 0
            },
            {
              className: "function",
              begin: "(" + TYPE_IDENT_RE + "\\s+)+" + hljs3.IDENT_RE + "\\s*(<[^=]+>\\s*)?\\(",
              returnBegin: true,
              end: /\s*[{;=]/,
              excludeEnd: true,
              keywords: KEYWORDS2,
              contains: [
                // prevents these from being highlighted `title`
                {
                  beginKeywords: FUNCTION_MODIFIERS.join(" "),
                  relevance: 0
                },
                {
                  begin: hljs3.IDENT_RE + "\\s*(<[^=]+>\\s*)?\\(",
                  returnBegin: true,
                  contains: [
                    hljs3.TITLE_MODE,
                    GENERIC_MODIFIER
                  ],
                  relevance: 0
                },
                { match: /\(\)/ },
                {
                  className: "params",
                  begin: /\(/,
                  end: /\)/,
                  excludeBegin: true,
                  excludeEnd: true,
                  keywords: KEYWORDS2,
                  relevance: 0,
                  contains: [
                    STRING,
                    NUMBERS,
                    hljs3.C_BLOCK_COMMENT_MODE
                  ]
                },
                hljs3.C_LINE_COMMENT_MODE,
                hljs3.C_BLOCK_COMMENT_MODE
              ]
            },
            AT_IDENTIFIER
          ]
        };
      }
      function cpp(hljs3) {
        const regex = hljs3.regex;
        const C_LINE_COMMENT_MODE2 = hljs3.COMMENT("//", "$", { contains: [{ begin: /\\\n/ }] });
        const DECLTYPE_AUTO_RE = "decltype\\(auto\\)";
        const NAMESPACE_RE = "[a-zA-Z_]\\w*::";
        const TEMPLATE_ARGUMENT_RE = "<[^<>]+>";
        const FUNCTION_TYPE_RE = "(?!struct)(" + DECLTYPE_AUTO_RE + "|" + regex.optional(NAMESPACE_RE) + "[a-zA-Z_]\\w*" + regex.optional(TEMPLATE_ARGUMENT_RE) + ")";
        const CPP_PRIMITIVE_TYPES = {
          className: "type",
          begin: "\\b[a-z\\d_]*_t\\b"
        };
        const CHARACTER_ESCAPES = "\\\\(x[0-9A-Fa-f]{2}|u[0-9A-Fa-f]{4,8}|[0-7]{3}|\\S)";
        const STRINGS = {
          className: "string",
          variants: [
            {
              begin: '(u8?|U|L)?"',
              end: '"',
              illegal: "\\n",
              contains: [hljs3.BACKSLASH_ESCAPE]
            },
            {
              begin: "(u8?|U|L)?'(" + CHARACTER_ESCAPES + "|.)",
              end: "'",
              illegal: "."
            },
            // https://en.cppreference.com/w/cpp/language/string_literal
            // a d-char-sequence never contains parentheses, backslashes or whitespace;
            // quotes are excluded as well so the closing delimiter cannot swallow the
            // quote that actually terminates the literal
            hljs3.END_SAME_AS_BEGIN({
              begin: /(?:u8?|U|L)?R"([^()\\\s"]{0,16})\(/,
              end: /\)([^()\\\s"]{0,16})"/
            })
          ]
        };
        const NUMBERS = {
          className: "number",
          variants: [
            // Floating-point literal.
            {
              begin: "[+-]?(?:(?:\\b[0-9](?:'?[0-9])*\\.(?:[0-9](?:'?[0-9])*)?|\\.[0-9](?:'?[0-9])*)(?:[Ee][+-]?[0-9](?:'?[0-9])*)?|\\b[0-9](?:'?[0-9])*[Ee][+-]?[0-9](?:'?[0-9])*|\\b0[Xx](?:[0-9A-Fa-f](?:'?[0-9A-Fa-f])*(?:\\.(?:[0-9A-Fa-f](?:'?[0-9A-Fa-f])*)?)?|\\.[0-9A-Fa-f](?:'?[0-9A-Fa-f])*)[Pp][+-]?[0-9](?:'?[0-9])*)(?:[Ff](?:16|32|64|128)?|(BF|bf)16|[Ll]|)"
            },
            // Integer literal.
            {
              begin: "[+-]?\\b(?:0[Bb][01](?:'?[01])*|0[Xx][0-9A-Fa-f](?:'?[0-9A-Fa-f])*|0(?:'?[0-7])*|[1-9](?:'?[0-9])*)(?:[Uu](?:LL?|ll?)|[Uu][Zz]?|(?:LL?|ll?)[Uu]?|[Zz][Uu]|)"
              // Note: there are user-defined literal suffixes too, but perhaps having the custom suffix not part of the
              // literal highlight actually makes it stand out more.
            }
          ],
          relevance: 0
        };
        const PREPROCESSOR_INCLUDE = {
          scope: "meta",
          begin: /#\s*include\b/,
          end: /$/,
          keywords: { keyword: "include" },
          contains: [
            {
              // the `\` at the end of a line signaling continuation
              begin: /\\\n/
            },
            STRINGS,
            {
              scope: "string",
              begin: /<.*?>/
            },
            C_LINE_COMMENT_MODE2,
            hljs3.C_BLOCK_COMMENT_MODE
          ]
        };
        const PREPROCESSOR = {
          className: "meta",
          begin: /#\s*[a-z]+\b/,
          end: /$/,
          keywords: { keyword: "if else elif endif define undef warning error line pragma _Pragma ifdef ifndef include" },
          contains: [
            {
              begin: /\\\n/,
              relevance: 0
            },
            hljs3.inherit(STRINGS, { className: "string" }),
            C_LINE_COMMENT_MODE2,
            hljs3.C_BLOCK_COMMENT_MODE
          ]
        };
        const PREPROCESSORS = [
          PREPROCESSOR_INCLUDE,
          PREPROCESSOR
        ];
        const TITLE_MODE2 = {
          className: "title",
          begin: regex.optional(NAMESPACE_RE) + hljs3.IDENT_RE,
          relevance: 0
        };
        const FUNCTION_TITLE = regex.optional(NAMESPACE_RE) + hljs3.IDENT_RE + "\\s*\\(";
        const MAX_FUNCTION_TYPE_TOKENS = 12;
        const RESERVED_KEYWORDS = [
          "alignas",
          "alignof",
          "and",
          "and_eq",
          "asm",
          "atomic_cancel",
          "atomic_commit",
          "atomic_noexcept",
          "auto",
          "bitand",
          "bitor",
          "break",
          "case",
          "catch",
          "class",
          "co_await",
          "co_return",
          "co_yield",
          "compl",
          "concept",
          "const_cast|10",
          "consteval",
          "constexpr",
          "constinit",
          "continue",
          "decltype",
          "default",
          "delete",
          "do",
          "dynamic_cast|10",
          "else",
          "enum",
          "explicit",
          "export",
          "extern",
          "false",
          "final",
          "for",
          "friend",
          "goto",
          "if",
          "import",
          "inline",
          "module",
          "mutable",
          "namespace",
          "new",
          "noexcept",
          "not",
          "not_eq",
          "nullptr",
          "operator",
          "or",
          "or_eq",
          "override",
          "private",
          "protected",
          "public",
          "reflexpr",
          "register",
          "reinterpret_cast|10",
          "requires",
          "return",
          "sizeof",
          "static_assert",
          "static_cast|10",
          "struct",
          "switch",
          "synchronized",
          "template",
          "this",
          "thread_local",
          "throw",
          "transaction_safe",
          "transaction_safe_dynamic",
          "true",
          "try",
          "typedef",
          "typeid",
          "typename",
          "union",
          "using",
          "virtual",
          "volatile",
          "while",
          "xor",
          "xor_eq"
        ];
        const RESERVED_TYPES = [
          "bool",
          "char",
          "char16_t",
          "char32_t",
          "char8_t",
          "double",
          "float",
          "int",
          "long",
          "short",
          "void",
          "wchar_t",
          "unsigned",
          "signed",
          "const",
          "static"
        ];
        const TYPE_HINTS = [
          "any",
          "auto_ptr",
          "barrier",
          "binary_semaphore",
          "bitset",
          "complex",
          "condition_variable",
          "condition_variable_any",
          "counting_semaphore",
          "deque",
          "false_type",
          "flat_map",
          "flat_set",
          "future",
          "imaginary",
          "initializer_list",
          "istringstream",
          "jthread",
          "latch",
          "lock_guard",
          "multimap",
          "multiset",
          "mutex",
          "optional",
          "ostringstream",
          "packaged_task",
          "pair",
          "promise",
          "priority_queue",
          "queue",
          "recursive_mutex",
          "recursive_timed_mutex",
          "scoped_lock",
          "set",
          "shared_future",
          "shared_lock",
          "shared_mutex",
          "shared_timed_mutex",
          "shared_ptr",
          "stack",
          "string_view",
          "stringstream",
          "timed_mutex",
          "thread",
          "true_type",
          "tuple",
          "unique_lock",
          "unique_ptr",
          "unordered_map",
          "unordered_multimap",
          "unordered_multiset",
          "unordered_set",
          "variant",
          "vector",
          "weak_ptr",
          "wstring",
          "wstring_view"
        ];
        const FUNCTION_HINTS = [
          "abort",
          "abs",
          "acos",
          "apply",
          "as_const",
          "asin",
          "atan",
          "atan2",
          "calloc",
          "ceil",
          "cerr",
          "cin",
          "clog",
          "cos",
          "cosh",
          "cout",
          "declval",
          "endl",
          "exchange",
          "exit",
          "exp",
          "fabs",
          "floor",
          "fmod",
          "forward",
          "fprintf",
          "fputs",
          "free",
          "frexp",
          "fscanf",
          "future",
          "invoke",
          "isalnum",
          "isalpha",
          "iscntrl",
          "isdigit",
          "isgraph",
          "islower",
          "isprint",
          "ispunct",
          "isspace",
          "isupper",
          "isxdigit",
          "labs",
          "launder",
          "ldexp",
          "log",
          "log10",
          "make_pair",
          "make_shared",
          "make_shared_for_overwrite",
          "make_tuple",
          "make_unique",
          "malloc",
          "memchr",
          "memcmp",
          "memcpy",
          "memset",
          "modf",
          "move",
          "pow",
          "printf",
          "putchar",
          "puts",
          "realloc",
          "scanf",
          "sin",
          "sinh",
          "snprintf",
          "sprintf",
          "sqrt",
          "sscanf",
          "std",
          "stderr",
          "stdin",
          "stdout",
          "strcat",
          "strchr",
          "strcmp",
          "strcpy",
          "strcspn",
          "strlen",
          "strncat",
          "strncmp",
          "strncpy",
          "strpbrk",
          "strrchr",
          "strspn",
          "strstr",
          "swap",
          "tan",
          "tanh",
          "terminate",
          "to_underlying",
          "tolower",
          "toupper",
          "vfprintf",
          "visit",
          "vprintf",
          "vsprintf"
        ];
        const LITERALS2 = [
          "NULL",
          "false",
          "nullopt",
          "nullptr",
          "true"
        ];
        const BUILT_IN = ["_Pragma"];
        const CPP_KEYWORDS = {
          type: RESERVED_TYPES,
          keyword: RESERVED_KEYWORDS,
          literal: LITERALS2,
          built_in: BUILT_IN,
          _type_hints: TYPE_HINTS
        };
        const FUNCTION_DISPATCH = {
          className: "function.dispatch",
          relevance: 0,
          keywords: {
            // Only for relevance, not highlighting.
            _hint: FUNCTION_HINTS
          },
          begin: regex.concat(
            /\b/,
            `(?!${RESERVED_KEYWORDS.join("|")})`,
            hljs3.IDENT_RE,
            regex.lookahead(/(<[^<>]+>|)\s*\(/)
          )
        };
        const EXPRESSION_CONTAINS = [
          FUNCTION_DISPATCH,
          ...PREPROCESSORS,
          CPP_PRIMITIVE_TYPES,
          C_LINE_COMMENT_MODE2,
          hljs3.C_BLOCK_COMMENT_MODE,
          NUMBERS,
          STRINGS
        ];
        const EXPRESSION_CONTEXT = {
          // This mode covers expression context where we can't expect a function
          // definition and shouldn't highlight anything that looks like one:
          // `return some()`, `else if()`, `(x*sum(1, 2))`
          variants: [
            {
              begin: /=/,
              end: /;/
            },
            {
              begin: /\(/,
              end: /\)/
            },
            {
              beginKeywords: "new throw return else",
              end: /;/
            }
          ],
          keywords: CPP_KEYWORDS,
          contains: EXPRESSION_CONTAINS.concat([
            {
              begin: /\(/,
              end: /\)/,
              keywords: CPP_KEYWORDS,
              contains: EXPRESSION_CONTAINS.concat(["self"]),
              relevance: 0
            }
          ]),
          relevance: 0
        };
        const FUNCTION_DECLARATION = {
          className: "function",
          begin: "(" + FUNCTION_TYPE_RE + "[\\*&\\s]+){1," + MAX_FUNCTION_TYPE_TOKENS + "}" + FUNCTION_TITLE,
          returnBegin: true,
          end: /[{;=]/,
          excludeEnd: true,
          keywords: CPP_KEYWORDS,
          illegal: /[^\w\s\*&:<>.]/,
          contains: [
            {
              // to prevent it from being confused as the function title
              begin: DECLTYPE_AUTO_RE,
              keywords: CPP_KEYWORDS,
              relevance: 0
            },
            {
              begin: FUNCTION_TITLE,
              returnBegin: true,
              contains: [TITLE_MODE2],
              relevance: 0
            },
            // needed because we do not have look-behind on the below rule
            // to prevent it from grabbing the final : in a :: pair
            {
              begin: /::/,
              relevance: 0
            },
            // initializers
            {
              begin: /:/,
              endsWithParent: true,
              contains: [
                STRINGS,
                NUMBERS
              ]
            },
            // allow for multiple declarations, e.g.:
            // extern void f(int), g(char);
            {
              relevance: 0,
              match: /,/
            },
            {
              className: "params",
              begin: /\(/,
              end: /\)/,
              keywords: CPP_KEYWORDS,
              relevance: 0,
              contains: [
                C_LINE_COMMENT_MODE2,
                hljs3.C_BLOCK_COMMENT_MODE,
                STRINGS,
                NUMBERS,
                CPP_PRIMITIVE_TYPES,
                // Count matching parentheses.
                {
                  begin: /\(/,
                  end: /\)/,
                  keywords: CPP_KEYWORDS,
                  relevance: 0,
                  contains: [
                    "self",
                    C_LINE_COMMENT_MODE2,
                    hljs3.C_BLOCK_COMMENT_MODE,
                    STRINGS,
                    NUMBERS,
                    CPP_PRIMITIVE_TYPES
                  ]
                }
              ]
            },
            CPP_PRIMITIVE_TYPES,
            C_LINE_COMMENT_MODE2,
            hljs3.C_BLOCK_COMMENT_MODE,
            ...PREPROCESSORS
          ]
        };
        return {
          name: "C++",
          aliases: [
            "cc",
            "c++",
            "h++",
            "hpp",
            "hh",
            "hxx",
            "cxx"
          ],
          keywords: CPP_KEYWORDS,
          illegal: "</",
          classNameAliases: { "function.dispatch": "built_in" },
          contains: [].concat(
            EXPRESSION_CONTEXT,
            FUNCTION_DECLARATION,
            FUNCTION_DISPATCH,
            EXPRESSION_CONTAINS,
            [
              ...PREPROCESSORS,
              {
                // containers: ie, `vector <int> rooms (9);`
                begin: "\\b(deque|list|queue|priority_queue|pair|stack|vector|map|set|bitset|multiset|multimap|unordered_map|unordered_set|unordered_multiset|unordered_multimap|array|tuple|optional|variant|function|flat_map|flat_set)\\s*<(?!<)",
                end: ">",
                keywords: CPP_KEYWORDS,
                contains: [
                  "self",
                  CPP_PRIMITIVE_TYPES
                ]
              },
              {
                begin: hljs3.IDENT_RE + "::",
                keywords: CPP_KEYWORDS
              },
              {
                match: [
                  // extra complexity to deal with `enum class` and `enum struct`
                  /\b(?:enum(?:\s+(?:class|struct))?|class|struct|union)/,
                  /\s+/,
                  /\w+/
                ],
                className: {
                  1: "keyword",
                  3: "title.class"
                }
              }
            ]
          )
        };
      }
      function c(hljs3) {
        const regex = hljs3.regex;
        const C_LINE_COMMENT_MODE2 = hljs3.COMMENT("//", "$", { contains: [{ begin: /\\\n/ }] });
        const DECLTYPE_AUTO_RE = "decltype\\(auto\\)";
        const NAMESPACE_RE = "[a-zA-Z_]\\w*::";
        const TEMPLATE_ARGUMENT_RE = "<[^<>]+>";
        const FUNCTION_TYPE_RE = "(" + DECLTYPE_AUTO_RE + "|" + regex.optional(NAMESPACE_RE) + "[a-zA-Z_]\\w*" + regex.optional(TEMPLATE_ARGUMENT_RE) + ")";
        const ATOMIC_TYPES = regex.concat(/\batomic_/, regex.either(
          "bool",
          "char",
          "schar",
          "uchar",
          "short",
          "ushort",
          "int",
          "uint",
          "long",
          "ulong",
          "llong",
          "ullong",
          "char16_t",
          "char32_t",
          "wchar_t",
          "int_least8_t",
          "uint_least8_t",
          "int_least16_t",
          "uint_least16_t",
          "int_least32_t",
          "uint_least32_t",
          "int_least64_t",
          "uint_least64_t",
          "int_fast8_t",
          "uint_fast8_t",
          "int_fast16_t",
          "uint_fast16_t",
          "int_fast32_t",
          "uint_fast32_t",
          "int_fast64_t",
          "uint_fast64_t",
          "intptr_t",
          "uintptr_t",
          "size_t",
          "ptrdiff_t",
          "intmax_t",
          "uintmax_t"
        ), /\b/);
        const TYPES2 = {
          className: "type",
          variants: [
            { begin: "\\b[a-z\\d_]*_t\\b" },
            { match: ATOMIC_TYPES }
          ]
        };
        const CHARACTER_ESCAPES = "\\\\(x[0-9A-Fa-f]{2}|u[0-9A-Fa-f]{4,8}|[0-7]{3}|\\S)";
        const STRINGS = {
          className: "string",
          variants: [
            {
              begin: '(u8?|U|L)?"',
              end: '"',
              illegal: "\\n",
              contains: [hljs3.BACKSLASH_ESCAPE]
            },
            {
              begin: "(u8?|U|L)?'(" + CHARACTER_ESCAPES + "|.)",
              end: "'",
              illegal: "."
            },
            // https://en.cppreference.com/w/cpp/language/string_literal
            // a d-char-sequence never contains parentheses, backslashes or whitespace;
            // quotes are excluded as well so the closing delimiter cannot swallow the
            // quote that actually terminates the literal
            hljs3.END_SAME_AS_BEGIN({
              begin: /(?:u8?|U|L)?R"([^()\\\s"]{0,16})\(/,
              end: /\)([^()\\\s"]{0,16})"/
            })
          ]
        };
        const NUMBERS = {
          className: "number",
          variants: [
            { match: /\b(0b[01']+)/ },
            { match: /(-?)\b([\d']+(\.[\d']*)?|\.[\d']+)((ll|LL|l|L)(u|U)?|(u|U)(ll|LL|l|L)?|f|F|b|B)/ },
            { match: /(-?)\b(0[xX][a-fA-F0-9]+(?:'[a-fA-F0-9]+)*(?:\.[a-fA-F0-9]*(?:'[a-fA-F0-9]*)*)?(?:[pP][-+]?[0-9]+)?(l|L)?(u|U)?)/ },
            { match: /(-?)\b\d+(?:'\d+)*(?:\.\d*(?:'\d*)*)?(?:[eE][-+]?\d+)?/ }
          ],
          relevance: 0
        };
        const PREPROCESSOR_INCLUDE = {
          scope: "meta",
          begin: /#\s*include\b/,
          end: /$/,
          keywords: { keyword: "include" },
          contains: [
            {
              // the `\` at the end of a line signaling continuation
              begin: /\\\n/
            },
            STRINGS,
            {
              scope: "string",
              begin: /<.*?>/
            },
            C_LINE_COMMENT_MODE2,
            hljs3.C_BLOCK_COMMENT_MODE
          ]
        };
        const PREPROCESSOR = {
          className: "meta",
          begin: /#\s*[a-z]+\b/,
          end: /$/,
          keywords: { keyword: "if else elif endif define undef warning error line pragma _Pragma ifdef ifndef elifdef elifndef include" },
          contains: [
            {
              begin: /\\\n/,
              relevance: 0
            },
            hljs3.inherit(STRINGS, { className: "string" }),
            C_LINE_COMMENT_MODE2,
            hljs3.C_BLOCK_COMMENT_MODE
          ]
        };
        const PREPROCESSORS = [
          PREPROCESSOR_INCLUDE,
          PREPROCESSOR
        ];
        const TITLE_MODE2 = {
          className: "title",
          begin: regex.optional(NAMESPACE_RE) + hljs3.IDENT_RE,
          relevance: 0
        };
        const FUNCTION_TITLE = regex.optional(NAMESPACE_RE) + hljs3.IDENT_RE + "\\s*\\(";
        const MAX_FUNCTION_TYPE_TOKENS = 12;
        const C_KEYWORDS = [
          "asm",
          "auto",
          "break",
          "case",
          "continue",
          "default",
          "do",
          "else",
          "enum",
          "extern",
          "for",
          "fortran",
          "goto",
          "if",
          "inline",
          "register",
          "restrict",
          "return",
          "sizeof",
          "typeof",
          "typeof_unqual",
          "struct",
          "switch",
          "typedef",
          "union",
          "volatile",
          "while",
          "_Alignas",
          "_Alignof",
          "_Atomic",
          "_Generic",
          "_Noreturn",
          "_Static_assert",
          "_Thread_local",
          // aliases
          "alignas",
          "alignof",
          "noreturn",
          "static_assert",
          "thread_local",
          // not a C keyword but is, for all intents and purposes, treated exactly like one.
          "_Pragma"
        ];
        const C_TYPES = [
          "float",
          "double",
          "signed",
          "unsigned",
          "int",
          "short",
          "long",
          "char",
          "void",
          "_Bool",
          "_BitInt",
          "_Complex",
          "_Imaginary",
          "_Decimal32",
          "_Decimal64",
          "_Decimal96",
          "_Decimal128",
          "_Decimal64x",
          "_Decimal128x",
          "_Float16",
          "_Float32",
          "_Float64",
          "_Float128",
          "_Float32x",
          "_Float64x",
          "_Float128x",
          // modifiers
          "const",
          "static",
          "constexpr",
          // aliases
          "complex",
          "bool",
          "imaginary"
        ];
        const KEYWORDS2 = {
          keyword: C_KEYWORDS,
          type: C_TYPES,
          literal: "true false NULL",
          // TODO: apply hinting work similar to what was done in cpp.js
          built_in: "std string wstring cin cout cerr clog stdin stdout stderr stringstream istringstream ostringstream auto_ptr deque list queue stack vector map set pair bitset multiset multimap unordered_set unordered_map unordered_multiset unordered_multimap priority_queue make_pair array shared_ptr abort terminate abs acos asin atan2 atan calloc ceil cosh cos exit exp fabs floor fmod fprintf fputs free frexp fscanf future isalnum isalpha iscntrl isdigit isgraph islower isprint ispunct isspace isupper isxdigit tolower toupper labs ldexp log10 log malloc realloc memchr memcmp memcpy memset modf pow printf putchar puts scanf sinh sin snprintf sprintf sqrt sscanf strcat strchr strcmp strcpy strcspn strlen strncat strncmp strncpy strpbrk strrchr strspn strstr tanh tan vfprintf vprintf vsprintf endl initializer_list unique_ptr"
        };
        const EXPRESSION_CONTAINS = [
          ...PREPROCESSORS,
          TYPES2,
          C_LINE_COMMENT_MODE2,
          hljs3.C_BLOCK_COMMENT_MODE,
          NUMBERS,
          STRINGS
        ];
        const EXPRESSION_CONTEXT = {
          // This mode covers expression context where we can't expect a function
          // definition and shouldn't highlight anything that looks like one:
          // `return some()`, `else if()`, `(x*sum(1, 2))`
          variants: [
            {
              begin: /=/,
              end: /;/
            },
            {
              begin: /\(/,
              end: /\)/
            },
            {
              beginKeywords: "new throw return else",
              end: /;/
            }
          ],
          keywords: KEYWORDS2,
          contains: EXPRESSION_CONTAINS.concat([
            {
              begin: /\(/,
              end: /\)/,
              keywords: KEYWORDS2,
              contains: EXPRESSION_CONTAINS.concat(["self"]),
              relevance: 0
            }
          ]),
          relevance: 0
        };
        const FUNCTION_DECLARATION = {
          begin: "(" + FUNCTION_TYPE_RE + "[\\*&\\s]+){1," + MAX_FUNCTION_TYPE_TOKENS + "}" + FUNCTION_TITLE,
          returnBegin: true,
          end: /[{;=]/,
          excludeEnd: true,
          keywords: KEYWORDS2,
          illegal: /[^\w\s\*&:<>.]/,
          contains: [
            {
              // to prevent it from being confused as the function title
              begin: DECLTYPE_AUTO_RE,
              keywords: KEYWORDS2,
              relevance: 0
            },
            {
              begin: FUNCTION_TITLE,
              returnBegin: true,
              contains: [hljs3.inherit(TITLE_MODE2, { className: "title.function" })],
              relevance: 0
            },
            // allow for multiple declarations, e.g.:
            // extern void f(int), g(char);
            {
              relevance: 0,
              match: /,/
            },
            {
              className: "params",
              begin: /\(/,
              end: /\)/,
              keywords: KEYWORDS2,
              relevance: 0,
              contains: [
                C_LINE_COMMENT_MODE2,
                hljs3.C_BLOCK_COMMENT_MODE,
                STRINGS,
                NUMBERS,
                TYPES2,
                // Count matching parentheses.
                {
                  begin: /\(/,
                  end: /\)/,
                  keywords: KEYWORDS2,
                  relevance: 0,
                  contains: [
                    "self",
                    C_LINE_COMMENT_MODE2,
                    hljs3.C_BLOCK_COMMENT_MODE,
                    STRINGS,
                    NUMBERS,
                    TYPES2
                  ]
                }
              ]
            },
            TYPES2,
            C_LINE_COMMENT_MODE2,
            hljs3.C_BLOCK_COMMENT_MODE,
            ...PREPROCESSORS
          ]
        };
        return {
          name: "C",
          aliases: ["h"],
          keywords: KEYWORDS2,
          // Until differentiations are added between `c` and `cpp`, `c` will
          // not be auto-detected to avoid auto-detect conflicts between C and C++
          disableAutodetect: true,
          illegal: "</",
          contains: [].concat(
            EXPRESSION_CONTEXT,
            FUNCTION_DECLARATION,
            EXPRESSION_CONTAINS,
            [
              ...PREPROCESSORS,
              {
                begin: hljs3.IDENT_RE + "::",
                keywords: KEYWORDS2
              },
              {
                className: "class",
                beginKeywords: "enum class struct union",
                end: /[{;:<>=]/,
                contains: [
                  { beginKeywords: "final class struct" },
                  hljs3.TITLE_MODE
                ]
              }
            ]
          ),
          exports: {
            preprocessor: PREPROCESSOR,
            strings: STRINGS,
            keywords: KEYWORDS2
          }
        };
      }
      var builtIns = /* @__PURE__ */ Object.freeze({
        __proto__: null,
        grmr_bash: bash,
        grmr_c: c,
        grmr_cpp: cpp,
        grmr_csharp: csharp,
        grmr_css: css,
        grmr_diff: diff,
        grmr_go: go,
        grmr_graphql: graphql,
        grmr_ini: ini,
        grmr_java: java,
        grmr_javascript: javascript,
        grmr_json: json,
        grmr_kotlin: kotlin,
        grmr_less: less,
        grmr_lua: lua,
        grmr_makefile: makefile,
        grmr_markdown: markdown,
        grmr_objectivec: objectivec,
        grmr_perl: perl,
        grmr_php: php,
        grmr_php_template: phpTemplate,
        grmr_plaintext: plaintext,
        grmr_python: python,
        grmr_python_repl: pythonRepl,
        grmr_r: r,
        grmr_ruby: ruby,
        grmr_rust: rust,
        grmr_scss: scss,
        grmr_shell: shell,
        grmr_sql: sql,
        grmr_swift: swift,
        grmr_typescript: typescript,
        grmr_vbnet: vbnet,
        grmr_wasm: wasm,
        grmr_xml: xml,
        grmr_yaml: yaml
      });
      const hljs2 = highlight2;
      for (const key of Object.keys(builtIns)) {
        const languageName = key.replace("grmr_", "").replace("_", "-");
        hljs2.registerLanguage(languageName, builtIns[key]);
      }
      return hljs2;
    })();
    if (typeof exports === "object" && typeof module !== "undefined") {
      module.exports = hljs;
    }
  }
});

// highlight.js
var import_highlight = __toESM(require_highlight());

// ini.js
function ini_default(hljs) {
  const regex = hljs.regex;
  const NUMBERS = {
    className: "number",
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
    className: "literal",
    begin: /\bon|off|true|false|yes|no\b/
  };
  const KEY = /[A-Za-z0-9_-]+/;
  const KEY_PREFIX = regex.concat(KEY, / *= */);
  return {
    name: "INI",
    case_insensitive: true,
    illegal: /\S/,
    contains: [
      COMMENTS,
      {
        className: "keyword",
        begin: /\[+/,
        end: /\]+/
      },
      {
        begin: KEY_PREFIX,
        className: "title",
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

// highlight.js
(0, import_highlight.registerLanguage)("ini", ini_default);
var export_addPlugin = import_highlight.addPlugin;
var export_highlight = import_highlight.highlight;
var export_highlightAll = import_highlight.highlightAll;
var export_highlightAuto = import_highlight.highlightAuto;
var export_highlightElement = import_highlight.highlightElement;
var export_registerLanguage = import_highlight.registerLanguage;
export {
  export_addPlugin as addPlugin,
  export_highlight as highlight,
  export_highlightAll as highlightAll,
  export_highlightAuto as highlightAuto,
  export_highlightElement as highlightElement,
  export_registerLanguage as registerLanguage
};
/*!
  Highlight.js v11.12.0 (git: f7f7d3803b)
  (c) 2006-2026 Josh Goebel <hello@joshgoebel.com> and other contributors
  License: BSD-3-Clause
 */
