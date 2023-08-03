var __defProp = Object.defineProperty;
var __getOwnPropDesc = Object.getOwnPropertyDescriptor;
var __getOwnPropNames = Object.getOwnPropertyNames;
var __hasOwnProp = Object.prototype.hasOwnProperty;
var __esm = (fn, res) => function __init() {
  return fn && (res = (0, fn[__getOwnPropNames(fn)[0]])(fn = 0)), res;
};
var __export = (target, all) => {
  for (var name in all)
    __defProp(target, name, { get: all[name], enumerable: true });
};
var __copyProps = (to, from, except, desc) => {
  if (from && typeof from === "object" || typeof from === "function") {
    for (let key of __getOwnPropNames(from))
      if (!__hasOwnProp.call(to, key) && key !== except)
        __defProp(to, key, { get: () => from[key], enumerable: !(desc = __getOwnPropDesc(from, key)) || desc.enumerable });
  }
  return to;
};
var __toCommonJS = (mod) => __copyProps(__defProp({}, "__esModule", { value: true }), mod);

// node_modules/lit-html/lit-html.js
function P(t3, i6) {
  if (!Array.isArray(t3) || !t3.hasOwnProperty("raw"))
    throw Error("invalid template strings array");
  return void 0 !== e ? e.createHTML(i6) : i6;
}
function S(t3, i6, s5 = t3, e7) {
  var o5, n5, l5, h5;
  if (i6 === T)
    return i6;
  let r3 = void 0 !== e7 ? null === (o5 = s5._$Co) || void 0 === o5 ? void 0 : o5[e7] : s5._$Cl;
  const u2 = d(i6) ? void 0 : i6._$litDirective$;
  return (null == r3 ? void 0 : r3.constructor) !== u2 && (null === (n5 = null == r3 ? void 0 : r3._$AO) || void 0 === n5 || n5.call(r3, false), void 0 === u2 ? r3 = void 0 : (r3 = new u2(t3), r3._$AT(t3, s5, e7)), void 0 !== e7 ? (null !== (l5 = (h5 = s5)._$Co) && void 0 !== l5 ? l5 : h5._$Co = [])[e7] = r3 : s5._$Cl = r3), void 0 !== r3 && (i6 = S(t3, r3._$AS(t3, i6.values), r3, e7)), i6;
}
var t, i, s, e, o, n, l, h, r, u, d, c, v, a, f, _, m, p, g, $, y, w, x, b, T, A, E, C, V, N, M, R, k, H, I, L, z, Z, j, B, D;
var init_lit_html = __esm({
  "node_modules/lit-html/lit-html.js"() {
    i = window;
    s = i.trustedTypes;
    e = s ? s.createPolicy("lit-html", { createHTML: (t3) => t3 }) : void 0;
    o = "$lit$";
    n = `lit$${(Math.random() + "").slice(9)}$`;
    l = "?" + n;
    h = `<${l}>`;
    r = document;
    u = () => r.createComment("");
    d = (t3) => null === t3 || "object" != typeof t3 && "function" != typeof t3;
    c = Array.isArray;
    v = (t3) => c(t3) || "function" == typeof (null == t3 ? void 0 : t3[Symbol.iterator]);
    a = "[ 	\n\f\r]";
    f = /<(?:(!--|\/[^a-zA-Z])|(\/?[a-zA-Z][^>\s]*)|(\/?$))/g;
    _ = /-->/g;
    m = />/g;
    p = RegExp(`>|${a}(?:([^\\s"'>=/]+)(${a}*=${a}*(?:[^ 	
\f\r"'\`<>=]|("|')|))|$)`, "g");
    g = /'/g;
    $ = /"/g;
    y = /^(?:script|style|textarea|title)$/i;
    w = (t3) => (i6, ...s5) => ({ _$litType$: t3, strings: i6, values: s5 });
    x = w(1);
    b = w(2);
    T = Symbol.for("lit-noChange");
    A = Symbol.for("lit-nothing");
    E = /* @__PURE__ */ new WeakMap();
    C = r.createTreeWalker(r, 129, null, false);
    V = (t3, i6) => {
      const s5 = t3.length - 1, e7 = [];
      let l5, r3 = 2 === i6 ? "<svg>" : "", u2 = f;
      for (let i7 = 0; i7 < s5; i7++) {
        const s6 = t3[i7];
        let d2, c4, v2 = -1, a3 = 0;
        for (; a3 < s6.length && (u2.lastIndex = a3, c4 = u2.exec(s6), null !== c4); )
          a3 = u2.lastIndex, u2 === f ? "!--" === c4[1] ? u2 = _ : void 0 !== c4[1] ? u2 = m : void 0 !== c4[2] ? (y.test(c4[2]) && (l5 = RegExp("</" + c4[2], "g")), u2 = p) : void 0 !== c4[3] && (u2 = p) : u2 === p ? ">" === c4[0] ? (u2 = null != l5 ? l5 : f, v2 = -1) : void 0 === c4[1] ? v2 = -2 : (v2 = u2.lastIndex - c4[2].length, d2 = c4[1], u2 = void 0 === c4[3] ? p : '"' === c4[3] ? $ : g) : u2 === $ || u2 === g ? u2 = p : u2 === _ || u2 === m ? u2 = f : (u2 = p, l5 = void 0);
        const w2 = u2 === p && t3[i7 + 1].startsWith("/>") ? " " : "";
        r3 += u2 === f ? s6 + h : v2 >= 0 ? (e7.push(d2), s6.slice(0, v2) + o + s6.slice(v2) + n + w2) : s6 + n + (-2 === v2 ? (e7.push(void 0), i7) : w2);
      }
      return [P(t3, r3 + (t3[s5] || "<?>") + (2 === i6 ? "</svg>" : "")), e7];
    };
    N = class {
      constructor({ strings: t3, _$litType$: i6 }, e7) {
        let h5;
        this.parts = [];
        let r3 = 0, d2 = 0;
        const c4 = t3.length - 1, v2 = this.parts, [a3, f2] = V(t3, i6);
        if (this.el = N.createElement(a3, e7), C.currentNode = this.el.content, 2 === i6) {
          const t4 = this.el.content, i7 = t4.firstChild;
          i7.remove(), t4.append(...i7.childNodes);
        }
        for (; null !== (h5 = C.nextNode()) && v2.length < c4; ) {
          if (1 === h5.nodeType) {
            if (h5.hasAttributes()) {
              const t4 = [];
              for (const i7 of h5.getAttributeNames())
                if (i7.endsWith(o) || i7.startsWith(n)) {
                  const s5 = f2[d2++];
                  if (t4.push(i7), void 0 !== s5) {
                    const t5 = h5.getAttribute(s5.toLowerCase() + o).split(n), i8 = /([.?@])?(.*)/.exec(s5);
                    v2.push({ type: 1, index: r3, name: i8[2], strings: t5, ctor: "." === i8[1] ? H : "?" === i8[1] ? L : "@" === i8[1] ? z : k });
                  } else
                    v2.push({ type: 6, index: r3 });
                }
              for (const i7 of t4)
                h5.removeAttribute(i7);
            }
            if (y.test(h5.tagName)) {
              const t4 = h5.textContent.split(n), i7 = t4.length - 1;
              if (i7 > 0) {
                h5.textContent = s ? s.emptyScript : "";
                for (let s5 = 0; s5 < i7; s5++)
                  h5.append(t4[s5], u()), C.nextNode(), v2.push({ type: 2, index: ++r3 });
                h5.append(t4[i7], u());
              }
            }
          } else if (8 === h5.nodeType)
            if (h5.data === l)
              v2.push({ type: 2, index: r3 });
            else {
              let t4 = -1;
              for (; -1 !== (t4 = h5.data.indexOf(n, t4 + 1)); )
                v2.push({ type: 7, index: r3 }), t4 += n.length - 1;
            }
          r3++;
        }
      }
      static createElement(t3, i6) {
        const s5 = r.createElement("template");
        return s5.innerHTML = t3, s5;
      }
    };
    M = class {
      constructor(t3, i6) {
        this._$AV = [], this._$AN = void 0, this._$AD = t3, this._$AM = i6;
      }
      get parentNode() {
        return this._$AM.parentNode;
      }
      get _$AU() {
        return this._$AM._$AU;
      }
      u(t3) {
        var i6;
        const { el: { content: s5 }, parts: e7 } = this._$AD, o5 = (null !== (i6 = null == t3 ? void 0 : t3.creationScope) && void 0 !== i6 ? i6 : r).importNode(s5, true);
        C.currentNode = o5;
        let n5 = C.nextNode(), l5 = 0, h5 = 0, u2 = e7[0];
        for (; void 0 !== u2; ) {
          if (l5 === u2.index) {
            let i7;
            2 === u2.type ? i7 = new R(n5, n5.nextSibling, this, t3) : 1 === u2.type ? i7 = new u2.ctor(n5, u2.name, u2.strings, this, t3) : 6 === u2.type && (i7 = new Z(n5, this, t3)), this._$AV.push(i7), u2 = e7[++h5];
          }
          l5 !== (null == u2 ? void 0 : u2.index) && (n5 = C.nextNode(), l5++);
        }
        return C.currentNode = r, o5;
      }
      v(t3) {
        let i6 = 0;
        for (const s5 of this._$AV)
          void 0 !== s5 && (void 0 !== s5.strings ? (s5._$AI(t3, s5, i6), i6 += s5.strings.length - 2) : s5._$AI(t3[i6])), i6++;
      }
    };
    R = class {
      constructor(t3, i6, s5, e7) {
        var o5;
        this.type = 2, this._$AH = A, this._$AN = void 0, this._$AA = t3, this._$AB = i6, this._$AM = s5, this.options = e7, this._$Cp = null === (o5 = null == e7 ? void 0 : e7.isConnected) || void 0 === o5 || o5;
      }
      get _$AU() {
        var t3, i6;
        return null !== (i6 = null === (t3 = this._$AM) || void 0 === t3 ? void 0 : t3._$AU) && void 0 !== i6 ? i6 : this._$Cp;
      }
      get parentNode() {
        let t3 = this._$AA.parentNode;
        const i6 = this._$AM;
        return void 0 !== i6 && 11 === (null == t3 ? void 0 : t3.nodeType) && (t3 = i6.parentNode), t3;
      }
      get startNode() {
        return this._$AA;
      }
      get endNode() {
        return this._$AB;
      }
      _$AI(t3, i6 = this) {
        t3 = S(this, t3, i6), d(t3) ? t3 === A || null == t3 || "" === t3 ? (this._$AH !== A && this._$AR(), this._$AH = A) : t3 !== this._$AH && t3 !== T && this._(t3) : void 0 !== t3._$litType$ ? this.g(t3) : void 0 !== t3.nodeType ? this.$(t3) : v(t3) ? this.T(t3) : this._(t3);
      }
      k(t3) {
        return this._$AA.parentNode.insertBefore(t3, this._$AB);
      }
      $(t3) {
        this._$AH !== t3 && (this._$AR(), this._$AH = this.k(t3));
      }
      _(t3) {
        this._$AH !== A && d(this._$AH) ? this._$AA.nextSibling.data = t3 : this.$(r.createTextNode(t3)), this._$AH = t3;
      }
      g(t3) {
        var i6;
        const { values: s5, _$litType$: e7 } = t3, o5 = "number" == typeof e7 ? this._$AC(t3) : (void 0 === e7.el && (e7.el = N.createElement(P(e7.h, e7.h[0]), this.options)), e7);
        if ((null === (i6 = this._$AH) || void 0 === i6 ? void 0 : i6._$AD) === o5)
          this._$AH.v(s5);
        else {
          const t4 = new M(o5, this), i7 = t4.u(this.options);
          t4.v(s5), this.$(i7), this._$AH = t4;
        }
      }
      _$AC(t3) {
        let i6 = E.get(t3.strings);
        return void 0 === i6 && E.set(t3.strings, i6 = new N(t3)), i6;
      }
      T(t3) {
        c(this._$AH) || (this._$AH = [], this._$AR());
        const i6 = this._$AH;
        let s5, e7 = 0;
        for (const o5 of t3)
          e7 === i6.length ? i6.push(s5 = new R(this.k(u()), this.k(u()), this, this.options)) : s5 = i6[e7], s5._$AI(o5), e7++;
        e7 < i6.length && (this._$AR(s5 && s5._$AB.nextSibling, e7), i6.length = e7);
      }
      _$AR(t3 = this._$AA.nextSibling, i6) {
        var s5;
        for (null === (s5 = this._$AP) || void 0 === s5 || s5.call(this, false, true, i6); t3 && t3 !== this._$AB; ) {
          const i7 = t3.nextSibling;
          t3.remove(), t3 = i7;
        }
      }
      setConnected(t3) {
        var i6;
        void 0 === this._$AM && (this._$Cp = t3, null === (i6 = this._$AP) || void 0 === i6 || i6.call(this, t3));
      }
    };
    k = class {
      constructor(t3, i6, s5, e7, o5) {
        this.type = 1, this._$AH = A, this._$AN = void 0, this.element = t3, this.name = i6, this._$AM = e7, this.options = o5, s5.length > 2 || "" !== s5[0] || "" !== s5[1] ? (this._$AH = Array(s5.length - 1).fill(new String()), this.strings = s5) : this._$AH = A;
      }
      get tagName() {
        return this.element.tagName;
      }
      get _$AU() {
        return this._$AM._$AU;
      }
      _$AI(t3, i6 = this, s5, e7) {
        const o5 = this.strings;
        let n5 = false;
        if (void 0 === o5)
          t3 = S(this, t3, i6, 0), n5 = !d(t3) || t3 !== this._$AH && t3 !== T, n5 && (this._$AH = t3);
        else {
          const e8 = t3;
          let l5, h5;
          for (t3 = o5[0], l5 = 0; l5 < o5.length - 1; l5++)
            h5 = S(this, e8[s5 + l5], i6, l5), h5 === T && (h5 = this._$AH[l5]), n5 || (n5 = !d(h5) || h5 !== this._$AH[l5]), h5 === A ? t3 = A : t3 !== A && (t3 += (null != h5 ? h5 : "") + o5[l5 + 1]), this._$AH[l5] = h5;
        }
        n5 && !e7 && this.j(t3);
      }
      j(t3) {
        t3 === A ? this.element.removeAttribute(this.name) : this.element.setAttribute(this.name, null != t3 ? t3 : "");
      }
    };
    H = class extends k {
      constructor() {
        super(...arguments), this.type = 3;
      }
      j(t3) {
        this.element[this.name] = t3 === A ? void 0 : t3;
      }
    };
    I = s ? s.emptyScript : "";
    L = class extends k {
      constructor() {
        super(...arguments), this.type = 4;
      }
      j(t3) {
        t3 && t3 !== A ? this.element.setAttribute(this.name, I) : this.element.removeAttribute(this.name);
      }
    };
    z = class extends k {
      constructor(t3, i6, s5, e7, o5) {
        super(t3, i6, s5, e7, o5), this.type = 5;
      }
      _$AI(t3, i6 = this) {
        var s5;
        if ((t3 = null !== (s5 = S(this, t3, i6, 0)) && void 0 !== s5 ? s5 : A) === T)
          return;
        const e7 = this._$AH, o5 = t3 === A && e7 !== A || t3.capture !== e7.capture || t3.once !== e7.once || t3.passive !== e7.passive, n5 = t3 !== A && (e7 === A || o5);
        o5 && this.element.removeEventListener(this.name, this, e7), n5 && this.element.addEventListener(this.name, this, t3), this._$AH = t3;
      }
      handleEvent(t3) {
        var i6, s5;
        "function" == typeof this._$AH ? this._$AH.call(null !== (s5 = null === (i6 = this.options) || void 0 === i6 ? void 0 : i6.host) && void 0 !== s5 ? s5 : this.element, t3) : this._$AH.handleEvent(t3);
      }
    };
    Z = class {
      constructor(t3, i6, s5) {
        this.element = t3, this.type = 6, this._$AN = void 0, this._$AM = i6, this.options = s5;
      }
      get _$AU() {
        return this._$AM._$AU;
      }
      _$AI(t3) {
        S(this, t3);
      }
    };
    j = { O: o, P: n, A: l, C: 1, M: V, L: M, R: v, D: S, I: R, V: k, H: L, N: z, U: H, F: Z };
    B = i.litHtmlPolyfillSupport;
    null == B || B(N, R), (null !== (t = i.litHtmlVersions) && void 0 !== t ? t : i.litHtmlVersions = []).push("2.8.0");
    D = (t3, i6, s5) => {
      var e7, o5;
      const n5 = null !== (e7 = null == s5 ? void 0 : s5.renderBefore) && void 0 !== e7 ? e7 : i6;
      let l5 = n5._$litPart$;
      if (void 0 === l5) {
        const t4 = null !== (o5 = null == s5 ? void 0 : s5.renderBefore) && void 0 !== o5 ? o5 : null;
        n5._$litPart$ = l5 = new R(i6.insertBefore(u(), t4), t4, void 0, null != s5 ? s5 : {});
      }
      return l5._$AI(t3), l5;
    };
  }
});

