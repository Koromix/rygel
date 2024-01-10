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
function C(t4, i6) {
  if (!Array.isArray(t4) || !t4.hasOwnProperty("raw"))
    throw Error("invalid template strings array");
  return void 0 !== s ? s.createHTML(i6) : i6;
}
function N(t4, i6, s4 = t4, e6) {
  if (i6 === w)
    return i6;
  let h5 = void 0 !== e6 ? s4._$Co?.[e6] : s4._$Cl;
  const o5 = c(i6) ? void 0 : i6._$litDirective$;
  return h5?.constructor !== o5 && (h5?._$AO?.(false), void 0 === o5 ? h5 = void 0 : (h5 = new o5(t4), h5._$AT(t4, s4, e6)), void 0 !== e6 ? (s4._$Co ??= [])[e6] = h5 : s4._$Cl = h5), void 0 !== h5 && (i6 = N(t4, h5._$AS(t4, i6.values), h5, e6)), i6;
}
var t, i, s, e, h, o, n, r, l, c, a, u, d, f, v, _, m, p, g, $, y, x, b, w, T, A, E, P, V, S, M, R, k, H, I, L, z, Z, j;
var init_lit_html = __esm({
  "node_modules/lit-html/lit-html.js"() {
    t = globalThis;
    i = t.trustedTypes;
    s = i ? i.createPolicy("lit-html", { createHTML: (t4) => t4 }) : void 0;
    e = "$lit$";
    h = `lit$${(Math.random() + "").slice(9)}$`;
    o = "?" + h;
    n = `<${o}>`;
    r = document;
    l = () => r.createComment("");
    c = (t4) => null === t4 || "object" != typeof t4 && "function" != typeof t4;
    a = Array.isArray;
    u = (t4) => a(t4) || "function" == typeof t4?.[Symbol.iterator];
    d = "[ 	\n\f\r]";
    f = /<(?:(!--|\/[^a-zA-Z])|(\/?[a-zA-Z][^>\s]*)|(\/?$))/g;
    v = /-->/g;
    _ = />/g;
    m = RegExp(`>|${d}(?:([^\\s"'>=/]+)(${d}*=${d}*(?:[^ 	
\f\r"'\`<>=]|("|')|))|$)`, "g");
    p = /'/g;
    g = /"/g;
    $ = /^(?:script|style|textarea|title)$/i;
    y = (t4) => (i6, ...s4) => ({ _$litType$: t4, strings: i6, values: s4 });
    x = y(1);
    b = y(2);
    w = Symbol.for("lit-noChange");
    T = Symbol.for("lit-nothing");
    A = /* @__PURE__ */ new WeakMap();
    E = r.createTreeWalker(r, 129);
    P = (t4, i6) => {
      const s4 = t4.length - 1, o5 = [];
      let r3, l3 = 2 === i6 ? "<svg>" : "", c4 = f;
      for (let i7 = 0; i7 < s4; i7++) {
        const s5 = t4[i7];
        let a2, u3, d2 = -1, y2 = 0;
        for (; y2 < s5.length && (c4.lastIndex = y2, u3 = c4.exec(s5), null !== u3); )
          y2 = c4.lastIndex, c4 === f ? "!--" === u3[1] ? c4 = v : void 0 !== u3[1] ? c4 = _ : void 0 !== u3[2] ? ($.test(u3[2]) && (r3 = RegExp("</" + u3[2], "g")), c4 = m) : void 0 !== u3[3] && (c4 = m) : c4 === m ? ">" === u3[0] ? (c4 = r3 ?? f, d2 = -1) : void 0 === u3[1] ? d2 = -2 : (d2 = c4.lastIndex - u3[2].length, a2 = u3[1], c4 = void 0 === u3[3] ? m : '"' === u3[3] ? g : p) : c4 === g || c4 === p ? c4 = m : c4 === v || c4 === _ ? c4 = f : (c4 = m, r3 = void 0);
        const x2 = c4 === m && t4[i7 + 1].startsWith("/>") ? " " : "";
        l3 += c4 === f ? s5 + n : d2 >= 0 ? (o5.push(a2), s5.slice(0, d2) + e + s5.slice(d2) + h + x2) : s5 + h + (-2 === d2 ? i7 : x2);
      }
      return [C(t4, l3 + (t4[s4] || "<?>") + (2 === i6 ? "</svg>" : "")), o5];
    };
    V = class _V {
      constructor({ strings: t4, _$litType$: s4 }, n5) {
        let r3;
        this.parts = [];
        let c4 = 0, a2 = 0;
        const u3 = t4.length - 1, d2 = this.parts, [f4, v2] = P(t4, s4);
        if (this.el = _V.createElement(f4, n5), E.currentNode = this.el.content, 2 === s4) {
          const t5 = this.el.content.firstChild;
          t5.replaceWith(...t5.childNodes);
        }
        for (; null !== (r3 = E.nextNode()) && d2.length < u3; ) {
          if (1 === r3.nodeType) {
            if (r3.hasAttributes())
              for (const t5 of r3.getAttributeNames())
                if (t5.endsWith(e)) {
                  const i6 = v2[a2++], s5 = r3.getAttribute(t5).split(h), e6 = /([.?@])?(.*)/.exec(i6);
                  d2.push({ type: 1, index: c4, name: e6[2], strings: s5, ctor: "." === e6[1] ? k : "?" === e6[1] ? H : "@" === e6[1] ? I : R }), r3.removeAttribute(t5);
                } else
                  t5.startsWith(h) && (d2.push({ type: 6, index: c4 }), r3.removeAttribute(t5));
            if ($.test(r3.tagName)) {
              const t5 = r3.textContent.split(h), s5 = t5.length - 1;
              if (s5 > 0) {
                r3.textContent = i ? i.emptyScript : "";
                for (let i6 = 0; i6 < s5; i6++)
                  r3.append(t5[i6], l()), E.nextNode(), d2.push({ type: 2, index: ++c4 });
                r3.append(t5[s5], l());
              }
            }
          } else if (8 === r3.nodeType)
            if (r3.data === o)
              d2.push({ type: 2, index: c4 });
            else {
              let t5 = -1;
              for (; -1 !== (t5 = r3.data.indexOf(h, t5 + 1)); )
                d2.push({ type: 7, index: c4 }), t5 += h.length - 1;
            }
          c4++;
        }
      }
      static createElement(t4, i6) {
        const s4 = r.createElement("template");
        return s4.innerHTML = t4, s4;
      }
    };
    S = class {
      constructor(t4, i6) {
        this._$AV = [], this._$AN = void 0, this._$AD = t4, this._$AM = i6;
      }
      get parentNode() {
        return this._$AM.parentNode;
      }
      get _$AU() {
        return this._$AM._$AU;
      }
      u(t4) {
        const { el: { content: i6 }, parts: s4 } = this._$AD, e6 = (t4?.creationScope ?? r).importNode(i6, true);
        E.currentNode = e6;
        let h5 = E.nextNode(), o5 = 0, n5 = 0, l3 = s4[0];
        for (; void 0 !== l3; ) {
          if (o5 === l3.index) {
            let i7;
            2 === l3.type ? i7 = new M(h5, h5.nextSibling, this, t4) : 1 === l3.type ? i7 = new l3.ctor(h5, l3.name, l3.strings, this, t4) : 6 === l3.type && (i7 = new L(h5, this, t4)), this._$AV.push(i7), l3 = s4[++n5];
          }
          o5 !== l3?.index && (h5 = E.nextNode(), o5++);
        }
        return E.currentNode = r, e6;
      }
      p(t4) {
        let i6 = 0;
        for (const s4 of this._$AV)
          void 0 !== s4 && (void 0 !== s4.strings ? (s4._$AI(t4, s4, i6), i6 += s4.strings.length - 2) : s4._$AI(t4[i6])), i6++;
      }
    };
    M = class _M {
      get _$AU() {
        return this._$AM?._$AU ?? this._$Cv;
      }
      constructor(t4, i6, s4, e6) {
        this.type = 2, this._$AH = T, this._$AN = void 0, this._$AA = t4, this._$AB = i6, this._$AM = s4, this.options = e6, this._$Cv = e6?.isConnected ?? true;
      }
      get parentNode() {
        let t4 = this._$AA.parentNode;
        const i6 = this._$AM;
        return void 0 !== i6 && 11 === t4?.nodeType && (t4 = i6.parentNode), t4;
      }
      get startNode() {
        return this._$AA;
      }
      get endNode() {
        return this._$AB;
      }
      _$AI(t4, i6 = this) {
        t4 = N(this, t4, i6), c(t4) ? t4 === T || null == t4 || "" === t4 ? (this._$AH !== T && this._$AR(), this._$AH = T) : t4 !== this._$AH && t4 !== w && this._(t4) : void 0 !== t4._$litType$ ? this.g(t4) : void 0 !== t4.nodeType ? this.$(t4) : u(t4) ? this.T(t4) : this._(t4);
      }
      k(t4) {
        return this._$AA.parentNode.insertBefore(t4, this._$AB);
      }
      $(t4) {
        this._$AH !== t4 && (this._$AR(), this._$AH = this.k(t4));
      }
      _(t4) {
        this._$AH !== T && c(this._$AH) ? this._$AA.nextSibling.data = t4 : this.$(r.createTextNode(t4)), this._$AH = t4;
      }
      g(t4) {
        const { values: i6, _$litType$: s4 } = t4, e6 = "number" == typeof s4 ? this._$AC(t4) : (void 0 === s4.el && (s4.el = V.createElement(C(s4.h, s4.h[0]), this.options)), s4);
        if (this._$AH?._$AD === e6)
          this._$AH.p(i6);
        else {
          const t5 = new S(e6, this), s5 = t5.u(this.options);
          t5.p(i6), this.$(s5), this._$AH = t5;
        }
      }
      _$AC(t4) {
        let i6 = A.get(t4.strings);
        return void 0 === i6 && A.set(t4.strings, i6 = new V(t4)), i6;
      }
      T(t4) {
        a(this._$AH) || (this._$AH = [], this._$AR());
        const i6 = this._$AH;
        let s4, e6 = 0;
        for (const h5 of t4)
          e6 === i6.length ? i6.push(s4 = new _M(this.k(l()), this.k(l()), this, this.options)) : s4 = i6[e6], s4._$AI(h5), e6++;
        e6 < i6.length && (this._$AR(s4 && s4._$AB.nextSibling, e6), i6.length = e6);
      }
      _$AR(t4 = this._$AA.nextSibling, i6) {
        for (this._$AP?.(false, true, i6); t4 && t4 !== this._$AB; ) {
          const i7 = t4.nextSibling;
          t4.remove(), t4 = i7;
        }
      }
      setConnected(t4) {
        void 0 === this._$AM && (this._$Cv = t4, this._$AP?.(t4));
      }
    };
    R = class {
      get tagName() {
        return this.element.tagName;
      }
      get _$AU() {
        return this._$AM._$AU;
      }
      constructor(t4, i6, s4, e6, h5) {
        this.type = 1, this._$AH = T, this._$AN = void 0, this.element = t4, this.name = i6, this._$AM = e6, this.options = h5, s4.length > 2 || "" !== s4[0] || "" !== s4[1] ? (this._$AH = Array(s4.length - 1).fill(new String()), this.strings = s4) : this._$AH = T;
      }
      _$AI(t4, i6 = this, s4, e6) {
        const h5 = this.strings;
        let o5 = false;
        if (void 0 === h5)
          t4 = N(this, t4, i6, 0), o5 = !c(t4) || t4 !== this._$AH && t4 !== w, o5 && (this._$AH = t4);
        else {
          const e7 = t4;
          let n5, r3;
          for (t4 = h5[0], n5 = 0; n5 < h5.length - 1; n5++)
            r3 = N(this, e7[s4 + n5], i6, n5), r3 === w && (r3 = this._$AH[n5]), o5 ||= !c(r3) || r3 !== this._$AH[n5], r3 === T ? t4 = T : t4 !== T && (t4 += (r3 ?? "") + h5[n5 + 1]), this._$AH[n5] = r3;
        }
        o5 && !e6 && this.O(t4);
      }
      O(t4) {
        t4 === T ? this.element.removeAttribute(this.name) : this.element.setAttribute(this.name, t4 ?? "");
      }
    };
    k = class extends R {
      constructor() {
        super(...arguments), this.type = 3;
      }
      O(t4) {
        this.element[this.name] = t4 === T ? void 0 : t4;
      }
    };
    H = class extends R {
      constructor() {
        super(...arguments), this.type = 4;
      }
      O(t4) {
        this.element.toggleAttribute(this.name, !!t4 && t4 !== T);
      }
    };
    I = class extends R {
      constructor(t4, i6, s4, e6, h5) {
        super(t4, i6, s4, e6, h5), this.type = 5;
      }
      _$AI(t4, i6 = this) {
        if ((t4 = N(this, t4, i6, 0) ?? T) === w)
          return;
        const s4 = this._$AH, e6 = t4 === T && s4 !== T || t4.capture !== s4.capture || t4.once !== s4.once || t4.passive !== s4.passive, h5 = t4 !== T && (s4 === T || e6);
        e6 && this.element.removeEventListener(this.name, this, s4), h5 && this.element.addEventListener(this.name, this, t4), this._$AH = t4;
      }
      handleEvent(t4) {
        "function" == typeof this._$AH ? this._$AH.call(this.options?.host ?? this.element, t4) : this._$AH.handleEvent(t4);
      }
    };
    L = class {
      constructor(t4, i6, s4) {
        this.element = t4, this.type = 6, this._$AN = void 0, this._$AM = i6, this.options = s4;
      }
      get _$AU() {
        return this._$AM._$AU;
      }
      _$AI(t4) {
        N(this, t4);
      }
    };
    z = { j: e, P: h, A: o, C: 1, M: P, L: S, R: u, V: N, D: M, I: R, H, N: I, U: k, B: L };
    Z = t.litHtmlPolyfillSupport;
    Z?.(V, M), (t.litHtmlVersions ??= []).push("3.1.1");
    j = (t4, i6, s4) => {
      const e6 = s4?.renderBefore ?? i6;
      let h5 = e6._$litPart$;
      if (void 0 === h5) {
        const t5 = s4?.renderBefore ?? null;
        e6._$litPart$ = h5 = new M(i6.insertBefore(l(), t5), t5, void 0, s4 ?? {});
      }
      return h5._$AI(t4), h5;
    };
  }
});

