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
function P(t4, i6) {
  if (!a(t4) || !t4.hasOwnProperty("raw"))
    throw Error("invalid template strings array");
  return void 0 !== s ? s.createHTML(i6) : i6;
}
function S(t4, i6, s4 = t4, e7) {
  if (i6 === w)
    return i6;
  let h5 = void 0 !== e7 ? s4._$Co?.[e7] : s4._$Cl;
  const o5 = c(i6) ? void 0 : i6._$litDirective$;
  return h5?.constructor !== o5 && (h5?._$AO?.(false), void 0 === o5 ? h5 = void 0 : (h5 = new o5(t4), h5._$AT(t4, s4, e7)), void 0 !== e7 ? (s4._$Co ??= [])[e7] = h5 : s4._$Cl = h5), void 0 !== h5 && (i6 = S(t4, h5._$AS(t4, i6.values), h5, e7)), i6;
}
var t, i, s, e, h, o, n, r, l, c, a, u, d, f, v, _, m, p, g, $, y, x, T, b, w, E, A, C, V, N, M, k, R, H, I, L, z, W, Z, j;
var init_lit_html = __esm({
  "node_modules/lit-html/lit-html.js"() {
    t = globalThis;
    i = t.trustedTypes;
    s = i?.createPolicy?.("lit-html", { createHTML: (t4) => t4 }) ?? void 0;
    e = "$lit$";
    h = `lit$${Math.random().toFixed(9).slice(2)}$`;
    o = "?" + h;
    n = `<${o}>`;
    r = void 0 === t.document ? { createTreeWalker: () => ({}) } : document;
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
    T = y(2);
    b = y(3);
    w = Symbol.for("lit-noChange");
    E = Symbol.for("lit-nothing");
    A = /* @__PURE__ */ new WeakMap();
    C = r.createTreeWalker(r, 129);
    V = (t4, i6) => {
      const s4 = t4.length - 1, o5 = [];
      let r3, l3 = 2 === i6 ? "<svg>" : 3 === i6 ? "<math>" : "", c4 = f;
      for (let i7 = 0; i7 < s4; i7++) {
        const s5 = t4[i7];
        let a2, u3, d2 = -1, y2 = 0;
        for (; y2 < s5.length && (c4.lastIndex = y2, u3 = c4.exec(s5), null !== u3); )
          y2 = c4.lastIndex, c4 === f ? "!--" === u3[1] ? c4 = v : void 0 !== u3[1] ? c4 = _ : void 0 !== u3[2] ? ($.test(u3[2]) && (r3 = RegExp("</" + u3[2], "g")), c4 = m) : void 0 !== u3[3] && (c4 = m) : c4 === m ? ">" === u3[0] ? (c4 = r3 ?? f, d2 = -1) : void 0 === u3[1] ? d2 = -2 : (d2 = c4.lastIndex - u3[2].length, a2 = u3[1], c4 = void 0 === u3[3] ? m : '"' === u3[3] ? g : p) : c4 === g || c4 === p ? c4 = m : c4 === v || c4 === _ ? c4 = f : (c4 = m, r3 = void 0);
        const x2 = c4 === m && t4[i7 + 1].startsWith("/>") ? " " : "";
        l3 += c4 === f ? s5 + n : d2 >= 0 ? (o5.push(a2), s5.slice(0, d2) + e + s5.slice(d2) + h + x2) : s5 + h + (-2 === d2 ? i7 : x2);
      }
      return [P(t4, l3 + (t4[s4] || "<?>") + (2 === i6 ? "</svg>" : 3 === i6 ? "</math>" : "")), o5];
    };
    N = class {
      constructor({ strings: t4, _$litType$: s4 }, n5) {
        let r3;
        this.parts = [];
        let c4 = 0, a2 = 0;
        const u3 = t4.length - 1, d2 = this.parts, [f4, v2] = V(t4, s4);
        if (this.el = N.createElement(f4, n5), C.currentNode = this.el.content, 2 === s4 || 3 === s4) {
          const t5 = this.el.content.firstChild;
          t5.replaceWith(...t5.childNodes);
        }
        for (; null !== (r3 = C.nextNode()) && d2.length < u3; ) {
          if (1 === r3.nodeType) {
            if (r3.hasAttributes())
              for (const t5 of r3.getAttributeNames())
                if (t5.endsWith(e)) {
                  const i6 = v2[a2++], s5 = r3.getAttribute(t5).split(h), e7 = /([.?@])?(.*)/.exec(i6);
                  d2.push({ type: 1, index: c4, name: e7[2], strings: s5, ctor: "." === e7[1] ? H : "?" === e7[1] ? I : "@" === e7[1] ? L : R }), r3.removeAttribute(t5);
                } else
                  t5.startsWith(h) && (d2.push({ type: 6, index: c4 }), r3.removeAttribute(t5));
            if ($.test(r3.tagName)) {
              const t5 = r3.textContent.split(h), s5 = t5.length - 1;
              if (s5 > 0) {
                r3.textContent = i ? i.emptyScript : "";
                for (let i6 = 0; i6 < s5; i6++)
                  r3.append(t5[i6], l()), C.nextNode(), d2.push({ type: 2, index: ++c4 });
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
    M = class {
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
        const { el: { content: i6 }, parts: s4 } = this._$AD, e7 = (t4?.creationScope ?? r).importNode(i6, true);
        C.currentNode = e7;
        let h5 = C.nextNode(), o5 = 0, n5 = 0, l3 = s4[0];
        for (; void 0 !== l3; ) {
          if (o5 === l3.index) {
            let i7;
            2 === l3.type ? i7 = new k(h5, h5.nextSibling, this, t4) : 1 === l3.type ? i7 = new l3.ctor(h5, l3.name, l3.strings, this, t4) : 6 === l3.type && (i7 = new z(h5, this, t4)), this._$AV.push(i7), l3 = s4[++n5];
          }
          o5 !== l3?.index && (h5 = C.nextNode(), o5++);
        }
        return C.currentNode = r, e7;
      }
      p(t4) {
        let i6 = 0;
        for (const s4 of this._$AV)
          void 0 !== s4 && (void 0 !== s4.strings ? (s4._$AI(t4, s4, i6), i6 += s4.strings.length - 2) : s4._$AI(t4[i6])), i6++;
      }
    };
    k = class {
      get _$AU() {
        return this._$AM?._$AU ?? this._$Cv;
      }
      constructor(t4, i6, s4, e7) {
        this.type = 2, this._$AH = E, this._$AN = void 0, this._$AA = t4, this._$AB = i6, this._$AM = s4, this.options = e7, this._$Cv = e7?.isConnected ?? true;
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
        t4 = S(this, t4, i6), c(t4) ? t4 === E || null == t4 || "" === t4 ? (this._$AH !== E && this._$AR(), this._$AH = E) : t4 !== this._$AH && t4 !== w && this._(t4) : void 0 !== t4._$litType$ ? this.$(t4) : void 0 !== t4.nodeType ? this.T(t4) : u(t4) ? this.k(t4) : this._(t4);
      }
      O(t4) {
        return this._$AA.parentNode.insertBefore(t4, this._$AB);
      }
      T(t4) {
        this._$AH !== t4 && (this._$AR(), this._$AH = this.O(t4));
      }
      _(t4) {
        this._$AH !== E && c(this._$AH) ? this._$AA.nextSibling.data = t4 : this.T(r.createTextNode(t4)), this._$AH = t4;
      }
      $(t4) {
        const { values: i6, _$litType$: s4 } = t4, e7 = "number" == typeof s4 ? this._$AC(t4) : (void 0 === s4.el && (s4.el = N.createElement(P(s4.h, s4.h[0]), this.options)), s4);
        if (this._$AH?._$AD === e7)
          this._$AH.p(i6);
        else {
          const t5 = new M(e7, this), s5 = t5.u(this.options);
          t5.p(i6), this.T(s5), this._$AH = t5;
        }
      }
      _$AC(t4) {
        let i6 = A.get(t4.strings);
        return void 0 === i6 && A.set(t4.strings, i6 = new N(t4)), i6;
      }
      k(t4) {
        a(this._$AH) || (this._$AH = [], this._$AR());
        const i6 = this._$AH;
        let s4, e7 = 0;
        for (const h5 of t4)
          e7 === i6.length ? i6.push(s4 = new k(this.O(l()), this.O(l()), this, this.options)) : s4 = i6[e7], s4._$AI(h5), e7++;
        e7 < i6.length && (this._$AR(s4 && s4._$AB.nextSibling, e7), i6.length = e7);
      }
      _$AR(t4 = this._$AA.nextSibling, i6) {
        for (this._$AP?.(false, true, i6); t4 !== this._$AB; ) {
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
      constructor(t4, i6, s4, e7, h5) {
        this.type = 1, this._$AH = E, this._$AN = void 0, this.element = t4, this.name = i6, this._$AM = e7, this.options = h5, s4.length > 2 || "" !== s4[0] || "" !== s4[1] ? (this._$AH = Array(s4.length - 1).fill(new String()), this.strings = s4) : this._$AH = E;
      }
      _$AI(t4, i6 = this, s4, e7) {
        const h5 = this.strings;
        let o5 = false;
        if (void 0 === h5)
          t4 = S(this, t4, i6, 0), o5 = !c(t4) || t4 !== this._$AH && t4 !== w, o5 && (this._$AH = t4);
        else {
          const e8 = t4;
          let n5, r3;
          for (t4 = h5[0], n5 = 0; n5 < h5.length - 1; n5++)
            r3 = S(this, e8[s4 + n5], i6, n5), r3 === w && (r3 = this._$AH[n5]), o5 ||= !c(r3) || r3 !== this._$AH[n5], r3 === E ? t4 = E : t4 !== E && (t4 += (r3 ?? "") + h5[n5 + 1]), this._$AH[n5] = r3;
        }
        o5 && !e7 && this.j(t4);
      }
      j(t4) {
        t4 === E ? this.element.removeAttribute(this.name) : this.element.setAttribute(this.name, t4 ?? "");
      }
    };
    H = class extends R {
      constructor() {
        super(...arguments), this.type = 3;
      }
      j(t4) {
        this.element[this.name] = t4 === E ? void 0 : t4;
      }
    };
    I = class extends R {
      constructor() {
        super(...arguments), this.type = 4;
      }
      j(t4) {
        this.element.toggleAttribute(this.name, !!t4 && t4 !== E);
      }
    };
    L = class extends R {
      constructor(t4, i6, s4, e7, h5) {
        super(t4, i6, s4, e7, h5), this.type = 5;
      }
      _$AI(t4, i6 = this) {
        if ((t4 = S(this, t4, i6, 0) ?? E) === w)
          return;
        const s4 = this._$AH, e7 = t4 === E && s4 !== E || t4.capture !== s4.capture || t4.once !== s4.once || t4.passive !== s4.passive, h5 = t4 !== E && (s4 === E || e7);
        e7 && this.element.removeEventListener(this.name, this, s4), h5 && this.element.addEventListener(this.name, this, t4), this._$AH = t4;
      }
      handleEvent(t4) {
        "function" == typeof this._$AH ? this._$AH.call(this.options?.host ?? this.element, t4) : this._$AH.handleEvent(t4);
      }
    };
    z = class {
      constructor(t4, i6, s4) {
        this.element = t4, this.type = 6, this._$AN = void 0, this._$AM = i6, this.options = s4;
      }
      get _$AU() {
        return this._$AM._$AU;
      }
      _$AI(t4) {
        S(this, t4);
      }
    };
    W = { M: e, P: h, A: o, C: 1, L: V, R: M, D: u, V: S, I: k, H: R, N: I, U: L, B: H, F: z };
    Z = t.litHtmlPolyfillSupport;
    Z?.(N, k), (t.litHtmlVersions ??= []).push("3.3.1");
    j = (t4, i6, s4) => {
      const e7 = s4?.renderBefore ?? i6;
      let h5 = e7._$litPart$;
      if (void 0 === h5) {
        const t5 = s4?.renderBefore ?? null;
        e7._$litPart$ = h5 = new k(i6.insertBefore(l(), t5), t5, void 0, s4 ?? {});
      }
      return h5._$AI(t4), h5;
    };
  }
});

// node_modules/lit/html.js
var html_exports = {};
__export(html_exports, {
  _$LH: () => W,
  html: () => x,
  mathml: () => b,
  noChange: () => w,
  nothing: () => E,
  render: () => j,
  svg: () => T
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
    e2 = (t4) => (...e7) => ({ _$litDirective$: t4, values: e7 });
    i2 = class {
      constructor(t4) {
      }
      get _$AU() {
        return this._$AM._$AU;
      }
      _$AT(t4, e7, i6) {
        this._$Ct = t4, this._$AM = e7, this._$Ci = i6;
      }
      _$AS(t4, e7) {
        return this.update(t4, e7);
      }
      update(t4, e7) {
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

// node_modules/lit-html/directives/class-map.js
var e3;
var init_class_map = __esm({
  "node_modules/lit-html/directives/class-map.js"() {
    init_lit_html();
    init_directive();
    e3 = e2(class extends i2 {
      constructor(t4) {
        if (super(t4), t4.type !== t2.ATTRIBUTE || "class" !== t4.name || t4.strings?.length > 2)
          throw Error("`classMap()` can only be used in the `class` attribute and must be the only part in the attribute.");
      }
      render(t4) {
        return " " + Object.keys(t4).filter((s4) => t4[s4]).join(" ") + " ";
      }
      update(s4, [i6]) {
        if (void 0 === this.st) {
          this.st = /* @__PURE__ */ new Set(), void 0 !== s4.strings && (this.nt = new Set(s4.strings.join(" ").split(/\s/).filter((t4) => "" !== t4)));
          for (const t4 in i6)
            i6[t4] && !this.nt?.has(t4) && this.st.add(t4);
          return this.render(i6);
        }
        const r3 = s4.element.classList;
        for (const t4 of this.st)
          t4 in i6 || (r3.remove(t4), this.st.delete(t4));
        for (const t4 in i6) {
          const s5 = !!i6[t4];
          s5 === this.st.has(t4) || this.nt?.has(t4) || (s5 ? (r3.add(t4), this.st.add(t4)) : (r3.remove(t4), this.st.delete(t4)));
        }
        return w;
      }
    });
  }
});

// node_modules/lit/directives/class-map.js
var class_map_exports = {};
__export(class_map_exports, {
  classMap: () => e3
});
var init_class_map2 = __esm({
  "node_modules/lit/directives/class-map.js"() {
    init_class_map();
  }
});

// node_modules/lit-html/directives/guard.js
var e4, i3;
var init_guard = __esm({
  "node_modules/lit-html/directives/guard.js"() {
    init_lit_html();
    init_directive();
    e4 = {};
    i3 = e2(class extends i2 {
      constructor() {
        super(...arguments), this.ot = e4;
      }
      render(r3, t4) {
        return t4();
      }
      update(t4, [s4, e7]) {
        if (Array.isArray(s4)) {
          if (Array.isArray(this.ot) && this.ot.length === s4.length && s4.every((r3, t5) => r3 === this.ot[t5]))
            return w;
        } else if (this.ot === s4)
          return w;
        return this.ot = Array.isArray(s4) ? Array.from(s4) : s4, this.render(s4, e7);
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
    ({ I: t3 } = W);
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
        if (t4 === w || t4 === E)
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
function n2(i6, t4 = false, e7 = 0) {
  const r3 = this._$AH, h5 = this._$AN;
  if (void 0 !== h5 && 0 !== h5.size)
    if (t4)
      if (Array.isArray(r3))
        for (let i7 = e7; i7 < r3.length; i7++)
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
      const e7 = i6._$AN;
      if (void 0 === e7)
        return false;
      for (const i7 of e7)
        i7._$AO?.(t4, false), s2(i7, t4);
      return true;
    };
    o2 = (i6) => {
      let t4, e7;
      do {
        if (void 0 === (t4 = i6._$AM))
          break;
        e7 = t4._$AN, e7.delete(i6), i6 = t4;
      } while (0 === e7?.size);
    };
    r2 = (i6) => {
      for (let t4; t4 = i6._$AM; i6 = t4) {
        let e7 = t4._$AN;
        if (void 0 === e7)
          t4._$AN = e7 = /* @__PURE__ */ new Set();
        else if (e7.has(i6))
          break;
        e7.add(i6), c2(t4);
      }
    };
    c2 = (i6) => {
      i6.type == t2.CHILD && (i6._$AP ??= n2, i6._$AQ ??= h2);
    };
    f3 = class extends i2 {
      constructor() {
        super(...arguments), this._$AN = void 0;
      }
      _$AT(i6, t4, e7) {
        super._$AT(i6, t4, e7), r2(this), this.isConnected = i6._$AU;
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
var e5, h3, o3, n3;
var init_ref = __esm({
  "node_modules/lit-html/directives/ref.js"() {
    init_lit_html();
    init_async_directive();
    init_directive();
    e5 = () => new h3();
    h3 = class {
    };
    o3 = /* @__PURE__ */ new WeakMap();
    n3 = e2(class extends f3 {
      render(i6) {
        return E;
      }
      update(i6, [s4]) {
        const e7 = s4 !== this.G;
        return e7 && void 0 !== this.G && this.rt(void 0), (e7 || this.lt !== this.ct) && (this.G = s4, this.ht = i6.options?.host, this.rt(this.ct = i6.element)), E;
      }
      rt(t4) {
        if (this.isConnected || (t4 = void 0), "function" == typeof this.G) {
          const i6 = this.ht ?? globalThis;
          let s4 = o3.get(i6);
          void 0 === s4 && (s4 = /* @__PURE__ */ new WeakMap(), o3.set(i6, s4)), void 0 !== s4.get(this.G) && this.G.call(this.ht, void 0), s4.set(this.G, t4), void 0 !== t4 && this.G.call(this.ht, t4);
        } else
          this.G.value = t4;
      }
      get lt() {
        return "function" == typeof this.G ? o3.get(this.ht ?? globalThis)?.get(this.G) : this.G?.value;
      }
      disconnected() {
        this.lt === this.ct && this.rt(void 0);
      }
      reconnected() {
        this.rt(this.ct);
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
        super(...arguments), this._$Cwt = h4, this._$Cbt = [], this._$CK = new s3(this), this._$CX = new i5();
      }
      render(...s4) {
        return s4.find((t4) => !n4(t4)) ?? w;
      }
      update(s4, i6) {
        const e7 = this._$Cbt;
        let r3 = e7.length;
        this._$Cbt = i6;
        const o5 = this._$CK, c4 = this._$CX;
        this.isConnected || this.disconnected();
        for (let t4 = 0; t4 < i6.length && !(t4 > this._$Cwt); t4++) {
          const s5 = i6[t4];
          if (!n4(s5))
            return this._$Cwt = t4, s5;
          t4 < r3 && s5 === e7[t4] || (this._$Cwt = h4, r3 = 0, Promise.resolve(s5).then(async (t5) => {
            for (; c4.get(); )
              await c4.get();
            const i7 = o5.deref();
            if (void 0 !== i7) {
              const e8 = i7._$Cbt.indexOf(s5);
              e8 > -1 && e8 < i7._$Cwt && (i7._$Cwt = e8, i7.setValue(t5));
            }
          }));
        }
        return w;
      }
      disconnected() {
        this._$CK.disconnect(), this._$CX.pause();
      }
      reconnected() {
        this._$CK.reconnect(this), this._$CX.resume();
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
var e6, o4;
var init_unsafe_html = __esm({
  "node_modules/lit-html/directives/unsafe-html.js"() {
    init_lit_html();
    init_directive();
    e6 = class extends i2 {
      constructor(i6) {
        if (super(i6), this.it = E, i6.type !== t2.CHILD)
          throw Error(this.constructor.directiveName + "() can only be used in child bindings");
      }
      render(r3) {
        if (r3 === E || null == r3)
          return this._t = void 0, this.it = r3;
        if (r3 === w)
          return r3;
        if ("string" != typeof r3)
          throw Error(this.constructor.directiveName + "() called with a non-string value");
        if (r3 === this.it)
          return this._t;
        this.it = r3;
        const s4 = [r3];
        return s4.raw = s4, this._t = { _$litType$: this.constructor.resultType, strings: s4, values: [] };
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
var { classMap } = (init_class_map2(), __toCommonJS(class_map_exports));
var { guard } = (init_guard2(), __toCommonJS(guard_exports));
var { live } = (init_live2(), __toCommonJS(live_exports));
var { ref } = (init_ref2(), __toCommonJS(ref_exports));
var { until } = (init_until2(), __toCommonJS(until_exports));
var { unsafeHTML } = (init_unsafe_html2(), __toCommonJS(unsafe_html_exports));
var html = (strings, ...values) => {
  if (typeof document == "undefined")
    return;
  transformValues(values);
  return {
    ["_$litType$"]: 1,
    strings,
    values
  };
};
var svg = (strings, ...values) => {
  if (typeof document == "undefined")
    return;
  transformValues(values);
  return {
    ["_$litType$"]: 2,
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
  classMap,
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
/**
 * @license
 * Copyright 2017 Google LLC
 * SPDX-License-Identifier: BSD-3-Clause
 */
/**
 * @license
 * Copyright 2018 Google LLC
 * SPDX-License-Identifier: BSD-3-Clause
 */
/**
 * @license
 * Copyright 2020 Google LLC
 * SPDX-License-Identifier: BSD-3-Clause
 */
/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: BSD-3-Clause
 */