// node_modules/lit/html.js
var html_exports = {};
__export(html_exports, {
  _$LH: () => j,
  html: () => x,
  noChange: () => T,
  nothing: () => A,
  render: () => D,
  svg: () => b
});
var init_html = __esm({
  "node_modules/lit/html.js"() {
    init_lit_html();
  }
});

// node_modules/lit-html/directive.js
var t2, e2, i2;
var init_directive = __esm({
  "node_modules/lit-html/directive.js"() {
    t2 = { ATTRIBUTE: 1, CHILD: 2, PROPERTY: 3, BOOLEAN_ATTRIBUTE: 4, EVENT: 5, ELEMENT: 6 };
    e2 = (t3) => (...e7) => ({ _$litDirective$: t3, values: e7 });
    i2 = class {
      constructor(t3) {
      }
      get _$AU() {
        return this._$AM._$AU;
      }
      _$AT(t3, e7, i6) {
        this._$Ct = t3, this._$AM = e7, this._$Ci = i6;
      }
      _$AS(t3, e7) {
        return this.update(t3, e7);
      }
      update(t3, e7) {
        return this.render(...e7);
      }
    };
  }
});

// node_modules/lit/directive.js
var directive_exports = {};
__export(directive_exports, {
  Directive: () => i2,
  PartType: () => t2,
  directive: () => e2
});
var init_directive2 = __esm({
  "node_modules/lit/directive.js"() {
    init_directive();
  }
});