// node_modules/lit/html.js
var html_exports = {};
__export(html_exports, {
  _$LH: () => z,
  html: () => x,
  noChange: () => w,
  nothing: () => T,
  render: () => j,
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
    e2 = (t4) => (...e6) => ({ _$litDirective$: t4, values: e6 });
    i2 = class {
      constructor(t4) {
      }
      get _$AU() {
        return this._$AM._$AU;
      }
      _$AT(t4, e6, i6) {
        this._$Ct = t4, this._$AM = e6, this._$Ci = i6;
      }
      _$AS(t4, e6) {
        return this.update(t4, e6);
      }
      update(t4, e6) {
        return this.render(...e6);
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
        super(...arguments), this.nt = e3;
      }
      render(r3, t4) {
        return t4();
      }
      update(t4, [s4, e6]) {
        if (Array.isArray(s4)) {
          if (Array.isArray(this.nt) && this.nt.length === s4.length && s4.every((r3, t5) => r3 === this.nt[t5]))
            return w;
        } else if (this.nt === s4)
          return w;
        return this.nt = Array.isArray(s4) ? Array.from(s4) : s4, this.render(s4, e6);
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
var t3, i4, f2, u2, m2;
var init_directive_helpers = __esm({
  "node_modules/lit-html/directive-helpers.js"() {
    init_lit_html();
    ({ D: t3 } = z);
    i4 = (o5) => null === o5 || "object" != typeof o5 && "function" != typeof o5;
    f2 = (o5) => void 0 === o5.strings;
    u2 = {};
    m2 = (o5, t4 = u2) => o5._$AH = t4;
  }
});

// node_modules/lit-html/directives/live.js
var l2;
var init_live = __esm({
  "node_modules/lit-html/directives/live.js"() {
    init_lit_html();
    init_directive();
    init_directive_helpers();
    l2 = e2(class extends i2 {
      constructor(r3) {
        if (super(r3), r3.type !== t2.PROPERTY && r3.type !== t2.ATTRIBUTE && r3.type !== t2.BOOLEAN_ATTRIBUTE)
          throw Error("The `live` directive is not allowed on child or event bindings");
        if (!f2(r3))
          throw Error("`live` bindings can only contain a single expression");
      }
      render(r3) {
        return r3;
      }
      update(i6, [t4]) {
        if (t4 === w || t4 === T)
          return t4;
        const o5 = i6.element, l3 = i6.name;
        if (i6.type === t2.PROPERTY) {
          if (t4 === o5[l3])
            return w;
        } else if (i6.type === t2.BOOLEAN_ATTRIBUTE) {
          if (!!t4 === o5.hasAttribute(l3))
            return w;
        } else if (i6.type === t2.ATTRIBUTE && o5.getAttribute(l3) === t4 + "")
          return w;
        return m2(i6), t4;
      }
    });
  }
});

// node_modules/lit/directives/live.js
var live_exports = {};
__export(live_exports, {
  live: () => l2
});
var init_live2 = __esm({
  "node_modules/lit/directives/live.js"() {
    init_live();
  }
});

// node_modules/lit-html/async-directive.js
function h2(i6) {
  void 0 !== this._$AN ? (o2(this), this._$AM = i6, r2(this)) : this._$AM = i6;
}
function n2(i6, t4 = false, e6 = 0) {
  const r3 = this._$AH, h5 = this._$AN;
  if (void 0 !== h5 && 0 !== h5.size)
    if (t4)
      if (Array.isArray(r3))
        for (let i7 = e6; i7 < r3.length; i7++)
          s2(r3[i7], false), o2(r3[i7]);
      else
        null != r3 && (s2(r3, false), o2(r3));
    else
      s2(this, i6);
}
var s2, o2, r2, c2, f3;
var init_async_directive = __esm({
  "node_modules/lit-html/async-directive.js"() {
    init_directive_helpers();
    init_directive();
    init_directive();
    s2 = (i6, t4) => {
      const e6 = i6._$AN;
      if (void 0 === e6)
        return false;
      for (const i7 of e6)
        i7._$AO?.(t4, false), s2(i7, t4);
      return true;
    };
    o2 = (i6) => {
      let t4, e6;
      do {
        if (void 0 === (t4 = i6._$AM))
          break;
        e6 = t4._$AN, e6.delete(i6), i6 = t4;
      } while (0 === e6?.size);
    };
    r2 = (i6) => {
      for (let t4; t4 = i6._$AM; i6 = t4) {
        let e6 = t4._$AN;
        if (void 0 === e6)
          t4._$AN = e6 = /* @__PURE__ */ new Set();
        else if (e6.has(i6))
          break;
        e6.add(i6), c2(t4);
      }
    };
    c2 = (i6) => {
      i6.type == t2.CHILD && (i6._$AP ??= n2, i6._$AQ ??= h2);
    };
    f3 = class extends i2 {
      constructor() {
        super(...arguments), this._$AN = void 0;
      }
      _$AT(i6, t4, e6) {
        super._$AT(i6, t4, e6), r2(this), this.isConnected = i6._$AU;
      }
      _$AO(i6, t4 = true) {
        i6 !== this.isConnected && (this.isConnected = i6, i6 ? this.reconnected?.() : this.disconnected?.()), t4 && (s2(this, i6), o2(this));
      }
      setValue(t4) {
        if (f2(this._$Ct))
          this._$Ct._$AI(t4, this);
        else {
          const i6 = [...this._$Ct._$AH];
          i6[this._$Ci] = t4, this._$Ct._$AI(i6, this, 0);
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
var e4, h3, o3, n3;
var init_ref = __esm({
  "node_modules/lit-html/directives/ref.js"() {
    init_lit_html();
    init_async_directive();
    init_directive();
    e4 = () => new h3();
    h3 = class {
    };
    o3 = /* @__PURE__ */ new WeakMap();
    n3 = e2(class extends f3 {
      render(i6) {
        return T;
      }
      update(i6, [s4]) {
        const e6 = s4 !== this.G;
        return e6 && void 0 !== this.G && this.ot(void 0), (e6 || this.rt !== this.lt) && (this.G = s4, this.ct = i6.options?.host, this.ot(this.lt = i6.element)), T;
      }
      ot(t4) {
        if ("function" == typeof this.G) {
          const i6 = this.ct ?? globalThis;
          let s4 = o3.get(i6);
          void 0 === s4 && (s4 = /* @__PURE__ */ new WeakMap(), o3.set(i6, s4)), void 0 !== s4.get(this.G) && this.G.call(this.ct, void 0), s4.set(this.G, t4), void 0 !== t4 && this.G.call(this.ct, t4);
        } else
          this.G.value = t4;
      }
      get rt() {
        return "function" == typeof this.G ? o3.get(this.ct ?? globalThis)?.get(this.G) : this.G?.value;
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
  createRef: () => e4,
  ref: () => n3
});
var init_ref2 = __esm({
  "node_modules/lit/directives/ref.js"() {
    init_ref();
  }
});

// node_modules/lit-html/directives/private-async-helpers.js
var s3, i5;
var init_private_async_helpers = __esm({
  "node_modules/lit-html/directives/private-async-helpers.js"() {
    s3 = class {
      constructor(t4) {
        this.G = t4;
      }
      disconnect() {
        this.G = void 0;
      }
      reconnect(t4) {
        this.G = t4;
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
        this.Y ??= new Promise((t4) => this.Z = t4);
      }
      resume() {
        this.Z?.(), this.Y = this.Z = void 0;
      }
    };
  }
});

// node_modules/lit-html/directives/until.js
var n4, h4, c3, m3;
var init_until = __esm({
  "node_modules/lit-html/directives/until.js"() {
    init_lit_html();
    init_directive_helpers();
    init_async_directive();
    init_private_async_helpers();
    init_directive();
    n4 = (t4) => !i4(t4) && "function" == typeof t4.then;
    h4 = 1073741823;
    c3 = class extends f3 {
      constructor() {
        super(...arguments), this._$C_t = h4, this._$Cwt = [], this._$Cq = new s3(this), this._$CK = new i5();
      }
      render(...s4) {
        return s4.find((t4) => !n4(t4)) ?? w;
      }
      update(s4, i6) {
        const e6 = this._$Cwt;
        let r3 = e6.length;
        this._$Cwt = i6;
        const o5 = this._$Cq, c4 = this._$CK;
        this.isConnected || this.disconnected();
        for (let t4 = 0; t4 < i6.length && !(t4 > this._$C_t); t4++) {
          const s5 = i6[t4];
          if (!n4(s5))
            return this._$C_t = t4, s5;
          t4 < r3 && s5 === e6[t4] || (this._$C_t = h4, r3 = 0, Promise.resolve(s5).then(async (t5) => {
            for (; c4.get(); )
              await c4.get();
            const i7 = o5.deref();
            if (void 0 !== i7) {
              const e7 = i7._$Cwt.indexOf(s5);
              e7 > -1 && e7 < i7._$C_t && (i7._$C_t = e7, i7.setValue(t5));
            }
          }));
        }
        return w;
      }
      disconnected() {
        this._$Cq.disconnect(), this._$CK.pause();
      }
      reconnected() {
        this._$Cq.reconnect(this), this._$CK.resume();
      }
    };
    m3 = e2(c3);
  }
});

// node_modules/lit/directives/until.js
var until_exports = {};
__export(until_exports, {
  UntilDirective: () => c3,
  until: () => m3
});
var init_until2 = __esm({
  "node_modules/lit/directives/until.js"() {
    init_until();
  }
});

// node_modules/lit-html/directives/unsafe-html.js
var e5, o4;
var init_unsafe_html = __esm({
  "node_modules/lit-html/directives/unsafe-html.js"() {
    init_lit_html();
    init_directive();
    e5 = class extends i2 {
      constructor(i6) {
        if (super(i6), this.et = T, i6.type !== t2.CHILD)
          throw Error(this.constructor.directiveName + "() can only be used in child bindings");
      }
      render(r3) {
        if (r3 === T || null == r3)
          return this.vt = void 0, this.et = r3;
        if (r3 === w)
          return r3;
        if ("string" != typeof r3)
          throw Error(this.constructor.directiveName + "() called with a non-string value");
        if (r3 === this.et)
          return this.vt;
        this.et = r3;
        const s4 = [r3];
        return s4.raw = s4, this.vt = { _$litType$: this.constructor.resultType, strings: s4, values: [] };
      }
    };
    e5.directiveName = "unsafeHTML", e5.resultType = 1;
    o4 = e2(e5);
  }
});

// node_modules/lit/directives/unsafe-html.js
var unsafe_html_exports = {};
__export(unsafe_html_exports, {
  UnsafeHTMLDirective: () => e5,
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
