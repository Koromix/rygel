// node_modules/temporal-polyfill/chunks/internal.js
function clampProp(e2, n2, t2, o2, r2) {
  return clampEntity(n2, getDefinedProp(e2, n2), t2, o2, r2);
}
function clampEntity(e2, n2, t2, o2, r2, i2) {
  const a2 = clampNumber(n2, t2, o2);
  if (r2 && n2 !== a2) {
    throw new RangeError(numberOutOfRange(e2, n2, t2, o2, i2));
  }
  return a2;
}
function getDefinedProp(e2, n2) {
  const t2 = e2[n2];
  if (void 0 === t2) {
    throw new TypeError(missingField(n2));
  }
  return t2;
}
function z(e2) {
  return null !== e2 && /object|function/.test(typeof e2);
}
function Jn(e2, n2 = Map) {
  const t2 = new n2();
  return (n3, ...o2) => {
    if (t2.has(n3)) {
      return t2.get(n3);
    }
    const r2 = e2(n3, ...o2);
    return t2.set(n3, r2), r2;
  };
}
function D(e2) {
  return p({
    name: e2
  }, 1);
}
function p(e2, n2) {
  return T((e3) => ({
    value: e3,
    configurable: 1,
    writable: !n2
  }), e2);
}
function O(e2) {
  return T((e3) => ({
    get: e3,
    configurable: 1
  }), e2);
}
function h(e2) {
  return {
    [Symbol.toStringTag]: {
      value: e2,
      configurable: 1
    }
  };
}
function zipProps(e2, n2) {
  const t2 = {};
  let o2 = e2.length;
  for (const r2 of n2) {
    t2[e2[--o2]] = r2;
  }
  return t2;
}
function T(e2, n2, t2) {
  const o2 = {};
  for (const r2 in n2) {
    o2[r2] = e2(n2[r2], r2, t2);
  }
  return o2;
}
function b(e2, n2, t2) {
  const o2 = {};
  for (let r2 = 0; r2 < n2.length; r2++) {
    const i2 = n2[r2];
    o2[i2] = e2(i2, r2, t2);
  }
  return o2;
}
function remapProps(e2, n2, t2) {
  const o2 = {};
  for (let r2 = 0; r2 < e2.length; r2++) {
    o2[n2[r2]] = t2[e2[r2]];
  }
  return o2;
}
function Vn(e2, n2) {
  const t2 = {};
  for (const o2 of e2) {
    t2[o2] = n2[o2];
  }
  return t2;
}
function V(e2, n2) {
  const t2 = {};
  for (const o2 in n2) {
    e2.has(o2) || (t2[o2] = n2[o2]);
  }
  return t2;
}
function nn(e2) {
  e2 = {
    ...e2
  };
  const n2 = Object.keys(e2);
  for (const t2 of n2) {
    void 0 === e2[t2] && delete e2[t2];
  }
  return e2;
}
function C(e2, n2) {
  for (const t2 of n2) {
    if (!(t2 in e2)) {
      return 0;
    }
  }
  return 1;
}
function allPropsEqual(e2, n2, t2) {
  for (const o2 of e2) {
    if (n2[o2] !== t2[o2]) {
      return 0;
    }
  }
  return 1;
}
function zeroOutProps(e2, n2, t2) {
  const o2 = {
    ...t2
  };
  for (let t3 = 0; t3 < n2; t3++) {
    o2[e2[t3]] = 0;
  }
  return o2;
}
function E(e2, ...n2) {
  return (...t2) => e2(...n2, ...t2);
}
function capitalize(e2) {
  return e2[0].toUpperCase() + e2.substring(1);
}
function sortStrings(e2) {
  return e2.slice().sort();
}
function padNumber(e2, n2) {
  return String(n2).padStart(e2, "0");
}
function compareNumbers(e2, n2) {
  return Math.sign(e2 - n2);
}
function clampNumber(e2, n2, t2) {
  return Math.min(Math.max(e2, n2), t2);
}
function divModFloor(e2, n2) {
  return [Math.floor(e2 / n2), modFloor(e2, n2)];
}
function modFloor(e2, n2) {
  return (e2 % n2 + n2) % n2;
}
function divModTrunc(e2, n2) {
  return [divTrunc(e2, n2), modTrunc(e2, n2)];
}
function divTrunc(e2, n2) {
  return Math.trunc(e2 / n2) || 0;
}
function modTrunc(e2, n2) {
  return e2 % n2 || 0;
}
function hasHalf(e2) {
  return 0.5 === Math.abs(e2 % 1);
}
function givenFieldsToBigNano(e2, n2, t2) {
  let o2 = 0, r2 = 0;
  for (let i3 = 0; i3 <= n2; i3++) {
    const n3 = e2[t2[i3]], a3 = Xr[i3], s2 = Qr / a3, [c2, u2] = divModTrunc(n3, s2);
    o2 += u2 * a3, r2 += c2;
  }
  const [i2, a2] = divModTrunc(o2, Qr);
  return [r2 + i2, a2];
}
function nanoToGivenFields(e2, n2, t2) {
  const o2 = {};
  for (let r2 = n2; r2 >= 0; r2--) {
    const n3 = Xr[r2];
    o2[t2[r2]] = divTrunc(e2, n3), e2 = modTrunc(e2, n3);
  }
  return o2;
}
function un(e2) {
  return e2 === X ? si : [];
}
function cn(e2) {
  return e2 === X ? li : [];
}
function ln(e2) {
  return e2 === X ? ["year", "day"] : [];
}
function l(e2) {
  if (void 0 !== e2) {
    return m(e2);
  }
}
function S(e2) {
  if (void 0 !== e2) {
    return d(e2);
  }
}
function c(e2) {
  if (void 0 !== e2) {
    return u(e2);
  }
}
function d(e2) {
  return requireNumberIsPositive(u(e2));
}
function u(e2) {
  return requireNumberIsInteger(Mi(e2));
}
function on(e2) {
  if (null == e2) {
    throw new TypeError("Cannot be null or undefined");
  }
  return e2;
}
function requirePropDefined(e2, n2) {
  if (null == n2) {
    throw new RangeError(missingField(e2));
  }
  return n2;
}
function de(e2) {
  if (!z(e2)) {
    throw new TypeError(hr);
  }
  return e2;
}
function requireType(e2, n2, t2 = e2) {
  if (typeof n2 !== e2) {
    throw new TypeError(invalidEntity(t2, n2));
  }
  return n2;
}
function requireNumberIsInteger(e2, n2 = "number") {
  if (!Number.isInteger(e2)) {
    throw new RangeError(expectedInteger(n2, e2));
  }
  return e2 || 0;
}
function requireNumberIsPositive(e2, n2 = "number") {
  if (e2 <= 0) {
    throw new RangeError(expectedPositive(n2, e2));
  }
  return e2;
}
function toString(e2) {
  if ("symbol" == typeof e2) {
    throw new TypeError(pr);
  }
  return String(e2);
}
function toStringViaPrimitive(e2, n2) {
  return z(e2) ? String(e2) : m(e2, n2);
}
function toBigInt(e2) {
  if ("string" == typeof e2) {
    return BigInt(e2);
  }
  if ("bigint" != typeof e2) {
    throw new TypeError(invalidBigInt(e2));
  }
  return e2;
}
function toNumber(e2, n2 = "number") {
  if ("bigint" == typeof e2) {
    throw new TypeError(forbiddenBigIntToNumber(n2));
  }
  if (e2 = Number(e2), !Number.isFinite(e2)) {
    throw new RangeError(expectedFinite(n2, e2));
  }
  return e2;
}
function toInteger(e2, n2) {
  return Math.trunc(toNumber(e2, n2)) || 0;
}
function toStrictInteger(e2, n2) {
  return requireNumberIsInteger(toNumber(e2, n2), n2);
}
function toPositiveInteger(e2, n2) {
  return requireNumberIsPositive(toInteger(e2, n2), n2);
}
function createBigNano(e2, n2) {
  let [t2, o2] = divModTrunc(n2, Qr), r2 = e2 + t2;
  const i2 = Math.sign(r2);
  return i2 && i2 === -Math.sign(o2) && (r2 -= i2, o2 += i2 * Qr), [r2, o2];
}
function addBigNanos(e2, n2, t2 = 1) {
  return createBigNano(e2[0] + n2[0] * t2, e2[1] + n2[1] * t2);
}
function moveBigNano(e2, n2) {
  return createBigNano(e2[0], e2[1] + n2);
}
function re(e2, n2) {
  return addBigNanos(n2, e2, -1);
}
function te(e2, n2) {
  return compareNumbers(e2[0], n2[0]) || compareNumbers(e2[1], n2[1]);
}
function bigNanoOutside(e2, n2, t2) {
  return -1 === te(e2, n2) || 1 === te(e2, t2);
}
function bigIntToBigNano(e2, n2 = 1) {
  const t2 = BigInt(Qr / n2);
  return [Number(e2 / t2), Number(e2 % t2) * n2];
}
function he(e2, n2 = 1) {
  const t2 = Qr / n2, [o2, r2] = divModTrunc(e2, t2);
  return [o2, r2 * n2];
}
function bigNanoToBigInt(e2, n2 = 1) {
  const [t2, o2] = e2, r2 = Math.floor(o2 / n2), i2 = Qr / n2;
  return BigInt(t2) * BigInt(i2) + BigInt(r2);
}
function oe(e2, n2 = 1, t2) {
  const [o2, r2] = e2, [i2, a2] = divModTrunc(r2, n2);
  return o2 * (Qr / n2) + (i2 + (t2 ? a2 / n2 : 0));
}
function divModBigNano(e2, n2, t2 = divModFloor) {
  const [o2, r2] = e2, [i2, a2] = t2(r2, n2);
  return [o2 * (Qr / n2) + i2, a2];
}
function hashIntlFormatParts(e2, n2) {
  const t2 = e2.formatToParts(n2), o2 = {};
  for (const e3 of t2) {
    o2[e3.type] = e3.value;
  }
  return o2;
}
function checkIsoYearMonthInBounds(e2) {
  return clampProp(e2, "isoYear", Li, Ai, 1), e2.isoYear === Li ? clampProp(e2, "isoMonth", 4, 12, 1) : e2.isoYear === Ai && clampProp(e2, "isoMonth", 1, 9, 1), e2;
}
function checkIsoDateInBounds(e2) {
  return checkIsoDateTimeInBounds({
    ...e2,
    ...Dt,
    isoHour: 12
  }), e2;
}
function checkIsoDateTimeInBounds(e2) {
  const n2 = clampProp(e2, "isoYear", Li, Ai, 1), t2 = n2 === Li ? 1 : n2 === Ai ? -1 : 0;
  return t2 && checkEpochNanoInBounds(isoToEpochNano({
    ...e2,
    isoDay: e2.isoDay + t2,
    isoNanosecond: e2.isoNanosecond - t2
  })), e2;
}
function checkEpochNanoInBounds(e2) {
  if (!e2 || bigNanoOutside(e2, Ui, qi)) {
    throw new RangeError(Cr);
  }
  return e2;
}
function isoTimeFieldsToNano(e2) {
  return givenFieldsToBigNano(e2, 5, j)[1];
}
function nanoToIsoTimeAndDay(e2) {
  const [n2, t2] = divModFloor(e2, Qr);
  return [nanoToGivenFields(t2, 5, j), n2];
}
function epochNanoToSec(e2) {
  return epochNanoToSecMod(e2)[0];
}
function epochNanoToSecMod(e2) {
  return divModBigNano(e2, _r);
}
function isoToEpochMilli(e2) {
  return isoArgsToEpochMilli(e2.isoYear, e2.isoMonth, e2.isoDay, e2.isoHour, e2.isoMinute, e2.isoSecond, e2.isoMillisecond);
}
function isoToEpochNano(e2) {
  const n2 = isoToEpochMilli(e2);
  if (void 0 !== n2) {
    const [t2, o2] = divModTrunc(n2, Gr);
    return [t2, o2 * be + (e2.isoMicrosecond || 0) * Vr + (e2.isoNanosecond || 0)];
  }
}
function isoToEpochNanoWithOffset(e2, n2) {
  const [t2, o2] = nanoToIsoTimeAndDay(isoTimeFieldsToNano(e2) - n2);
  return checkEpochNanoInBounds(isoToEpochNano({
    ...e2,
    isoDay: e2.isoDay + o2,
    ...t2
  }));
}
function isoArgsToEpochSec(...e2) {
  return isoArgsToEpochMilli(...e2) / Hr;
}
function isoArgsToEpochMilli(...e2) {
  const [n2, t2] = isoToLegacyDate(...e2), o2 = n2.valueOf();
  if (!isNaN(o2)) {
    return o2 - t2 * Gr;
  }
}
function isoToLegacyDate(e2, n2 = 1, t2 = 1, o2 = 0, r2 = 0, i2 = 0, a2 = 0) {
  const s2 = e2 === Li ? 1 : e2 === Ai ? -1 : 0, c2 = /* @__PURE__ */ new Date();
  return c2.setUTCHours(o2, r2, i2, a2), c2.setUTCFullYear(e2, n2 - 1, t2 + s2), [c2, s2];
}
function Ie(e2, n2) {
  let [t2, o2] = moveBigNano(e2, n2);
  o2 < 0 && (o2 += Qr, t2 -= 1);
  const [r2, i2] = divModFloor(o2, be), [a2, s2] = divModFloor(i2, Vr);
  return epochMilliToIso(t2 * Gr + r2, a2, s2);
}
function epochMilliToIso(e2, n2 = 0, t2 = 0) {
  const o2 = Math.ceil(Math.max(0, Math.abs(e2) - zi) / Gr) * Math.sign(e2), r2 = new Date(e2 - o2 * Gr);
  return zipProps(wi, [r2.getUTCFullYear(), r2.getUTCMonth() + 1, r2.getUTCDate() + o2, r2.getUTCHours(), r2.getUTCMinutes(), r2.getUTCSeconds(), r2.getUTCMilliseconds(), n2, t2]);
}
function computeIsoDateParts(e2) {
  return [e2.isoYear, e2.isoMonth, e2.isoDay];
}
function computeIsoMonthsInYear() {
  return xi;
}
function computeIsoDaysInMonth(e2, n2) {
  switch (n2) {
    case 2:
      return computeIsoInLeapYear(e2) ? 29 : 28;
    case 4:
    case 6:
    case 9:
    case 11:
      return 30;
  }
  return 31;
}
function computeIsoDaysInYear(e2) {
  return computeIsoInLeapYear(e2) ? 366 : 365;
}
function computeIsoInLeapYear(e2) {
  return e2 % 4 == 0 && (e2 % 100 != 0 || e2 % 400 == 0);
}
function computeIsoDayOfWeek(e2) {
  const [n2, t2] = isoToLegacyDate(e2.isoYear, e2.isoMonth, e2.isoDay);
  return modFloor(n2.getUTCDay() - t2, 7) || 7;
}
function computeGregoryEraParts({ isoYear: e2 }) {
  return e2 < 1 ? ["bce", 1 - e2] : ["ce", e2];
}
function computeJapaneseEraParts(e2) {
  const n2 = isoToEpochMilli(e2);
  if (n2 < $i) {
    return computeGregoryEraParts(e2);
  }
  const t2 = hashIntlFormatParts(La(Ti), n2), { era: o2, eraYear: r2 } = parseIntlYear(t2, Ti);
  return [o2, r2];
}
function checkIsoDateTimeFields(e2) {
  return checkIsoDateFields(e2), constrainIsoTimeFields(e2, 1), e2;
}
function checkIsoDateFields(e2) {
  return constrainIsoDateFields(e2, 1), e2;
}
function isIsoDateFieldsValid(e2) {
  return allPropsEqual(Oi, e2, constrainIsoDateFields(e2));
}
function constrainIsoDateFields(e2, n2) {
  const { isoYear: t2 } = e2, o2 = clampProp(e2, "isoMonth", 1, computeIsoMonthsInYear(), n2);
  return {
    isoYear: t2,
    isoMonth: o2,
    isoDay: clampProp(e2, "isoDay", 1, computeIsoDaysInMonth(t2, o2), n2)
  };
}
function constrainIsoTimeFields(e2, n2) {
  return zipProps(j, [clampProp(e2, "isoHour", 0, 23, n2), clampProp(e2, "isoMinute", 0, 59, n2), clampProp(e2, "isoSecond", 0, 59, n2), clampProp(e2, "isoMillisecond", 0, 999, n2), clampProp(e2, "isoMicrosecond", 0, 999, n2), clampProp(e2, "isoNanosecond", 0, 999, n2)]);
}
function H(e2) {
  return void 0 === e2 ? 0 : ua(de(e2));
}
function wn(e2, n2 = 0) {
  e2 = normalizeOptions(e2);
  const t2 = la(e2), o2 = fa(e2, n2);
  return [ua(e2), o2, t2];
}
function ve(e2) {
  return la(normalizeOptions(e2));
}
function _t(e2) {
  return e2 = normalizeOptions(e2), sa(e2, 9, 6, 1);
}
function refineDiffOptions(e2, n2, t2, o2 = 9, r2 = 0, i2 = 4) {
  n2 = normalizeOptions(n2);
  let a2 = sa(n2, o2, r2), s2 = parseRoundingIncInteger(n2), c2 = ha(n2, i2);
  const u2 = aa(n2, o2, r2, 1);
  return null == a2 ? a2 = Math.max(t2, u2) : checkLargestSmallestUnit(a2, u2), s2 = refineRoundingInc(s2, u2, 1), e2 && (c2 = ((e3) => e3 < 4 ? (e3 + 2) % 4 : e3)(c2)), [a2, u2, s2, c2];
}
function refineRoundingOptions(e2, n2 = 6, t2) {
  let o2 = parseRoundingIncInteger(e2 = normalizeOptionsOrString(e2, Hi));
  const r2 = ha(e2, 7);
  let i2 = aa(e2, n2);
  return i2 = requirePropDefined(Hi, i2), o2 = refineRoundingInc(o2, i2, void 0, t2), [i2, o2, r2];
}
function refineDateDisplayOptions(e2) {
  return da(normalizeOptions(e2));
}
function refineTimeDisplayOptions(e2, n2) {
  return refineTimeDisplayTuple(normalizeOptions(e2), n2);
}
function refineTimeDisplayTuple(e2, n2 = 4) {
  const t2 = refineSubsecDigits(e2);
  return [ha(e2, 4), ...refineSmallestUnitAndSubsecDigits(aa(e2, n2), t2)];
}
function refineSmallestUnitAndSubsecDigits(e2, n2) {
  return null != e2 ? [Xr[e2], e2 < 4 ? 9 - 3 * e2 : -1] : [void 0 === n2 ? 1 : 10 ** (9 - n2), n2];
}
function parseRoundingIncInteger(e2) {
  const n2 = e2[_i];
  return void 0 === n2 ? 1 : toInteger(n2, _i);
}
function refineRoundingInc(e2, n2, t2, o2) {
  const r2 = o2 ? Qr : Xr[n2 + 1];
  if (r2) {
    const t3 = Xr[n2];
    if (r2 % ((e2 = clampEntity(_i, e2, 1, r2 / t3 - (o2 ? 0 : 1), 1)) * t3)) {
      throw new RangeError(invalidEntity(_i, e2));
    }
  } else {
    e2 = clampEntity(_i, e2, 1, t2 ? 10 ** 9 : 1, 1);
  }
  return e2;
}
function refineSubsecDigits(e2) {
  let n2 = e2[Ji];
  if (void 0 !== n2) {
    if ("number" != typeof n2) {
      if ("auto" === toString(n2)) {
        return;
      }
      throw new RangeError(invalidEntity(Ji, n2));
    }
    n2 = clampEntity(Ji, Math.floor(n2), 0, 9, 1);
  }
  return n2;
}
function normalizeOptions(e2) {
  return void 0 === e2 ? {} : de(e2);
}
function normalizeOptionsOrString(e2, n2) {
  return "string" == typeof e2 ? {
    [n2]: e2
  } : de(e2);
}
function U(e2) {
  if (void 0 !== e2) {
    if (z(e2)) {
      return Object.assign(/* @__PURE__ */ Object.create(null), e2);
    }
    throw new TypeError(hr);
  }
}
function overrideOverflowOptions(e2, n2) {
  return e2 && Object.assign(/* @__PURE__ */ Object.create(null), e2, {
    overflow: Xi[n2]
  });
}
function refineUnitOption(e2, n2, t2 = 9, o2 = 0, r2) {
  let i2 = n2[e2];
  if (void 0 === i2) {
    return r2 ? o2 : void 0;
  }
  if (i2 = toString(i2), "auto" === i2) {
    return r2 ? o2 : null;
  }
  let a2 = $r[i2];
  if (void 0 === a2 && (a2 = Ei[i2]), void 0 === a2) {
    throw new RangeError(invalidChoice(e2, i2, $r));
  }
  return clampEntity(e2, a2, o2, t2, 1, Et), a2;
}
function refineChoiceOption(e2, n2, t2, o2 = 0) {
  const r2 = t2[e2];
  if (void 0 === r2) {
    return o2;
  }
  const i2 = toString(r2), a2 = n2[i2];
  if (void 0 === a2) {
    throw new RangeError(invalidChoice(e2, i2, n2));
  }
  return a2;
}
function checkLargestSmallestUnit(e2, n2) {
  if (n2 > e2) {
    throw new RangeError(Ar);
  }
}
function _(e2) {
  return {
    branding: Oe,
    epochNanoseconds: e2
  };
}
function Yn(e2, n2, t2) {
  return {
    branding: Te,
    calendar: t2,
    timeZone: n2,
    epochNanoseconds: e2
  };
}
function ee(e2, n2 = e2.calendar) {
  return {
    branding: We,
    calendar: n2,
    ...Vn(Yi, e2)
  };
}
function v(e2, n2 = e2.calendar) {
  return {
    branding: J,
    calendar: n2,
    ...Vn(Bi, e2)
  };
}
function createPlainYearMonthSlots(e2, n2 = e2.calendar) {
  return {
    branding: L,
    calendar: n2,
    ...Vn(Bi, e2)
  };
}
function createPlainMonthDaySlots(e2, n2 = e2.calendar) {
  return {
    branding: q,
    calendar: n2,
    ...Vn(Bi, e2)
  };
}
function Ge(e2) {
  return {
    branding: xe,
    ...Vn(ki, e2)
  };
}
function Vt(e2) {
  return {
    branding: qt,
    sign: computeDurationSign(e2),
    ...Vn(Ni, e2)
  };
}
function M(e2) {
  return epochNanoToSec(e2.epochNanoseconds);
}
function y(e2) {
  return divModBigNano(e2.epochNanoseconds, be)[0];
}
function N(e2) {
  return bigNanoToBigInt(e2.epochNanoseconds, Vr);
}
function B(e2) {
  return bigNanoToBigInt(e2.epochNanoseconds);
}
function extractEpochNano(e2) {
  return e2.epochNanoseconds;
}
function I(e2) {
  return "string" == typeof e2 ? e2 : m(e2.id);
}
function isIdLikeEqual(e2, n2) {
  return e2 === n2 || I(e2) === I(n2);
}
function Ut(e2, n2, t2, o2, r2) {
  const i2 = getMaxDurationUnit(o2), [a2, s2] = ((e3, n3) => {
    const t3 = n3((e3 = normalizeOptionsOrString(e3, Vi))[Ki]);
    let o3 = ca(e3);
    return o3 = requirePropDefined(Vi, o3), [o3, t3];
  })(r2, e2);
  if (isUniformUnit(Math.max(a2, i2), s2)) {
    return totalDayTimeDuration(o2, a2);
  }
  if (!s2) {
    throw new RangeError(zr);
  }
  const [c2, u2, l2] = createMarkerSystem(n2, t2, s2), f2 = createMarkerToEpochNano(l2), d2 = createMoveMarker(l2), m2 = createDiffMarkers(l2), p2 = d2(u2, c2, o2), h2 = m2(u2, c2, p2, a2);
  return isUniformUnit(a2, s2) ? totalDayTimeDuration(h2, a2) : ((e3, n3, t3, o3, r3, i3, a3) => {
    const s3 = computeDurationSign(e3), [c3, u3] = clampRelativeDuration(o3, bi(t3, e3), t3, s3, r3, i3, a3), l3 = computeEpochNanoFrac(n3, c3, u3);
    return e3[F[t3]] + l3 * s3;
  })(h2, f2(p2), a2, u2, c2, f2, d2);
}
function totalDayTimeDuration(e2, n2) {
  return oe(durationFieldsToBigNano(e2), Xr[n2], 1);
}
function clampRelativeDuration(e2, n2, t2, o2, r2, i2, a2) {
  const s2 = F[t2], c2 = {
    ...n2,
    [s2]: n2[s2] + o2
  }, u2 = a2(e2, r2, n2), l2 = a2(e2, r2, c2);
  return [i2(u2), i2(l2)];
}
function computeEpochNanoFrac(e2, n2, t2) {
  const o2 = oe(re(n2, t2));
  if (!o2) {
    throw new RangeError(vr);
  }
  return oe(re(n2, e2)) / o2;
}
function ce(e2, n2) {
  const [t2, o2, r2] = refineRoundingOptions(n2, 5, 1);
  return _(roundBigNano(e2.epochNanoseconds, t2, o2, r2, 1));
}
function Pn(e2, n2, t2) {
  let { epochNanoseconds: o2, timeZone: r2, calendar: i2 } = n2;
  const [a2, s2, c2] = refineRoundingOptions(t2);
  if (0 === a2 && 1 === s2) {
    return n2;
  }
  const u2 = e2(r2);
  if (6 === a2) {
    o2 = ((e3, n3, t3, o3) => {
      const r3 = fn(t3, n3), [i3, a3] = e3(r3), s3 = t3.epochNanoseconds, c3 = we(n3, i3), u3 = we(n3, a3);
      if (bigNanoOutside(s3, c3, u3)) {
        throw new RangeError(vr);
      }
      return roundWithMode(computeEpochNanoFrac(s3, c3, u3), o3) ? u3 : c3;
    })(computeDayInterval, u2, n2, c2);
  } else {
    const e3 = u2.getOffsetNanosecondsFor(o2);
    o2 = getMatchingInstantFor(u2, roundDateTime(Ie(o2, e3), a2, s2, c2), e3, 2, 0, 1);
  }
  return Yn(o2, r2, i2);
}
function dt(e2, n2) {
  return ee(roundDateTime(e2, ...refineRoundingOptions(n2)), e2.calendar);
}
function Ee(e2, n2) {
  const [t2, o2, r2] = refineRoundingOptions(n2, 5);
  var i2;
  return Ge((i2 = r2, roundTimeToNano(e2, computeNanoInc(t2, o2), i2)[0]));
}
function dn(e2, n2) {
  const t2 = e2(n2.timeZone), o2 = fn(n2, t2), [r2, i2] = computeDayInterval(o2), a2 = oe(re(we(t2, r2), we(t2, i2)), Kr, 1);
  if (a2 <= 0) {
    throw new RangeError(vr);
  }
  return a2;
}
function Cn(e2, n2) {
  const { timeZone: t2, calendar: o2 } = n2, r2 = ((e3, n3, t3) => we(n3, e3(fn(t3, n3))))(computeDayFloor, e2(t2), n2);
  return Yn(r2, t2, o2);
}
function roundDateTime(e2, n2, t2, o2) {
  return roundDateTimeToNano(e2, computeNanoInc(n2, t2), o2);
}
function roundDateTimeToNano(e2, n2, t2) {
  const [o2, r2] = roundTimeToNano(e2, n2, t2);
  return checkIsoDateTimeInBounds({
    ...moveByDays(e2, r2),
    ...o2
  });
}
function roundTimeToNano(e2, n2, t2) {
  return nanoToIsoTimeAndDay(roundByInc(isoTimeFieldsToNano(e2), n2, t2));
}
function roundToMinute(e2) {
  return roundByInc(e2, Jr, 7);
}
function computeNanoInc(e2, n2) {
  return Xr[e2] * n2;
}
function computeDayInterval(e2) {
  const n2 = computeDayFloor(e2);
  return [n2, moveByDays(n2, 1)];
}
function computeDayFloor(e2) {
  return Ci(6, e2);
}
function roundDayTimeDurationByInc(e2, n2, t2) {
  const o2 = Math.min(getMaxDurationUnit(e2), 6);
  return nanoToDurationDayTimeFields(roundBigNanoByInc(durationFieldsToBigNano(e2, o2), n2, t2), o2);
}
function roundRelativeDuration(e2, n2, t2, o2, r2, i2, a2, s2, c2, u2) {
  if (0 === o2 && 1 === r2) {
    return e2;
  }
  const l2 = isUniformUnit(o2, s2) ? isZonedEpochSlots(s2) && o2 < 6 && t2 >= 6 ? nudgeZonedTimeDuration : nudgeDayTimeDuration : nudgeRelativeDuration;
  let [f2, d2, m2] = l2(e2, n2, t2, o2, r2, i2, a2, s2, c2, u2);
  return m2 && 7 !== o2 && (f2 = ((e3, n3, t3, o3, r3, i3, a3, s3) => {
    const c3 = computeDurationSign(e3);
    for (let u3 = o3 + 1; u3 <= t3; u3++) {
      if (7 === u3 && 7 !== t3) {
        continue;
      }
      const o4 = bi(u3, e3);
      o4[F[u3]] += c3;
      const l3 = oe(re(a3(s3(r3, i3, o4)), n3));
      if (l3 && Math.sign(l3) !== c3) {
        break;
      }
      e3 = o4;
    }
    return e3;
  })(f2, d2, t2, Math.max(6, o2), a2, s2, c2, u2)), f2;
}
function roundBigNano(e2, n2, t2, o2, r2) {
  if (6 === n2) {
    const n3 = ((e3) => e3[0] + e3[1] / Qr)(e2);
    return [roundByInc(n3, t2, o2), 0];
  }
  return roundBigNanoByInc(e2, computeNanoInc(n2, t2), o2, r2);
}
function roundBigNanoByInc(e2, n2, t2, o2) {
  let [r2, i2] = e2;
  o2 && i2 < 0 && (i2 += Qr, r2 -= 1);
  const [a2, s2] = divModFloor(roundByInc(i2, n2, t2), Qr);
  return createBigNano(r2 + a2, s2);
}
function roundByInc(e2, n2, t2) {
  return roundWithMode(e2 / n2, t2) * n2;
}
function roundWithMode(e2, n2) {
  return ga[n2](e2);
}
function nudgeDayTimeDuration(e2, n2, t2, o2, r2, i2) {
  const a2 = computeDurationSign(e2), s2 = durationFieldsToBigNano(e2), c2 = roundBigNano(s2, o2, r2, i2), u2 = re(s2, c2), l2 = Math.sign(c2[0] - s2[0]) === a2, f2 = nanoToDurationDayTimeFields(c2, Math.min(t2, 6));
  return [{
    ...e2,
    ...f2
  }, addBigNanos(n2, u2), l2];
}
function nudgeZonedTimeDuration(e2, n2, t2, o2, r2, i2, a2, s2, c2, u2) {
  const l2 = computeDurationSign(e2), f2 = oe(durationFieldsToBigNano(e2, 5)), d2 = computeNanoInc(o2, r2);
  let m2 = roundByInc(f2, d2, i2);
  const [p2, h2] = clampRelativeDuration(a2, {
    ...e2,
    ...Fi
  }, 6, l2, s2, c2, u2), g2 = m2 - oe(re(p2, h2));
  let T2 = 0;
  g2 && Math.sign(g2) !== l2 ? n2 = moveBigNano(p2, m2) : (T2 += l2, m2 = roundByInc(g2, d2, i2), n2 = moveBigNano(h2, m2));
  const D2 = nanoToDurationTimeFields(m2);
  return [{
    ...e2,
    ...D2,
    days: e2.days + T2
  }, n2, Boolean(T2)];
}
function nudgeRelativeDuration(e2, n2, t2, o2, r2, i2, a2, s2, c2, u2) {
  const l2 = computeDurationSign(e2), f2 = F[o2], d2 = bi(o2, e2);
  7 === o2 && (e2 = {
    ...e2,
    weeks: e2.weeks + Math.trunc(e2.days / 7)
  });
  const m2 = divTrunc(e2[f2], r2) * r2;
  d2[f2] = m2;
  const [p2, h2] = clampRelativeDuration(a2, d2, o2, r2 * l2, s2, c2, u2), g2 = m2 + computeEpochNanoFrac(n2, p2, h2) * l2 * r2, T2 = roundByInc(g2, r2, i2), D2 = Math.sign(T2 - g2) === l2;
  return d2[f2] = T2, [d2, D2 ? h2 : p2, D2];
}
function me(e2, n2, t2, o2) {
  const [r2, i2, a2, s2] = ((e3) => {
    const n3 = refineTimeDisplayTuple(e3 = normalizeOptions(e3));
    return [e3.timeZone, ...n3];
  })(o2), c2 = void 0 !== r2;
  return ((e3, n3, t3, o3, r3, i3) => {
    t3 = roundBigNanoByInc(t3, r3, o3, 1);
    const a3 = n3.getOffsetNanosecondsFor(t3);
    return formatIsoDateTimeFields(Ie(t3, a3), i3) + (e3 ? Fe(roundToMinute(a3)) : "Z");
  })(c2, n2(c2 ? e2(r2) : Ta), t2.epochNanoseconds, i2, a2, s2);
}
function In(e2, n2, t2) {
  const [o2, r2, i2, a2, s2, c2] = ((e3) => {
    e3 = normalizeOptions(e3);
    const n3 = da(e3), t3 = refineSubsecDigits(e3), o3 = pa(e3), r3 = ha(e3, 4), i3 = aa(e3, 4);
    return [n3, ma(e3), o3, r3, ...refineSmallestUnitAndSubsecDigits(i3, t3)];
  })(t2);
  return ((e3, n3, t3, o3, r3, i3, a3, s3, c3, u2) => {
    o3 = roundBigNanoByInc(o3, c3, s3, 1);
    const l2 = e3(t3).getOffsetNanosecondsFor(o3);
    return formatIsoDateTimeFields(Ie(o3, l2), u2) + Fe(roundToMinute(l2), a3) + ((e4, n4) => 1 !== n4 ? "[" + (2 === n4 ? "!" : "") + I(e4) + "]" : "")(t3, i3) + formatCalendar(n3, r3);
  })(e2, n2.calendar, n2.timeZone, n2.epochNanoseconds, o2, r2, i2, a2, s2, c2);
}
function Tt(e2, n2) {
  const [t2, o2, r2, i2] = ((e3) => (e3 = normalizeOptions(e3), [da(e3), ...refineTimeDisplayTuple(e3)]))(n2);
  return a2 = e2.calendar, s2 = t2, c2 = i2, formatIsoDateTimeFields(roundDateTimeToNano(e2, r2, o2), c2) + formatCalendar(a2, s2);
  var a2, s2, c2;
}
function yt(e2, n2) {
  return t2 = e2.calendar, o2 = e2, r2 = refineDateDisplayOptions(n2), formatIsoDateFields(o2) + formatCalendar(t2, r2);
  var t2, o2, r2;
}
function et(e2, n2) {
  return formatDateLikeIso(e2.calendar, formatIsoYearMonthFields, e2, refineDateDisplayOptions(n2));
}
function W(e2, n2) {
  return formatDateLikeIso(e2.calendar, formatIsoMonthDayFields, e2, refineDateDisplayOptions(n2));
}
function qe(e2, n2) {
  const [t2, o2, r2] = refineTimeDisplayOptions(n2);
  return i2 = r2, formatIsoTimeFields(roundTimeToNano(e2, o2, t2)[0], i2);
  var i2;
}
function zt(e2, n2) {
  const [t2, o2, r2] = refineTimeDisplayOptions(n2, 3);
  return o2 > 1 && (e2 = {
    ...e2,
    ...roundDayTimeDurationByInc(e2, o2, t2)
  }), ((e3, n3) => {
    const { sign: t3 } = e3, o3 = -1 === t3 ? negateDurationFields(e3) : e3, { hours: r3, minutes: i2 } = o3, [a2, s2] = divModBigNano(durationFieldsToBigNano(o3, 3), _r, divModTrunc);
    checkDurationTimeUnit(a2);
    const c2 = formatSubsecNano(s2, n3), u2 = n3 >= 0 || !t3 || c2;
    return (t3 < 0 ? "-" : "") + "P" + formatDurationFragments({
      Y: formatDurationNumber(o3.years),
      M: formatDurationNumber(o3.months),
      W: formatDurationNumber(o3.weeks),
      D: formatDurationNumber(o3.days)
    }) + (r3 || i2 || a2 || u2 ? "T" + formatDurationFragments({
      H: formatDurationNumber(r3),
      M: formatDurationNumber(i2),
      S: formatDurationNumber(a2, u2) + c2
    }) : "");
  })(e2, r2);
}
function formatDateLikeIso(e2, n2, t2, o2) {
  const r2 = I(e2), i2 = o2 > 1 || 0 === o2 && r2 !== X;
  return 1 === o2 ? r2 === X ? n2(t2) : formatIsoDateFields(t2) : i2 ? formatIsoDateFields(t2) + formatCalendarId(r2, 2 === o2) : n2(t2);
}
function formatDurationFragments(e2) {
  const n2 = [];
  for (const t2 in e2) {
    const o2 = e2[t2];
    o2 && n2.push(o2, t2);
  }
  return n2.join("");
}
function formatIsoDateTimeFields(e2, n2) {
  return formatIsoDateFields(e2) + "T" + formatIsoTimeFields(e2, n2);
}
function formatIsoDateFields(e2) {
  return formatIsoYearMonthFields(e2) + "-" + xr(e2.isoDay);
}
function formatIsoYearMonthFields(e2) {
  const { isoYear: n2 } = e2;
  return (n2 < 0 || n2 > 9999 ? getSignStr(n2) + padNumber(6, Math.abs(n2)) : padNumber(4, n2)) + "-" + xr(e2.isoMonth);
}
function formatIsoMonthDayFields(e2) {
  return xr(e2.isoMonth) + "-" + xr(e2.isoDay);
}
function formatIsoTimeFields(e2, n2) {
  const t2 = [xr(e2.isoHour), xr(e2.isoMinute)];
  return -1 !== n2 && t2.push(xr(e2.isoSecond) + ((e3, n3, t3, o2) => formatSubsecNano(e3 * be + n3 * Vr + t3, o2))(e2.isoMillisecond, e2.isoMicrosecond, e2.isoNanosecond, n2)), t2.join(":");
}
function Fe(e2, n2 = 0) {
  if (1 === n2) {
    return "";
  }
  const [t2, o2] = divModFloor(Math.abs(e2), Kr), [r2, i2] = divModFloor(o2, Jr), [a2, s2] = divModFloor(i2, _r);
  return getSignStr(e2) + xr(t2) + ":" + xr(r2) + (a2 || s2 ? ":" + xr(a2) + formatSubsecNano(s2) : "");
}
function formatCalendar(e2, n2) {
  if (1 !== n2) {
    const t2 = I(e2);
    if (n2 > 1 || 0 === n2 && t2 !== X) {
      return formatCalendarId(t2, 2 === n2);
    }
  }
  return "";
}
function formatCalendarId(e2, n2) {
  return "[" + (n2 ? "!" : "") + "u-ca=" + e2 + "]";
}
function formatSubsecNano(e2, n2) {
  let t2 = padNumber(9, e2);
  return t2 = void 0 === n2 ? t2.replace(Na, "") : t2.slice(0, n2), t2 ? "." + t2 : "";
}
function getSignStr(e2) {
  return e2 < 0 ? "-" : "+";
}
function formatDurationNumber(e2, n2) {
  return e2 || n2 ? e2.toLocaleString("fullwide", {
    useGrouping: 0
  }) : "";
}
function _zonedEpochSlotsToIso(e2, n2) {
  const { epochNanoseconds: t2 } = e2, o2 = (n2.getOffsetNanosecondsFor ? n2 : n2(e2.timeZone)).getOffsetNanosecondsFor(t2), r2 = Ie(t2, o2);
  return {
    calendar: e2.calendar,
    ...r2,
    offsetNanoseconds: o2
  };
}
function mn(e2, n2) {
  const t2 = fn(n2, e2);
  return {
    calendar: n2.calendar,
    ...Vn(Yi, t2),
    offset: Fe(t2.offsetNanoseconds),
    timeZone: n2.timeZone
  };
}
function getMatchingInstantFor(e2, n2, t2, o2 = 0, r2 = 0, i2, a2) {
  if (void 0 !== t2 && 1 === o2 && (1 === o2 || a2)) {
    return isoToEpochNanoWithOffset(n2, t2);
  }
  const s2 = e2.getPossibleInstantsFor(n2);
  if (void 0 !== t2 && 3 !== o2) {
    const e3 = ((e4, n3, t3, o3) => {
      const r3 = isoToEpochNano(n3);
      o3 && (t3 = roundToMinute(t3));
      for (const n4 of e4) {
        let e5 = oe(re(n4, r3));
        if (o3 && (e5 = roundToMinute(e5)), e5 === t3) {
          return n4;
        }
      }
    })(s2, n2, t2, i2);
    if (void 0 !== e3) {
      return e3;
    }
    if (0 === o2) {
      throw new RangeError(kr);
    }
  }
  return a2 ? isoToEpochNano(n2) : we(e2, n2, r2, s2);
}
function we(e2, n2, t2 = 0, o2 = e2.getPossibleInstantsFor(n2)) {
  if (1 === o2.length) {
    return o2[0];
  }
  if (1 === t2) {
    throw new RangeError(Yr);
  }
  if (o2.length) {
    return o2[3 === t2 ? 1 : 0];
  }
  const r2 = isoToEpochNano(n2), i2 = ((e3, n3) => {
    const t3 = e3.getOffsetNanosecondsFor(moveBigNano(n3, -Qr));
    return ne(e3.getOffsetNanosecondsFor(moveBigNano(n3, Qr)) - t3);
  })(e2, r2), a2 = i2 * (2 === t2 ? -1 : 1);
  return (o2 = e2.getPossibleInstantsFor(Ie(r2, a2)))[2 === t2 ? 0 : o2.length - 1];
}
function ae(e2) {
  if (Math.abs(e2) >= Qr) {
    throw new RangeError(wr);
  }
  return e2;
}
function ne(e2) {
  if (e2 > Qr) {
    throw new RangeError(Br);
  }
  return e2;
}
function se(e2, n2, t2) {
  return _(checkEpochNanoInBounds(addBigNanos(n2.epochNanoseconds, ((e3) => {
    if (durationHasDateParts(e3)) {
      throw new RangeError(qr);
    }
    return durationFieldsToBigNano(e3, 5);
  })(e2 ? negateDurationFields(t2) : t2))));
}
function hn(e2, n2, t2, o2, r2, i2 = /* @__PURE__ */ Object.create(null)) {
  const a2 = n2(o2.timeZone), s2 = e2(o2.calendar);
  return {
    ...o2,
    ...moveZonedEpochs(a2, s2, o2, t2 ? negateDurationFields(r2) : r2, i2)
  };
}
function ct(e2, n2, t2, o2, r2 = /* @__PURE__ */ Object.create(null)) {
  const { calendar: i2 } = t2;
  return ee(moveDateTime(e2(i2), t2, n2 ? negateDurationFields(o2) : o2, r2), i2);
}
function bt(e2, n2, t2, o2, r2) {
  const { calendar: i2 } = t2;
  return v(moveDate(e2(i2), t2, n2 ? negateDurationFields(o2) : o2, r2), i2);
}
function Qe(e2, n2, t2, o2, r2 = /* @__PURE__ */ Object.create(null)) {
  const i2 = t2.calendar, a2 = e2(i2);
  let s2 = moveToDayOfMonthUnsafe(a2, t2);
  n2 && (o2 = xt(o2)), o2.sign < 0 && (s2 = a2.dateAdd(s2, {
    ...Si,
    months: 1
  }), s2 = moveByDays(s2, -1));
  const c2 = a2.dateAdd(s2, o2, r2);
  return createPlainYearMonthSlots(moveToDayOfMonthUnsafe(a2, c2), i2);
}
function Ye(e2, n2, t2) {
  return Ge(moveTime(n2, e2 ? negateDurationFields(t2) : t2)[0]);
}
function moveZonedEpochs(e2, n2, t2, o2, r2) {
  const i2 = durationFieldsToBigNano(o2, 5);
  let a2 = t2.epochNanoseconds;
  if (durationHasDateParts(o2)) {
    const s2 = fn(t2, e2);
    a2 = addBigNanos(we(e2, {
      ...moveDate(n2, s2, {
        ...o2,
        ...Fi
      }, r2),
      ...Vn(j, s2)
    }), i2);
  } else {
    a2 = addBigNanos(a2, i2), H(r2);
  }
  return {
    epochNanoseconds: checkEpochNanoInBounds(a2)
  };
}
function moveDateTime(e2, n2, t2, o2) {
  const [r2, i2] = moveTime(n2, t2);
  return checkIsoDateTimeInBounds({
    ...moveDate(e2, n2, {
      ...t2,
      ...Fi,
      days: t2.days + i2
    }, o2),
    ...r2
  });
}
function moveDate(e2, n2, t2, o2) {
  if (t2.years || t2.months || t2.weeks) {
    return e2.dateAdd(n2, t2, o2);
  }
  H(o2);
  const r2 = t2.days + durationFieldsToBigNano(t2, 5)[0];
  return r2 ? checkIsoDateInBounds(moveByDays(n2, r2)) : n2;
}
function moveToDayOfMonthUnsafe(e2, n2, t2 = 1) {
  return moveByDays(n2, t2 - e2.day(n2));
}
function moveTime(e2, n2) {
  const [t2, o2] = durationFieldsToBigNano(n2, 5), [r2, i2] = nanoToIsoTimeAndDay(isoTimeFieldsToNano(e2) + o2);
  return [r2, t2 + i2];
}
function moveByDays(e2, n2) {
  return n2 ? {
    ...e2,
    ...epochMilliToIso(isoToEpochMilli(e2) + n2 * Gr)
  } : e2;
}
function createMarkerSystem(e2, n2, t2) {
  const o2 = e2(t2.calendar);
  return isZonedEpochSlots(t2) ? [t2, o2, n2(t2.timeZone)] : [{
    ...t2,
    ...Dt
  }, o2];
}
function createMarkerToEpochNano(e2) {
  return e2 ? extractEpochNano : isoToEpochNano;
}
function createMoveMarker(e2) {
  return e2 ? E(moveZonedEpochs, e2) : moveDateTime;
}
function createDiffMarkers(e2) {
  return e2 ? E(diffZonedEpochsExact, e2) : diffDateTimesExact;
}
function isZonedEpochSlots(e2) {
  return e2 && e2.epochNanoseconds;
}
function isUniformUnit(e2, n2) {
  return e2 <= 6 - (isZonedEpochSlots(n2) ? 1 : 0);
}
function Wt(e2, n2, t2, o2, r2, i2, a2) {
  const s2 = e2(normalizeOptions(a2).relativeTo), c2 = Math.max(getMaxDurationUnit(r2), getMaxDurationUnit(i2));
  if (isUniformUnit(c2, s2)) {
    return Vt(checkDurationUnits(((e3, n3, t3, o3) => {
      const r3 = addBigNanos(durationFieldsToBigNano(e3), durationFieldsToBigNano(n3), o3 ? -1 : 1);
      if (!Number.isFinite(r3[0])) {
        throw new RangeError(Cr);
      }
      return {
        ...Si,
        ...nanoToDurationDayTimeFields(r3, t3)
      };
    })(r2, i2, c2, o2)));
  }
  if (!s2) {
    throw new RangeError(zr);
  }
  o2 && (i2 = negateDurationFields(i2));
  const [u2, l2, f2] = createMarkerSystem(n2, t2, s2), d2 = createMoveMarker(f2), m2 = createDiffMarkers(f2), p2 = d2(l2, u2, r2);
  return Vt(m2(l2, u2, d2(l2, p2, i2), c2));
}
function Gt(e2, n2, t2, o2, r2) {
  const i2 = getMaxDurationUnit(o2), [a2, s2, c2, u2, l2] = ((e3, n3, t3) => {
    e3 = normalizeOptionsOrString(e3, Hi);
    let o3 = sa(e3);
    const r3 = t3(e3[Ki]);
    let i3 = parseRoundingIncInteger(e3);
    const a3 = ha(e3, 7);
    let s3 = aa(e3);
    if (void 0 === o3 && void 0 === s3) {
      throw new RangeError(Ur);
    }
    return null == s3 && (s3 = 0), null == o3 && (o3 = Math.max(s3, n3)), checkLargestSmallestUnit(o3, s3), i3 = refineRoundingInc(i3, s3, 1), [o3, s3, i3, a3, r3];
  })(r2, i2, e2), f2 = Math.max(i2, a2);
  if (!isZonedEpochSlots(l2) && f2 <= 6) {
    return Vt(checkDurationUnits(((e3, n3, t3, o3, r3) => {
      const i3 = roundBigNano(durationFieldsToBigNano(e3), t3, o3, r3);
      return {
        ...Si,
        ...nanoToDurationDayTimeFields(i3, n3)
      };
    })(o2, a2, s2, c2, u2)));
  }
  if (!l2) {
    throw new RangeError(zr);
  }
  const [d2, m2, p2] = createMarkerSystem(n2, t2, l2), h2 = createMarkerToEpochNano(p2), g2 = createMoveMarker(p2), T2 = createDiffMarkers(p2), D2 = g2(m2, d2, o2);
  let I2 = T2(m2, d2, D2, a2);
  const M2 = o2.sign, N2 = computeDurationSign(I2);
  if (M2 && N2 && M2 !== N2) {
    throw new RangeError(vr);
  }
  return N2 && (I2 = roundRelativeDuration(I2, h2(D2), a2, s2, c2, u2, m2, d2, h2, g2)), Vt(I2);
}
function Rt(e2) {
  return -1 === e2.sign ? xt(e2) : e2;
}
function xt(e2) {
  return Vt(negateDurationFields(e2));
}
function negateDurationFields(e2) {
  const n2 = {};
  for (const t2 of F) {
    n2[t2] = -1 * e2[t2] || 0;
  }
  return n2;
}
function Jt(e2) {
  return !e2.sign;
}
function computeDurationSign(e2, n2 = F) {
  let t2 = 0;
  for (const o2 of n2) {
    const n3 = Math.sign(e2[o2]);
    if (n3) {
      if (t2 && t2 !== n3) {
        throw new RangeError(Rr);
      }
      t2 = n3;
    }
  }
  return t2;
}
function checkDurationUnits(e2) {
  for (const n2 of vi) {
    clampEntity(n2, e2[n2], -ya, ya, 1);
  }
  return checkDurationTimeUnit(oe(durationFieldsToBigNano(e2), _r)), e2;
}
function checkDurationTimeUnit(e2) {
  if (!Number.isSafeInteger(e2)) {
    throw new RangeError(Zr);
  }
}
function durationFieldsToBigNano(e2, n2 = 6) {
  return givenFieldsToBigNano(e2, n2, F);
}
function nanoToDurationDayTimeFields(e2, n2 = 6) {
  const [t2, o2] = e2, r2 = nanoToGivenFields(o2, n2, F);
  if (r2[F[n2]] += t2 * (Qr / Xr[n2]), !Number.isFinite(r2[F[n2]])) {
    throw new RangeError(Cr);
  }
  return r2;
}
function nanoToDurationTimeFields(e2, n2 = 5) {
  return nanoToGivenFields(e2, n2, F);
}
function durationHasDateParts(e2) {
  return Boolean(computeDurationSign(e2, Pi));
}
function getMaxDurationUnit(e2) {
  let n2 = 9;
  for (; n2 > 0 && !e2[F[n2]]; n2--) {
  }
  return n2;
}
function createSplitTuple(e2, n2) {
  return [e2, n2];
}
function computePeriod(e2) {
  const n2 = Math.floor(e2 / Da) * Da;
  return [n2, n2 + Da];
}
function pe(e2) {
  const n2 = parseDateTimeLike(e2 = toStringViaPrimitive(e2));
  if (!n2) {
    throw new RangeError(failedParse(e2));
  }
  let t2;
  if (n2.m) {
    t2 = 0;
  } else {
    if (!n2.offset) {
      throw new RangeError(failedParse(e2));
    }
    t2 = parseOffsetNano(n2.offset);
  }
  return n2.timeZone && parseOffsetNanoMaybe(n2.timeZone, 1), _(isoToEpochNanoWithOffset(checkIsoDateTimeFields(n2), t2));
}
function Xt(e2) {
  const n2 = parseDateTimeLike(m(e2));
  if (!n2) {
    throw new RangeError(failedParse(e2));
  }
  if (n2.timeZone) {
    return finalizeZonedDateTime(n2, n2.offset ? parseOffsetNano(n2.offset) : void 0);
  }
  if (n2.m) {
    throw new RangeError(failedParse(e2));
  }
  return finalizeDate(n2);
}
function Mn(e2, n2) {
  const t2 = parseDateTimeLike(m(e2));
  if (!t2 || !t2.timeZone) {
    throw new RangeError(failedParse(e2));
  }
  const { offset: o2 } = t2, r2 = o2 ? parseOffsetNano(o2) : void 0, [, i2, a2] = wn(n2);
  return finalizeZonedDateTime(t2, r2, i2, a2);
}
function parseOffsetNano(e2) {
  const n2 = parseOffsetNanoMaybe(e2);
  if (void 0 === n2) {
    throw new RangeError(failedParse(e2));
  }
  return n2;
}
function Ct(e2) {
  const n2 = parseDateTimeLike(m(e2));
  if (!n2 || n2.m) {
    throw new RangeError(failedParse(e2));
  }
  return ee(finalizeDateTime(n2));
}
function At(e2) {
  const n2 = parseDateTimeLike(m(e2));
  if (!n2 || n2.m) {
    throw new RangeError(failedParse(e2));
  }
  return v(n2.p ? finalizeDateTime(n2) : finalizeDate(n2));
}
function ot(e2, n2) {
  const t2 = parseYearMonthOnly(m(n2));
  if (t2) {
    return requireIsoCalendar(t2), createPlainYearMonthSlots(checkIsoYearMonthInBounds(checkIsoDateFields(t2)));
  }
  const o2 = At(n2);
  return createPlainYearMonthSlots(moveToDayOfMonthUnsafe(e2(o2.calendar), o2));
}
function requireIsoCalendar(e2) {
  if (e2.calendar !== X) {
    throw new RangeError(invalidSubstring(e2.calendar));
  }
}
function Q(e2, n2) {
  const t2 = parseMonthDayOnly(m(n2));
  if (t2) {
    return requireIsoCalendar(t2), createPlainMonthDaySlots(checkIsoDateFields(t2));
  }
  const o2 = At(n2), { calendar: r2 } = o2, i2 = e2(r2), [a2, s2, c2] = i2.h(o2), [u2, l2] = i2.I(a2, s2), [f2, d2] = i2.N(u2, l2, c2);
  return createPlainMonthDaySlots(checkIsoDateInBounds(i2.P(f2, d2, c2)), r2);
}
function ze(e2) {
  let n2, t2 = ((e3) => {
    const n3 = Ca.exec(e3);
    return n3 ? (organizeAnnotationParts(n3[10]), organizeTimeParts(n3)) : void 0;
  })(m(e2));
  if (!t2) {
    if (t2 = parseDateTimeLike(e2), !t2) {
      throw new RangeError(failedParse(e2));
    }
    if (!t2.p) {
      throw new RangeError(failedParse(e2));
    }
    if (t2.m) {
      throw new RangeError(invalidSubstring("Z"));
    }
    requireIsoCalendar(t2);
  }
  if ((n2 = parseYearMonthOnly(e2)) && isIsoDateFieldsValid(n2)) {
    throw new RangeError(failedParse(e2));
  }
  if ((n2 = parseMonthDayOnly(e2)) && isIsoDateFieldsValid(n2)) {
    throw new RangeError(failedParse(e2));
  }
  return Ge(constrainIsoTimeFields(t2, 1));
}
function Kt(e2) {
  const n2 = ((e3) => {
    const n3 = za.exec(e3);
    return n3 ? ((e4) => {
      function parseUnit(e5, r3, i2) {
        let a2 = 0, s2 = 0;
        if (i2 && ([a2, o2] = divModFloor(o2, Xr[i2])), void 0 !== e5) {
          if (t2) {
            throw new RangeError(invalidSubstring(e5));
          }
          s2 = ((e6) => {
            const n5 = parseInt(e6);
            if (!Number.isFinite(n5)) {
              throw new RangeError(invalidSubstring(e6));
            }
            return n5;
          })(e5), n4 = 1, r3 && (o2 = parseSubsecNano(r3) * (Xr[i2] / _r), t2 = 1);
        }
        return a2 + s2;
      }
      let n4 = 0, t2 = 0, o2 = 0, r2 = {
        ...zipProps(F, [parseUnit(e4[2]), parseUnit(e4[3]), parseUnit(e4[4]), parseUnit(e4[5]), parseUnit(e4[6], e4[7], 5), parseUnit(e4[8], e4[9], 4), parseUnit(e4[10], e4[11], 3)]),
        ...nanoToGivenFields(o2, 2, F)
      };
      if (!n4) {
        throw new RangeError(noValidFields(F));
      }
      return parseSign(e4[1]) < 0 && (r2 = negateDurationFields(r2)), r2;
    })(n3) : void 0;
  })(m(e2));
  if (!n2) {
    throw new RangeError(failedParse(e2));
  }
  return Vt(checkDurationUnits(n2));
}
function sn(e2) {
  const n2 = parseDateTimeLike(e2) || parseYearMonthOnly(e2) || parseMonthDayOnly(e2);
  return n2 ? n2.calendar : e2;
}
function Ne(e2) {
  const n2 = parseDateTimeLike(e2);
  return n2 && (n2.timeZone || n2.m && Ta || n2.offset) || e2;
}
function finalizeZonedDateTime(e2, n2, t2 = 0, o2 = 0) {
  const r2 = ye(e2.timeZone), i2 = ie(r2);
  return Yn(getMatchingInstantFor(i2, checkIsoDateTimeFields(e2), n2, t2, o2, !i2.v, e2.m), r2, an(e2.calendar));
}
function finalizeDateTime(e2) {
  return resolveSlotsCalendar(checkIsoDateTimeInBounds(checkIsoDateTimeFields(e2)));
}
function finalizeDate(e2) {
  return resolveSlotsCalendar(checkIsoDateInBounds(checkIsoDateFields(e2)));
}
function resolveSlotsCalendar(e2) {
  return {
    ...e2,
    calendar: an(e2.calendar)
  };
}
function parseDateTimeLike(e2) {
  const n2 = Ya.exec(e2);
  return n2 ? ((e3) => {
    const n3 = e3[10], t2 = "Z" === (n3 || "").toUpperCase();
    return {
      isoYear: organizeIsoYearParts(e3),
      isoMonth: parseInt(e3[4]),
      isoDay: parseInt(e3[5]),
      ...organizeTimeParts(e3.slice(5)),
      ...organizeAnnotationParts(e3[16]),
      p: Boolean(e3[6]),
      m: t2,
      offset: t2 ? void 0 : n3
    };
  })(n2) : void 0;
}
function parseYearMonthOnly(e2) {
  const n2 = Ba.exec(e2);
  return n2 ? ((e3) => ({
    isoYear: organizeIsoYearParts(e3),
    isoMonth: parseInt(e3[4]),
    isoDay: 1,
    ...organizeAnnotationParts(e3[5])
  }))(n2) : void 0;
}
function parseMonthDayOnly(e2) {
  const n2 = ka.exec(e2);
  return n2 ? ((e3) => ({
    isoYear: ji,
    isoMonth: parseInt(e3[1]),
    isoDay: parseInt(e3[2]),
    ...organizeAnnotationParts(e3[3])
  }))(n2) : void 0;
}
function parseOffsetNanoMaybe(e2, n2) {
  const t2 = Za.exec(e2);
  return t2 ? ((e3, n3) => {
    const t3 = e3[4] || e3[5];
    if (n3 && t3) {
      throw new RangeError(invalidSubstring(t3));
    }
    return ae((parseInt0(e3[2]) * Kr + parseInt0(e3[3]) * Jr + parseInt0(e3[4]) * _r + parseSubsecNano(e3[5] || "")) * parseSign(e3[1]));
  })(t2, n2) : void 0;
}
function organizeIsoYearParts(e2) {
  const n2 = parseSign(e2[1]), t2 = parseInt(e2[2] || e2[3]);
  if (n2 < 0 && !t2) {
    throw new RangeError(invalidSubstring(-0));
  }
  return n2 * t2;
}
function organizeTimeParts(e2) {
  const n2 = parseInt0(e2[3]);
  return {
    ...nanoToIsoTimeAndDay(parseSubsecNano(e2[4] || ""))[0],
    isoHour: parseInt0(e2[1]),
    isoMinute: parseInt0(e2[2]),
    isoSecond: 60 === n2 ? 59 : n2
  };
}
function organizeAnnotationParts(e2) {
  let n2, t2;
  const o2 = [];
  if (e2.replace(Ra, (e3, r2, i2) => {
    const a2 = Boolean(r2), [s2, c2] = i2.split("=").reverse();
    if (c2) {
      if ("u-ca" === c2) {
        o2.push(s2), n2 || (n2 = a2);
      } else if (a2 || /[A-Z]/.test(c2)) {
        throw new RangeError(invalidSubstring(e3));
      }
    } else {
      if (t2) {
        throw new RangeError(invalidSubstring(e3));
      }
      t2 = s2;
    }
    return "";
  }), o2.length > 1 && n2) {
    throw new RangeError(invalidSubstring(e2));
  }
  return {
    timeZone: t2,
    calendar: o2[0] || X
  };
}
function parseSubsecNano(e2) {
  return parseInt(e2.padEnd(9, "0"));
}
function createRegExp(e2) {
  return new RegExp(`^${e2}$`, "i");
}
function parseSign(e2) {
  return e2 && "+" !== e2 ? -1 : 1;
}
function parseInt0(e2) {
  return void 0 === e2 ? 0 : parseInt(e2);
}
function Me(e2) {
  return ye(m(e2));
}
function ye(e2) {
  const n2 = getTimeZoneEssence(e2);
  return "number" == typeof n2 ? Fe(n2) : n2 ? ((e3) => {
    if (Ua.test(e3)) {
      throw new RangeError(br);
    }
    return e3.toLowerCase().split("/").map((e4, n3) => (e4.length <= 3 || /\d/.test(e4)) && !/etc|yap/.test(e4) ? e4.toUpperCase() : e4.replace(/baja|dumont|[a-z]+/g, (e5, t2) => e5.length <= 2 && !n3 || "in" === e5 || "chat" === e5 ? e5.toUpperCase() : e5.length > 2 || !t2 ? capitalize(e5).replace(/island|noronha|murdo|rivadavia|urville/, capitalize) : e5)).join("/");
  })(e2) : Ta;
}
function getTimeZoneAtomic(e2) {
  const n2 = getTimeZoneEssence(e2);
  return "number" == typeof n2 ? n2 : n2 ? n2.resolvedOptions().timeZone : Ta;
}
function getTimeZoneEssence(e2) {
  const n2 = parseOffsetNanoMaybe(e2 = e2.toUpperCase(), 1);
  return void 0 !== n2 ? n2 : e2 !== Ta ? qa(e2) : void 0;
}
function Ze(e2, n2) {
  return te(e2.epochNanoseconds, n2.epochNanoseconds);
}
function yn(e2, n2) {
  return te(e2.epochNanoseconds, n2.epochNanoseconds);
}
function $t(e2, n2, t2, o2, r2, i2) {
  const a2 = e2(normalizeOptions(i2).relativeTo), s2 = Math.max(getMaxDurationUnit(o2), getMaxDurationUnit(r2));
  if (allPropsEqual(F, o2, r2)) {
    return 0;
  }
  if (isUniformUnit(s2, a2)) {
    return te(durationFieldsToBigNano(o2), durationFieldsToBigNano(r2));
  }
  if (!a2) {
    throw new RangeError(zr);
  }
  const [c2, u2, l2] = createMarkerSystem(n2, t2, a2), f2 = createMarkerToEpochNano(l2), d2 = createMoveMarker(l2);
  return te(f2(d2(u2, c2, o2)), f2(d2(u2, c2, r2)));
}
function gt(e2, n2) {
  return rt(e2, n2) || He(e2, n2);
}
function rt(e2, n2) {
  return compareNumbers(isoToEpochMilli(e2), isoToEpochMilli(n2));
}
function He(e2, n2) {
  return compareNumbers(isoTimeFieldsToNano(e2), isoTimeFieldsToNano(n2));
}
function ue(e2, n2) {
  return !Ze(e2, n2);
}
function gn(e2, n2) {
  return !yn(e2, n2) && !!je(e2.timeZone, n2.timeZone) && isIdLikeEqual(e2.calendar, n2.calendar);
}
function ft(e2, n2) {
  return !gt(e2, n2) && isIdLikeEqual(e2.calendar, n2.calendar);
}
function It(e2, n2) {
  return !rt(e2, n2) && isIdLikeEqual(e2.calendar, n2.calendar);
}
function $e(e2, n2) {
  return !rt(e2, n2) && isIdLikeEqual(e2.calendar, n2.calendar);
}
function x(e2, n2) {
  return !rt(e2, n2) && isIdLikeEqual(e2.calendar, n2.calendar);
}
function Ve(e2, n2) {
  return !He(e2, n2);
}
function je(e2, n2) {
  if (e2 === n2) {
    return 1;
  }
  const t2 = I(e2), o2 = I(n2);
  if (t2 === o2) {
    return 1;
  }
  try {
    return getTimeZoneAtomic(t2) === getTimeZoneAtomic(o2);
  } catch (e3) {
  }
}
function le(e2, n2, t2, o2) {
  const r2 = refineDiffOptions(e2, U(o2), 3, 5), i2 = diffEpochNanos(n2.epochNanoseconds, t2.epochNanoseconds, ...r2);
  return Vt(e2 ? negateDurationFields(i2) : i2);
}
function Dn(e2, n2, t2, o2, r2, i2) {
  const a2 = getCommonCalendarSlot(o2.calendar, r2.calendar), s2 = U(i2), [c2, u2, l2, f2] = refineDiffOptions(t2, s2, 5), d2 = o2.epochNanoseconds, m2 = r2.epochNanoseconds, p2 = te(m2, d2);
  let h2;
  if (p2) {
    if (c2 < 6) {
      h2 = diffEpochNanos(d2, m2, c2, u2, l2, f2);
    } else {
      const t3 = n2(((e3, n3) => {
        if (!je(e3, n3)) {
          throw new RangeError(Fr);
        }
        return e3;
      })(o2.timeZone, r2.timeZone)), i3 = e2(a2);
      h2 = diffZonedEpochsBig(i3, t3, o2, r2, p2, c2, s2), h2 = roundRelativeDuration(h2, m2, c2, u2, l2, f2, i3, o2, extractEpochNano, E(moveZonedEpochs, t3));
    }
  } else {
    h2 = Si;
  }
  return Vt(t2 ? negateDurationFields(h2) : h2);
}
function ut(e2, n2, t2, o2, r2) {
  const i2 = getCommonCalendarSlot(t2.calendar, o2.calendar), a2 = U(r2), [s2, c2, u2, l2] = refineDiffOptions(n2, a2, 6), f2 = isoToEpochNano(t2), d2 = isoToEpochNano(o2), m2 = te(d2, f2);
  let p2;
  if (m2) {
    if (s2 <= 6) {
      p2 = diffEpochNanos(f2, d2, s2, c2, u2, l2);
    } else {
      const n3 = e2(i2);
      p2 = diffDateTimesBig(n3, t2, o2, m2, s2, a2), p2 = roundRelativeDuration(p2, d2, s2, c2, u2, l2, n3, t2, isoToEpochNano, moveDateTime);
    }
  } else {
    p2 = Si;
  }
  return Vt(n2 ? negateDurationFields(p2) : p2);
}
function Ft(e2, n2, t2, o2, r2) {
  const i2 = getCommonCalendarSlot(t2.calendar, o2.calendar), a2 = U(r2);
  return diffDateLike(n2, () => e2(i2), t2, o2, ...refineDiffOptions(n2, a2, 6, 9, 6), a2);
}
function Xe(e2, n2, t2, o2, r2) {
  const i2 = getCommonCalendarSlot(t2.calendar, o2.calendar), a2 = U(r2), s2 = refineDiffOptions(n2, a2, 9, 9, 8), c2 = e2(i2);
  return diffDateLike(n2, () => c2, moveToDayOfMonthUnsafe(c2, t2), moveToDayOfMonthUnsafe(c2, o2), ...s2, a2);
}
function diffDateLike(e2, n2, t2, o2, r2, i2, a2, s2, c2) {
  const u2 = isoToEpochNano(t2), l2 = isoToEpochNano(o2);
  let f2;
  if (te(l2, u2)) {
    if (6 === r2) {
      f2 = diffEpochNanos(u2, l2, r2, i2, a2, s2);
    } else {
      const e3 = n2();
      f2 = e3.dateUntil(t2, o2, r2, c2), 6 === i2 && 1 === a2 || (f2 = roundRelativeDuration(f2, l2, r2, i2, a2, s2, e3, t2, isoToEpochNano, moveDate));
    }
  } else {
    f2 = Si;
  }
  return Vt(e2 ? negateDurationFields(f2) : f2);
}
function Ae(e2, n2, t2, o2) {
  const r2 = U(o2), [i2, a2, s2, c2] = refineDiffOptions(e2, r2, 5, 5), u2 = roundByInc(diffTimes(n2, t2), computeNanoInc(a2, s2), c2), l2 = {
    ...Si,
    ...nanoToDurationTimeFields(u2, i2)
  };
  return Vt(e2 ? negateDurationFields(l2) : l2);
}
function diffZonedEpochsExact(e2, n2, t2, o2, r2, i2) {
  const a2 = te(o2.epochNanoseconds, t2.epochNanoseconds);
  return a2 ? r2 < 6 ? diffEpochNanosExact(t2.epochNanoseconds, o2.epochNanoseconds, r2) : diffZonedEpochsBig(n2, e2, t2, o2, a2, r2, i2) : Si;
}
function diffDateTimesExact(e2, n2, t2, o2, r2) {
  const i2 = isoToEpochNano(n2), a2 = isoToEpochNano(t2), s2 = te(a2, i2);
  return s2 ? o2 <= 6 ? diffEpochNanosExact(i2, a2, o2) : diffDateTimesBig(e2, n2, t2, s2, o2, r2) : Si;
}
function diffZonedEpochsBig(e2, n2, t2, o2, r2, i2, a2) {
  const [s2, c2, u2] = ((e3, n3, t3, o3) => {
    function updateMid() {
      return l3 = {
        ...moveByDays(a3, c3++ * -o3),
        ...i3
      }, f3 = we(e3, l3), te(s3, f3) === -o3;
    }
    const r3 = fn(n3, e3), i3 = Vn(j, r3), a3 = fn(t3, e3), s3 = t3.epochNanoseconds;
    let c3 = 0;
    const u3 = diffTimes(r3, a3);
    let l3, f3;
    if (Math.sign(u3) === -o3 && c3++, updateMid() && (-1 === o3 || updateMid())) {
      throw new RangeError(vr);
    }
    const d2 = oe(re(f3, s3));
    return [r3, l3, d2];
  })(n2, t2, o2, r2);
  var l2, f2;
  return {
    ...6 === i2 ? (l2 = s2, f2 = c2, {
      ...Si,
      days: diffDays(l2, f2)
    }) : e2.dateUntil(s2, c2, i2, a2),
    ...nanoToDurationTimeFields(u2)
  };
}
function diffDateTimesBig(e2, n2, t2, o2, r2, i2) {
  const [a2, s2, c2] = ((e3, n3, t3) => {
    let o3 = n3, r3 = diffTimes(e3, n3);
    return Math.sign(r3) === -t3 && (o3 = moveByDays(n3, -t3), r3 += Qr * t3), [e3, o3, r3];
  })(n2, t2, o2);
  return {
    ...e2.dateUntil(a2, s2, r2, i2),
    ...nanoToDurationTimeFields(c2)
  };
}
function diffEpochNanos(e2, n2, t2, o2, r2, i2) {
  return {
    ...Si,
    ...nanoToDurationDayTimeFields(roundBigNano(re(e2, n2), o2, r2, i2), t2)
  };
}
function diffEpochNanosExact(e2, n2, t2) {
  return {
    ...Si,
    ...nanoToDurationDayTimeFields(re(e2, n2), t2)
  };
}
function diffDays(e2, n2) {
  return diffEpochMilliByDay(isoToEpochMilli(e2), isoToEpochMilli(n2));
}
function diffEpochMilliByDay(e2, n2) {
  return Math.trunc((n2 - e2) / Gr);
}
function diffTimes(e2, n2) {
  return isoTimeFieldsToNano(n2) - isoTimeFieldsToNano(e2);
}
function getCommonCalendarSlot(e2, n2) {
  if (!isIdLikeEqual(e2, n2)) {
    throw new RangeError(Er);
  }
  return e2;
}
function createIntlCalendar(e2) {
  function epochMilliToIntlFields(e3) {
    return ((e4, n3) => ({
      ...parseIntlYear(e4, n3),
      F: e4.month,
      day: parseInt(e4.day)
    }))(hashIntlFormatParts(n2, e3), t2);
  }
  const n2 = La(e2), t2 = computeCalendarIdBase(e2);
  return {
    id: e2,
    O: createIntlFieldCache(epochMilliToIntlFields),
    B: createIntlYearDataCache(epochMilliToIntlFields)
  };
}
function createIntlFieldCache(e2) {
  return Jn((n2) => {
    const t2 = isoToEpochMilli(n2);
    return e2(t2);
  }, WeakMap);
}
function createIntlYearDataCache(e2) {
  const n2 = e2(0).year - Wi;
  return Jn((t2) => {
    let o2, r2 = isoArgsToEpochMilli(t2 - n2);
    const i2 = [], a2 = [];
    do {
      r2 += 400 * Gr;
    } while ((o2 = e2(r2)).year <= t2);
    do {
      r2 += (1 - o2.day) * Gr, o2.year === t2 && (i2.push(r2), a2.push(o2.F)), r2 -= Gr;
    } while ((o2 = e2(r2)).year >= t2);
    return {
      k: i2.reverse(),
      C: Wr(a2.reverse())
    };
  });
}
function parseIntlYear(e2, n2) {
  let t2, o2, r2 = parseIntlPartsYear(e2);
  if (e2.era) {
    const i2 = Di[n2];
    void 0 !== i2 && (t2 = "islamic" === n2 ? "ah" : e2.era.normalize("NFD").toLowerCase().replace(/[^a-z0-9]/g, ""), "bc" === t2 || "b" === t2 ? t2 = "bce" : "ad" !== t2 && "a" !== t2 || (t2 = "ce"), o2 = r2, r2 = eraYearToYear(o2, i2[t2] || 0));
  }
  return {
    era: t2,
    eraYear: o2,
    year: r2
  };
}
function parseIntlPartsYear(e2) {
  return parseInt(e2.relatedYear || e2.year);
}
function computeIntlDateParts(e2) {
  const { year: n2, F: t2, day: o2 } = this.O(e2), { C: r2 } = this.B(n2);
  return [n2, r2[t2] + 1, o2];
}
function computeIntlEpochMilli(e2, n2 = 1, t2 = 1) {
  return this.B(e2).k[n2 - 1] + (t2 - 1) * Gr;
}
function computeIntlLeapMonth(e2) {
  const n2 = queryMonthStrings(this, e2), t2 = queryMonthStrings(this, e2 - 1), o2 = n2.length;
  if (o2 > t2.length) {
    const e3 = getCalendarLeapMonthMeta(this);
    if (e3 < 0) {
      return -e3;
    }
    for (let e4 = 0; e4 < o2; e4++) {
      if (n2[e4] !== t2[e4]) {
        return e4 + 1;
      }
    }
  }
}
function computeIntlDaysInYear(e2) {
  return diffEpochMilliByDay(computeIntlEpochMilli.call(this, e2), computeIntlEpochMilli.call(this, e2 + 1));
}
function computeIntlDaysInMonth(e2, n2) {
  const { k: t2 } = this.B(e2);
  let o2 = n2 + 1, r2 = t2;
  return o2 > t2.length && (o2 = 1, r2 = this.B(e2 + 1).k), diffEpochMilliByDay(t2[n2 - 1], r2[o2 - 1]);
}
function computeIntlMonthsInYear(e2) {
  return this.B(e2).k.length;
}
function queryMonthStrings(e2, n2) {
  return Object.keys(e2.B(n2).C);
}
function rn(e2) {
  return an(m(e2));
}
function an(e2) {
  if ((e2 = e2.toLowerCase()) !== X && e2 !== gi && computeCalendarIdBase(e2) !== computeCalendarIdBase(La(e2).resolvedOptions().calendar)) {
    throw new RangeError(invalidCalendar(e2));
  }
  return e2;
}
function computeCalendarIdBase(e2) {
  return "islamicc" === e2 && (e2 = "islamic"), e2.split("-")[0];
}
function computeNativeWeekOfYear(e2) {
  return this.R(e2)[0];
}
function computeNativeYearOfWeek(e2) {
  return this.R(e2)[1];
}
function computeNativeDayOfYear(e2) {
  const [n2] = this.h(e2);
  return diffEpochMilliByDay(this.q(n2), isoToEpochMilli(e2)) + 1;
}
function parseMonthCode(e2) {
  const n2 = Wa.exec(e2);
  if (!n2) {
    throw new RangeError(invalidMonthCode(e2));
  }
  return [parseInt(n2[1]), Boolean(n2[2])];
}
function monthCodeNumberToMonth(e2, n2, t2) {
  return e2 + (n2 || t2 && e2 >= t2 ? 1 : 0);
}
function monthToMonthCodeNumber(e2, n2) {
  return e2 - (n2 && e2 >= n2 ? 1 : 0);
}
function eraYearToYear(e2, n2) {
  return (n2 + e2) * (Math.sign(n2) || 1) || 0;
}
function getCalendarEraOrigins(e2) {
  return Di[getCalendarIdBase(e2)];
}
function getCalendarLeapMonthMeta(e2) {
  return Ii[getCalendarIdBase(e2)];
}
function getCalendarIdBase(e2) {
  return computeCalendarIdBase(e2.id || X);
}
function Qt(e2, n2, t2, o2) {
  const r2 = refineCalendarFields(t2, o2, en, [], ri);
  if (void 0 !== r2.timeZone) {
    const o3 = t2.dateFromFields(r2), i2 = refineTimeBag(r2), a2 = e2(r2.timeZone);
    return {
      epochNanoseconds: getMatchingInstantFor(n2(a2), {
        ...o3,
        ...i2
      }, void 0 !== r2.offset ? parseOffsetNano(r2.offset) : void 0),
      timeZone: a2
    };
  }
  return {
    ...t2.dateFromFields(r2),
    ...Dt
  };
}
function jn(e2, n2, t2, o2, r2, i2) {
  const a2 = refineCalendarFields(t2, r2, en, ti, ri), s2 = e2(a2.timeZone), [c2, u2, l2] = wn(i2), f2 = t2.dateFromFields(a2, overrideOverflowOptions(i2, c2)), d2 = refineTimeBag(a2, c2);
  return Yn(getMatchingInstantFor(n2(s2), {
    ...f2,
    ...d2
  }, void 0 !== a2.offset ? parseOffsetNano(a2.offset) : void 0, u2, l2), s2, o2);
}
function Pt(e2, n2, t2) {
  const o2 = refineCalendarFields(e2, n2, en, [], w), r2 = H(t2);
  return ee(checkIsoDateTimeInBounds({
    ...e2.dateFromFields(o2, overrideOverflowOptions(t2, r2)),
    ...refineTimeBag(o2, r2)
  }));
}
function Yt(e2, n2, t2, o2 = []) {
  const r2 = refineCalendarFields(e2, n2, en, o2);
  return e2.dateFromFields(r2, t2);
}
function nt(e2, n2, t2, o2) {
  const r2 = refineCalendarFields(e2, n2, fi, o2);
  return e2.yearMonthFromFields(r2, t2);
}
function K(e2, n2, t2, o2, r2 = []) {
  const i2 = refineCalendarFields(e2, t2, en, r2);
  return n2 && void 0 !== i2.month && void 0 === i2.monthCode && void 0 === i2.year && (i2.year = ji), e2.monthDayFromFields(i2, o2);
}
function Ue(e2, n2) {
  const t2 = H(n2);
  return Ge(refineTimeBag(refineFields(e2, ei, [], 1), t2));
}
function Ht(e2) {
  const n2 = refineFields(e2, Ni);
  return Vt(checkDurationUnits({
    ...Si,
    ...n2
  }));
}
function refineCalendarFields(e2, n2, t2, o2 = [], r2 = []) {
  return refineFields(n2, [...e2.fields(t2), ...r2].sort(), o2);
}
function refineFields(e2, n2, t2, o2 = !t2) {
  const r2 = {};
  let i2, a2 = 0;
  for (const o3 of n2) {
    if (o3 === i2) {
      throw new RangeError(duplicateFields(o3));
    }
    if ("constructor" === o3 || "__proto__" === o3) {
      throw new RangeError(tn(o3));
    }
    let n3 = e2[o3];
    if (void 0 !== n3) {
      a2 = 1, Ga[o3] && (n3 = Ga[o3](n3, o3)), r2[o3] = n3;
    } else if (t2) {
      if (t2.includes(o3)) {
        throw new TypeError(missingField(o3));
      }
      r2[o3] = hi[o3];
    }
    i2 = o3;
  }
  if (o2 && !a2) {
    throw new TypeError(noValidFields(n2));
  }
  return r2;
}
function refineTimeBag(e2, n2) {
  return constrainIsoTimeFields(Ha({
    ...hi,
    ...e2
  }), n2);
}
function Sn(e2, n2, t2, o2, r2, i2) {
  const a2 = U(i2), { calendar: s2, timeZone: c2 } = t2;
  return Yn(((e3, n3, t3, o3, r3) => {
    const i3 = mergeCalendarFields(e3, t3, o3, en, oi, ni), [a3, s3, c3] = wn(r3, 2);
    return getMatchingInstantFor(n3, {
      ...e3.dateFromFields(i3, overrideOverflowOptions(r3, a3)),
      ...refineTimeBag(i3, a3)
    }, parseOffsetNano(i3.offset), s3, c3);
  })(e2(s2), n2(c2), o2, r2, a2), c2, s2);
}
function at(e2, n2, t2, o2, r2) {
  const i2 = U(r2);
  return ee(((e3, n3, t3, o3) => {
    const r3 = mergeCalendarFields(e3, n3, t3, en, w), i3 = H(o3);
    return checkIsoDateTimeInBounds({
      ...e3.dateFromFields(r3, overrideOverflowOptions(o3, i3)),
      ...refineTimeBag(r3, i3)
    });
  })(e2(n2.calendar), t2, o2, i2));
}
function Zt(e2, n2, t2, o2, r2) {
  const i2 = U(r2);
  return ((e3, n3, t3, o3) => {
    const r3 = mergeCalendarFields(e3, n3, t3, en);
    return e3.dateFromFields(r3, o3);
  })(e2(n2.calendar), t2, o2, i2);
}
function Ke(e2, n2, t2, o2, r2) {
  const i2 = U(r2);
  return createPlainYearMonthSlots(((e3, n3, t3, o3) => {
    const r3 = mergeCalendarFields(e3, n3, t3, fi);
    return e3.yearMonthFromFields(r3, o3);
  })(e2(n2.calendar), t2, o2, i2));
}
function k(e2, n2, t2, o2, r2) {
  const i2 = U(r2);
  return ((e3, n3, t3, o3) => {
    const r3 = mergeCalendarFields(e3, n3, t3, en);
    return e3.monthDayFromFields(r3, o3);
  })(e2(n2.calendar), t2, o2, i2);
}
function Be(e2, n2, t2) {
  return Ge(((e3, n3, t3) => {
    const o2 = H(t3);
    return refineTimeBag({
      ...Vn(ei, e3),
      ...refineFields(n3, ei)
    }, o2);
  })(e2, n2, t2));
}
function kt(e2, n2) {
  return Vt((t2 = e2, o2 = n2, checkDurationUnits({
    ...t2,
    ...refineFields(o2, Ni)
  })));
  var t2, o2;
}
function mergeCalendarFields(e2, n2, t2, o2, r2 = [], i2 = []) {
  const a2 = [...e2.fields(o2), ...r2].sort();
  let s2 = refineFields(n2, a2, i2);
  const c2 = refineFields(t2, a2);
  return s2 = e2.mergeFields(s2, c2), refineFields(s2, a2, []);
}
function convertToPlainMonthDay(e2, n2) {
  const t2 = refineCalendarFields(e2, n2, pi);
  return e2.monthDayFromFields(t2);
}
function convertToPlainYearMonth(e2, n2, t2) {
  const o2 = refineCalendarFields(e2, n2, di);
  return e2.yearMonthFromFields(o2, t2);
}
function convertToIso(e2, n2, t2, o2, r2) {
  n2 = Vn(t2 = e2.fields(t2), n2), o2 = refineFields(o2, r2 = e2.fields(r2), []);
  let i2 = e2.mergeFields(n2, o2);
  return i2 = refineFields(i2, [...t2, ...r2].sort(), []), e2.dateFromFields(i2);
}
function refineYear(e2, n2) {
  let { era: t2, eraYear: o2, year: r2 } = n2;
  const i2 = getCalendarEraOrigins(e2);
  if (void 0 !== t2 || void 0 !== o2) {
    if (void 0 === t2 || void 0 === o2) {
      throw new TypeError(Dr);
    }
    if (!i2) {
      throw new RangeError(gr);
    }
    const e3 = i2[t2];
    if (void 0 === e3) {
      throw new RangeError(invalidEra(t2));
    }
    const n3 = eraYearToYear(o2, e3);
    if (void 0 !== r2 && r2 !== n3) {
      throw new RangeError(Ir);
    }
    r2 = n3;
  } else if (void 0 === r2) {
    throw new TypeError(missingYear(i2));
  }
  return r2;
}
function refineMonth(e2, n2, t2, o2) {
  let { month: r2, monthCode: i2 } = n2;
  if (void 0 !== i2) {
    const n3 = ((e3, n4, t3, o3) => {
      const r3 = e3.U(t3), [i3, a2] = parseMonthCode(n4);
      let s2 = monthCodeNumberToMonth(i3, a2, r3);
      if (a2) {
        const n5 = getCalendarLeapMonthMeta(e3);
        if (void 0 === n5) {
          throw new RangeError(Pr);
        }
        if (n5 > 0) {
          if (s2 > n5) {
            throw new RangeError(Pr);
          }
          if (void 0 === r3) {
            if (1 === o3) {
              throw new RangeError(Pr);
            }
            s2--;
          }
        } else {
          if (s2 !== -n5) {
            throw new RangeError(Pr);
          }
          if (void 0 === r3 && 1 === o3) {
            throw new RangeError(Pr);
          }
        }
      }
      return s2;
    })(e2, i2, t2, o2);
    if (void 0 !== r2 && r2 !== n3) {
      throw new RangeError(Mr);
    }
    r2 = n3, o2 = 1;
  } else if (void 0 === r2) {
    throw new TypeError(Nr);
  }
  return clampEntity("month", r2, 1, e2.L(t2), o2);
}
function refineDay(e2, n2, t2, o2, r2) {
  return clampProp(n2, "day", 1, e2.j(o2, t2), r2);
}
function spliceFields(e2, n2, t2, o2) {
  let r2 = 0;
  const i2 = [];
  for (const e3 of t2) {
    void 0 !== n2[e3] ? r2 = 1 : i2.push(e3);
  }
  if (Object.assign(e2, n2), r2) {
    for (const n3 of o2 || i2) {
      delete e2[n3];
    }
  }
}
function Se(e2) {
  return _(checkEpochNanoInBounds(bigIntToBigNano(toBigInt(e2))));
}
function vn(e2, n2, t2, o2, r2 = X) {
  return Yn(checkEpochNanoInBounds(bigIntToBigNano(toBigInt(t2))), n2(o2), e2(r2));
}
function pt(e2, n2, t2, o2, r2 = 0, i2 = 0, a2 = 0, s2 = 0, c2 = 0, u2 = 0, l2 = X) {
  return ee(checkIsoDateTimeInBounds(checkIsoDateTimeFields(T(toInteger, zipProps(wi, [n2, t2, o2, r2, i2, a2, s2, c2, u2])))), e2(l2));
}
function Nt(e2, n2, t2, o2, r2 = X) {
  return v(checkIsoDateInBounds(checkIsoDateFields(T(toInteger, {
    isoYear: n2,
    isoMonth: t2,
    isoDay: o2
  }))), e2(r2));
}
function tt(e2, n2, t2, o2 = X, r2 = 1) {
  const i2 = toInteger(n2), a2 = toInteger(t2), s2 = e2(o2);
  return createPlainYearMonthSlots(checkIsoYearMonthInBounds(checkIsoDateFields({
    isoYear: i2,
    isoMonth: a2,
    isoDay: toInteger(r2)
  })), s2);
}
function G(e2, n2, t2, o2 = X, r2 = ji) {
  const i2 = toInteger(n2), a2 = toInteger(t2), s2 = e2(o2);
  return createPlainMonthDaySlots(checkIsoDateInBounds(checkIsoDateFields({
    isoYear: toInteger(r2),
    isoMonth: i2,
    isoDay: a2
  })), s2);
}
function ke(e2 = 0, n2 = 0, t2 = 0, o2 = 0, r2 = 0, i2 = 0) {
  return Ge(constrainIsoTimeFields(T(toInteger, zipProps(j, [e2, n2, t2, o2, r2, i2])), 1));
}
function Lt(e2 = 0, n2 = 0, t2 = 0, o2 = 0, r2 = 0, i2 = 0, a2 = 0, s2 = 0, c2 = 0, u2 = 0) {
  return Vt(checkDurationUnits(T(toStrictInteger, zipProps(F, [e2, n2, t2, o2, r2, i2, a2, s2, c2, u2]))));
}
function fe(e2, n2, t2 = X) {
  return Yn(e2.epochNanoseconds, n2, t2);
}
function Zn(e2) {
  return _(e2.epochNanoseconds);
}
function ht(e2, n2) {
  return ee(fn(n2, e2));
}
function Bt(e2, n2) {
  return v(fn(n2, e2));
}
function bn(e2, n2, t2) {
  return convertToPlainYearMonth(e2(n2.calendar), t2);
}
function Fn(e2, n2, t2) {
  return convertToPlainMonthDay(e2(n2.calendar), t2);
}
function Re(e2, n2) {
  return Ge(fn(n2, e2));
}
function mt(e2, n2, t2, o2) {
  const r2 = ((e3, n3, t3, o3) => {
    const r3 = ve(o3);
    return we(e3(n3), t3, r3);
  })(e2, t2, n2, o2);
  return Yn(checkEpochNanoInBounds(r2), t2, n2.calendar);
}
function St(e2, n2, t2) {
  const o2 = e2(n2.calendar);
  return createPlainYearMonthSlots({
    ...n2,
    ...convertToPlainYearMonth(o2, t2)
  });
}
function Ot(e2, n2, t2) {
  return convertToPlainMonthDay(e2(n2.calendar), t2);
}
function vt(e2, n2, t2, o2, r2) {
  const i2 = e2(r2.timeZone), a2 = r2.plainTime, s2 = void 0 !== a2 ? n2(a2) : Dt;
  return Yn(we(t2(i2), {
    ...o2,
    ...s2
  }), i2, o2.calendar);
}
function wt(e2, n2 = Dt) {
  return ee(checkIsoDateTimeInBounds({
    ...e2,
    ...n2
  }));
}
function jt(e2, n2, t2) {
  return convertToPlainYearMonth(e2(n2.calendar), t2);
}
function Mt(e2, n2, t2) {
  return convertToPlainMonthDay(e2(n2.calendar), t2);
}
function _e(e2, n2, t2, o2) {
  return ((e3, n3, t3) => convertToIso(e3, n3, di, de(t3), li))(e2(n2.calendar), t2, o2);
}
function R(e2, n2, t2, o2) {
  return ((e3, n3, t3) => convertToIso(e3, n3, pi, de(t3), si))(e2(n2.calendar), t2, o2);
}
function Je(e2, n2, t2, o2, r2) {
  const i2 = de(r2), a2 = n2(i2.plainDate), s2 = e2(i2.timeZone);
  return Yn(we(t2(s2), {
    ...a2,
    ...o2
  }), s2, a2.calendar);
}
function Le(e2, n2) {
  return ee(checkIsoDateTimeInBounds({
    ...e2,
    ...n2
  }));
}
function De(e2) {
  return _(checkEpochNanoInBounds(he(e2, _r)));
}
function Pe(e2) {
  return _(checkEpochNanoInBounds(he(e2, be)));
}
function Ce(e2) {
  return _(checkEpochNanoInBounds(bigIntToBigNano(toBigInt(e2), Vr)));
}
function ge(e2) {
  return _(checkEpochNanoInBounds(bigIntToBigNano(toBigInt(e2))));
}
function pn(e2, n2, t2 = Dt) {
  const o2 = n2.timeZone, r2 = e2(o2), i2 = {
    ...fn(n2, r2),
    ...t2
  };
  return Yn(getMatchingInstantFor(r2, i2, i2.offsetNanoseconds, 2), o2, n2.calendar);
}
function Tn(e2, n2, t2) {
  const o2 = n2.timeZone, r2 = e2(o2), i2 = {
    ...fn(n2, r2),
    ...t2
  }, a2 = getPreferredCalendarSlot(n2.calendar, t2.calendar);
  return Yn(getMatchingInstantFor(r2, i2, i2.offsetNanoseconds, 2), o2, a2);
}
function lt(e2, n2 = Dt) {
  return ee({
    ...e2,
    ...n2
  });
}
function st(e2, n2) {
  return ee({
    ...e2,
    ...n2
  }, getPreferredCalendarSlot(e2.calendar, n2.calendar));
}
function it(e2, n2) {
  return {
    ...e2,
    calendar: n2
  };
}
function On(e2, n2) {
  return {
    ...e2,
    timeZone: n2
  };
}
function getPreferredCalendarSlot(e2, n2) {
  if (e2 === n2) {
    return e2;
  }
  const t2 = I(e2), o2 = I(n2);
  if (t2 === o2 || t2 === X) {
    return n2;
  }
  if (o2 === X) {
    return e2;
  }
  throw new RangeError(Er);
}
function createNativeOpsCreator(e2, n2) {
  return (t2) => t2 === X ? e2 : t2 === gi || t2 === Ti ? Object.assign(Object.create(e2), {
    id: t2
  }) : Object.assign(Object.create(n2), Aa(t2));
}
function createOptionsTransformer(e2, n2, t2) {
  const o2 = new Set(t2);
  return (r2) => (((e3, n3) => {
    for (const t3 of n3) {
      if (t3 in e3) {
        return 1;
      }
    }
    return 0;
  })(r2 = V(o2, r2), e2) || Object.assign(r2, n2), t2 && (r2.timeZone = Ta, ["full", "long"].includes(r2.timeStyle) && (r2.timeStyle = "medium")), r2);
}
function e(e2, n2 = qn) {
  const [t2, , , o2] = e2;
  return (r2, i2 = Ns, ...a2) => {
    const s2 = n2(o2 && o2(...a2), r2, i2, t2), c2 = s2.resolvedOptions();
    return [s2, ...toEpochMillis(e2, c2, a2)];
  };
}
function qn(e2, n2, t2, o2) {
  if (t2 = o2(t2), e2) {
    if (void 0 !== t2.timeZone) {
      throw new TypeError(Lr);
    }
    t2.timeZone = e2;
  }
  return new En(n2, t2);
}
function toEpochMillis(e2, n2, t2) {
  const [, o2, r2] = e2;
  return t2.map((e3) => (e3.calendar && ((e4, n3, t3) => {
    if ((t3 || e4 !== X) && e4 !== n3) {
      throw new RangeError(Er);
    }
  })(I(e3.calendar), n2.calendar, r2), o2(e3, n2)));
}
function An(e2) {
  const n2 = Bn();
  return Ie(n2, e2.getOffsetNanosecondsFor(n2));
}
function Bn() {
  return he(Date.now(), be);
}
function Nn() {
  return ys || (ys = new En().resolvedOptions().timeZone);
}
var expectedInteger = (e2, n2) => `Non-integer ${e2}: ${n2}`;
var expectedPositive = (e2, n2) => `Non-positive ${e2}: ${n2}`;
var expectedFinite = (e2, n2) => `Non-finite ${e2}: ${n2}`;
var forbiddenBigIntToNumber = (e2) => `Cannot convert bigint to ${e2}`;
var invalidBigInt = (e2) => `Invalid bigint: ${e2}`;
var pr = "Cannot convert Symbol to string";
var hr = "Invalid object";
var numberOutOfRange = (e2, n2, t2, o2, r2) => r2 ? numberOutOfRange(e2, r2[n2], r2[t2], r2[o2]) : invalidEntity(e2, n2) + `; must be between ${t2}-${o2}`;
var invalidEntity = (e2, n2) => `Invalid ${e2}: ${n2}`;
var missingField = (e2) => `Missing ${e2}`;
var tn = (e2) => `Invalid field ${e2}`;
var duplicateFields = (e2) => `Duplicate field ${e2}`;
var noValidFields = (e2) => "No valid fields: " + e2.join();
var Z = "Invalid bag";
var invalidChoice = (e2, n2, t2) => invalidEntity(e2, n2) + "; must be " + Object.keys(t2).join();
var A = "Cannot use valueOf";
var P = "Invalid calling context";
var gr = "Forbidden era/eraYear";
var Dr = "Mismatching era/eraYear";
var Ir = "Mismatching year/eraYear";
var invalidEra = (e2) => `Invalid era: ${e2}`;
var missingYear = (e2) => "Missing year" + (e2 ? "/era/eraYear" : "");
var invalidMonthCode = (e2) => `Invalid monthCode: ${e2}`;
var Mr = "Mismatching month/monthCode";
var Nr = "Missing month/monthCode";
var yr = "Cannot guess year";
var Pr = "Invalid leap month";
var g = "Invalid protocol";
var vr = "Invalid protocol results";
var Er = "Mismatching Calendars";
var invalidCalendar = (e2) => `Invalid Calendar: ${e2}`;
var Fr = "Mismatching TimeZones";
var br = "Forbidden ICU TimeZone";
var wr = "Out-of-bounds offset";
var Br = "Out-of-bounds TimeZone gap";
var kr = "Invalid TimeZone offset";
var Yr = "Ambiguous offset";
var Cr = "Out-of-bounds date";
var Zr = "Out-of-bounds duration";
var Rr = "Cannot mix duration signs";
var zr = "Missing relativeTo";
var qr = "Cannot use large units";
var Ur = "Required smallestUnit or largestUnit";
var Ar = "smallestUnit > largestUnit";
var failedParse = (e2) => `Cannot parse: ${e2}`;
var invalidSubstring = (e2) => `Invalid substring: ${e2}`;
var Ln = (e2) => `Cannot format ${e2}`;
var kn = "Mismatching types for formatting";
var Lr = "Cannot specify TimeZone";
var Wr = /* @__PURE__ */ E(b, (e2, n2) => n2);
var jr = /* @__PURE__ */ E(b, (e2, n2, t2) => t2);
var xr = /* @__PURE__ */ E(padNumber, 2);
var $r = {
  nanosecond: 0,
  microsecond: 1,
  millisecond: 2,
  second: 3,
  minute: 4,
  hour: 5,
  day: 6,
  week: 7,
  month: 8,
  year: 9
};
var Et = /* @__PURE__ */ Object.keys($r);
var Gr = 864e5;
var Hr = 1e3;
var Vr = 1e3;
var be = 1e6;
var _r = 1e9;
var Jr = 6e10;
var Kr = 36e11;
var Qr = 864e11;
var Xr = [1, Vr, be, _r, Jr, Kr, Qr];
var w = /* @__PURE__ */ Et.slice(0, 6);
var ei = /* @__PURE__ */ sortStrings(w);
var ni = ["offset"];
var ti = ["timeZone"];
var oi = /* @__PURE__ */ w.concat(ni);
var ri = /* @__PURE__ */ oi.concat(ti);
var ii = ["era", "eraYear"];
var ai = /* @__PURE__ */ ii.concat(["year"]);
var si = ["year"];
var ci = ["monthCode"];
var ui = /* @__PURE__ */ ["month"].concat(ci);
var li = ["day"];
var fi = /* @__PURE__ */ ui.concat(si);
var di = /* @__PURE__ */ ci.concat(si);
var en = /* @__PURE__ */ li.concat(fi);
var mi = /* @__PURE__ */ li.concat(ui);
var pi = /* @__PURE__ */ li.concat(ci);
var hi = /* @__PURE__ */ jr(w, 0);
var X = "iso8601";
var gi = "gregory";
var Ti = "japanese";
var Di = {
  [gi]: {
    bce: -1,
    ce: 0
  },
  [Ti]: {
    bce: -1,
    ce: 0,
    meiji: 1867,
    taisho: 1911,
    showa: 1925,
    heisei: 1988,
    reiwa: 2018
  },
  ethioaa: {
    era0: 0
  },
  ethiopic: {
    era0: 0,
    era1: 5500
  },
  coptic: {
    era0: -1,
    era1: 0
  },
  roc: {
    beforeroc: -1,
    minguo: 0
  },
  buddhist: {
    be: 0
  },
  islamic: {
    ah: 0
  },
  indian: {
    saka: 0
  },
  persian: {
    ap: 0
  }
};
var Ii = {
  chinese: 13,
  dangi: 13,
  hebrew: -6
};
var m = /* @__PURE__ */ E(requireType, "string");
var f = /* @__PURE__ */ E(requireType, "boolean");
var Mi = /* @__PURE__ */ E(requireType, "number");
var $ = /* @__PURE__ */ E(requireType, "function");
var F = /* @__PURE__ */ Et.map((e2) => e2 + "s");
var Ni = /* @__PURE__ */ sortStrings(F);
var yi = /* @__PURE__ */ F.slice(0, 6);
var Pi = /* @__PURE__ */ F.slice(6);
var vi = /* @__PURE__ */ Pi.slice(1);
var Ei = /* @__PURE__ */ Wr(F);
var Si = /* @__PURE__ */ jr(F, 0);
var Fi = /* @__PURE__ */ jr(yi, 0);
var bi = /* @__PURE__ */ E(zeroOutProps, F);
var j = ["isoNanosecond", "isoMicrosecond", "isoMillisecond", "isoSecond", "isoMinute", "isoHour"];
var Oi = ["isoDay", "isoMonth", "isoYear"];
var wi = /* @__PURE__ */ j.concat(Oi);
var Bi = /* @__PURE__ */ sortStrings(Oi);
var ki = /* @__PURE__ */ sortStrings(j);
var Yi = /* @__PURE__ */ sortStrings(wi);
var Dt = /* @__PURE__ */ jr(ki, 0);
var Ci = /* @__PURE__ */ E(zeroOutProps, wi);
var En = Intl.DateTimeFormat;
var Zi = "en-GB";
var Ri = 1e8;
var zi = Ri * Gr;
var qi = [Ri, 0];
var Ui = [-Ri, 0];
var Ai = 275760;
var Li = -271821;
var Wi = 1970;
var ji = 1972;
var xi = 12;
var $i = /* @__PURE__ */ isoArgsToEpochMilli(1868, 9, 8);
var Gi = /* @__PURE__ */ Jn(computeJapaneseEraParts, WeakMap);
var Hi = "smallestUnit";
var Vi = "unit";
var _i = "roundingIncrement";
var Ji = "fractionalSecondDigits";
var Ki = "relativeTo";
var Qi = {
  constrain: 0,
  reject: 1
};
var Xi = /* @__PURE__ */ Object.keys(Qi);
var ea = {
  compatible: 0,
  reject: 1,
  earlier: 2,
  later: 3
};
var na = {
  reject: 0,
  use: 1,
  prefer: 2,
  ignore: 3
};
var ta = {
  auto: 0,
  never: 1,
  critical: 2,
  always: 3
};
var oa = {
  auto: 0,
  never: 1,
  critical: 2
};
var ra = {
  auto: 0,
  never: 1
};
var ia = {
  floor: 0,
  halfFloor: 1,
  ceil: 2,
  halfCeil: 3,
  trunc: 4,
  halfTrunc: 5,
  expand: 6,
  halfExpand: 7,
  halfEven: 8
};
var aa = /* @__PURE__ */ E(refineUnitOption, Hi);
var sa = /* @__PURE__ */ E(refineUnitOption, "largestUnit");
var ca = /* @__PURE__ */ E(refineUnitOption, Vi);
var ua = /* @__PURE__ */ E(refineChoiceOption, "overflow", Qi);
var la = /* @__PURE__ */ E(refineChoiceOption, "disambiguation", ea);
var fa = /* @__PURE__ */ E(refineChoiceOption, "offset", na);
var da = /* @__PURE__ */ E(refineChoiceOption, "calendarName", ta);
var ma = /* @__PURE__ */ E(refineChoiceOption, "timeZoneName", oa);
var pa = /* @__PURE__ */ E(refineChoiceOption, "offset", ra);
var ha = /* @__PURE__ */ E(refineChoiceOption, "roundingMode", ia);
var L = "PlainYearMonth";
var q = "PlainMonthDay";
var J = "PlainDate";
var We = "PlainDateTime";
var xe = "PlainTime";
var Te = "ZonedDateTime";
var Oe = "Instant";
var qt = "Duration";
var ga = [Math.floor, (e2) => hasHalf(e2) ? Math.floor(e2) : Math.round(e2), Math.ceil, (e2) => hasHalf(e2) ? Math.ceil(e2) : Math.round(e2), Math.trunc, (e2) => hasHalf(e2) ? Math.trunc(e2) || 0 : Math.round(e2), (e2) => e2 < 0 ? Math.floor(e2) : Math.ceil(e2), (e2) => Math.sign(e2) * Math.round(Math.abs(e2)) || 0, (e2) => hasHalf(e2) ? (e2 = Math.trunc(e2) || 0) + e2 % 2 : Math.round(e2)];
var Ta = "UTC";
var Da = 5184e3;
var Ia = /* @__PURE__ */ isoArgsToEpochSec(1847);
var Ma = /* @__PURE__ */ isoArgsToEpochSec(/* @__PURE__ */ (/* @__PURE__ */ new Date()).getUTCFullYear() + 10);
var Na = /0+$/;
var fn = /* @__PURE__ */ Jn(_zonedEpochSlotsToIso, WeakMap);
var ya = 2 ** 32 - 1;
var ie = /* @__PURE__ */ Jn((e2) => {
  const n2 = getTimeZoneEssence(e2);
  return "object" == typeof n2 ? new IntlTimeZone(n2) : new FixedTimeZone(n2 || 0);
});
var FixedTimeZone = class {
  constructor(e2) {
    this.v = e2;
  }
  getOffsetNanosecondsFor() {
    return this.v;
  }
  getPossibleInstantsFor(e2) {
    return [isoToEpochNanoWithOffset(e2, this.v)];
  }
  l() {
  }
};
var IntlTimeZone = class {
  constructor(e2) {
    this.$ = ((e3) => {
      function getOffsetSec(e4) {
        const i2 = clampNumber(e4, o2, r2), [a2, s2] = computePeriod(i2), c2 = n2(a2), u2 = n2(s2);
        return c2 === u2 ? c2 : pinch(t2(a2, s2), c2, u2, e4);
      }
      function pinch(n3, t3, o3, r3) {
        let i2, a2;
        for (; (void 0 === r3 || void 0 === (i2 = r3 < n3[0] ? t3 : r3 >= n3[1] ? o3 : void 0)) && (a2 = n3[1] - n3[0]); ) {
          const t4 = n3[0] + Math.floor(a2 / 2);
          e3(t4) === o3 ? n3[1] = t4 : n3[0] = t4 + 1;
        }
        return i2;
      }
      const n2 = Jn(e3), t2 = Jn(createSplitTuple);
      let o2 = Ia, r2 = Ma;
      return {
        G(e4) {
          const n3 = getOffsetSec(e4 - 86400), t3 = getOffsetSec(e4 + 86400), o3 = e4 - n3, r3 = e4 - t3;
          if (n3 === t3) {
            return [o3];
          }
          const i2 = getOffsetSec(o3);
          return i2 === getOffsetSec(r3) ? [e4 - i2] : n3 > t3 ? [o3, r3] : [];
        },
        V: getOffsetSec,
        l(e4, i2) {
          const a2 = clampNumber(e4, o2, r2);
          let [s2, c2] = computePeriod(a2);
          const u2 = Da * i2, l2 = i2 < 0 ? () => c2 > o2 || (o2 = a2, 0) : () => s2 < r2 || (r2 = a2, 0);
          for (; l2(); ) {
            const o3 = n2(s2), r3 = n2(c2);
            if (o3 !== r3) {
              const n3 = t2(s2, c2);
              pinch(n3, o3, r3);
              const a3 = n3[0];
              if ((compareNumbers(a3, e4) || 1) === i2) {
                return a3;
              }
            }
            s2 += u2, c2 += u2;
          }
        }
      };
    })(/* @__PURE__ */ ((e3) => (n2) => {
      const t2 = hashIntlFormatParts(e3, n2 * Hr);
      return isoArgsToEpochSec(parseIntlPartsYear(t2), parseInt(t2.month), parseInt(t2.day), parseInt(t2.hour), parseInt(t2.minute), parseInt(t2.second)) - n2;
    })(e2));
  }
  getOffsetNanosecondsFor(e2) {
    return this.$.V(epochNanoToSec(e2)) * _r;
  }
  getPossibleInstantsFor(e2) {
    const [n2, t2] = [isoArgsToEpochSec((o2 = e2).isoYear, o2.isoMonth, o2.isoDay, o2.isoHour, o2.isoMinute, o2.isoSecond), o2.isoMillisecond * be + o2.isoMicrosecond * Vr + o2.isoNanosecond];
    var o2;
    return this.$.G(n2).map((e3) => checkEpochNanoInBounds(moveBigNano(he(e3, _r), t2)));
  }
  l(e2, n2) {
    const [t2, o2] = epochNanoToSecMod(e2), r2 = this.$.l(t2 + (n2 > 0 || o2 ? 1 : 0), n2);
    if (void 0 !== r2) {
      return he(r2, _r);
    }
  }
};
var Pa = "([+\u2212-])";
var va = "(?:[.,](\\d{1,9}))?";
var Ea = `(?:(?:${Pa}(\\d{6}))|(\\d{4}))-?(\\d{2})`;
var Sa = "(\\d{2})(?::?(\\d{2})(?::?(\\d{2})" + va + ")?)?";
var Fa = Pa + Sa;
var ba = Ea + "-?(\\d{2})(?:[T ]" + Sa + "(Z|" + Fa + ")?)?";
var Oa = "\\[(!?)([^\\]]*)\\]";
var wa = `((?:${Oa}){0,9})`;
var Ba = /* @__PURE__ */ createRegExp(Ea + wa);
var ka = /* @__PURE__ */ createRegExp("(?:--)?(\\d{2})-?(\\d{2})" + wa);
var Ya = /* @__PURE__ */ createRegExp(ba + wa);
var Ca = /* @__PURE__ */ createRegExp("T?" + Sa + "(?:" + Fa + ")?" + wa);
var Za = /* @__PURE__ */ createRegExp(Fa);
var Ra = /* @__PURE__ */ new RegExp(Oa, "g");
var za = /* @__PURE__ */ createRegExp(`${Pa}?P(\\d+Y)?(\\d+M)?(\\d+W)?(\\d+D)?(?:T(?:(\\d+)${va}H)?(?:(\\d+)${va}M)?(?:(\\d+)${va}S)?)?`);
var qa = /* @__PURE__ */ Jn((e2) => new En(Zi, {
  timeZone: e2,
  era: "short",
  year: "numeric",
  month: "numeric",
  day: "numeric",
  hour: "numeric",
  minute: "numeric",
  second: "numeric"
}));
var Ua = /^(AC|AE|AG|AR|AS|BE|BS|CA|CN|CS|CT|EA|EC|IE|IS|JS|MI|NE|NS|PL|PN|PR|PS|SS|VS)T$/;
var Aa = /* @__PURE__ */ Jn(createIntlCalendar);
var La = /* @__PURE__ */ Jn((e2) => new En(Zi, {
  calendar: e2,
  timeZone: Ta,
  era: "short",
  year: "numeric",
  month: "short",
  day: "numeric"
}));
var Wa = /^M(\d{2})(L?)$/;
var ja = {
  era: toStringViaPrimitive,
  eraYear: toInteger,
  year: toInteger,
  month: toPositiveInteger,
  monthCode: toStringViaPrimitive,
  day: toPositiveInteger
};
var xa = /* @__PURE__ */ jr(w, toInteger);
var $a = /* @__PURE__ */ jr(F, toStrictInteger);
var Ga = /* @__PURE__ */ Object.assign({}, ja, xa, $a, {
  offset: toStringViaPrimitive
});
var Ha = /* @__PURE__ */ E(remapProps, w, j);
var Va = {
  dateAdd(e2, n2, t2) {
    const o2 = H(t2);
    let r2, { years: i2, months: a2, weeks: s2, days: c2 } = n2;
    if (c2 += durationFieldsToBigNano(n2, 5)[0], i2 || a2) {
      r2 = ((e3, n3, t3, o3, r3) => {
        let [i3, a3, s3] = e3.h(n3);
        if (t3) {
          const [n4, o4] = e3.I(i3, a3);
          i3 += t3, a3 = monthCodeNumberToMonth(n4, o4, e3.U(i3)), a3 = clampEntity("month", a3, 1, e3.L(i3), r3);
        }
        return o3 && ([i3, a3] = e3._(i3, a3, o3)), s3 = clampEntity("day", s3, 1, e3.j(i3, a3), r3), e3.q(i3, a3, s3);
      })(this, e2, i2, a2, o2);
    } else {
      if (!s2 && !c2) {
        return e2;
      }
      r2 = isoToEpochMilli(e2);
    }
    return r2 += (7 * s2 + c2) * Gr, checkIsoDateInBounds(epochMilliToIso(r2));
  },
  dateUntil(e2, n2, t2) {
    if (t2 <= 7) {
      let o3 = 0, r3 = diffDays({
        ...e2,
        ...Dt
      }, {
        ...n2,
        ...Dt
      });
      return 7 === t2 && ([o3, r3] = divModTrunc(r3, 7)), {
        ...Si,
        weeks: o3,
        days: r3
      };
    }
    const o2 = this.h(e2), r2 = this.h(n2);
    let [i2, a2, s2] = ((e3, n3, t3, o3, r3, i3, a3) => {
      let s3 = r3 - n3, c2 = i3 - t3, u2 = a3 - o3;
      if (s3 || c2) {
        const l2 = Math.sign(s3 || c2);
        let f2 = e3.j(r3, i3), d2 = 0;
        if (Math.sign(u2) === -l2) {
          const o4 = f2;
          [r3, i3] = e3._(r3, i3, -l2), s3 = r3 - n3, c2 = i3 - t3, f2 = e3.j(r3, i3), d2 = l2 < 0 ? -o4 : f2;
        }
        if (u2 = a3 - Math.min(o3, f2) + d2, s3) {
          const [o4, a4] = e3.I(n3, t3), [u3, f3] = e3.I(r3, i3);
          if (c2 = u3 - o4 || Number(f3) - Number(a4), Math.sign(c2) === -l2) {
            const t4 = l2 < 0 && -e3.L(r3);
            s3 = (r3 -= l2) - n3, c2 = i3 - monthCodeNumberToMonth(o4, a4, e3.U(r3)) + (t4 || e3.L(r3));
          }
        }
      }
      return [s3, c2, u2];
    })(this, ...o2, ...r2);
    return 8 === t2 && (a2 += this.J(i2, o2[0]), i2 = 0), {
      ...Si,
      years: i2,
      months: a2,
      days: s2
    };
  },
  dateFromFields(e2, n2) {
    const t2 = H(n2), o2 = refineYear(this, e2), r2 = refineMonth(this, e2, o2, t2), i2 = refineDay(this, e2, r2, o2, t2);
    return v(checkIsoDateInBounds(this.P(o2, r2, i2)), this.id || X);
  },
  yearMonthFromFields(e2, n2) {
    const t2 = H(n2), o2 = refineYear(this, e2), r2 = refineMonth(this, e2, o2, t2);
    return createPlainYearMonthSlots(checkIsoYearMonthInBounds(this.P(o2, r2, 1)), this.id || X);
  },
  monthDayFromFields(e2, n2) {
    const t2 = H(n2), o2 = !this.id, { monthCode: r2, year: i2, month: a2 } = e2;
    let s2, c2, u2, l2, f2;
    if (void 0 !== r2) {
      [s2, c2] = parseMonthCode(r2), f2 = getDefinedProp(e2, "day");
      const n3 = this.N(s2, c2, f2);
      if (!n3) {
        throw new RangeError(yr);
      }
      if ([u2, l2] = n3, void 0 !== a2 && a2 !== l2) {
        throw new RangeError(Mr);
      }
      o2 && (l2 = clampEntity("month", l2, 1, xi, 1), f2 = clampEntity("day", f2, 1, computeIsoDaysInMonth(void 0 !== i2 ? i2 : u2, l2), t2));
    } else {
      u2 = void 0 === i2 && o2 ? ji : refineYear(this, e2), l2 = refineMonth(this, e2, u2, t2), f2 = refineDay(this, e2, l2, u2, t2);
      const n3 = this.U(u2);
      c2 = l2 === n3, s2 = monthToMonthCodeNumber(l2, n3);
      const r3 = this.N(s2, c2, f2);
      if (!r3) {
        throw new RangeError(yr);
      }
      [u2, l2] = r3;
    }
    return createPlainMonthDaySlots(checkIsoDateInBounds(this.P(u2, l2, f2)), this.id || X);
  },
  fields(e2) {
    return getCalendarEraOrigins(this) && e2.includes("year") ? [...e2, ...ii] : e2;
  },
  mergeFields(e2, n2) {
    const t2 = Object.assign(/* @__PURE__ */ Object.create(null), e2);
    return spliceFields(t2, n2, ui), getCalendarEraOrigins(this) && (spliceFields(t2, n2, ai), this.id === Ti && spliceFields(t2, n2, mi, ii)), t2;
  },
  inLeapYear(e2) {
    const [n2] = this.h(e2);
    return this.K(n2);
  },
  monthsInYear(e2) {
    const [n2] = this.h(e2);
    return this.L(n2);
  },
  daysInMonth(e2) {
    const [n2, t2] = this.h(e2);
    return this.j(n2, t2);
  },
  daysInYear(e2) {
    const [n2] = this.h(e2);
    return this.X(n2);
  },
  dayOfYear: computeNativeDayOfYear,
  era(e2) {
    return this.ee(e2)[0];
  },
  eraYear(e2) {
    return this.ee(e2)[1];
  },
  monthCode(e2) {
    const [n2, t2] = this.h(e2), [o2, r2] = this.I(n2, t2);
    return ((e3, n3) => "M" + xr(e3) + (n3 ? "L" : ""))(o2, r2);
  },
  dayOfWeek: computeIsoDayOfWeek,
  daysInWeek() {
    return 7;
  }
};
var _a = {
  dayOfYear: computeNativeDayOfYear,
  h: computeIsoDateParts,
  q: isoArgsToEpochMilli
};
var Ja = /* @__PURE__ */ Object.assign({}, _a, {
  weekOfYear: computeNativeWeekOfYear,
  yearOfWeek: computeNativeYearOfWeek,
  R(e2) {
    function computeWeekShift(e3) {
      return (7 - e3 < n2 ? 7 : 0) - e3;
    }
    function computeWeeksInYear(e3) {
      const n3 = computeIsoDaysInYear(l2 + e3), t3 = e3 || 1, o3 = computeWeekShift(modFloor(a2 + n3 * t3, 7));
      return c2 = (n3 + (o3 - s2) * t3) / 7;
    }
    const n2 = this.id ? 1 : 4, t2 = computeIsoDayOfWeek(e2), o2 = this.dayOfYear(e2), r2 = modFloor(t2 - 1, 7), i2 = o2 - 1, a2 = modFloor(r2 - i2, 7), s2 = computeWeekShift(a2);
    let c2, u2 = Math.floor((i2 - s2) / 7) + 1, l2 = e2.isoYear;
    return u2 ? u2 > computeWeeksInYear(0) && (u2 = 1, l2++) : (u2 = computeWeeksInYear(-1), l2--), [u2, l2, c2];
  }
});
var Ka = {
  dayOfYear: computeNativeDayOfYear,
  h: computeIntlDateParts,
  q: computeIntlEpochMilli,
  weekOfYear: computeNativeWeekOfYear,
  yearOfWeek: computeNativeYearOfWeek,
  R() {
    return [];
  }
};
var Y = /* @__PURE__ */ createNativeOpsCreator(/* @__PURE__ */ Object.assign({}, Va, Ja, {
  h: computeIsoDateParts,
  ee(e2) {
    return this.id === gi ? computeGregoryEraParts(e2) : this.id === Ti ? Gi(e2) : [];
  },
  I: (e2, n2) => [n2, 0],
  N(e2, n2) {
    if (!n2) {
      return [ji, e2];
    }
  },
  K: computeIsoInLeapYear,
  U() {
  },
  L: computeIsoMonthsInYear,
  J: (e2) => e2 * xi,
  j: computeIsoDaysInMonth,
  X: computeIsoDaysInYear,
  P: (e2, n2, t2) => ({
    isoYear: e2,
    isoMonth: n2,
    isoDay: t2
  }),
  q: isoArgsToEpochMilli,
  _: (e2, n2, t2) => (e2 += divTrunc(t2, xi), (n2 += modTrunc(t2, xi)) < 1 ? (e2--, n2 += xi) : n2 > xi && (e2++, n2 -= xi), [e2, n2]),
  year(e2) {
    return e2.isoYear;
  },
  month(e2) {
    return e2.isoMonth;
  },
  day: (e2) => e2.isoDay
}), /* @__PURE__ */ Object.assign({}, Va, Ka, {
  h: computeIntlDateParts,
  ee(e2) {
    const n2 = this.O(e2);
    return [n2.era, n2.eraYear];
  },
  I(e2, n2) {
    const t2 = computeIntlLeapMonth.call(this, e2);
    return [monthToMonthCodeNumber(n2, t2), t2 === n2];
  },
  N(e2, n2, t2) {
    let [o2, r2, i2] = computeIntlDateParts.call(this, {
      isoYear: ji,
      isoMonth: xi,
      isoDay: 31
    });
    const a2 = computeIntlLeapMonth.call(this, o2), s2 = r2 === a2;
    1 === (compareNumbers(e2, monthToMonthCodeNumber(r2, a2)) || compareNumbers(Number(n2), Number(s2)) || compareNumbers(t2, i2)) && o2--;
    for (let r3 = 0; r3 < 100; r3++) {
      const i3 = o2 - r3, a3 = computeIntlLeapMonth.call(this, i3), s3 = monthCodeNumberToMonth(e2, n2, a3);
      if (n2 === (s3 === a3) && t2 <= computeIntlDaysInMonth.call(this, i3, s3)) {
        return [i3, s3];
      }
    }
  },
  K(e2) {
    const n2 = computeIntlDaysInYear.call(this, e2);
    return n2 > computeIntlDaysInYear.call(this, e2 - 1) && n2 > computeIntlDaysInYear.call(this, e2 + 1);
  },
  U: computeIntlLeapMonth,
  L: computeIntlMonthsInYear,
  J(e2, n2) {
    const t2 = n2 + e2, o2 = Math.sign(e2), r2 = o2 < 0 ? -1 : 0;
    let i2 = 0;
    for (let e3 = n2; e3 !== t2; e3 += o2) {
      i2 += computeIntlMonthsInYear.call(this, e3 + r2);
    }
    return i2;
  },
  j: computeIntlDaysInMonth,
  X: computeIntlDaysInYear,
  P(e2, n2, t2) {
    return epochMilliToIso(computeIntlEpochMilli.call(this, e2, n2, t2));
  },
  q: computeIntlEpochMilli,
  _(e2, n2, t2) {
    if (t2) {
      if (n2 += t2, !Number.isSafeInteger(n2)) {
        throw new RangeError(Cr);
      }
      if (t2 < 0) {
        for (; n2 < 1; ) {
          n2 += computeIntlMonthsInYear.call(this, --e2);
        }
      } else {
        let t3;
        for (; n2 > (t3 = computeIntlMonthsInYear.call(this, e2)); ) {
          n2 -= t3, e2++;
        }
      }
    }
    return [e2, n2];
  },
  year(e2) {
    return this.O(e2).year;
  },
  month(e2) {
    const { year: n2, F: t2 } = this.O(e2), { C: o2 } = this.B(n2);
    return o2[t2] + 1;
  },
  day(e2) {
    return this.O(e2).day;
  }
}));
var Qa = "numeric";
var Xa = ["timeZoneName"];
var es = {
  month: Qa,
  day: Qa
};
var ns = {
  year: Qa,
  month: Qa
};
var ts = /* @__PURE__ */ Object.assign({}, ns, {
  day: Qa
});
var os = {
  hour: Qa,
  minute: Qa,
  second: Qa
};
var rs = /* @__PURE__ */ Object.assign({}, ts, os);
var is = /* @__PURE__ */ Object.assign({}, rs, {
  timeZoneName: "short"
});
var as = /* @__PURE__ */ Object.keys(ns);
var ss = /* @__PURE__ */ Object.keys(es);
var cs = /* @__PURE__ */ Object.keys(ts);
var us = /* @__PURE__ */ Object.keys(os);
var ls = ["dateStyle"];
var fs = /* @__PURE__ */ as.concat(ls);
var ds = /* @__PURE__ */ ss.concat(ls);
var ms = /* @__PURE__ */ cs.concat(ls, ["weekday"]);
var ps = /* @__PURE__ */ us.concat(["dayPeriod", "timeStyle"]);
var hs = /* @__PURE__ */ ms.concat(ps);
var gs = /* @__PURE__ */ hs.concat(Xa);
var Ts = /* @__PURE__ */ Xa.concat(ps);
var Ds = /* @__PURE__ */ Xa.concat(ms);
var Is = /* @__PURE__ */ Xa.concat(["day", "weekday"], ps);
var Ms = /* @__PURE__ */ Xa.concat(["year", "weekday"], ps);
var Ns = {};
var t = [/* @__PURE__ */ createOptionsTransformer(hs, rs), y];
var s = [/* @__PURE__ */ createOptionsTransformer(gs, is), y, 0, (e2, n2) => {
  const t2 = I(e2.timeZone);
  if (n2 && I(n2.timeZone) !== t2) {
    throw new RangeError(Fr);
  }
  return t2;
}];
var n = [/* @__PURE__ */ createOptionsTransformer(hs, rs, Xa), isoToEpochMilli];
var o = [/* @__PURE__ */ createOptionsTransformer(ms, ts, Ts), isoToEpochMilli];
var r = [/* @__PURE__ */ createOptionsTransformer(ps, os, Ds), (e2) => isoTimeFieldsToNano(e2) / be];
var a = [/* @__PURE__ */ createOptionsTransformer(fs, ns, Is), isoToEpochMilli, 1];
var i = [/* @__PURE__ */ createOptionsTransformer(ds, es, Ms), isoToEpochMilli, 1];
var ys;