// node_modules/lit-html/directives/guard.js
var e3, i3;
var init_guard = __esm({
  "node_modules/lit-html/directives/guard.js"() {
    init_lit_html();
    init_directive();
    e3 = {};
    i3 = e2(class extends i2 {
      constructor() {
        super(...arguments), this.st = e3;
      }
      render(r3, t3) {
        return t3();
      }
      update(t3, [s5, e7]) {
        if (Array.isArray(s5)) {
          if (Array.isArray(this.st) && this.st.length === s5.length && s5.every((r3, t4) => r3 === this.st[t4]))
            return T;
        } else if (this.st === s5)
          return T;
        return this.st = Array.isArray(s5) ? Array.from(s5) : s5, this.render(s5, e7);
      }
    });
  }
});

// node_modules/lit/directives/guard.js
var guard_exports = {};
__export(guard_exports, {
  guard: () => i3
});
var init_guard2 = __esm({
  "node_modules/lit/directives/guard.js"() {
    init_guard();
  }
});

// node_modules/lit-html/directive-helpers.js
var l2, i4, e4, s2, a2;
var init_directive_helpers = __esm({
  "node_modules/lit-html/directive-helpers.js"() {
    init_lit_html();
    ({ I: l2 } = j);
    i4 = (o5) => null === o5 || "object" != typeof o5 && "function" != typeof o5;
    e4 = (o5) => void 0 === o5.strings;
    s2 = {};
    a2 = (o5, l5 = s2) => o5._$AH = l5;
  }
});

// node_modules/lit-html/directives/live.js
var l3;
var init_live = __esm({
  "node_modules/lit-html/directives/live.js"() {
    init_lit_html();
    init_directive();
    init_directive_helpers();
    l3 = e2(class extends i2 {
      constructor(r3) {
        if (super(r3), r3.type !== t2.PROPERTY && r3.type !== t2.ATTRIBUTE && r3.type !== t2.BOOLEAN_ATTRIBUTE)
          throw Error("The `live` directive is not allowed on child or event bindings");
        if (!e4(r3))
          throw Error("`live` bindings can only contain a single expression");
      }
      render(r3) {
        return r3;
      }
      update(i6, [t3]) {
        if (t3 === T || t3 === A)
          return t3;
        const o5 = i6.element, l5 = i6.name;
        if (i6.type === t2.PROPERTY) {
          if (t3 === o5[l5])
            return T;
        } else if (i6.type === t2.BOOLEAN_ATTRIBUTE) {
          if (!!t3 === o5.hasAttribute(l5))
            return T;
        } else if (i6.type === t2.ATTRIBUTE && o5.getAttribute(l5) === t3 + "")
          return T;
        return a2(i6), t3;
      }
    });
  }
});

// node_modules/lit/directives/live.js
var live_exports = {};
__export(live_exports, {
  live: () => l3
});
var init_live2 = __esm({
  "node_modules/lit/directives/live.js"() {
    init_live();
  }
});