// node_modules/temporal-polyfill/chunks/classApi.js
function createSlotClass(e2, t2, n2, o2, r2) {
  function Class(...e3) {
    if (!(this instanceof Class)) {
      throw new TypeError(P);
    }
    oo(this, t2(...e3));
  }
  function bindMethod(e3, t3) {
    return Object.defineProperties(function(...t4) {
      return e3.call(this, getSpecificSlots(this), ...t4);
    }, D(t3));
  }
  function getSpecificSlots(t3) {
    const n3 = no(t3);
    if (!n3 || n3.branding !== e2) {
      throw new TypeError(P);
    }
    return n3;
  }
  return Object.defineProperties(Class.prototype, {
    ...O(T(bindMethod, n2)),
    ...p(T(bindMethod, o2)),
    ...h("Temporal." + e2)
  }), Object.defineProperties(Class, {
    ...p(r2),
    ...D(e2)
  }), [Class, (e3) => {
    const t3 = Object.create(Class.prototype);
    return oo(t3, e3), t3;
  }, getSpecificSlots];
}
function createProtocolValidator(e2) {
  return e2 = e2.concat("id").sort(), (t2) => {
    if (!C(t2, e2)) {
      throw new TypeError(g);
    }
    return t2;
  };
}
function rejectInvalidBag(e2) {
  if (no(e2) || void 0 !== e2.calendar || void 0 !== e2.timeZone) {
    throw new TypeError(Z);
  }
  return e2;
}
function createCalendarFieldMethods(e2, t2) {
  const n2 = {};
  for (const o2 in e2) {
    n2[o2] = ({ o: e3 }, n3) => {
      const r2 = no(n3) || {}, { branding: a2 } = r2, i2 = a2 === J || t2.includes(a2) ? r2 : toPlainDateSlots(n3);
      return e3[o2](i2);
    };
  }
  return n2;
}
function createCalendarGetters(e2) {
  const t2 = {};
  for (const n2 in e2) {
    t2[n2] = (e3) => {
      const { calendar: t3 } = e3;
      return (o2 = t3, "string" == typeof o2 ? Y(o2) : (r2 = o2, Object.assign(Object.create(co), {
        i: r2
      })))[n2](e3);
      var o2, r2;
    };
  }
  return t2;
}
function neverValueOf() {
  throw new TypeError(A);
}
function createCalendarFromSlots({ calendar: e2 }) {
  return "string" == typeof e2 ? new lr(e2) : e2;
}
function toPlainMonthDaySlots(e2, t2) {
  if (t2 = U(t2), z(e2)) {
    const n3 = no(e2);
    if (n3 && n3.branding === q) {
      return H(t2), n3;
    }
    const o2 = extractCalendarSlotFromBag(e2);
    return K(Qo(o2 || X), !o2, e2, t2);
  }
  const n2 = Q(Y, e2);
  return H(t2), n2;
}
function getOffsetNanosecondsForAdapter(e2, t2, n2) {
  return o2 = t2.call(e2, Co(_(n2))), ae(u(o2));
  var o2;
}
function createAdapterOps(e2, t2 = ho) {
  const n2 = Object.keys(t2).sort(), o2 = {};
  for (const r2 of n2) {
    o2[r2] = E(t2[r2], e2, $(e2[r2]));
  }
  return o2;
}
function createTimeZoneOps(e2, t2) {
  return "string" == typeof e2 ? ie(e2) : createAdapterOps(e2, t2);
}
function createTimeZoneOffsetOps(e2) {
  return createTimeZoneOps(e2, Do);
}
function toInstantSlots(e2) {
  if (z(e2)) {
    const t2 = no(e2);
    if (t2) {
      switch (t2.branding) {
        case Oe:
          return t2;
        case Te:
          return _(t2.epochNanoseconds);
      }
    }
  }
  return pe(e2);
}
function toTemporalInstant() {
  return Co(_(he(this.valueOf(), be)));
}
function getImplTransition(e2, t2, n2) {
  const o2 = t2.l(toInstantSlots(n2).epochNanoseconds, e2);
  return o2 ? Co(_(o2)) : null;
}
function refineTimeZoneSlot(e2) {
  return z(e2) ? (no(e2) || {}).timeZone || Fo(e2) : ((e3) => ye(Ne(m(e3))))(e2);
}
function toPlainTimeSlots(e2, t2) {
  if (z(e2)) {
    const n2 = no(e2) || {};
    switch (n2.branding) {
      case xe:
        return H(t2), n2;
      case We:
        return H(t2), Ge(n2);
      case Te:
        return H(t2), Re(createTimeZoneOffsetOps, n2);
    }
    return Ue(e2, t2);
  }
  return H(t2), ze(e2);
}
function optionalToPlainTimeFields(e2) {
  return void 0 === e2 ? void 0 : toPlainTimeSlots(e2);
}
function toPlainYearMonthSlots(e2, t2) {
  if (t2 = U(t2), z(e2)) {
    const n3 = no(e2);
    return n3 && n3.branding === L ? (H(t2), n3) : nt(Ho(getCalendarSlotFromBag(e2)), e2, t2);
  }
  const n2 = ot(Y, e2);
  return H(t2), n2;
}
function toPlainDateTimeSlots(e2, t2) {
  if (t2 = U(t2), z(e2)) {
    const n3 = no(e2) || {};
    switch (n3.branding) {
      case We:
        return H(t2), n3;
      case J:
        return H(t2), ee({
          ...n3,
          ...Dt
        });
      case Te:
        return H(t2), ht(createTimeZoneOffsetOps, n3);
    }
    return Pt(Ko(getCalendarSlotFromBag(e2)), e2, t2);
  }
  const n2 = Ct(e2);
  return H(t2), n2;
}
function toPlainDateSlots(e2, t2) {
  if (t2 = U(t2), z(e2)) {
    const n3 = no(e2) || {};
    switch (n3.branding) {
      case J:
        return H(t2), n3;
      case We:
        return H(t2), v(n3);
      case Te:
        return H(t2), Bt(createTimeZoneOffsetOps, n3);
    }
    return Yt(Ko(getCalendarSlotFromBag(e2)), e2, t2);
  }
  const n2 = At(e2);
  return H(t2), n2;
}
function dayAdapter(e2, t2, n2) {
  return d(t2.call(e2, Yo(v(n2, e2))));
}
function createCompoundOpsCreator(e2) {
  return (t2) => "string" == typeof t2 ? Y(t2) : ((e3, t3) => {
    const n2 = Object.keys(t3).sort(), o2 = {};
    for (const r2 of n2) {
      o2[r2] = E(t3[r2], e3, e3[r2]);
    }
    return o2;
  })(t2, e2);
}
function toDurationSlots(e2) {
  if (z(e2)) {
    const t2 = no(e2);
    return t2 && t2.branding === qt ? t2 : Ht(e2);
  }
  return Kt(e2);
}
function refinePublicRelativeTo(e2) {
  if (void 0 !== e2) {
    if (z(e2)) {
      const t2 = no(e2) || {};
      switch (t2.branding) {
        case Te:
        case J:
          return t2;
        case We:
          return v(t2);
      }
      const n2 = getCalendarSlotFromBag(e2);
      return {
        ...Qt(refineTimeZoneSlot, createTimeZoneOps, Ko(n2), e2),
        calendar: n2
      };
    }
    return Xt(e2);
  }
}
function getCalendarSlotFromBag(e2) {
  return extractCalendarSlotFromBag(e2) || X;
}
function extractCalendarSlotFromBag(e2) {
  const { calendar: t2 } = e2;
  if (void 0 !== t2) {
    return refineCalendarSlot(t2);
  }
}
function refineCalendarSlot(e2) {
  return z(e2) ? (no(e2) || {}).calendar || cr(e2) : ((e3) => an(sn(m(e3))))(e2);
}
function toZonedDateTimeSlots(e2, t2) {
  if (t2 = U(t2), z(e2)) {
    const n2 = no(e2);
    if (n2 && n2.branding === Te) {
      return wn(t2), n2;
    }
    const o2 = getCalendarSlotFromBag(e2);
    return jn(refineTimeZoneSlot, createTimeZoneOps, Ko(o2), o2, e2, t2);
  }
  return Mn(e2, t2);
}
function adaptDateMethods(e2) {
  return T((e3) => (t2) => e3(slotsToIso(t2)), e2);
}
function slotsToIso(e2) {
  return fn(e2, createTimeZoneOffsetOps);
}
function createDateTimeFormatClass() {
  const e2 = En.prototype, t2 = Object.getOwnPropertyDescriptors(e2), n2 = Object.getOwnPropertyDescriptors(En), DateTimeFormat = function(e3, t3 = {}) {
    if (!(this instanceof DateTimeFormat)) {
      return new DateTimeFormat(e3, t3);
    }
    Or.set(this, ((e4, t4 = {}) => {
      const n3 = new En(e4, t4), o2 = n3.resolvedOptions(), r2 = o2.locale, a2 = Vn(Object.keys(t4), o2), i2 = Jn(createFormatPrepperForBranding), prepFormat = (...e5) => {
        let t5;
        const o3 = e5.map((e6, n4) => {
          const o4 = no(e6), r3 = (o4 || {}).branding;
          if (n4 && t5 && t5 !== r3) {
            throw new TypeError(kn);
          }
          return t5 = r3, o4;
        });
        return t5 ? i2(t5)(r2, a2, ...o3) : [n3, ...e5];
      };
      return prepFormat.u = n3, prepFormat;
    })(e3, t3));
  };
  for (const e3 in t2) {
    const n3 = t2[e3], o2 = e3.startsWith("format") && createFormatMethod(e3);
    "function" == typeof n3.value ? n3.value = "constructor" === e3 ? DateTimeFormat : o2 || createProxiedMethod(e3) : o2 && (n3.get = function() {
      return o2.bind(this);
    });
  }
  return n2.prototype.value = Object.create(e2, t2), Object.defineProperties(DateTimeFormat, n2), DateTimeFormat;
}
function createFormatMethod(e2) {
  return function(...t2) {
    const n2 = Or.get(this), [o2, ...r2] = n2(...t2);
    return o2[e2](...r2);
  };
}
function createProxiedMethod(e2) {
  return function(...t2) {
    return Or.get(this).u[e2](...t2);
  };
}
function createFormatPrepperForBranding(t2) {
  const n2 = xn[t2];
  if (!n2) {
    throw new TypeError(Ln(t2));
  }
  return e(n2, Jn(qn));
}
var xn = {
  Instant: t,
  PlainDateTime: n,
  PlainDate: o,
  PlainTime: r,
  PlainYearMonth: a,
  PlainMonthDay: i
};
var Rn = /* @__PURE__ */ e(t);
var Wn = /* @__PURE__ */ e(s);
var Gn = /* @__PURE__ */ e(n);
var Un = /* @__PURE__ */ e(o);
var zn = /* @__PURE__ */ e(r);
var Hn = /* @__PURE__ */ e(a);
var Kn = /* @__PURE__ */ e(i);
var Qn = {
  era: l,
  eraYear: c,
  year: u,
  month: d,
  daysInMonth: d,
  daysInYear: d,
  inLeapYear: f,
  monthsInYear: d
};
var Xn = {
  monthCode: m
};
var $n = {
  day: d
};
var _n = {
  dayOfWeek: d,
  dayOfYear: d,
  weekOfYear: S,
  yearOfWeek: c,
  daysInWeek: d
};
var eo = /* @__PURE__ */ Object.assign({}, Qn, Xn, $n, _n);
var to = /* @__PURE__ */ new WeakMap();
var no = /* @__PURE__ */ to.get.bind(to);
var oo = /* @__PURE__ */ to.set.bind(to);
var ro = {
  ...createCalendarFieldMethods(Qn, [L]),
  ...createCalendarFieldMethods(_n, []),
  ...createCalendarFieldMethods(Xn, [L, q]),
  ...createCalendarFieldMethods($n, [q])
};
var ao = /* @__PURE__ */ createCalendarGetters(eo);
var io = /* @__PURE__ */ createCalendarGetters({
  ...Qn,
  ...Xn
});
var so = /* @__PURE__ */ createCalendarGetters({
  ...Xn,
  ...$n
});
var lo = {
  calendarId: (e2) => I(e2.calendar)
};
var co = /* @__PURE__ */ T((e2, t2) => function(n2) {
  const { i: o2 } = this;
  return e2(o2[t2](Yo(v(n2, o2))));
}, eo);
var uo = /* @__PURE__ */ b((e2) => (t2) => t2[e2], F.concat("sign"));
var fo = /* @__PURE__ */ b((e2, t2) => (e3) => e3[j[t2]], w);
var mo = {
  epochSeconds: M,
  epochMilliseconds: y,
  epochMicroseconds: N,
  epochNanoseconds: B
};
var So = /* @__PURE__ */ E(V, /* @__PURE__ */ new Set(["branding"]));
var [Oo, To, po] = createSlotClass(q, E(G, refineCalendarSlot), {
  ...lo,
  ...so
}, {
  getISOFields: So,
  getCalendar: createCalendarFromSlots,
  with(e2, t2, n2) {
    return To(k(_o, e2, this, rejectInvalidBag(t2), n2));
  },
  equals: (e2, t2) => x(e2, toPlainMonthDaySlots(t2)),
  toPlainDate(e2, t2) {
    return Yo(R($o, e2, this, t2));
  },
  toLocaleString(e2, t2, n2) {
    const [o2, r2] = Kn(t2, n2, e2);
    return o2.format(r2);
  },
  toString: W,
  toJSON: (e2) => W(e2),
  valueOf: neverValueOf
}, {
  from: (e2, t2) => To(toPlainMonthDaySlots(e2, t2))
});
var ho = {
  getOffsetNanosecondsFor: getOffsetNanosecondsForAdapter,
  getPossibleInstantsFor(e2, t2, n2) {
    const o2 = [...t2.call(e2, No(ee(n2, X)))].map((e3) => go(e3).epochNanoseconds), r2 = o2.length;
    return r2 > 1 && (o2.sort(te), ne(oe(re(o2[0], o2[r2 - 1])))), o2;
  }
};
var Do = {
  getOffsetNanosecondsFor: getOffsetNanosecondsForAdapter
};
var [Po, Co, go] = createSlotClass(Oe, Se, mo, {
  add: (e2, t2) => Co(se(0, e2, toDurationSlots(t2))),
  subtract: (e2, t2) => Co(se(1, e2, toDurationSlots(t2))),
  until: (e2, t2, n2) => ar(le(0, e2, toInstantSlots(t2), n2)),
  since: (e2, t2, n2) => ar(le(1, e2, toInstantSlots(t2), n2)),
  round: (e2, t2) => Co(ce(e2, t2)),
  equals: (e2, t2) => ue(e2, toInstantSlots(t2)),
  toZonedDateTime(e2, t2) {
    const n2 = de(t2);
    return dr(fe(e2, refineTimeZoneSlot(n2.timeZone), refineCalendarSlot(n2.calendar)));
  },
  toZonedDateTimeISO: (e2, t2) => dr(fe(e2, refineTimeZoneSlot(t2))),
  toLocaleString(e2, t2, n2) {
    const [o2, r2] = Rn(t2, n2, e2);
    return o2.format(r2);
  },
  toString: (e2, t2) => me(refineTimeZoneSlot, createTimeZoneOffsetOps, e2, t2),
  toJSON: (e2) => me(refineTimeZoneSlot, createTimeZoneOffsetOps, e2),
  valueOf: neverValueOf
}, {
  from: (e2) => Co(toInstantSlots(e2)),
  fromEpochSeconds: (e2) => Co(De(e2)),
  fromEpochMilliseconds: (e2) => Co(Pe(e2)),
  fromEpochMicroseconds: (e2) => Co(Ce(e2)),
  fromEpochNanoseconds: (e2) => Co(ge(e2)),
  compare: (e2, t2) => Ze(toInstantSlots(e2), toInstantSlots(t2))
});
var [Zo, bo] = createSlotClass("TimeZone", (e2) => {
  const t2 = Me(e2);
  return {
    branding: "TimeZone",
    id: t2,
    o: ie(t2)
  };
}, {
  id: (e2) => e2.id
}, {
  getPossibleInstantsFor: ({ o: e2 }, t2) => e2.getPossibleInstantsFor(toPlainDateTimeSlots(t2)).map((e3) => Co(_(e3))),
  getOffsetNanosecondsFor: ({ o: e2 }, t2) => e2.getOffsetNanosecondsFor(toInstantSlots(t2).epochNanoseconds),
  getOffsetStringFor(e2, t2) {
    const n2 = toInstantSlots(t2).epochNanoseconds, o2 = createAdapterOps(this, Do).getOffsetNanosecondsFor(n2);
    return Fe(o2);
  },
  getPlainDateTimeFor(e2, t2, n2 = X) {
    const o2 = toInstantSlots(t2).epochNanoseconds, r2 = createAdapterOps(this, Do).getOffsetNanosecondsFor(o2);
    return No(ee(Ie(o2, r2), refineCalendarSlot(n2)));
  },
  getInstantFor(e2, t2, n2) {
    const o2 = toPlainDateTimeSlots(t2), r2 = ve(n2), a2 = createAdapterOps(this);
    return Co(_(we(a2, o2, r2)));
  },
  getNextTransition: ({ o: e2 }, t2) => getImplTransition(1, e2, t2),
  getPreviousTransition: ({ o: e2 }, t2) => getImplTransition(-1, e2, t2),
  equals(e2, t2) {
    return !!je(this, refineTimeZoneSlot(t2));
  },
  toString: (e2) => e2.id,
  toJSON: (e2) => e2.id
}, {
  from(e2) {
    const t2 = refineTimeZoneSlot(e2);
    return "string" == typeof t2 ? new Zo(t2) : t2;
  }
});
var Fo = /* @__PURE__ */ createProtocolValidator(Object.keys(ho));
var [Io, vo] = createSlotClass(xe, ke, fo, {
  getISOFields: So,
  with(e2, t2, n2) {
    return vo(Be(this, rejectInvalidBag(t2), n2));
  },
  add: (e2, t2) => vo(Ye(0, e2, toDurationSlots(t2))),
  subtract: (e2, t2) => vo(Ye(1, e2, toDurationSlots(t2))),
  until: (e2, t2, n2) => ar(Ae(0, e2, toPlainTimeSlots(t2), n2)),
  since: (e2, t2, n2) => ar(Ae(1, e2, toPlainTimeSlots(t2), n2)),
  round: (e2, t2) => vo(Ee(e2, t2)),
  equals: (e2, t2) => Ve(e2, toPlainTimeSlots(t2)),
  toZonedDateTime: (e2, t2) => dr(Je(refineTimeZoneSlot, toPlainDateSlots, createTimeZoneOps, e2, t2)),
  toPlainDateTime: (e2, t2) => No(Le(e2, toPlainDateSlots(t2))),
  toLocaleString(e2, t2, n2) {
    const [o2, r2] = zn(t2, n2, e2);
    return o2.format(r2);
  },
  toString: qe,
  toJSON: (e2) => qe(e2),
  valueOf: neverValueOf
}, {
  from: (e2, t2) => vo(toPlainTimeSlots(e2, t2)),
  compare: (e2, t2) => He(toPlainTimeSlots(e2), toPlainTimeSlots(t2))
});
var [wo, jo, Mo] = createSlotClass(L, E(tt, refineCalendarSlot), {
  ...lo,
  ...io
}, {
  getISOFields: So,
  getCalendar: createCalendarFromSlots,
  with(e2, t2, n2) {
    return jo(Ke(Xo, e2, this, rejectInvalidBag(t2), n2));
  },
  add: (e2, t2, n2) => jo(Qe(nr, 0, e2, toDurationSlots(t2), n2)),
  subtract: (e2, t2, n2) => jo(Qe(nr, 1, e2, toDurationSlots(t2), n2)),
  until: (e2, t2, n2) => ar(Xe(or, 0, e2, toPlainYearMonthSlots(t2), n2)),
  since: (e2, t2, n2) => ar(Xe(or, 1, e2, toPlainYearMonthSlots(t2), n2)),
  equals: (e2, t2) => $e(e2, toPlainYearMonthSlots(t2)),
  toPlainDate(e2, t2) {
    return Yo(_e($o, e2, this, t2));
  },
  toLocaleString(e2, t2, n2) {
    const [o2, r2] = Hn(t2, n2, e2);
    return o2.format(r2);
  },
  toString: et,
  toJSON: (e2) => et(e2),
  valueOf: neverValueOf
}, {
  from: (e2, t2) => jo(toPlainYearMonthSlots(e2, t2)),
  compare: (e2, t2) => rt(toPlainYearMonthSlots(e2), toPlainYearMonthSlots(t2))
});
var [yo, No] = createSlotClass(We, E(pt, refineCalendarSlot), {
  ...lo,
  ...ao,
  ...fo
}, {
  getISOFields: So,
  getCalendar: createCalendarFromSlots,
  with(e2, t2, n2) {
    return No(at($o, e2, this, rejectInvalidBag(t2), n2));
  },
  withCalendar: (e2, t2) => No(it(e2, refineCalendarSlot(t2))),
  withPlainDate: (e2, t2) => No(st(e2, toPlainDateSlots(t2))),
  withPlainTime: (e2, t2) => No(lt(e2, optionalToPlainTimeFields(t2))),
  add: (e2, t2, n2) => No(ct(er, 0, e2, toDurationSlots(t2), n2)),
  subtract: (e2, t2, n2) => No(ct(er, 1, e2, toDurationSlots(t2), n2)),
  until: (e2, t2, n2) => ar(ut(tr, 0, e2, toPlainDateTimeSlots(t2), n2)),
  since: (e2, t2, n2) => ar(ut(tr, 1, e2, toPlainDateTimeSlots(t2), n2)),
  round: (e2, t2) => No(dt(e2, t2)),
  equals: (e2, t2) => ft(e2, toPlainDateTimeSlots(t2)),
  toZonedDateTime: (e2, t2, n2) => dr(mt(createTimeZoneOps, e2, refineTimeZoneSlot(t2), n2)),
  toPlainDate: (e2) => Yo(v(e2)),
  toPlainTime: (e2) => vo(Ge(e2)),
  toPlainYearMonth(e2) {
    return jo(St(Ho, e2, this));
  },
  toPlainMonthDay(e2) {
    return To(Ot(Qo, e2, this));
  },
  toLocaleString(e2, t2, n2) {
    const [o2, r2] = Gn(t2, n2, e2);
    return o2.format(r2);
  },
  toString: Tt,
  toJSON: (e2) => Tt(e2),
  valueOf: neverValueOf
}, {
  from: (e2, t2) => No(toPlainDateTimeSlots(e2, t2)),
  compare: (e2, t2) => gt(toPlainDateTimeSlots(e2), toPlainDateTimeSlots(t2))
});
var [Bo, Yo, Ao] = createSlotClass(J, E(Nt, refineCalendarSlot), {
  ...lo,
  ...ao
}, {
  getISOFields: So,
  getCalendar: createCalendarFromSlots,
  with(e2, t2, n2) {
    return Yo(Zt($o, e2, this, rejectInvalidBag(t2), n2));
  },
  withCalendar: (e2, t2) => Yo(it(e2, refineCalendarSlot(t2))),
  add: (e2, t2, n2) => Yo(bt(er, 0, e2, toDurationSlots(t2), n2)),
  subtract: (e2, t2, n2) => Yo(bt(er, 1, e2, toDurationSlots(t2), n2)),
  until: (e2, t2, n2) => ar(Ft(tr, 0, e2, toPlainDateSlots(t2), n2)),
  since: (e2, t2, n2) => ar(Ft(tr, 1, e2, toPlainDateSlots(t2), n2)),
  equals: (e2, t2) => It(e2, toPlainDateSlots(t2)),
  toZonedDateTime(e2, t2) {
    const n2 = !z(t2) || t2 instanceof Zo ? {
      timeZone: t2
    } : t2;
    return dr(vt(refineTimeZoneSlot, toPlainTimeSlots, createTimeZoneOps, e2, n2));
  },
  toPlainDateTime: (e2, t2) => No(wt(e2, optionalToPlainTimeFields(t2))),
  toPlainYearMonth(e2) {
    return jo(jt(Ho, e2, this));
  },
  toPlainMonthDay(e2) {
    return To(Mt(Qo, e2, this));
  },
  toLocaleString(e2, t2, n2) {
    const [o2, r2] = Un(t2, n2, e2);
    return o2.format(r2);
  },
  toString: yt,
  toJSON: (e2) => yt(e2),
  valueOf: neverValueOf
}, {
  from: (e2, t2) => Yo(toPlainDateSlots(e2, t2)),
  compare: (e2, t2) => rt(toPlainDateSlots(e2), toPlainDateSlots(t2))
});
var Eo = {
  fields(e2, t2, n2) {
    return [...t2.call(e2, n2)];
  }
};
var Vo = /* @__PURE__ */ Object.assign({
  dateFromFields(e2, t2, n2, o2) {
    return Ao(t2.call(e2, Object.assign(/* @__PURE__ */ Object.create(null), n2), o2));
  }
}, Eo);
var Jo = /* @__PURE__ */ Object.assign({
  yearMonthFromFields(e2, t2, n2, o2) {
    return Mo(t2.call(e2, Object.assign(/* @__PURE__ */ Object.create(null), n2), o2));
  }
}, Eo);
var Lo = /* @__PURE__ */ Object.assign({
  monthDayFromFields(e2, t2, n2, o2) {
    return po(t2.call(e2, Object.assign(/* @__PURE__ */ Object.create(null), n2), o2));
  }
}, Eo);
var qo = {
  mergeFields(e2, t2, n2, o2) {
    return de(t2.call(e2, Object.assign(/* @__PURE__ */ Object.create(null), n2), Object.assign(/* @__PURE__ */ Object.create(null), o2)));
  }
};
var ko = /* @__PURE__ */ Object.assign({}, Vo, qo);
var xo = /* @__PURE__ */ Object.assign({}, Jo, qo);
var Ro = /* @__PURE__ */ Object.assign({}, Lo, qo);
var Wo = {
  dateAdd(e2, t2, n2, o2, r2) {
    return Ao(t2.call(e2, Yo(v(n2, e2)), ar(Vt(o2)), r2));
  }
};
var Go = /* @__PURE__ */ Object.assign({}, Wo, {
  dateUntil(e2, t2, n2, o2, r2, a2) {
    return ir(t2.call(e2, Yo(v(n2, e2)), Yo(v(o2, e2)), Object.assign(/* @__PURE__ */ Object.create(null), a2, {
      largestUnit: Et[r2]
    })));
  }
});
var Uo = /* @__PURE__ */ Object.assign({}, Wo, {
  day: dayAdapter
});
var zo = /* @__PURE__ */ Object.assign({}, Go, {
  day: dayAdapter
});
var Ho = /* @__PURE__ */ createCompoundOpsCreator(Jo);
var Ko = /* @__PURE__ */ createCompoundOpsCreator(Vo);
var Qo = /* @__PURE__ */ createCompoundOpsCreator(Lo);
var Xo = /* @__PURE__ */ createCompoundOpsCreator(xo);
var $o = /* @__PURE__ */ createCompoundOpsCreator(ko);
var _o = /* @__PURE__ */ createCompoundOpsCreator(Ro);
var er = /* @__PURE__ */ createCompoundOpsCreator(Wo);
var tr = /* @__PURE__ */ createCompoundOpsCreator(Go);
var nr = /* @__PURE__ */ createCompoundOpsCreator(Uo);
var or = /* @__PURE__ */ createCompoundOpsCreator(zo);
var [rr, ar, ir] = createSlotClass(qt, Lt, {
  ...uo,
  blank: Jt
}, {
  with: (e2, t2) => ar(kt(e2, t2)),
  negated: (e2) => ar(xt(e2)),
  abs: (e2) => ar(Rt(e2)),
  add: (e2, t2, n2) => ar(Wt(refinePublicRelativeTo, tr, createTimeZoneOps, 0, e2, toDurationSlots(t2), n2)),
  subtract: (e2, t2, n2) => ar(Wt(refinePublicRelativeTo, tr, createTimeZoneOps, 1, e2, toDurationSlots(t2), n2)),
  round: (e2, t2) => ar(Gt(refinePublicRelativeTo, tr, createTimeZoneOps, e2, t2)),
  total: (e2, t2) => Ut(refinePublicRelativeTo, tr, createTimeZoneOps, e2, t2),
  toLocaleString(e2, t2, n2) {
    return Intl.DurationFormat ? new Intl.DurationFormat(t2, n2).format(this) : zt(e2);
  },
  toString: zt,
  toJSON: (e2) => zt(e2),
  valueOf: neverValueOf
}, {
  from: (e2) => ar(toDurationSlots(e2)),
  compare: (e2, t2, n2) => $t(refinePublicRelativeTo, er, createTimeZoneOps, toDurationSlots(e2), toDurationSlots(t2), n2)
});
var sr = {
  toString: (e2) => e2.id,
  toJSON: (e2) => e2.id,
  ...ro,
  dateAdd: ({ id: e2, o: t2 }, n2, o2, r2) => Yo(v(t2.dateAdd(toPlainDateSlots(n2), toDurationSlots(o2), r2), e2)),
  dateUntil: ({ o: e2 }, t2, n2, o2) => ar(Vt(e2.dateUntil(toPlainDateSlots(t2), toPlainDateSlots(n2), _t(o2)))),
  dateFromFields: ({ id: e2, o: t2 }, n2, o2) => Yo(Yt(t2, n2, o2, ln(e2))),
  yearMonthFromFields: ({ id: e2, o: t2 }, n2, o2) => jo(nt(t2, n2, o2, un(e2))),
  monthDayFromFields: ({ id: e2, o: t2 }, n2, o2) => To(K(t2, 0, n2, o2, cn(e2))),
  fields({ o: e2 }, t2) {
    const n2 = new Set(en), o2 = [];
    for (const e3 of t2) {
      if (m(e3), !n2.has(e3)) {
        throw new RangeError(tn(e3));
      }
      n2.delete(e3), o2.push(e3);
    }
    return e2.fields(o2);
  },
  mergeFields: ({ o: e2 }, t2, n2) => e2.mergeFields(nn(on(t2)), nn(on(n2)))
};
var [lr] = createSlotClass("Calendar", (e2) => {
  const t2 = rn(e2);
  return {
    branding: "Calendar",
    id: t2,
    o: Y(t2)
  };
}, {
  id: (e2) => e2.id
}, sr, {
  from(e2) {
    const t2 = refineCalendarSlot(e2);
    return "string" == typeof t2 ? new lr(t2) : t2;
  }
});
var cr = /* @__PURE__ */ createProtocolValidator(Object.keys(sr).slice(4));
var [ur, dr] = createSlotClass(Te, E(vn, refineCalendarSlot, refineTimeZoneSlot), {
  ...mo,
  ...lo,
  ...adaptDateMethods(ao),
  ...adaptDateMethods(fo),
  offset: (e2) => Fe(slotsToIso(e2).offsetNanoseconds),
  offsetNanoseconds: (e2) => slotsToIso(e2).offsetNanoseconds,
  timeZoneId: (e2) => I(e2.timeZone),
  hoursInDay: (e2) => dn(createTimeZoneOps, e2)
}, {
  getISOFields: (e2) => mn(createTimeZoneOffsetOps, e2),
  getCalendar: createCalendarFromSlots,
  getTimeZone: ({ timeZone: e2 }) => "string" == typeof e2 ? new Zo(e2) : e2,
  with(e2, t2, n2) {
    return dr(Sn($o, createTimeZoneOps, e2, this, rejectInvalidBag(t2), n2));
  },
  withCalendar: (e2, t2) => dr(it(e2, refineCalendarSlot(t2))),
  withTimeZone: (e2, t2) => dr(On(e2, refineTimeZoneSlot(t2))),
  withPlainDate: (e2, t2) => dr(Tn(createTimeZoneOps, e2, toPlainDateSlots(t2))),
  withPlainTime: (e2, t2) => dr(pn(createTimeZoneOps, e2, optionalToPlainTimeFields(t2))),
  add: (e2, t2, n2) => dr(hn(er, createTimeZoneOps, 0, e2, toDurationSlots(t2), n2)),
  subtract: (e2, t2, n2) => dr(hn(er, createTimeZoneOps, 1, e2, toDurationSlots(t2), n2)),
  until: (e2, t2, n2) => ar(Vt(Dn(tr, createTimeZoneOps, 0, e2, toZonedDateTimeSlots(t2), n2))),
  since: (e2, t2, n2) => ar(Vt(Dn(tr, createTimeZoneOps, 1, e2, toZonedDateTimeSlots(t2), n2))),
  round: (e2, t2) => dr(Pn(createTimeZoneOps, e2, t2)),
  startOfDay: (e2) => dr(Cn(createTimeZoneOps, e2)),
  equals: (e2, t2) => gn(e2, toZonedDateTimeSlots(t2)),
  toInstant: (e2) => Co(Zn(e2)),
  toPlainDateTime: (e2) => No(ht(createTimeZoneOffsetOps, e2)),
  toPlainDate: (e2) => Yo(Bt(createTimeZoneOffsetOps, e2)),
  toPlainTime: (e2) => vo(Re(createTimeZoneOffsetOps, e2)),
  toPlainYearMonth(e2) {
    return jo(bn(Ho, e2, this));
  },
  toPlainMonthDay(e2) {
    return To(Fn(Qo, e2, this));
  },
  toLocaleString(e2, t2, n2 = {}) {
    const [o2, r2] = Wn(t2, n2, e2);
    return o2.format(r2);
  },
  toString: (e2, t2) => In(createTimeZoneOffsetOps, e2, t2),
  toJSON: (e2) => In(createTimeZoneOffsetOps, e2),
  valueOf: neverValueOf
}, {
  from: (e2, t2) => dr(toZonedDateTimeSlots(e2, t2)),
  compare: (e2, t2) => yn(toZonedDateTimeSlots(e2), toZonedDateTimeSlots(t2))
});
var fr = /* @__PURE__ */ Object.defineProperties({}, {
  ...h("Temporal.Now"),
  ...p({
    timeZoneId: () => Nn(),
    instant: () => Co(_(Bn())),
    zonedDateTime: (e2, t2 = Nn()) => dr(Yn(Bn(), refineTimeZoneSlot(t2), refineCalendarSlot(e2))),
    zonedDateTimeISO: (e2 = Nn()) => dr(Yn(Bn(), refineTimeZoneSlot(e2), X)),
    plainDateTime: (e2, t2 = Nn()) => No(ee(An(createTimeZoneOffsetOps(refineTimeZoneSlot(t2))), refineCalendarSlot(e2))),
    plainDateTimeISO: (e2 = Nn()) => No(ee(An(createTimeZoneOffsetOps(refineTimeZoneSlot(e2))), X)),
    plainDate: (e2, t2 = Nn()) => Yo(v(An(createTimeZoneOffsetOps(refineTimeZoneSlot(t2))), refineCalendarSlot(e2))),
    plainDateISO: (e2 = Nn()) => Yo(v(An(createTimeZoneOffsetOps(refineTimeZoneSlot(e2))), X)),
    plainTimeISO: (e2 = Nn()) => vo(Ge(An(createTimeZoneOffsetOps(refineTimeZoneSlot(e2)))))
  })
});
var mr = /* @__PURE__ */ Object.defineProperties({}, {
  ...h("Temporal"),
  ...p({
    PlainYearMonth: wo,
    PlainMonthDay: Oo,
    PlainDate: Bo,
    PlainTime: Io,
    PlainDateTime: yo,
    ZonedDateTime: ur,
    Instant: Po,
    Calendar: lr,
    TimeZone: Zo,
    Duration: rr,
    Now: fr
  })
});
var Sr = /* @__PURE__ */ createDateTimeFormatClass();
var Or = /* @__PURE__ */ new WeakMap();
var Tr = /* @__PURE__ */ Object.defineProperties(Object.create(Intl), p({
  DateTimeFormat: Sr
}));

// node_modules/temporal-polyfill/global.esm.js
Object.defineProperties(globalThis, p({
  Temporal: mr
})), Object.defineProperties(Intl, p({
  DateTimeFormat: Sr
})), Object.defineProperties(Date.prototype, p({
  toTemporalInstant
}));