// node_modules/lit-html/async-directive.js
function n2(i6) {
  void 0 !== this._$AN ? (o2(this), this._$AM = i6, r2(this)) : this._$AM = i6;
}
function h2(i6, t3 = false, e7 = 0) {
  const r3 = this._$AH, n5 = this._$AN;
  if (void 0 !== n5 && 0 !== n5.size)
    if (t3)
      if (Array.isArray(r3))
        for (let i7 = e7; i7 < r3.length; i7++)
          s3(r3[i7], false), o2(r3[i7]);
      else
        null != r3 && (s3(r3, false), o2(r3));
    else
      s3(this, i6);
}
var s3, o2, r2, l4, c2;
var init_async_directive = __esm({
  "node_modules/lit-html/async-directive.js"() {
    init_directive_helpers();
    init_directive();
    init_directive();
    s3 = (i6, t3) => {
      var e7, o5;
      const r3 = i6._$AN;
      if (void 0 === r3)
        return false;
      for (const i7 of r3)
        null === (o5 = (e7 = i7)._$AO) || void 0 === o5 || o5.call(e7, t3, false), s3(i7, t3);
      return true;
    };
    o2 = (i6) => {
      let t3, e7;
      do {
        if (void 0 === (t3 = i6._$AM))
          break;
        e7 = t3._$AN, e7.delete(i6), i6 = t3;
      } while (0 === (null == e7 ? void 0 : e7.size));
    };
    r2 = (i6) => {
      for (let t3; t3 = i6._$AM; i6 = t3) {
        let e7 = t3._$AN;
        if (void 0 === e7)
          t3._$AN = e7 = /* @__PURE__ */ new Set();
        else if (e7.has(i6))
          break;
        e7.add(i6), l4(t3);
      }
    };
    l4 = (i6) => {
      var t3, s5, o5, r3;
      i6.type == t2.CHILD && (null !== (t3 = (o5 = i6)._$AP) && void 0 !== t3 || (o5._$AP = h2), null !== (s5 = (r3 = i6)._$AQ) && void 0 !== s5 || (r3._$AQ = n2));
    };
    c2 = class extends i2 {
      constructor() {
        super(...arguments), this._$AN = void 0;
      }
      _$AT(i6, t3, e7) {
        super._$AT(i6, t3, e7), r2(this), this.isConnected = i6._$AU;
      }
      _$AO(i6, t3 = true) {
        var e7, r3;
        i6 !== this.isConnected && (this.isConnected = i6, i6 ? null === (e7 = this.reconnected) || void 0 === e7 || e7.call(this) : null === (r3 = this.disconnected) || void 0 === r3 || r3.call(this)), t3 && (s3(this, i6), o2(this));
      }
      setValue(t3) {
        if (e4(this._$Ct))
          this._$Ct._$AI(t3, this);
        else {
          const i6 = [...this._$Ct._$AH];
          i6[this._$Ci] = t3, this._$Ct._$AI(i6, this, 0);
        }
      }
      disconnected() {
      }
      reconnected() {
      }
    };
  }
});

// node_modules/lit-html/directives/ref.js
var e5, o3, h3, n3;
var init_ref = __esm({
  "node_modules/lit-html/directives/ref.js"() {
    init_lit_html();
    init_async_directive();
    init_directive();
    e5 = () => new o3();
    o3 = class {
    };
    h3 = /* @__PURE__ */ new WeakMap();
    n3 = e2(class extends c2 {
      render(t3) {
        return A;
      }
      update(t3, [s5]) {
        var e7;
        const o5 = s5 !== this.G;
        return o5 && void 0 !== this.G && this.ot(void 0), (o5 || this.rt !== this.lt) && (this.G = s5, this.dt = null === (e7 = t3.options) || void 0 === e7 ? void 0 : e7.host, this.ot(this.lt = t3.element)), A;
      }
      ot(i6) {
        var t3;
        if ("function" == typeof this.G) {
          const s5 = null !== (t3 = this.dt) && void 0 !== t3 ? t3 : globalThis;
          let e7 = h3.get(s5);
          void 0 === e7 && (e7 = /* @__PURE__ */ new WeakMap(), h3.set(s5, e7)), void 0 !== e7.get(this.G) && this.G.call(this.dt, void 0), e7.set(this.G, i6), void 0 !== i6 && this.G.call(this.dt, i6);
        } else
          this.G.value = i6;
      }
      get rt() {
        var i6, t3, s5;
        return "function" == typeof this.G ? null === (t3 = h3.get(null !== (i6 = this.dt) && void 0 !== i6 ? i6 : globalThis)) || void 0 === t3 ? void 0 : t3.get(this.G) : null === (s5 = this.G) || void 0 === s5 ? void 0 : s5.value;
      }
      disconnected() {
        this.rt === this.lt && this.ot(void 0);
      }
      reconnected() {
        this.ot(this.lt);
      }
    });
  }
});

// node_modules/lit/directives/ref.js
var ref_exports = {};
__export(ref_exports, {
  createRef: () => e5,
  ref: () => n3
});
var init_ref2 = __esm({
  "node_modules/lit/directives/ref.js"() {
    init_ref();
  }
});

// node_modules/lit-html/directives/private-async-helpers.js
var s4, i5;
var init_private_async_helpers = __esm({
  "node_modules/lit-html/directives/private-async-helpers.js"() {
    s4 = class {
      constructor(t3) {
        this.G = t3;
      }
      disconnect() {
        this.G = void 0;
      }
      reconnect(t3) {
        this.G = t3;
      }
      deref() {
        return this.G;
      }
    };
    i5 = class {
      constructor() {
        this.Y = void 0, this.Z = void 0;
      }
      get() {
        return this.Y;
      }
      pause() {
        var t3;
        null !== (t3 = this.Y) && void 0 !== t3 || (this.Y = new Promise((t4) => this.Z = t4));
      }
      resume() {
        var t3;
        null === (t3 = this.Z) || void 0 === t3 || t3.call(this), this.Y = this.Z = void 0;
      }
    };
  }
});

// node_modules/lit-html/directives/until.js
var n4, h4, c3, m2;
var init_until = __esm({
  "node_modules/lit-html/directives/until.js"() {
    init_lit_html();
    init_directive_helpers();
    init_async_directive();
    init_private_async_helpers();
    init_directive();
    n4 = (t3) => !i4(t3) && "function" == typeof t3.then;
    h4 = 1073741823;
    c3 = class extends c2 {
      constructor() {
        super(...arguments), this._$C_t = h4, this._$Cwt = [], this._$Cq = new s4(this), this._$CK = new i5();
      }
      render(...s5) {
        var i6;
        return null !== (i6 = s5.find((t3) => !n4(t3))) && void 0 !== i6 ? i6 : T;
      }
      update(s5, i6) {
        const r3 = this._$Cwt;
        let e7 = r3.length;
        this._$Cwt = i6;
        const o5 = this._$Cq, c4 = this._$CK;
        this.isConnected || this.disconnected();
        for (let t3 = 0; t3 < i6.length && !(t3 > this._$C_t); t3++) {
          const s6 = i6[t3];
          if (!n4(s6))
            return this._$C_t = t3, s6;
          t3 < e7 && s6 === r3[t3] || (this._$C_t = h4, e7 = 0, Promise.resolve(s6).then(async (t4) => {
            for (; c4.get(); )
              await c4.get();
            const i7 = o5.deref();
            if (void 0 !== i7) {
              const r4 = i7._$Cwt.indexOf(s6);
              r4 > -1 && r4 < i7._$C_t && (i7._$C_t = r4, i7.setValue(t4));
            }
          }));
        }
        return T;
      }
      disconnected() {
        this._$Cq.disconnect(), this._$CK.pause();
      }
      reconnected() {
        this._$Cq.reconnect(this), this._$CK.resume();
      }
    };
    m2 = e2(c3);
  }
});

// node_modules/lit/directives/until.js
var until_exports = {};
__export(until_exports, {
  UntilDirective: () => c3,
  until: () => m2
});
var init_until2 = __esm({
  "node_modules/lit/directives/until.js"() {
    init_until();
  }
});

// node_modules/lit-html/directives/unsafe-html.js
var e6, o4;
var init_unsafe_html = __esm({
  "node_modules/lit-html/directives/unsafe-html.js"() {
    init_lit_html();
    init_directive();
    e6 = class extends i2 {
      constructor(i6) {
        if (super(i6), this.et = A, i6.type !== t2.CHILD)
          throw Error(this.constructor.directiveName + "() can only be used in child bindings");
      }
      render(r3) {
        if (r3 === A || null == r3)
          return this.ft = void 0, this.et = r3;
        if (r3 === T)
          return r3;
        if ("string" != typeof r3)
          throw Error(this.constructor.directiveName + "() called with a non-string value");
        if (r3 === this.et)
          return this.ft;
        this.et = r3;
        const s5 = [r3];
        return s5.raw = s5, this.ft = { _$litType$: this.constructor.resultType, strings: s5, values: [] };
      }
    };
    e6.directiveName = "unsafeHTML", e6.resultType = 1;
    o4 = e2(e6);
  }
});

// node_modules/lit/directives/unsafe-html.js
var unsafe_html_exports = {};
__export(unsafe_html_exports, {
  UnsafeHTMLDirective: () => e6,
  unsafeHTML: () => o4
});
var init_unsafe_html2 = __esm({
  "node_modules/lit/directives/unsafe-html.js"() {
    init_unsafe_html();
  }
});

// lit-html.js
var { render, noChange, nothing } = (init_html(), __toCommonJS(html_exports));
var { directive, Directive } = (init_directive2(), __toCommonJS(directive_exports));
var { guard } = (init_guard2(), __toCommonJS(guard_exports));
var { live } = (init_live2(), __toCommonJS(live_exports));
var { ref } = (init_ref2(), __toCommonJS(ref_exports));
var { until } = (init_until2(), __toCommonJS(until_exports));
var { unsafeHTML } = (init_unsafe_html2(), __toCommonJS(unsafe_html_exports));
var html = (strings, ...values) => {
  transformValues(values);
  return {
    ["_$litType$"]: 1,
    // HTML_RESULT
    strings,
    values
  };
};
var svg = (strings, ...values) => {
  transformValues(values);
  return {
    ["_$litType$"]: 2,
    // SVG_RESULT
    strings,
    values
  };
};
function transformValues(values) {
  for (let i6 = 0; i6 < values.length; i6++) {
    const value = values[i6];
    if (value != null && typeof value.toHTML === "function")
      values[i6] = value.toHTML();
  }
}
export {
  Directive,
  directive,
  guard,
  html,
  live,
  noChange,
  nothing,
  ref,
  render,
  svg,
  unsafeHTML,
  until
};
/*! Bundled license information:

lit-html/lit-html.js:
  (**
   * @license
   * Copyright 2017 Google LLC
   * SPDX-License-Identifier: BSD-3-Clause
   *)

lit-html/directive.js:
  (**
   * @license
   * Copyright 2017 Google LLC
   * SPDX-License-Identifier: BSD-3-Clause
   *)

lit-html/directives/guard.js:
  (**
   * @license
   * Copyright 2018 Google LLC
   * SPDX-License-Identifier: BSD-3-Clause
   *)

lit-html/directive-helpers.js:
  (**
   * @license
   * Copyright 2020 Google LLC
   * SPDX-License-Identifier: BSD-3-Clause
   *)

lit-html/directives/live.js:
  (**
   * @license
   * Copyright 2020 Google LLC
   * SPDX-License-Identifier: BSD-3-Clause
   *)

lit-html/async-directive.js:
  (**
   * @license
   * Copyright 2017 Google LLC
   * SPDX-License-Identifier: BSD-3-Clause
   *)

lit-html/directives/ref.js:
  (**
   * @license
   * Copyright 2020 Google LLC
   * SPDX-License-Identifier: BSD-3-Clause
   *)

lit-html/directives/private-async-helpers.js:
  (**
   * @license
   * Copyright 2021 Google LLC
   * SPDX-License-Identifier: BSD-3-Clause
   *)

lit-html/directives/until.js:
  (**
   * @license
   * Copyright 2017 Google LLC
   * SPDX-License-Identifier: BSD-3-Clause
   *)

lit-html/directives/unsafe-html.js:
  (**
   * @license
   * Copyright 2017 Google LLC
   * SPDX-License-Identifier: BSD-3-Clause
   *)
*/
