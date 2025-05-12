// node_modules/temporal-polyfill/chunks/internal.js
function clampProp(e2, n2, t2, o2, r2) {
  return clampEntity(n2, ((e3, n3) => {
    const t3 = e3[n3];
    if (void 0 === t3) {
      throw new TypeError(missingField(n3));
    }
    return t3;
  })(e2, n2), t2, o2, r2);
}
function clampEntity(e2, n2, t2, o2, r2, i2) {
  const a2 = clampNumber(n2, t2, o2);
  if (r2 && n2 !== a2) {
    throw new RangeError(numberOutOfRange(e2, n2, t2, o2, i2));
  }
  return a2;
}
function s(e2) {
  return null !== e2 && /object|function/.test(typeof e2);
}
function on(e2, n2 = Map) {
  const t2 = new n2();
  return (n3, ...o2) => {
    if (t2.has(n3)) {
      return t2.get(n3);
    }
    const r2 = e2(n3, ...o2);
    return t2.set(n3, r2), r2;
  };
}
function r(e2) {
  return n({
    name: e2
  }, 1);
}
function n(n2, t2) {
  return e((e2) => ({
    value: e2,
    configurable: 1,
    writable: !t2
  }), n2);
}
function t(n2) {
  return e((e2) => ({
    get: e2,
    configurable: 1
  }), n2);
}
function o(e2) {
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
function e(e2, n2, t2) {
  const o2 = {};
  for (const r2 in n2) {
    o2[r2] = e2(n2[r2], r2, t2);
  }
  return o2;
}
function g(e2, n2, t2) {
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
function nn(e2, n2) {
  const t2 = /* @__PURE__ */ Object.create(null);
  for (const o2 of e2) {
    t2[o2] = n2[o2];
  }
  return t2;
}
function hasAnyPropsByName(e2, n2) {
  for (const t2 of n2) {
    if (t2 in e2) {
      return 1;
    }
  }
  return 0;
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
function Pt(e2, ...n2) {
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
    const n3 = e2[t2[i3]], a3 = Ao[i3], s2 = Uo / a3, [c2, u2] = divModTrunc(n3, s2);
    o2 += u2 * a3, r2 += c2;
  }
  const [i2, a2] = divModTrunc(o2, Uo);
  return [r2 + i2, a2];
}
function nanoToGivenFields(e2, n2, t2) {
  const o2 = {};
  for (let r2 = n2; r2 >= 0; r2--) {
    const n3 = Ao[r2];
    o2[t2[r2]] = divTrunc(e2, n3), e2 = modTrunc(e2, n3);
  }
  return o2;
}
function d(e2) {
  if (void 0 !== e2) {
    return m(e2);
  }
}
function P(e2) {
  if (void 0 !== e2) {
    return h(e2);
  }
}
function S(e2) {
  if (void 0 !== e2) {
    return T(e2);
  }
}
function h(e2) {
  return requireNumberIsPositive(T(e2));
}
function T(e2) {
  return ze(cr(e2));
}
function requirePropDefined(e2, n2) {
  if (null == n2) {
    throw new RangeError(missingField(e2));
  }
  return n2;
}
function requireObjectLike(e2) {
  if (!s(e2)) {
    throw new TypeError(oo);
  }
  return e2;
}
function requireType(e2, n2, t2 = e2) {
  if (typeof n2 !== e2) {
    throw new TypeError(invalidEntity(t2, n2));
  }
  return n2;
}
function ze(e2, n2 = "number") {
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
    throw new TypeError(no);
  }
  return String(e2);
}
function toStringViaPrimitive(e2, n2) {
  return s(e2) ? String(e2) : m(e2, n2);
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
  return ze(toNumber(e2, n2), n2);
}
function toPositiveInteger(e2, n2) {
  return requireNumberIsPositive(toInteger(e2, n2), n2);
}
function createBigNano(e2, n2) {
  let [t2, o2] = divModTrunc(n2, Uo), r2 = e2 + t2;
  const i2 = Math.sign(r2);
  return i2 && i2 === -Math.sign(o2) && (r2 -= i2, o2 += i2 * Uo), [r2, o2];
}
function addBigNanos(e2, n2, t2 = 1) {
  return createBigNano(e2[0] + n2[0] * t2, e2[1] + n2[1] * t2);
}
function moveBigNano(e2, n2) {
  return createBigNano(e2[0], e2[1] + n2);
}
function diffBigNanos(e2, n2) {
  return addBigNanos(n2, e2, -1);
}
function compareBigNanos(e2, n2) {
  return compareNumbers(e2[0], n2[0]) || compareNumbers(e2[1], n2[1]);
}
function bigNanoOutside(e2, n2, t2) {
  return -1 === compareBigNanos(e2, n2) || 1 === compareBigNanos(e2, t2);
}
function bigIntToBigNano(e2, n2 = 1) {
  const t2 = BigInt(Uo / n2);
  return [Number(e2 / t2), Number(e2 % t2) * n2];
}
function Ge(e2, n2 = 1) {
  const t2 = Uo / n2, [o2, r2] = divModTrunc(e2, t2);
  return [o2, r2 * n2];
}
function bigNanoToNumber(e2, n2 = 1, t2) {
  const [o2, r2] = e2, [i2, a2] = divModTrunc(r2, n2);
  return o2 * (Uo / n2) + (i2 + (t2 ? a2 / n2 : 0));
}
function divModBigNano(e2, n2, t2 = divModFloor) {
  const [o2, r2] = e2, [i2, a2] = t2(r2, n2);
  return [o2 * (Uo / n2) + i2, a2];
}
function checkIsoYearMonthInBounds(e2) {
  return clampProp(e2, "isoYear", wr, Fr, 1), e2.isoYear === wr ? clampProp(e2, "isoMonth", 4, 12, 1) : e2.isoYear === Fr && clampProp(e2, "isoMonth", 1, 9, 1), e2;
}
function checkIsoDateInBounds(e2) {
  return checkIsoDateTimeInBounds({
    ...e2,
    ...Nt,
    isoHour: 12
  }), e2;
}
function checkIsoDateTimeInBounds(e2) {
  const n2 = clampProp(e2, "isoYear", wr, Fr, 1), t2 = n2 === wr ? 1 : n2 === Fr ? -1 : 0;
  return t2 && checkEpochNanoInBounds(isoToEpochNano({
    ...e2,
    isoDay: e2.isoDay + t2,
    isoNanosecond: e2.isoNanosecond - t2
  })), e2;
}
function checkEpochNanoInBounds(e2) {
  if (!e2 || bigNanoOutside(e2, Sr, Er)) {
    throw new RangeError(Io);
  }
  return e2;
}
function isoTimeFieldsToNano(e2) {
  return givenFieldsToBigNano(e2, 5, w)[1];
}
function nanoToIsoTimeAndDay(e2) {
  const [n2, t2] = divModFloor(e2, Uo);
  return [nanoToGivenFields(t2, 5, w), n2];
}
function epochNanoToSecMod(e2) {
  return divModBigNano(e2, Ro);
}
function isoToEpochMilli(e2) {
  return isoArgsToEpochMilli(e2.isoYear, e2.isoMonth, e2.isoDay, e2.isoHour, e2.isoMinute, e2.isoSecond, e2.isoMillisecond);
}
function isoToEpochNano(e2) {
  const n2 = isoToEpochMilli(e2);
  if (void 0 !== n2) {
    const [t2, o2] = divModTrunc(n2, ko);
    return [t2, o2 * Qe + (e2.isoMicrosecond || 0) * Yo + (e2.isoNanosecond || 0)];
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
  return isoArgsToEpochMilli(...e2) / Co;
}
function isoArgsToEpochMilli(...e2) {
  const [n2, t2] = isoToLegacyDate(...e2), o2 = n2.valueOf();
  if (!isNaN(o2)) {
    return o2 - t2 * ko;
  }
}
function isoToLegacyDate(e2, n2 = 1, t2 = 1, o2 = 0, r2 = 0, i2 = 0, a2 = 0) {
  const s2 = e2 === wr ? 1 : e2 === Fr ? -1 : 0, c2 = /* @__PURE__ */ new Date();
  return c2.setUTCHours(o2, r2, i2, a2), c2.setUTCFullYear(e2, n2 - 1, t2 + s2), [c2, s2];
}
function epochNanoToIso(e2, n2) {
  let [t2, o2] = moveBigNano(e2, n2);
  o2 < 0 && (o2 += Uo, t2 -= 1);
  const [r2, i2] = divModFloor(o2, Qe), [a2, s2] = divModFloor(i2, Yo);
  return epochMilliToIso(t2 * ko + r2, a2, s2);
}
function epochMilliToIso(e2, n2 = 0, t2 = 0) {
  const o2 = Math.ceil(Math.max(0, Math.abs(e2) - Pr) / ko) * Math.sign(e2), r2 = new Date(e2 - o2 * ko);
  return zipProps(Tr, [r2.getUTCFullYear(), r2.getUTCMonth() + 1, r2.getUTCDate() + o2, r2.getUTCHours(), r2.getUTCMinutes(), r2.getUTCSeconds(), r2.getUTCMilliseconds(), n2, t2]);
}
function hashIntlFormatParts(e2, n2) {
  if (n2 < -Pr) {
    throw new RangeError(Io);
  }
  const t2 = e2.formatToParts(n2), o2 = {};
  for (const e3 of t2) {
    o2[e3.type] = e3.value;
  }
  return o2;
}
function computeIsoDateParts(e2) {
  return [e2.isoYear, e2.isoMonth, e2.isoDay];
}
function computeIsoMonthCodeParts(e2, n2) {
  return [n2, 0];
}
function computeIsoMonthsInYear() {
  return kr;
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
function computeIsoEraParts(e2) {
  return this.id === or ? (({ isoYear: e3 }) => e3 < 1 ? ["gregory-inverse", 1 - e3] : ["gregory", e3])(e2) : this.id === rr ? Yr(e2) : [];
}
function computeJapaneseEraParts(e2) {
  const n2 = isoToEpochMilli(e2);
  if (n2 < Cr) {
    const { isoYear: n3 } = e2;
    return n3 < 1 ? ["japanese-inverse", 1 - n3] : ["japanese", n3];
  }
  const t2 = hashIntlFormatParts(Ci(rr), n2), { era: o2, eraYear: r2 } = parseIntlYear(t2, rr);
  return [o2, r2];
}
function checkIsoDateTimeFields(e2) {
  return checkIsoDateFields(e2), constrainIsoTimeFields(e2, 1), e2;
}
function checkIsoDateFields(e2) {
  return constrainIsoDateFields(e2, 1), e2;
}
function isIsoDateFieldsValid(e2) {
  return allPropsEqual(Dr, e2, constrainIsoDateFields(e2));
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
  return zipProps(w, [clampProp(e2, "isoHour", 0, 23, n2), clampProp(e2, "isoMinute", 0, 59, n2), clampProp(e2, "isoSecond", 0, 59, n2), clampProp(e2, "isoMillisecond", 0, 999, n2), clampProp(e2, "isoMicrosecond", 0, 999, n2), clampProp(e2, "isoNanosecond", 0, 999, n2)]);
}
function mt(e2) {
  return void 0 === e2 ? 0 : Xr(requireObjectLike(e2));
}
function je(e2, n2 = 0) {
  e2 = normalizeOptions(e2);
  const t2 = ei(e2), o2 = ni(e2, n2);
  return [Xr(e2), o2, t2];
}
function refineDiffOptions(e2, n2, t2, o2 = 9, r2 = 0, i2 = 4) {
  n2 = normalizeOptions(n2);
  let a2 = Kr(n2, o2, r2), s2 = parseRoundingIncInteger(n2), c2 = ii(n2, i2);
  const u2 = Jr(n2, o2, r2, 1);
  return null == a2 ? a2 = Math.max(t2, u2) : checkLargestSmallestUnit(a2, u2), s2 = refineRoundingInc(s2, u2, 1), e2 && (c2 = ((e3) => e3 < 4 ? (e3 + 2) % 4 : e3)(c2)), [a2, u2, s2, c2];
}
function refineRoundingOptions(e2, n2 = 6, t2) {
  let o2 = parseRoundingIncInteger(e2 = normalizeOptionsOrString(e2, Rr));
  const r2 = ii(e2, 7);
  let i2 = Jr(e2, n2);
  return i2 = requirePropDefined(Rr, i2), o2 = refineRoundingInc(o2, i2, void 0, t2), [i2, o2, r2];
}
function refineDateDisplayOptions(e2) {
  return ti(normalizeOptions(e2));
}
function refineTimeDisplayOptions(e2, n2) {
  return refineTimeDisplayTuple(normalizeOptions(e2), n2);
}
function Me(e2) {
  const n2 = normalizeOptionsOrString(e2, qr), t2 = refineChoiceOption(qr, _r, n2, 0);
  if (!t2) {
    throw new RangeError(invalidEntity(qr, t2));
  }
  return t2;
}
function refineTimeDisplayTuple(e2, n2 = 4) {
  const t2 = refineSubsecDigits(e2);
  return [ii(e2, 4), ...refineSmallestUnitAndSubsecDigits(Jr(e2, n2), t2)];
}
function refineSmallestUnitAndSubsecDigits(e2, n2) {
  return null != e2 ? [Ao[e2], e2 < 4 ? 9 - 3 * e2 : -1] : [void 0 === n2 ? 1 : 10 ** (9 - n2), n2];
}
function parseRoundingIncInteger(e2) {
  const n2 = e2[zr];
  return void 0 === n2 ? 1 : toInteger(n2, zr);
}
function refineRoundingInc(e2, n2, t2, o2) {
  const r2 = o2 ? Uo : Ao[n2 + 1];
  if (r2) {
    const t3 = Ao[n2];
    if (r2 % ((e2 = clampEntity(zr, e2, 1, r2 / t3 - (o2 ? 0 : 1), 1)) * t3)) {
      throw new RangeError(invalidEntity(zr, e2));
    }
  } else {
    e2 = clampEntity(zr, e2, 1, t2 ? 10 ** 9 : 1, 1);
  }
  return e2;
}
function refineSubsecDigits(e2) {
  let n2 = e2[Ur];
  if (void 0 !== n2) {
    if ("number" != typeof n2) {
      if ("auto" === toString(n2)) {
        return;
      }
      throw new RangeError(invalidEntity(Ur, n2));
    }
    n2 = clampEntity(Ur, Math.floor(n2), 0, 9, 1);
  }
  return n2;
}
function normalizeOptions(e2) {
  return void 0 === e2 ? {} : requireObjectLike(e2);
}
function normalizeOptionsOrString(e2, n2) {
  return "string" == typeof e2 ? {
    [n2]: e2
  } : requireObjectLike(e2);
}
function fabricateOverflowOptions(e2) {
  return {
    overflow: jr[e2]
  };
}
function refineUnitOption(e2, n2, t2 = 9, o2 = 0, r2) {
  let i2 = n2[e2];
  if (void 0 === i2) {
    return r2 ? o2 : void 0;
  }
  if (i2 = toString(i2), "auto" === i2) {
    return r2 ? o2 : null;
  }
  let a2 = Oo[i2];
  if (void 0 === a2 && (a2 = mr[i2]), void 0 === a2) {
    throw new RangeError(invalidChoice(e2, i2, Oo));
  }
  return clampEntity(e2, a2, o2, t2, 1, Bo), a2;
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
    throw new RangeError(Eo);
  }
}
function xe(e2) {
  return {
    branding: Re,
    epochNanoseconds: e2
  };
}
function _e(e2, n2, t2) {
  return {
    branding: z,
    calendar: t2,
    timeZone: n2,
    epochNanoseconds: e2
  };
}
function jt(e2, n2 = e2.calendar) {
  return {
    branding: x,
    calendar: n2,
    ...nn(Nr, e2)
  };
}
function W(e2, n2 = e2.calendar) {
  return {
    branding: G,
    calendar: n2,
    ...nn(Ir, e2)
  };
}
function createPlainYearMonthSlots(e2, n2 = e2.calendar) {
  return {
    branding: Ut,
    calendar: n2,
    ...nn(Ir, e2)
  };
}
function createPlainMonthDaySlots(e2, n2 = e2.calendar) {
  return {
    branding: qt,
    calendar: n2,
    ...nn(Ir, e2)
  };
}
function St(e2) {
  return {
    branding: ft,
    ...nn(Mr, e2)
  };
}
function Oe(e2) {
  return {
    branding: N,
    sign: computeDurationSign(e2),
    ...nn(ur, e2)
  };
}
function I(e2) {
  return divModBigNano(e2.epochNanoseconds, Qe)[0];
}
function v(e2) {
  return ((e3, n2 = 1) => {
    const [t2, o2] = e3, r2 = Math.floor(o2 / n2), i2 = Uo / n2;
    return BigInt(t2) * BigInt(i2) + BigInt(r2);
  })(e2.epochNanoseconds);
}
function extractEpochNano(e2) {
  return e2.epochNanoseconds;
}
function J(e2, n2, t2, o2, r2) {
  const i2 = getMaxDurationUnit(o2), [a2, s2] = ((e3, n3) => {
    const t3 = n3((e3 = normalizeOptionsOrString(e3, Zr))[Ar]);
    let o3 = Qr(e3);
    return o3 = requirePropDefined(Zr, o3), [o3, t3];
  })(r2, e2), c2 = Math.max(a2, i2);
  if (!s2 && isUniformUnit(c2, s2)) {
    return totalDayTimeDuration(o2, a2);
  }
  if (!s2) {
    throw new RangeError(yo);
  }
  if (!o2.sign) {
    return 0;
  }
  const [u2, f2, l2] = createMarkerSystem(n2, t2, s2), d2 = createMarkerToEpochNano(l2), m2 = createMoveMarker(l2), h2 = createDiffMarkers(l2), g2 = m2(f2, u2, o2);
  isZonedEpochSlots(s2) || (checkIsoDateTimeInBounds(u2), checkIsoDateTimeInBounds(g2));
  const D2 = h2(f2, u2, g2, a2);
  return isUniformUnit(a2, s2) ? totalDayTimeDuration(D2, a2) : ((e3, n3, t3, o3, r3, i3, a3) => {
    const s3 = computeDurationSign(e3), [c3, u3] = clampRelativeDuration(o3, gr(t3, e3), t3, s3, r3, i3, a3), f3 = computeEpochNanoFrac(n3, c3, u3);
    return e3[p[t3]] + f3 * s3;
  })(D2, d2(g2), a2, f2, u2, d2, m2);
}
function totalDayTimeDuration(e2, n2) {
  return bigNanoToNumber(durationFieldsToBigNano(e2), Ao[n2], 1);
}
function clampRelativeDuration(e2, n2, t2, o2, r2, i2, a2) {
  const s2 = p[t2], c2 = {
    ...n2,
    [s2]: n2[s2] + o2
  }, u2 = a2(e2, r2, n2), f2 = a2(e2, r2, c2);
  return [i2(u2), i2(f2)];
}
function computeEpochNanoFrac(e2, n2, t2) {
  const o2 = bigNanoToNumber(diffBigNanos(n2, t2));
  if (!o2) {
    throw new RangeError(fo);
  }
  return bigNanoToNumber(diffBigNanos(n2, e2)) / o2;
}
function Le(e2, n2) {
  const [t2, o2, r2] = refineRoundingOptions(n2, 5, 1);
  return xe(roundBigNano(e2.epochNanoseconds, t2, o2, r2, 1));
}
function Ie(e2, n2, t2) {
  let { epochNanoseconds: o2, timeZone: r2, calendar: i2 } = n2;
  const [a2, s2, c2] = refineRoundingOptions(t2);
  if (0 === a2 && 1 === s2) {
    return n2;
  }
  const u2 = e2(r2);
  if (6 === a2) {
    o2 = ((e3, n3, t3, o3) => {
      const r3 = he(t3, n3), [i3, a3] = e3(r3), s3 = t3.epochNanoseconds, c3 = getStartOfDayInstantFor(n3, i3), u3 = getStartOfDayInstantFor(n3, a3);
      if (bigNanoOutside(s3, c3, u3)) {
        throw new RangeError(fo);
      }
      return roundWithMode(computeEpochNanoFrac(s3, c3, u3), o3) ? u3 : c3;
    })(computeDayInterval, u2, n2, c2);
  } else {
    const e3 = u2.R(o2);
    o2 = getMatchingInstantFor(u2, roundDateTime(epochNanoToIso(o2, e3), a2, s2, c2), e3, 2, 0, 1);
  }
  return _e(o2, r2, i2);
}
function vt(e2, n2) {
  return jt(roundDateTime(e2, ...refineRoundingOptions(n2)), e2.calendar);
}
function lt(e2, n2) {
  const [t2, o2, r2] = refineRoundingOptions(n2, 5);
  var i2;
  return St((i2 = r2, roundTimeToNano(e2, computeNanoInc(t2, o2), i2)[0]));
}
function Te(e2, n2) {
  const t2 = e2(n2.timeZone), o2 = he(n2, t2), [r2, i2] = computeDayInterval(o2), a2 = bigNanoToNumber(diffBigNanos(getStartOfDayInstantFor(t2, r2), getStartOfDayInstantFor(t2, i2)), zo, 1);
  if (a2 <= 0) {
    throw new RangeError(fo);
  }
  return a2;
}
function ve(e2, n2) {
  const { timeZone: t2, calendar: o2 } = n2, r2 = ((e3, n3, t3) => getStartOfDayInstantFor(n3, e3(he(t3, n3))))(computeDayFloor, e2(t2), n2);
  return _e(r2, t2, o2);
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
  return roundByInc(e2, Zo, 7);
}
function computeNanoInc(e2, n2) {
  return Ao[e2] * n2;
}
function computeDayInterval(e2) {
  const n2 = computeDayFloor(e2);
  return [n2, moveByDays(n2, 1)];
}
function computeDayFloor(e2) {
  return yr(6, e2);
}
function roundDayTimeDurationByInc(e2, n2, t2) {
  const o2 = Math.min(getMaxDurationUnit(e2), 6);
  return nanoToDurationDayTimeFields(roundBigNanoByInc(durationFieldsToBigNano(e2, o2), n2, t2), o2);
}
function roundRelativeDuration(e2, n2, t2, o2, r2, i2, a2, s2, c2, u2) {
  if (0 === o2 && 1 === r2) {
    return e2;
  }
  const f2 = isUniformUnit(o2, s2) ? isZonedEpochSlots(s2) && o2 < 6 && t2 >= 6 ? nudgeZonedTimeDuration : nudgeDayTimeDuration : nudgeRelativeDuration;
  let [l2, d2, m2] = f2(e2, n2, t2, o2, r2, i2, a2, s2, c2, u2);
  return m2 && 7 !== o2 && (l2 = ((e3, n3, t3, o3, r3, i3, a3, s3) => {
    const c3 = computeDurationSign(e3);
    for (let u3 = o3 + 1; u3 <= t3; u3++) {
      if (7 === u3 && 7 !== t3) {
        continue;
      }
      const o4 = gr(u3, e3);
      o4[p[u3]] += c3;
      const f3 = bigNanoToNumber(diffBigNanos(a3(s3(r3, i3, o4)), n3));
      if (f3 && Math.sign(f3) !== c3) {
        break;
      }
      e3 = o4;
    }
    return e3;
  })(l2, d2, t2, Math.max(6, o2), a2, s2, c2, u2)), l2;
}
function roundBigNano(e2, n2, t2, o2, r2) {
  if (6 === n2) {
    const n3 = ((e3) => e3[0] + e3[1] / Uo)(e2);
    return [roundByInc(n3, t2, o2), 0];
  }
  return roundBigNanoByInc(e2, computeNanoInc(n2, t2), o2, r2);
}
function roundBigNanoByInc(e2, n2, t2, o2) {
  let [r2, i2] = e2;
  o2 && i2 < 0 && (i2 += Uo, r2 -= 1);
  const [a2, s2] = divModFloor(roundByInc(i2, n2, t2), Uo);
  return createBigNano(r2 + a2, s2);
}
function roundByInc(e2, n2, t2) {
  return roundWithMode(e2 / n2, t2) * n2;
}
function roundWithMode(e2, n2) {
  return ai[n2](e2);
}
function nudgeDayTimeDuration(e2, n2, t2, o2, r2, i2) {
  const a2 = computeDurationSign(e2), s2 = durationFieldsToBigNano(e2), c2 = roundBigNano(s2, o2, r2, i2), u2 = diffBigNanos(s2, c2), f2 = Math.sign(c2[0] - s2[0]) === a2, l2 = nanoToDurationDayTimeFields(c2, Math.min(t2, 6));
  return [{
    ...e2,
    ...l2
  }, addBigNanos(n2, u2), f2];
}
function nudgeZonedTimeDuration(e2, n2, t2, o2, r2, i2, a2, s2, c2, u2) {
  const f2 = computeDurationSign(e2) || 1, l2 = bigNanoToNumber(durationFieldsToBigNano(e2, 5)), d2 = computeNanoInc(o2, r2);
  let m2 = roundByInc(l2, d2, i2);
  const [p2, h2] = clampRelativeDuration(a2, {
    ...e2,
    ...hr
  }, 6, f2, s2, c2, u2), g2 = m2 - bigNanoToNumber(diffBigNanos(p2, h2));
  let D2 = 0;
  g2 && Math.sign(g2) !== f2 ? n2 = moveBigNano(p2, m2) : (D2 += f2, m2 = roundByInc(g2, d2, i2), n2 = moveBigNano(h2, m2));
  const T2 = nanoToDurationTimeFields(m2);
  return [{
    ...e2,
    ...T2,
    days: e2.days + D2
  }, n2, Boolean(D2)];
}
function nudgeRelativeDuration(e2, n2, t2, o2, r2, i2, a2, s2, c2, u2) {
  const f2 = computeDurationSign(e2), l2 = p[o2], d2 = gr(o2, e2);
  7 === o2 && (e2 = {
    ...e2,
    weeks: e2.weeks + Math.trunc(e2.days / 7)
  });
  const m2 = divTrunc(e2[l2], r2) * r2;
  d2[l2] = m2;
  const [h2, g2] = clampRelativeDuration(a2, d2, o2, r2 * f2, s2, c2, u2), D2 = m2 + computeEpochNanoFrac(n2, h2, g2) * f2 * r2, T2 = roundByInc(D2, r2, i2), I2 = Math.sign(T2 - D2) === f2;
  return d2[l2] = T2, [d2, I2 ? g2 : h2, I2];
}
function ke(e2, n2, t2, o2) {
  const [r2, i2, a2, s2] = ((e3) => {
    const n3 = refineTimeDisplayTuple(e3 = normalizeOptions(e3));
    return [e3.timeZone, ...n3];
  })(o2), c2 = void 0 !== r2;
  return ((e3, n3, t3, o3, r3, i3) => {
    t3 = roundBigNanoByInc(t3, r3, o3, 1);
    const a3 = n3.R(t3);
    return formatIsoDateTimeFields(epochNanoToIso(t3, a3), i3) + (e3 ? Se(roundToMinute(a3)) : "Z");
  })(c2, n2(c2 ? e2(r2) : si), t2.epochNanoseconds, i2, a2, s2);
}
function Fe(e2, n2, t2) {
  const [o2, r2, i2, a2, s2, c2] = ((e3) => {
    e3 = normalizeOptions(e3);
    const n3 = ti(e3), t3 = refineSubsecDigits(e3), o3 = ri(e3), r3 = ii(e3, 4), i3 = Jr(e3, 4);
    return [n3, oi(e3), o3, r3, ...refineSmallestUnitAndSubsecDigits(i3, t3)];
  })(t2);
  return ((e3, n3, t3, o3, r3, i3, a3, s3, c3, u2) => {
    o3 = roundBigNanoByInc(o3, c3, s3, 1);
    const f2 = e3(t3).R(o3);
    return formatIsoDateTimeFields(epochNanoToIso(o3, f2), u2) + Se(roundToMinute(f2), a3) + ((e4, n4) => 1 !== n4 ? "[" + (2 === n4 ? "!" : "") + e4 + "]" : "")(t3, i3) + formatCalendar(n3, r3);
  })(e2, n2.calendar, n2.timeZone, n2.epochNanoseconds, o2, r2, i2, a2, s2, c2);
}
function Ft(e2, n2) {
  const [t2, o2, r2, i2] = ((e3) => (e3 = normalizeOptions(e3), [ti(e3), ...refineTimeDisplayTuple(e3)]))(n2);
  return a2 = e2.calendar, s2 = t2, c2 = i2, formatIsoDateTimeFields(roundDateTimeToNano(e2, r2, o2), c2) + formatCalendar(a2, s2);
  var a2, s2, c2;
}
function ce(e2, n2) {
  return t2 = e2.calendar, o2 = e2, r2 = refineDateDisplayOptions(n2), formatIsoDateFields(o2) + formatCalendar(t2, r2);
  var t2, o2, r2;
}
function Kt(e2, n2) {
  return formatDateLikeIso(e2.calendar, formatIsoYearMonthFields, e2, refineDateDisplayOptions(n2));
}
function Jt(e2, n2) {
  return formatDateLikeIso(e2.calendar, formatIsoMonthDayFields, e2, refineDateDisplayOptions(n2));
}
function ct(e2, n2) {
  const [t2, o2, r2] = refineTimeDisplayOptions(n2);
  return i2 = r2, formatIsoTimeFields(roundTimeToNano(e2, o2, t2)[0], i2);
  var i2;
}
function k(e2, n2) {
  const [t2, o2, r2] = refineTimeDisplayOptions(n2, 3);
  return o2 > 1 && checkDurationUnits(e2 = {
    ...e2,
    ...roundDayTimeDurationByInc(e2, o2, t2)
  }), ((e3, n3) => {
    const { sign: t3 } = e3, o3 = -1 === t3 ? negateDurationFields(e3) : e3, { hours: r3, minutes: i2 } = o3, [a2, s2] = divModBigNano(durationFieldsToBigNano(o3, 3), Ro, divModTrunc);
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
  const r2 = o2 > 1 || 0 === o2 && e2 !== l;
  return 1 === o2 ? e2 === l ? n2(t2) : formatIsoDateFields(t2) : r2 ? formatIsoDateFields(t2) + formatCalendarId(e2, 2 === o2) : n2(t2);
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
  return formatIsoYearMonthFields(e2) + "-" + bo(e2.isoDay);
}
function formatIsoYearMonthFields(e2) {
  const { isoYear: n2 } = e2;
  return (n2 < 0 || n2 > 9999 ? getSignStr(n2) + padNumber(6, Math.abs(n2)) : padNumber(4, n2)) + "-" + bo(e2.isoMonth);
}
function formatIsoMonthDayFields(e2) {
  return bo(e2.isoMonth) + "-" + bo(e2.isoDay);
}
function formatIsoTimeFields(e2, n2) {
  const t2 = [bo(e2.isoHour), bo(e2.isoMinute)];
  return -1 !== n2 && t2.push(bo(e2.isoSecond) + ((e3, n3, t3, o2) => formatSubsecNano(e3 * Qe + n3 * Yo + t3, o2))(e2.isoMillisecond, e2.isoMicrosecond, e2.isoNanosecond, n2)), t2.join(":");
}
function Se(e2, n2 = 0) {
  if (1 === n2) {
    return "";
  }
  const [t2, o2] = divModFloor(Math.abs(e2), zo), [r2, i2] = divModFloor(o2, Zo), [a2, s2] = divModFloor(i2, Ro);
  return getSignStr(e2) + bo(t2) + ":" + bo(r2) + (a2 || s2 ? ":" + bo(a2) + formatSubsecNano(s2) : "");
}
function formatCalendar(e2, n2) {
  return 1 !== n2 && (n2 > 1 || 0 === n2 && e2 !== l) ? formatCalendarId(e2, 2 === n2) : "";
}
function formatCalendarId(e2, n2) {
  return "[" + (n2 ? "!" : "") + "u-ca=" + e2 + "]";
}
function formatSubsecNano(e2, n2) {
  let t2 = padNumber(9, e2);
  return t2 = void 0 === n2 ? t2.replace(li, "") : t2.slice(0, n2), t2 ? "." + t2 : "";
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
  const { epochNanoseconds: t2 } = e2, o2 = (n2.R ? n2 : n2(e2.timeZone)).R(t2), r2 = epochNanoToIso(t2, o2);
  return {
    calendar: e2.calendar,
    ...r2,
    offsetNanoseconds: o2
  };
}
function getMatchingInstantFor(e2, n2, t2, o2 = 0, r2 = 0, i2, a2) {
  if (void 0 !== t2 && 1 === o2 && (1 === o2 || a2)) {
    return isoToEpochNanoWithOffset(n2, t2);
  }
  const s2 = e2.I(n2);
  if (void 0 !== t2 && 3 !== o2) {
    const e3 = ((e4, n3, t3, o3) => {
      const r3 = isoToEpochNano(n3);
      o3 && (t3 = roundToMinute(t3));
      for (const n4 of e4) {
        let e5 = bigNanoToNumber(diffBigNanos(n4, r3));
        if (o3 && (e5 = roundToMinute(e5)), e5 === t3) {
          return n4;
        }
      }
    })(s2, n2, t2, i2);
    if (void 0 !== e3) {
      return e3;
    }
    if (0 === o2) {
      throw new RangeError(Do);
    }
  }
  return a2 ? isoToEpochNano(n2) : getSingleInstantFor(e2, n2, r2, s2);
}
function getSingleInstantFor(e2, n2, t2 = 0, o2 = e2.I(n2)) {
  if (1 === o2.length) {
    return o2[0];
  }
  if (1 === t2) {
    throw new RangeError(To);
  }
  if (o2.length) {
    return o2[3 === t2 ? 1 : 0];
  }
  const r2 = isoToEpochNano(n2), i2 = ((e3, n3) => {
    const t3 = e3.R(moveBigNano(n3, -Uo));
    return ((e4) => {
      if (e4 > Uo) {
        throw new RangeError(go);
      }
      return e4;
    })(e3.R(moveBigNano(n3, Uo)) - t3);
  })(e2, r2), a2 = i2 * (2 === t2 ? -1 : 1);
  return (o2 = e2.I(epochNanoToIso(r2, a2)))[2 === t2 ? 0 : o2.length - 1];
}
function getStartOfDayInstantFor(e2, n2) {
  const t2 = e2.I(n2);
  if (t2.length) {
    return t2[0];
  }
  const o2 = moveBigNano(isoToEpochNano(n2), -Uo);
  return e2.O(o2, 1);
}
function Ye(e2, n2, t2) {
  return xe(checkEpochNanoInBounds(addBigNanos(n2.epochNanoseconds, ((e3) => {
    if (durationHasDateParts(e3)) {
      throw new RangeError(vo);
    }
    return durationFieldsToBigNano(e3, 5);
  })(e2 ? negateDurationFields(t2) : t2))));
}
function pe(e2, n2, t2, o2, r2, i2 = /* @__PURE__ */ Object.create(null)) {
  const a2 = n2(o2.timeZone), s2 = e2(o2.calendar);
  return {
    ...o2,
    ...moveZonedEpochs(a2, s2, o2, t2 ? negateDurationFields(r2) : r2, i2)
  };
}
function wt(e2, n2, t2, o2, r2 = /* @__PURE__ */ Object.create(null)) {
  const { calendar: i2 } = t2;
  return jt(moveDateTime(e2(i2), t2, n2 ? negateDurationFields(o2) : o2, r2), i2);
}
function ne(e2, n2, t2, o2, r2) {
  const { calendar: i2 } = t2;
  return W(moveDate(e2(i2), t2, n2 ? negateDurationFields(o2) : o2, r2), i2);
}
function Gt(e2, n2, t2, o2, r2) {
  const i2 = t2.calendar, a2 = e2(i2);
  let s2 = checkIsoDateInBounds(moveToDayOfMonthUnsafe(a2, t2));
  n2 && (o2 = B(o2)), o2.sign < 0 && (s2 = a2.P(s2, {
    ...pr,
    months: 1
  }), s2 = moveByDays(s2, -1));
  const c2 = a2.P(s2, o2, r2);
  return createPlainYearMonthSlots(moveToDayOfMonthUnsafe(a2, c2), i2);
}
function at(e2, n2, t2) {
  return St(moveTime(n2, e2 ? negateDurationFields(t2) : t2)[0]);
}
function moveZonedEpochs(e2, n2, t2, o2, r2) {
  const i2 = durationFieldsToBigNano(o2, 5);
  let a2 = t2.epochNanoseconds;
  if (durationHasDateParts(o2)) {
    const s2 = he(t2, e2);
    a2 = addBigNanos(getSingleInstantFor(e2, {
      ...moveDate(n2, s2, {
        ...o2,
        ...hr
      }, r2),
      ...nn(w, s2)
    }), i2);
  } else {
    a2 = addBigNanos(a2, i2), mt(r2);
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
      ...hr,
      days: t2.days + i2
    }, o2),
    ...r2
  });
}
function moveDate(e2, n2, t2, o2) {
  if (t2.years || t2.months || t2.weeks) {
    return e2.P(n2, t2, o2);
  }
  mt(o2);
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
    ...epochMilliToIso(isoToEpochMilli(e2) + n2 * ko)
  } : e2;
}
function createMarkerSystem(e2, n2, t2) {
  const o2 = e2(t2.calendar);
  return isZonedEpochSlots(t2) ? [t2, o2, n2(t2.timeZone)] : [{
    ...t2,
    ...Nt
  }, o2];
}
function createMarkerToEpochNano(e2) {
  return e2 ? extractEpochNano : isoToEpochNano;
}
function createMoveMarker(e2) {
  return e2 ? Pt(moveZonedEpochs, e2) : moveDateTime;
}
function createDiffMarkers(e2) {
  return e2 ? Pt(diffZonedEpochsExact, e2) : diffDateTimesExact;
}
function isZonedEpochSlots(e2) {
  return e2 && e2.epochNanoseconds;
}
function isUniformUnit(e2, n2) {
  return e2 <= 6 - (isZonedEpochSlots(n2) ? 1 : 0);
}
function E(e2, n2, t2, o2, r2, i2, a2) {
  const s2 = e2(normalizeOptions(a2).relativeTo), c2 = Math.max(getMaxDurationUnit(r2), getMaxDurationUnit(i2));
  if (isUniformUnit(c2, s2)) {
    return Oe(checkDurationUnits(((e3, n3, t3, o3) => {
      const r3 = addBigNanos(durationFieldsToBigNano(e3), durationFieldsToBigNano(n3), o3 ? -1 : 1);
      if (!Number.isFinite(r3[0])) {
        throw new RangeError(Io);
      }
      return {
        ...pr,
        ...nanoToDurationDayTimeFields(r3, t3)
      };
    })(r2, i2, c2, o2)));
  }
  if (!s2) {
    throw new RangeError(yo);
  }
  o2 && (i2 = negateDurationFields(i2));
  const [u2, f2, l2] = createMarkerSystem(n2, t2, s2), d2 = createMoveMarker(l2), m2 = createDiffMarkers(l2), p2 = d2(f2, u2, r2);
  return Oe(m2(f2, u2, d2(f2, p2, i2), c2));
}
function V(e2, n2, t2, o2, r2) {
  const i2 = getMaxDurationUnit(o2), [a2, s2, c2, u2, f2] = ((e3, n3, t3) => {
    e3 = normalizeOptionsOrString(e3, Rr);
    let o3 = Kr(e3);
    const r3 = t3(e3[Ar]);
    let i3 = parseRoundingIncInteger(e3);
    const a3 = ii(e3, 7);
    let s3 = Jr(e3);
    if (void 0 === o3 && void 0 === s3) {
      throw new RangeError(Po);
    }
    if (null == s3 && (s3 = 0), null == o3 && (o3 = Math.max(s3, n3)), checkLargestSmallestUnit(o3, s3), i3 = refineRoundingInc(i3, s3, 1), i3 > 1 && s3 > 5 && o3 !== s3) {
      throw new RangeError("For calendar units with roundingIncrement > 1, use largestUnit = smallestUnit");
    }
    return [o3, s3, i3, a3, r3];
  })(r2, i2, e2), l2 = Math.max(i2, a2);
  if (!f2 && l2 <= 6) {
    return Oe(checkDurationUnits(((e3, n3, t3, o3, r3) => {
      const i3 = roundBigNano(durationFieldsToBigNano(e3), t3, o3, r3);
      return {
        ...pr,
        ...nanoToDurationDayTimeFields(i3, n3)
      };
    })(o2, a2, s2, c2, u2)));
  }
  if (!isZonedEpochSlots(f2) && !o2.sign) {
    return o2;
  }
  if (!f2) {
    throw new RangeError(yo);
  }
  const [d2, m2, p2] = createMarkerSystem(n2, t2, f2), h2 = createMarkerToEpochNano(p2), g2 = createMoveMarker(p2), D2 = createDiffMarkers(p2), T2 = g2(m2, d2, o2);
  isZonedEpochSlots(f2) || (checkIsoDateTimeInBounds(d2), checkIsoDateTimeInBounds(T2));
  let I2 = D2(m2, d2, T2, a2);
  const M2 = o2.sign, N2 = computeDurationSign(I2);
  if (M2 && N2 && M2 !== N2) {
    throw new RangeError(fo);
  }
  return I2 = roundRelativeDuration(I2, h2(T2), a2, s2, c2, u2, m2, d2, h2, g2), Oe(I2);
}
function Y(e2) {
  return -1 === e2.sign ? B(e2) : e2;
}
function B(e2) {
  return Oe(negateDurationFields(e2));
}
function negateDurationFields(e2) {
  const n2 = {};
  for (const t2 of p) {
    n2[t2] = -1 * e2[t2] || 0;
  }
  return n2;
}
function y(e2) {
  return !e2.sign;
}
function computeDurationSign(e2, n2 = p) {
  let t2 = 0;
  for (const o2 of n2) {
    const n3 = Math.sign(e2[o2]);
    if (n3) {
      if (t2 && t2 !== n3) {
        throw new RangeError(No);
      }
      t2 = n3;
    }
  }
  return t2;
}
function checkDurationUnits(e2) {
  for (const n2 of dr) {
    clampEntity(n2, e2[n2], -di, di, 1);
  }
  return checkDurationTimeUnit(bigNanoToNumber(durationFieldsToBigNano(e2), Ro)), e2;
}
function checkDurationTimeUnit(e2) {
  if (!Number.isSafeInteger(e2)) {
    throw new RangeError(Mo);
  }
}
function durationFieldsToBigNano(e2, n2 = 6) {
  return givenFieldsToBigNano(e2, n2, p);
}
function nanoToDurationDayTimeFields(e2, n2 = 6) {
  const [t2, o2] = e2, r2 = nanoToGivenFields(o2, n2, p);
  if (r2[p[n2]] += t2 * (Uo / Ao[n2]), !Number.isFinite(r2[p[n2]])) {
    throw new RangeError(Io);
  }
  return r2;
}
function nanoToDurationTimeFields(e2, n2 = 5) {
  return nanoToGivenFields(e2, n2, p);
}
function durationHasDateParts(e2) {
  return Boolean(computeDurationSign(e2, lr));
}
function getMaxDurationUnit(e2) {
  let n2 = 9;
  for (; n2 > 0 && !e2[p[n2]]; n2--) {
  }
  return n2;
}
function createSplitTuple(e2, n2) {
  return [e2, n2];
}
function computePeriod(e2) {
  const n2 = Math.floor(e2 / ci) * ci;
  return [n2, n2 + ci];
}
function We(e2) {
  const n2 = parseDateTimeLike(e2 = toStringViaPrimitive(e2));
  if (!n2) {
    throw new RangeError(failedParse(e2));
  }
  let t2;
  if (n2.j) {
    t2 = 0;
  } else {
    if (!n2.offset) {
      throw new RangeError(failedParse(e2));
    }
    t2 = parseOffsetNano(n2.offset);
  }
  return n2.timeZone && parseOffsetNanoMaybe(n2.timeZone, 1), xe(isoToEpochNanoWithOffset(checkIsoDateTimeFields(n2), t2));
}
function H(e2) {
  const n2 = parseDateTimeLike(m(e2));
  if (!n2) {
    throw new RangeError(failedParse(e2));
  }
  if (n2.timeZone) {
    return finalizeZonedDateTime(n2, n2.offset ? parseOffsetNano(n2.offset) : void 0);
  }
  if (n2.j) {
    throw new RangeError(failedParse(e2));
  }
  return finalizeDate(n2);
}
function Ae(e2, n2) {
  const t2 = parseDateTimeLike(m(e2));
  if (!t2 || !t2.timeZone) {
    throw new RangeError(failedParse(e2));
  }
  const { offset: o2 } = t2, r2 = o2 ? parseOffsetNano(o2) : void 0, [, i2, a2] = je(n2);
  return finalizeZonedDateTime(t2, r2, i2, a2);
}
function parseOffsetNano(e2) {
  const n2 = parseOffsetNanoMaybe(e2);
  if (void 0 === n2) {
    throw new RangeError(failedParse(e2));
  }
  return n2;
}
function Bt(e2) {
  const n2 = parseDateTimeLike(m(e2));
  if (!n2 || n2.j) {
    throw new RangeError(failedParse(e2));
  }
  return jt(finalizeDateTime(n2));
}
function de(e2, n2, t2) {
  let o2 = parseDateTimeLike(m(e2));
  if (!o2 || o2.j) {
    throw new RangeError(failedParse(e2));
  }
  return n2 ? o2.calendar === l && (o2 = -271821 === o2.isoYear && 4 === o2.isoMonth ? {
    ...o2,
    isoDay: 20,
    ...Nt
  } : {
    ...o2,
    isoDay: 1,
    ...Nt
  }) : t2 && o2.calendar === l && (o2 = {
    ...o2,
    isoYear: Br
  }), W(o2.C ? finalizeDateTime(o2) : finalizeDate(o2));
}
function _t(e2, n2) {
  const t2 = parseYearMonthOnly(m(n2));
  if (t2) {
    return requireIsoCalendar(t2), createPlainYearMonthSlots(checkIsoYearMonthInBounds(checkIsoDateFields(t2)));
  }
  const o2 = de(n2, 1);
  return createPlainYearMonthSlots(moveToDayOfMonthUnsafe(e2(o2.calendar), o2));
}
function requireIsoCalendar(e2) {
  if (e2.calendar !== l) {
    throw new RangeError(invalidSubstring(e2.calendar));
  }
}
function xt(e2, n2) {
  const t2 = parseMonthDayOnly(m(n2));
  if (t2) {
    return requireIsoCalendar(t2), createPlainMonthDaySlots(checkIsoDateFields(t2));
  }
  const o2 = de(n2, 0, 1), { calendar: r2 } = o2, i2 = e2(r2), [a2, s2, c2] = i2.v(o2), [u2, f2] = i2.q(a2, s2), [l2, d2] = i2.G(u2, f2, c2);
  return createPlainMonthDaySlots(checkIsoDateInBounds(i2.V(l2, d2, c2)), r2);
}
function ht(e2) {
  let n2, t2 = ((e3) => {
    const n3 = Pi.exec(e3);
    return n3 ? (organizeAnnotationParts(n3[10]), organizeTimeParts(n3)) : void 0;
  })(m(e2));
  if (!t2) {
    if (t2 = parseDateTimeLike(e2), !t2) {
      throw new RangeError(failedParse(e2));
    }
    if (!t2.C) {
      throw new RangeError(failedParse(e2));
    }
    if (t2.j) {
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
  return St(constrainIsoTimeFields(t2, 1));
}
function R(e2) {
  const n2 = ((e3) => {
    const n3 = Fi.exec(e3);
    return n3 ? ((e4) => {
      function parseUnit(e5, r3, i2) {
        let a2 = 0, s2 = 0;
        if (i2 && ([a2, o2] = divModFloor(o2, Ao[i2])), void 0 !== e5) {
          if (t2) {
            throw new RangeError(invalidSubstring(e5));
          }
          s2 = ((e6) => {
            const n5 = parseInt(e6);
            if (!Number.isFinite(n5)) {
              throw new RangeError(invalidSubstring(e6));
            }
            return n5;
          })(e5), n4 = 1, r3 && (o2 = parseSubsecNano(r3) * (Ao[i2] / Ro), t2 = 1);
        }
        return a2 + s2;
      }
      let n4 = 0, t2 = 0, o2 = 0, r2 = {
        ...zipProps(p, [parseUnit(e4[2]), parseUnit(e4[3]), parseUnit(e4[4]), parseUnit(e4[5]), parseUnit(e4[6], e4[7], 5), parseUnit(e4[8], e4[9], 4), parseUnit(e4[10], e4[11], 3)]),
        ...nanoToGivenFields(o2, 2, p)
      };
      if (!n4) {
        throw new RangeError(noValidFields(p));
      }
      return parseSign(e4[1]) < 0 && (r2 = negateDurationFields(r2)), r2;
    })(n3) : void 0;
  })(m(e2));
  if (!n2) {
    throw new RangeError(failedParse(e2));
  }
  return Oe(checkDurationUnits(n2));
}
function f(e2) {
  const n2 = parseDateTimeLike(e2) || parseYearMonthOnly(e2) || parseMonthDayOnly(e2);
  return n2 ? n2.calendar : e2;
}
function Z(e2) {
  const n2 = parseDateTimeLike(e2);
  return n2 && (n2.timeZone || n2.j && si || n2.offset) || e2;
}
function finalizeZonedDateTime(e2, n2, t2 = 0, o2 = 0) {
  const r2 = M(e2.timeZone), i2 = L(r2);
  let a2;
  return checkIsoDateTimeFields(e2), a2 = e2.C ? getMatchingInstantFor(i2, e2, n2, t2, o2, !i2.$, e2.j) : getStartOfDayInstantFor(i2, e2), _e(a2, r2, u(e2.calendar));
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
    calendar: u(e2.calendar)
  };
}
function parseDateTimeLike(e2) {
  const n2 = vi.exec(e2);
  return n2 ? ((e3) => {
    const n3 = e3[10], t2 = "Z" === (n3 || "").toUpperCase();
    return {
      isoYear: organizeIsoYearParts(e3),
      isoMonth: parseInt(e3[4]),
      isoDay: parseInt(e3[5]),
      ...organizeTimeParts(e3.slice(5)),
      ...organizeAnnotationParts(e3[16]),
      C: Boolean(e3[6]),
      j: t2,
      offset: t2 ? void 0 : n3
    };
  })(n2) : void 0;
}
function parseYearMonthOnly(e2) {
  const n2 = Ni.exec(e2);
  return n2 ? ((e3) => ({
    isoYear: organizeIsoYearParts(e3),
    isoMonth: parseInt(e3[4]),
    isoDay: 1,
    ...organizeAnnotationParts(e3[5])
  }))(n2) : void 0;
}
function parseMonthDayOnly(e2) {
  const n2 = yi.exec(e2);
  return n2 ? ((e3) => ({
    isoYear: Br,
    isoMonth: parseInt(e3[1]),
    isoDay: parseInt(e3[2]),
    ...organizeAnnotationParts(e3[3])
  }))(n2) : void 0;
}
function parseOffsetNanoMaybe(e2, n2) {
  const t2 = Ei.exec(e2);
  return t2 ? ((e3, n3) => {
    const t3 = e3[4] || e3[5];
    if (n3 && t3) {
      throw new RangeError(invalidSubstring(t3));
    }
    return ((e4) => {
      if (Math.abs(e4) >= Uo) {
        throw new RangeError(ho);
      }
      return e4;
    })((parseInt0(e3[2]) * zo + parseInt0(e3[3]) * Zo + parseInt0(e3[4]) * Ro + parseSubsecNano(e3[5] || "")) * parseSign(e3[1]));
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
  if (e2.replace(Si, (e3, r2, i2) => {
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
    calendar: o2[0] || l
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
function Ze(e2) {
  return M(m(e2));
}
function M(e2) {
  const n2 = getTimeZoneEssence(e2);
  return "number" == typeof n2 ? Se(n2) : n2 ? ((e3) => {
    if (Oi.test(e3)) {
      throw new RangeError(F(e3));
    }
    if (bi.test(e3)) {
      throw new RangeError(po);
    }
    return e3.toLowerCase().split("/").map((e4, n3) => (e4.length <= 3 || /\d/.test(e4)) && !/etc|yap/.test(e4) ? e4.toUpperCase() : e4.replace(/baja|dumont|[a-z]+/g, (e5, t2) => e5.length <= 2 && !n3 || "in" === e5 || "chat" === e5 ? e5.toUpperCase() : e5.length > 2 || !t2 ? capitalize(e5).replace(/island|noronha|murdo|rivadavia|urville/, capitalize) : e5)).join("/");
  })(e2) : si;
}
function getTimeZoneAtomic(e2) {
  const n2 = getTimeZoneEssence(e2);
  return "number" == typeof n2 ? n2 : n2 ? n2.resolvedOptions().timeZone : si;
}
function getTimeZoneEssence(e2) {
  const n2 = parseOffsetNanoMaybe(e2 = e2.toUpperCase(), 1);
  return void 0 !== n2 ? n2 : e2 !== si ? wi(e2) : void 0;
}
function Ke(e2, n2) {
  return compareBigNanos(e2.epochNanoseconds, n2.epochNanoseconds);
}
function Be(e2, n2) {
  return compareBigNanos(e2.epochNanoseconds, n2.epochNanoseconds);
}
function K(e2, n2, t2, o2, r2, i2) {
  const a2 = e2(normalizeOptions(i2).relativeTo), s2 = Math.max(getMaxDurationUnit(o2), getMaxDurationUnit(r2));
  if (allPropsEqual(p, o2, r2)) {
    return 0;
  }
  if (isUniformUnit(s2, a2)) {
    return compareBigNanos(durationFieldsToBigNano(o2), durationFieldsToBigNano(r2));
  }
  if (!a2) {
    throw new RangeError(yo);
  }
  const [c2, u2, f2] = createMarkerSystem(n2, t2, a2), l2 = createMarkerToEpochNano(f2), d2 = createMoveMarker(f2);
  return compareBigNanos(l2(d2(u2, c2, o2)), l2(d2(u2, c2, r2)));
}
function Yt(e2, n2) {
  return te(e2, n2) || Dt(e2, n2);
}
function te(e2, n2) {
  return compareNumbers(isoToEpochMilli(e2), isoToEpochMilli(n2));
}
function Dt(e2, n2) {
  return compareNumbers(isoTimeFieldsToNano(e2), isoTimeFieldsToNano(n2));
}
function Ve(e2, n2) {
  return !Ke(e2, n2);
}
function Ce(e2, n2) {
  return !Be(e2, n2) && !!isTimeZoneIdsEqual(e2.timeZone, n2.timeZone) && e2.calendar === n2.calendar;
}
function Ct(e2, n2) {
  return !Yt(e2, n2) && e2.calendar === n2.calendar;
}
function re(e2, n2) {
  return !te(e2, n2) && e2.calendar === n2.calendar;
}
function $t(e2, n2) {
  return !te(e2, n2) && e2.calendar === n2.calendar;
}
function Lt(e2, n2) {
  return !te(e2, n2) && e2.calendar === n2.calendar;
}
function st(e2, n2) {
  return !Dt(e2, n2);
}
function isTimeZoneIdsEqual(e2, n2) {
  if (e2 === n2) {
    return 1;
  }
  try {
    return getTimeZoneAtomic(e2) === getTimeZoneAtomic(n2);
  } catch (e3) {
  }
}
function Ee(e2, n2, t2, o2) {
  const r2 = refineDiffOptions(e2, o2, 3, 5), i2 = diffEpochNanos(n2.epochNanoseconds, t2.epochNanoseconds, ...r2);
  return Oe(e2 ? negateDurationFields(i2) : i2);
}
function we(e2, n2, t2, o2, r2, i2) {
  const a2 = getCommonCalendarId(o2.calendar, r2.calendar), [s2, c2, u2, f2] = refineDiffOptions(t2, i2, 5), l2 = o2.epochNanoseconds, d2 = r2.epochNanoseconds, m2 = compareBigNanos(d2, l2);
  let p2;
  if (m2) {
    if (s2 < 6) {
      p2 = diffEpochNanos(l2, d2, s2, c2, u2, f2);
    } else {
      const t3 = n2(((e3, n3) => {
        if (!isTimeZoneIdsEqual(e3, n3)) {
          throw new RangeError(mo);
        }
        return e3;
      })(o2.timeZone, r2.timeZone)), l3 = e2(a2);
      p2 = diffZonedEpochsBig(l3, t3, o2, r2, m2, s2, i2), p2 = roundRelativeDuration(p2, d2, s2, c2, u2, f2, l3, o2, extractEpochNano, Pt(moveZonedEpochs, t3));
    }
  } else {
    p2 = pr;
  }
  return Oe(t2 ? negateDurationFields(p2) : p2);
}
function It(e2, n2, t2, o2, r2) {
  const i2 = getCommonCalendarId(t2.calendar, o2.calendar), [a2, s2, c2, u2] = refineDiffOptions(n2, r2, 6), f2 = isoToEpochNano(t2), l2 = isoToEpochNano(o2), d2 = compareBigNanos(l2, f2);
  let m2;
  if (d2) {
    if (a2 <= 6) {
      m2 = diffEpochNanos(f2, l2, a2, s2, c2, u2);
    } else {
      const n3 = e2(i2);
      m2 = diffDateTimesBig(n3, t2, o2, d2, a2, r2), m2 = roundRelativeDuration(m2, l2, a2, s2, c2, u2, n3, t2, isoToEpochNano, moveDateTime);
    }
  } else {
    m2 = pr;
  }
  return Oe(n2 ? negateDurationFields(m2) : m2);
}
function oe(e2, n2, t2, o2, r2) {
  const i2 = getCommonCalendarId(t2.calendar, o2.calendar);
  return diffDateLike(n2, () => e2(i2), t2, o2, ...refineDiffOptions(n2, r2, 6, 9, 6));
}
function zt(e2, n2, t2, o2, r2) {
  const i2 = getCommonCalendarId(t2.calendar, o2.calendar), a2 = refineDiffOptions(n2, r2, 9, 9, 8), s2 = e2(i2), c2 = moveToDayOfMonthUnsafe(s2, t2), u2 = moveToDayOfMonthUnsafe(s2, o2);
  return c2.isoYear === u2.isoYear && c2.isoMonth === u2.isoMonth && c2.isoDay === u2.isoDay ? Oe(pr) : diffDateLike(n2, () => s2, checkIsoDateInBounds(c2), checkIsoDateInBounds(u2), ...a2, 8);
}
function diffDateLike(e2, n2, t2, o2, r2, i2, a2, s2, c2 = 6) {
  const u2 = isoToEpochNano(t2), f2 = isoToEpochNano(o2);
  if (void 0 === u2 || void 0 === f2) {
    throw new RangeError(Io);
  }
  let l2;
  if (compareBigNanos(f2, u2)) {
    if (6 === r2) {
      l2 = diffEpochNanos(u2, f2, r2, i2, a2, s2);
    } else {
      const e3 = n2();
      l2 = e3.N(t2, o2, r2), i2 === c2 && 1 === a2 || (l2 = roundRelativeDuration(l2, f2, r2, i2, a2, s2, e3, t2, isoToEpochNano, moveDate));
    }
  } else {
    l2 = pr;
  }
  return Oe(e2 ? negateDurationFields(l2) : l2);
}
function it(e2, n2, t2, o2) {
  const [r2, i2, a2, s2] = refineDiffOptions(e2, o2, 5, 5), c2 = roundByInc(diffTimes(n2, t2), computeNanoInc(i2, a2), s2), u2 = {
    ...pr,
    ...nanoToDurationTimeFields(c2, r2)
  };
  return Oe(e2 ? negateDurationFields(u2) : u2);
}
function diffZonedEpochsExact(e2, n2, t2, o2, r2, i2) {
  const a2 = compareBigNanos(o2.epochNanoseconds, t2.epochNanoseconds);
  return a2 ? r2 < 6 ? diffEpochNanosExact(t2.epochNanoseconds, o2.epochNanoseconds, r2) : diffZonedEpochsBig(n2, e2, t2, o2, a2, r2, i2) : pr;
}
function diffDateTimesExact(e2, n2, t2, o2, r2) {
  const i2 = isoToEpochNano(n2), a2 = isoToEpochNano(t2), s2 = compareBigNanos(a2, i2);
  return s2 ? o2 <= 6 ? diffEpochNanosExact(i2, a2, o2) : diffDateTimesBig(e2, n2, t2, s2, o2, r2) : pr;
}
function diffZonedEpochsBig(e2, n2, t2, o2, r2, i2, a2) {
  const [s2, c2, u2] = ((e3, n3, t3, o3) => {
    function updateMid() {
      return f3 = {
        ...moveByDays(a3, c3++ * -o3),
        ...i3
      }, l3 = getSingleInstantFor(e3, f3), compareBigNanos(s3, l3) === -o3;
    }
    const r3 = he(n3, e3), i3 = nn(w, r3), a3 = he(t3, e3), s3 = t3.epochNanoseconds;
    let c3 = 0;
    const u3 = diffTimes(r3, a3);
    let f3, l3;
    if (Math.sign(u3) === -o3 && c3++, updateMid() && (-1 === o3 || updateMid())) {
      throw new RangeError(fo);
    }
    const d2 = bigNanoToNumber(diffBigNanos(l3, s3));
    return [r3, f3, d2];
  })(n2, t2, o2, r2);
  var f2, l2;
  return {
    ...6 === i2 ? (f2 = s2, l2 = c2, {
      ...pr,
      days: diffDays(f2, l2)
    }) : e2.N(s2, c2, i2, a2),
    ...nanoToDurationTimeFields(u2)
  };
}
function diffDateTimesBig(e2, n2, t2, o2, r2, i2) {
  const [a2, s2, c2] = ((e3, n3, t3) => {
    let o3 = n3, r3 = diffTimes(e3, n3);
    return Math.sign(r3) === -t3 && (o3 = moveByDays(n3, -t3), r3 += Uo * t3), [e3, o3, r3];
  })(n2, t2, o2);
  return {
    ...e2.N(a2, s2, r2, i2),
    ...nanoToDurationTimeFields(c2)
  };
}
function diffEpochNanos(e2, n2, t2, o2, r2, i2) {
  return {
    ...pr,
    ...nanoToDurationDayTimeFields(roundBigNano(diffBigNanos(e2, n2), o2, r2, i2), t2)
  };
}
function diffEpochNanosExact(e2, n2, t2) {
  return {
    ...pr,
    ...nanoToDurationDayTimeFields(diffBigNanos(e2, n2), t2)
  };
}
function diffDays(e2, n2) {
  return diffEpochMilliByDay(isoToEpochMilli(e2), isoToEpochMilli(n2));
}
function diffEpochMilliByDay(e2, n2) {
  return Math.trunc((n2 - e2) / ko);
}
function diffTimes(e2, n2) {
  return isoTimeFieldsToNano(n2) - isoTimeFieldsToNano(e2);
}
function getCommonCalendarId(e2, n2) {
  if (e2 !== n2) {
    throw new RangeError(lo);
  }
  return e2;
}
function computeNativeWeekOfYear(e2) {
  return this.m(e2)[0];
}
function computeNativeYearOfWeek(e2) {
  return this.m(e2)[1];
}
function computeNativeDayOfYear(e2) {
  const [n2] = this.v(e2);
  return diffEpochMilliByDay(this.p(n2), isoToEpochMilli(e2)) + 1;
}
function parseMonthCode(e2) {
  const n2 = Bi.exec(e2);
  if (!n2) {
    throw new RangeError(invalidMonthCode(e2));
  }
  return [parseInt(n2[1]), Boolean(n2[2])];
}
function formatMonthCode(e2, n2) {
  return "M" + bo(e2) + (n2 ? "L" : "");
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
  return ir[getCalendarIdBase(e2)];
}
function getCalendarLeapMonthMeta(e2) {
  return sr[getCalendarIdBase(e2)];
}
function getCalendarIdBase(e2) {
  return computeCalendarIdBase(e2.id || l);
}
function createIntlCalendar(e2) {
  function epochMilliToIntlFields(e3) {
    return ((e4, n3) => ({
      ...parseIntlYear(e4, n3),
      o: e4.month,
      day: parseInt(e4.day)
    }))(hashIntlFormatParts(n2, e3), t2);
  }
  const n2 = Ci(e2), t2 = computeCalendarIdBase(e2);
  return {
    id: e2,
    h: createIntlFieldCache(epochMilliToIntlFields),
    l: createIntlYearDataCache(epochMilliToIntlFields)
  };
}
function createIntlFieldCache(e2) {
  return on((n2) => {
    const t2 = isoToEpochMilli(n2);
    return e2(t2);
  }, WeakMap);
}
function createIntlYearDataCache(e2) {
  const n2 = e2(0).year - Or;
  return on((t2) => {
    let o2, r2 = isoArgsToEpochMilli(t2 - n2), i2 = 0;
    const a2 = [], s2 = [];
    do {
      r2 += 400 * ko;
    } while ((o2 = e2(r2)).year <= t2);
    do {
      if (r2 += (1 - o2.day) * ko, o2.year === t2 && (a2.push(r2), s2.push(o2.o)), r2 -= ko, ++i2 > 100 || r2 < -Pr) {
        throw new RangeError(fo);
      }
    } while ((o2 = e2(r2)).year >= t2);
    return {
      i: a2.reverse(),
      u: Fo(s2.reverse())
    };
  });
}
function parseIntlYear(e2, n2) {
  let t2, o2, r2 = parseIntlPartsYear(e2);
  if (e2.era) {
    const i2 = ir[n2], a2 = ar[n2] || {};
    void 0 !== i2 && (t2 = "islamic" === n2 ? "ah" : e2.era.normalize("NFD").toLowerCase().replace(/[^a-z0-9]/g, ""), "bc" === t2 || "b" === t2 ? t2 = "bce" : "ad" === t2 || "a" === t2 ? t2 = "ce" : "beforeroc" === t2 && (t2 = "broc"), t2 = a2[t2] || t2, o2 = r2, r2 = eraYearToYear(o2, i2[t2] || 0));
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
  const { year: n2, o: t2, day: o2 } = this.h(e2), { u: r2 } = this.l(n2);
  return [n2, r2[t2] + 1, o2];
}
function computeIntlEpochMilli(e2, n2 = 1, t2 = 1) {
  return this.l(e2).i[n2 - 1] + (t2 - 1) * ko;
}
function computeIntlMonthCodeParts(e2, n2) {
  const t2 = computeIntlLeapMonth.call(this, e2);
  return [monthToMonthCodeNumber(n2, t2), t2 === n2];
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
  const { i: t2 } = this.l(e2);
  let o2 = n2 + 1, r2 = t2;
  return o2 > t2.length && (o2 = 1, r2 = this.l(e2 + 1).i), diffEpochMilliByDay(t2[n2 - 1], r2[o2 - 1]);
}
function computeIntlMonthsInYear(e2) {
  return this.l(e2).i.length;
}
function computeIntlEraParts(e2) {
  const n2 = this.h(e2);
  return [n2.era, n2.eraYear];
}
function queryMonthStrings(e2, n2) {
  return Object.keys(e2.l(n2).u);
}
function Mt(e2) {
  return u(m(e2));
}
function u(e2) {
  if ((e2 = e2.toLowerCase()) !== l && e2 !== or) {
    const n2 = Ci(e2).resolvedOptions().calendar;
    if (computeCalendarIdBase(e2) !== computeCalendarIdBase(n2)) {
      throw new RangeError(c(e2));
    }
    return n2;
  }
  return e2;
}
function computeCalendarIdBase(e2) {
  return "islamicc" === e2 && (e2 = "islamic"), e2.split("-")[0];
}
function createNativeOpsCreator(e2, n2) {
  return (t2) => t2 === l ? e2 : t2 === or || t2 === rr ? Object.assign(Object.create(e2), {
    id: t2
  }) : Object.assign(Object.create(n2), ki(t2));
}
function $(e2, n2, t2, o2) {
  const r2 = refineCalendarFields(t2, o2, Xo, [], xo);
  if (void 0 !== r2.timeZone) {
    const o3 = t2.F(r2), i2 = refineTimeBag(r2), a2 = e2(r2.timeZone);
    return {
      epochNanoseconds: getMatchingInstantFor(n2(a2), {
        ...o3,
        ...i2
      }, void 0 !== r2.offset ? parseOffsetNano(r2.offset) : void 0),
      timeZone: a2
    };
  }
  return {
    ...t2.F(r2),
    ...Nt
  };
}
function Ne(e2, n2, t2, o2, r2, i2) {
  const a2 = refineCalendarFields(t2, r2, Xo, jo, xo), s2 = e2(a2.timeZone), [c2, u2, f2] = je(i2), l2 = t2.F(a2, fabricateOverflowOptions(c2)), d2 = refineTimeBag(a2, c2);
  return _e(getMatchingInstantFor(n2(s2), {
    ...l2,
    ...d2
  }, void 0 !== a2.offset ? parseOffsetNano(a2.offset) : void 0, u2, f2), s2, o2);
}
function At(e2, n2, t2) {
  const o2 = refineCalendarFields(e2, n2, Xo, [], O), r2 = mt(t2);
  return jt(checkIsoDateTimeInBounds({
    ...e2.F(o2, fabricateOverflowOptions(r2)),
    ...refineTimeBag(o2, r2)
  }));
}
function me(e2, n2, t2, o2 = []) {
  const r2 = refineCalendarFields(e2, n2, Xo, o2);
  return e2.F(r2, t2);
}
function Xt(e2, n2, t2, o2) {
  const r2 = refineCalendarFields(e2, n2, Ko, o2);
  return e2.K(r2, t2);
}
function Rt(e2, n2, t2, o2) {
  const r2 = refineCalendarFields(e2, t2, Xo, Jo);
  return n2 && void 0 !== r2.month && void 0 === r2.monthCode && void 0 === r2.year && (r2.year = Br), e2._(r2, o2);
}
function Tt(e2, n2) {
  return St(refineTimeBag(refineFields(e2, qo, [], 1), mt(n2)));
}
function q(e2) {
  const n2 = refineFields(e2, ur);
  return Oe(checkDurationUnits({
    ...pr,
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
      throw new RangeError(forbiddenField(o3));
    }
    let n3 = e2[o3];
    if (void 0 !== n3) {
      a2 = 1, Li[o3] && (n3 = Li[o3](n3, o3)), r2[o3] = n3;
    } else if (t2) {
      if (t2.includes(o3)) {
        throw new TypeError(missingField(o3));
      }
      r2[o3] = tr[o3];
    }
    i2 = o3;
  }
  if (o2 && !a2) {
    throw new TypeError(noValidFields(n2));
  }
  return r2;
}
function refineTimeBag(e2, n2) {
  return constrainIsoTimeFields(xi({
    ...tr,
    ...e2
  }), n2);
}
function De(e2, n2, t2, o2, r2) {
  const { calendar: i2, timeZone: a2 } = t2, s2 = e2(i2), c2 = n2(a2), u2 = [...s2.fields(Xo), ...Lo].sort(), f2 = ((e3) => {
    const n3 = he(e3, L), t3 = Se(n3.offsetNanoseconds), o3 = ji(e3.calendar), [r3, i3, a3] = o3.v(n3), [s3, c3] = o3.q(r3, i3), u3 = formatMonthCode(s3, c3);
    return {
      ...$i(n3),
      year: r3,
      monthCode: u3,
      day: a3,
      offset: t3
    };
  })(t2), l2 = refineFields(o2, u2), d2 = s2.k(f2, l2), m2 = {
    ...f2,
    ...l2
  }, [p2, h2, g2] = je(r2, 2);
  return _e(getMatchingInstantFor(c2, {
    ...s2.F(d2, fabricateOverflowOptions(p2)),
    ...constrainIsoTimeFields(xi(m2), p2)
  }, parseOffsetNano(m2.offset), h2, g2), a2, i2);
}
function gt(e2, n2, t2, o2) {
  const r2 = e2(n2.calendar), i2 = [...r2.fields(Xo), ...O].sort(), a2 = {
    ...computeDateEssentials(s2 = n2),
    hour: s2.isoHour,
    minute: s2.isoMinute,
    second: s2.isoSecond,
    millisecond: s2.isoMillisecond,
    microsecond: s2.isoMicrosecond,
    nanosecond: s2.isoNanosecond
  };
  var s2;
  const c2 = refineFields(t2, i2), u2 = mt(o2), f2 = r2.k(a2, c2), l2 = {
    ...a2,
    ...c2
  };
  return jt(checkIsoDateTimeInBounds({
    ...r2.F(f2, fabricateOverflowOptions(u2)),
    ...constrainIsoTimeFields(xi(l2), u2)
  }));
}
function ee(e2, n2, t2, o2) {
  const r2 = e2(n2.calendar), i2 = r2.fields(Xo).sort(), a2 = computeDateEssentials(n2), s2 = refineFields(t2, i2), c2 = r2.k(a2, s2);
  return r2.F(c2, o2);
}
function Wt(e2, n2, t2, o2) {
  const r2 = e2(n2.calendar), i2 = r2.fields(Ko).sort(), a2 = ((e3) => {
    const n3 = ji(e3.calendar), [t3, o3] = n3.v(e3), [r3, i3] = n3.q(t3, o3);
    return {
      year: t3,
      monthCode: formatMonthCode(r3, i3)
    };
  })(n2), s2 = refineFields(t2, i2), c2 = r2.k(a2, s2);
  return r2.K(c2, o2);
}
function Et(e2, n2, t2, o2) {
  const r2 = e2(n2.calendar), i2 = r2.fields(Xo).sort(), a2 = ((e3) => {
    const n3 = ji(e3.calendar), [t3, o3, r3] = n3.v(e3), [i3, a3] = n3.q(t3, o3);
    return {
      monthCode: formatMonthCode(i3, a3),
      day: r3
    };
  })(n2), s2 = refineFields(t2, i2), c2 = r2.k(a2, s2);
  return r2._(c2, o2);
}
function rt(e2, n2, t2) {
  return St(((e3, n3, t3) => refineTimeBag({
    ...nn(qo, e3),
    ...refineFields(n3, qo)
  }, mt(t3)))(e2, n2, t2));
}
function A(e2, n2) {
  return Oe((t2 = e2, o2 = n2, checkDurationUnits({
    ...t2,
    ...refineFields(o2, ur)
  })));
  var t2, o2;
}
function convertToIso(e2, n2, t2, o2, r2) {
  n2 = nn(t2 = e2.fields(t2), n2), o2 = refineFields(o2, r2 = e2.fields(r2), []);
  let i2 = e2.k(n2, o2);
  return i2 = refineFields(i2, [...t2, ...r2].sort(), []), e2.F(i2);
}
function refineYear(e2, n2) {
  const t2 = getCalendarEraOrigins(e2), o2 = ar[e2.id || ""] || {};
  let { era: r2, eraYear: i2, year: a2 } = n2;
  if (void 0 !== r2 || void 0 !== i2) {
    if (void 0 === r2 || void 0 === i2) {
      throw new TypeError(io);
    }
    if (!t2) {
      throw new RangeError(ro);
    }
    const e3 = t2[o2[r2] || r2];
    if (void 0 === e3) {
      throw new RangeError(invalidEra(r2));
    }
    const n3 = eraYearToYear(i2, e3);
    if (void 0 !== a2 && a2 !== n3) {
      throw new RangeError(ao);
    }
    a2 = n3;
  } else if (void 0 === a2) {
    throw new TypeError(missingYear(t2));
  }
  return a2;
}
function refineMonth(e2, n2, t2, o2) {
  let { month: r2, monthCode: i2 } = n2;
  if (void 0 !== i2) {
    const n3 = ((e3, n4, t3, o3) => {
      const r3 = e3.L(t3), [i3, a2] = parseMonthCode(n4);
      let s2 = monthCodeNumberToMonth(i3, a2, r3);
      if (a2) {
        const n5 = getCalendarLeapMonthMeta(e3);
        if (void 0 === n5) {
          throw new RangeError(uo);
        }
        if (n5 > 0) {
          if (s2 > n5) {
            throw new RangeError(uo);
          }
          if (void 0 === r3) {
            if (1 === o3) {
              throw new RangeError(uo);
            }
            s2--;
          }
        } else {
          if (s2 !== -n5) {
            throw new RangeError(uo);
          }
          if (void 0 === r3 && 1 === o3) {
            throw new RangeError(uo);
          }
        }
      }
      return s2;
    })(e2, i2, t2, o2);
    if (void 0 !== r2 && r2 !== n3) {
      throw new RangeError(so);
    }
    r2 = n3, o2 = 1;
  } else if (void 0 === r2) {
    throw new TypeError(co);
  }
  return clampEntity("month", r2, 1, e2.B(t2), o2);
}
function refineDay(e2, n2, t2, o2, r2) {
  return clampProp(n2, "day", 1, e2.U(o2, t2), r2);
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
function computeDateEssentials(e2) {
  const n2 = ji(e2.calendar), [t2, o2, r2] = n2.v(e2), [i2, a2] = n2.q(t2, o2);
  return {
    year: t2,
    monthCode: formatMonthCode(i2, a2),
    day: r2
  };
}
function qe(e2) {
  return xe(checkEpochNanoInBounds(bigIntToBigNano(toBigInt(e2))));
}
function ye(e2, n2, t2, o2, r2 = l) {
  return _e(checkEpochNanoInBounds(bigIntToBigNano(toBigInt(t2))), n2(o2), e2(r2));
}
function Zt(n2, t2, o2, r2, i2 = 0, a2 = 0, s2 = 0, c2 = 0, u2 = 0, f2 = 0, d2 = l) {
  return jt(checkIsoDateTimeInBounds(checkIsoDateTimeFields(e(toInteger, zipProps(Tr, [t2, o2, r2, i2, a2, s2, c2, u2, f2])))), n2(d2));
}
function ue(n2, t2, o2, r2, i2 = l) {
  return W(checkIsoDateInBounds(checkIsoDateFields(e(toInteger, {
    isoYear: t2,
    isoMonth: o2,
    isoDay: r2
  }))), n2(i2));
}
function Qt(e2, n2, t2, o2 = l, r2 = 1) {
  const i2 = toInteger(n2), a2 = toInteger(t2), s2 = e2(o2);
  return createPlainYearMonthSlots(checkIsoYearMonthInBounds(checkIsoDateFields({
    isoYear: i2,
    isoMonth: a2,
    isoDay: toInteger(r2)
  })), s2);
}
function kt(e2, n2, t2, o2 = l, r2 = Br) {
  const i2 = toInteger(n2), a2 = toInteger(t2), s2 = e2(o2);
  return createPlainMonthDaySlots(checkIsoDateInBounds(checkIsoDateFields({
    isoYear: toInteger(r2),
    isoMonth: i2,
    isoDay: a2
  })), s2);
}
function ut(n2 = 0, t2 = 0, o2 = 0, r2 = 0, i2 = 0, a2 = 0) {
  return St(constrainIsoTimeFields(e(toInteger, zipProps(w, [n2, t2, o2, r2, i2, a2])), 1));
}
function j(n2 = 0, t2 = 0, o2 = 0, r2 = 0, i2 = 0, a2 = 0, s2 = 0, c2 = 0, u2 = 0, f2 = 0) {
  return Oe(checkDurationUnits(e(toStrictInteger, zipProps(p, [n2, t2, o2, r2, i2, a2, s2, c2, u2, f2]))));
}
function Je(e2, n2, t2 = l) {
  return _e(e2.epochNanoseconds, n2, t2);
}
function be(e2) {
  return xe(e2.epochNanoseconds);
}
function yt(e2, n2) {
  return jt(he(n2, e2));
}
function fe(e2, n2) {
  return W(he(n2, e2));
}
function dt(e2, n2) {
  return St(he(n2, e2));
}
function bt(e2, n2, t2, o2) {
  const r2 = ((e3, n3, t3, o3) => {
    const r3 = ((e4) => ei(normalizeOptions(e4)))(o3);
    return getSingleInstantFor(e3(n3), t3, r3);
  })(e2, t2, n2, o2);
  return _e(checkEpochNanoInBounds(r2), t2, n2.calendar);
}
function ae(e2, n2, t2, o2, r2) {
  const i2 = e2(r2.timeZone), a2 = r2.plainTime, s2 = void 0 !== a2 ? n2(a2) : void 0, c2 = t2(i2);
  let u2;
  return u2 = s2 ? getSingleInstantFor(c2, {
    ...o2,
    ...s2
  }) : getStartOfDayInstantFor(c2, {
    ...o2,
    ...Nt
  }), _e(u2, i2, o2.calendar);
}
function ie(e2, n2 = Nt) {
  return jt(checkIsoDateTimeInBounds({
    ...e2,
    ...n2
  }));
}
function le(e2, n2, t2) {
  return ((e3, n3) => {
    const t3 = refineCalendarFields(e3, n3, Qo);
    return e3.K(t3, void 0);
  })(e2(n2.calendar), t2);
}
function se(e2, n2, t2) {
  return ((e3, n3) => {
    const t3 = refineCalendarFields(e3, n3, nr);
    return e3._(t3);
  })(e2(n2.calendar), t2);
}
function Ht(e2, n2, t2, o2) {
  return ((e3, n3, t3) => convertToIso(e3, n3, Qo, requireObjectLike(t3), Jo))(e2(n2.calendar), t2, o2);
}
function Vt(e2, n2, t2, o2) {
  return ((e3, n3, t3) => convertToIso(e3, n3, nr, requireObjectLike(t3), Go))(e2(n2.calendar), t2, o2);
}
function $e(e2) {
  return xe(checkEpochNanoInBounds(Ge(toStrictInteger(e2), Qe)));
}
function He(e2) {
  return xe(checkEpochNanoInBounds(bigIntToBigNano(toBigInt(e2))));
}
function createOptionsTransformer(e2, n2, t2) {
  const o2 = new Set(t2);
  return (r2, i2) => {
    const a2 = t2 && hasAnyPropsByName(r2, t2);
    if (!hasAnyPropsByName(r2 = ((e3, n3) => {
      const t3 = {};
      for (const o3 in n3) {
        e3.has(o3) || (t3[o3] = n3[o3]);
      }
      return t3;
    })(o2, r2), e2)) {
      if (i2 && a2) {
        throw new TypeError("Invalid formatting options");
      }
      r2 = {
        ...n2,
        ...r2
      };
    }
    return t2 && (r2.timeZone = si, ["full", "long"].includes(r2.J) && (r2.J = "medium")), r2;
  };
}
function Q(e2, n2 = an, t2 = 0) {
  const [o2, , , r2] = e2;
  return (i2, a2 = Na, ...s2) => {
    const c2 = n2(r2 && r2(...s2), i2, a2, o2, t2), u2 = c2.resolvedOptions();
    return [c2, ...toEpochMillis(e2, u2, s2)];
  };
}
function an(e2, n2, t2, o2, r2) {
  if (t2 = o2(t2, r2), e2) {
    if (void 0 !== t2.timeZone) {
      throw new TypeError(So);
    }
    t2.timeZone = e2;
  }
  return new en(n2, t2);
}
function toEpochMillis(e2, n2, t2) {
  const [, o2, r2] = e2;
  return t2.map((e3) => (e3.calendar && ((e4, n3, t3) => {
    if ((t3 || e4 !== l) && e4 !== n3) {
      throw new RangeError(lo);
    }
  })(e3.calendar, n2.calendar, r2), o2(e3, n2)));
}
function ge(e2, n2, t2) {
  const o2 = n2.timeZone, r2 = e2(o2), i2 = {
    ...he(n2, r2),
    ...t2 || Nt
  };
  let a2;
  return a2 = t2 ? getMatchingInstantFor(r2, i2, i2.offsetNanoseconds, 2) : getStartOfDayInstantFor(r2, i2), _e(a2, o2, n2.calendar);
}
function Ot(e2, n2 = Nt) {
  return jt(checkIsoDateTimeInBounds({
    ...e2,
    ...n2
  }));
}
function pt(e2, n2) {
  return {
    ...e2,
    calendar: n2
  };
}
function Pe(e2, n2) {
  return {
    ...e2,
    timeZone: n2
  };
}
function tn(e2) {
  const n2 = Xe();
  return epochNanoToIso(n2, e2.R(n2));
}
function Xe() {
  return Ge(Date.now(), Qe);
}
function Ue() {
  return va || (va = new en().resolvedOptions().timeZone);
}
var expectedInteger = (e2, n2) => `Non-integer ${e2}: ${n2}`;
var expectedPositive = (e2, n2) => `Non-positive ${e2}: ${n2}`;
var expectedFinite = (e2, n2) => `Non-finite ${e2}: ${n2}`;
var forbiddenBigIntToNumber = (e2) => `Cannot convert bigint to ${e2}`;
var invalidBigInt = (e2) => `Invalid bigint: ${e2}`;
var no = "Cannot convert Symbol to string";
var oo = "Invalid object";
var numberOutOfRange = (e2, n2, t2, o2, r2) => r2 ? numberOutOfRange(e2, r2[n2], r2[t2], r2[o2]) : invalidEntity(e2, n2) + `; must be between ${t2}-${o2}`;
var invalidEntity = (e2, n2) => `Invalid ${e2}: ${n2}`;
var missingField = (e2) => `Missing ${e2}`;
var forbiddenField = (e2) => `Invalid field ${e2}`;
var duplicateFields = (e2) => `Duplicate field ${e2}`;
var noValidFields = (e2) => "No valid fields: " + e2.join();
var i = "Invalid bag";
var invalidChoice = (e2, n2, t2) => invalidEntity(e2, n2) + "; must be " + Object.keys(t2).join();
var b = "Cannot use valueOf";
var a = "Invalid calling context";
var ro = "Forbidden era/eraYear";
var io = "Mismatching era/eraYear";
var ao = "Mismatching year/eraYear";
var invalidEra = (e2) => `Invalid era: ${e2}`;
var missingYear = (e2) => "Missing year" + (e2 ? "/era/eraYear" : "");
var invalidMonthCode = (e2) => `Invalid monthCode: ${e2}`;
var so = "Mismatching month/monthCode";
var co = "Missing month/monthCode";
var uo = "Invalid leap month";
var fo = "Invalid protocol results";
var c = (e2) => invalidEntity("Calendar", e2);
var lo = "Mismatching Calendars";
var F = (e2) => invalidEntity("TimeZone", e2);
var mo = "Mismatching TimeZones";
var po = "Forbidden ICU TimeZone";
var ho = "Out-of-bounds offset";
var go = "Out-of-bounds TimeZone gap";
var Do = "Invalid TimeZone offset";
var To = "Ambiguous offset";
var Io = "Out-of-bounds date";
var Mo = "Out-of-bounds duration";
var No = "Cannot mix duration signs";
var yo = "Missing relativeTo";
var vo = "Cannot use large units";
var Po = "Required smallestUnit or largestUnit";
var Eo = "smallestUnit > largestUnit";
var failedParse = (e2) => `Cannot parse: ${e2}`;
var invalidSubstring = (e2) => `Invalid substring: ${e2}`;
var rn = (e2) => `Cannot format ${e2}`;
var ln = "Mismatching types for formatting";
var So = "Cannot specify TimeZone";
var Fo = /* @__PURE__ */ Pt(g, (e2, n2) => n2);
var wo = /* @__PURE__ */ Pt(g, (e2, n2, t2) => t2);
var bo = /* @__PURE__ */ Pt(padNumber, 2);
var Oo = {
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
var Bo = /* @__PURE__ */ Object.keys(Oo);
var ko = 864e5;
var Co = 1e3;
var Yo = 1e3;
var Qe = 1e6;
var Ro = 1e9;
var Zo = 6e10;
var zo = 36e11;
var Uo = 864e11;
var Ao = [1, Yo, Qe, Ro, Zo, zo, Uo];
var O = /* @__PURE__ */ Bo.slice(0, 6);
var qo = /* @__PURE__ */ sortStrings(O);
var Wo = ["offset"];
var jo = ["timeZone"];
var Lo = /* @__PURE__ */ O.concat(Wo);
var xo = /* @__PURE__ */ Lo.concat(jo);
var $o = ["era", "eraYear"];
var Ho = /* @__PURE__ */ $o.concat(["year"]);
var Go = ["year"];
var Vo = ["monthCode"];
var _o = /* @__PURE__ */ ["month"].concat(Vo);
var Jo = ["day"];
var Ko = /* @__PURE__ */ _o.concat(Go);
var Qo = /* @__PURE__ */ Vo.concat(Go);
var Xo = /* @__PURE__ */ Jo.concat(Ko);
var er = /* @__PURE__ */ Jo.concat(_o);
var nr = /* @__PURE__ */ Jo.concat(Vo);
var tr = /* @__PURE__ */ wo(O, 0);
var l = "iso8601";
var or = "gregory";
var rr = "japanese";
var ir = {
  [or]: {
    "gregory-inverse": -1,
    gregory: 0
  },
  [rr]: {
    "japanese-inverse": -1,
    japanese: 0,
    meiji: 1867,
    taisho: 1911,
    showa: 1925,
    heisei: 1988,
    reiwa: 2018
  },
  ethiopic: {
    ethioaa: 0,
    ethiopic: 5500
  },
  coptic: {
    "coptic-inverse": -1,
    coptic: 0
  },
  roc: {
    "roc-inverse": -1,
    roc: 0
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
var ar = {
  [or]: {
    bce: "gregory-inverse",
    ce: "gregory"
  },
  [rr]: {
    bce: "japanese-inverse",
    ce: "japanese"
  },
  ethiopic: {
    era0: "ethioaa",
    era1: "ethiopic"
  },
  coptic: {
    era0: "coptic-inverse",
    era1: "coptic"
  },
  roc: {
    broc: "roc-inverse",
    minguo: "roc"
  }
};
var sr = {
  chinese: 13,
  dangi: 13,
  hebrew: -6
};
var m = /* @__PURE__ */ Pt(requireType, "string");
var D = /* @__PURE__ */ Pt(requireType, "boolean");
var cr = /* @__PURE__ */ Pt(requireType, "number");
var p = /* @__PURE__ */ Bo.map((e2) => e2 + "s");
var ur = /* @__PURE__ */ sortStrings(p);
var fr = /* @__PURE__ */ p.slice(0, 6);
var lr = /* @__PURE__ */ p.slice(6);
var dr = /* @__PURE__ */ lr.slice(1);
var mr = /* @__PURE__ */ Fo(p);
var pr = /* @__PURE__ */ wo(p, 0);
var hr = /* @__PURE__ */ wo(fr, 0);
var gr = /* @__PURE__ */ Pt(zeroOutProps, p);
var w = ["isoNanosecond", "isoMicrosecond", "isoMillisecond", "isoSecond", "isoMinute", "isoHour"];
var Dr = ["isoDay", "isoMonth", "isoYear"];
var Tr = /* @__PURE__ */ w.concat(Dr);
var Ir = /* @__PURE__ */ sortStrings(Dr);
var Mr = /* @__PURE__ */ sortStrings(w);
var Nr = /* @__PURE__ */ sortStrings(Tr);
var Nt = /* @__PURE__ */ wo(Mr, 0);
var yr = /* @__PURE__ */ Pt(zeroOutProps, Tr);
var vr = 1e8;
var Pr = vr * ko;
var Er = [vr, 0];
var Sr = [-vr, 0];
var Fr = 275760;
var wr = -271821;
var en = Intl.DateTimeFormat;
var br = "en-GB";
var Or = 1970;
var Br = 1972;
var kr = 12;
var Cr = /* @__PURE__ */ isoArgsToEpochMilli(1868, 9, 8);
var Yr = /* @__PURE__ */ on(computeJapaneseEraParts, WeakMap);
var Rr = "smallestUnit";
var Zr = "unit";
var zr = "roundingIncrement";
var Ur = "fractionalSecondDigits";
var Ar = "relativeTo";
var qr = "direction";
var Wr = {
  constrain: 0,
  reject: 1
};
var jr = /* @__PURE__ */ Object.keys(Wr);
var Lr = {
  compatible: 0,
  reject: 1,
  earlier: 2,
  later: 3
};
var xr = {
  reject: 0,
  use: 1,
  prefer: 2,
  ignore: 3
};
var $r = {
  auto: 0,
  never: 1,
  critical: 2,
  always: 3
};
var Hr = {
  auto: 0,
  never: 1,
  critical: 2
};
var Gr = {
  auto: 0,
  never: 1
};
var Vr = {
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
var _r = {
  previous: -1,
  next: 1
};
var Jr = /* @__PURE__ */ Pt(refineUnitOption, Rr);
var Kr = /* @__PURE__ */ Pt(refineUnitOption, "largestUnit");
var Qr = /* @__PURE__ */ Pt(refineUnitOption, Zr);
var Xr = /* @__PURE__ */ Pt(refineChoiceOption, "overflow", Wr);
var ei = /* @__PURE__ */ Pt(refineChoiceOption, "disambiguation", Lr);
var ni = /* @__PURE__ */ Pt(refineChoiceOption, "offset", xr);
var ti = /* @__PURE__ */ Pt(refineChoiceOption, "calendarName", $r);
var oi = /* @__PURE__ */ Pt(refineChoiceOption, "timeZoneName", Hr);
var ri = /* @__PURE__ */ Pt(refineChoiceOption, "offset", Gr);
var ii = /* @__PURE__ */ Pt(refineChoiceOption, "roundingMode", Vr);
var Ut = "PlainYearMonth";
var qt = "PlainMonthDay";
var G = "PlainDate";
var x = "PlainDateTime";
var ft = "PlainTime";
var z = "ZonedDateTime";
var Re = "Instant";
var N = "Duration";
var ai = [Math.floor, (e2) => hasHalf(e2) ? Math.floor(e2) : Math.round(e2), Math.ceil, (e2) => hasHalf(e2) ? Math.ceil(e2) : Math.round(e2), Math.trunc, (e2) => hasHalf(e2) ? Math.trunc(e2) || 0 : Math.round(e2), (e2) => e2 < 0 ? Math.floor(e2) : Math.ceil(e2), (e2) => Math.sign(e2) * Math.round(Math.abs(e2)) || 0, (e2) => hasHalf(e2) ? (e2 = Math.trunc(e2) || 0) + e2 % 2 : Math.round(e2)];
var si = "UTC";
var ci = 5184e3;
var ui = /* @__PURE__ */ isoArgsToEpochSec(1847);
var fi = /* @__PURE__ */ isoArgsToEpochSec(/* @__PURE__ */ (/* @__PURE__ */ new Date()).getUTCFullYear() + 10);
var li = /0+$/;
var he = /* @__PURE__ */ on(_zonedEpochSlotsToIso, WeakMap);
var di = 2 ** 32 - 1;
var L = /* @__PURE__ */ on((e2) => {
  const n2 = getTimeZoneEssence(e2);
  return "object" == typeof n2 ? new IntlTimeZone(n2) : new FixedTimeZone(n2 || 0);
});
var FixedTimeZone = class {
  constructor(e2) {
    this.$ = e2;
  }
  R() {
    return this.$;
  }
  I(e2) {
    return ((e3) => {
      const n2 = isoToEpochNano({
        ...e3,
        ...Nt
      });
      if (!n2 || Math.abs(n2[0]) > 1e8) {
        throw new RangeError(Io);
      }
    })(e2), [isoToEpochNanoWithOffset(e2, this.$)];
  }
  O() {
  }
};
var IntlTimeZone = class {
  constructor(e2) {
    this.nn = ((e3) => {
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
      const n2 = on(e3), t2 = on(createSplitTuple);
      let o2 = ui, r2 = fi;
      return {
        tn(e4) {
          const n3 = getOffsetSec(e4 - 86400), t3 = getOffsetSec(e4 + 86400), o3 = e4 - n3, r3 = e4 - t3;
          if (n3 === t3) {
            return [o3];
          }
          const i2 = getOffsetSec(o3);
          return i2 === getOffsetSec(r3) ? [e4 - i2] : n3 > t3 ? [o3, r3] : [];
        },
        rn: getOffsetSec,
        O(e4, i2) {
          const a2 = clampNumber(e4, o2, r2);
          let [s2, c2] = computePeriod(a2);
          const u2 = ci * i2, f2 = i2 < 0 ? () => c2 > o2 || (o2 = a2, 0) : () => s2 < r2 || (r2 = a2, 0);
          for (; f2(); ) {
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
      const t2 = hashIntlFormatParts(e3, n2 * Co);
      return isoArgsToEpochSec(parseIntlPartsYear(t2), parseInt(t2.month), parseInt(t2.day), parseInt(t2.hour), parseInt(t2.minute), parseInt(t2.second)) - n2;
    })(e2));
  }
  R(e2) {
    return this.nn.rn(((e3) => epochNanoToSecMod(e3)[0])(e2)) * Ro;
  }
  I(e2) {
    const [n2, t2] = [isoArgsToEpochSec((o2 = e2).isoYear, o2.isoMonth, o2.isoDay, o2.isoHour, o2.isoMinute, o2.isoSecond), o2.isoMillisecond * Qe + o2.isoMicrosecond * Yo + o2.isoNanosecond];
    var o2;
    return this.nn.tn(n2).map((e3) => checkEpochNanoInBounds(moveBigNano(Ge(e3, Ro), t2)));
  }
  O(e2, n2) {
    const [t2, o2] = epochNanoToSecMod(e2), r2 = this.nn.O(t2 + (n2 > 0 || o2 ? 1 : 0), n2);
    if (void 0 !== r2) {
      return Ge(r2, Ro);
    }
  }
};
var mi = "([+-])";
var pi = "(?:[.,](\\d{1,9}))?";
var hi = `(?:(?:${mi}(\\d{6}))|(\\d{4}))-?(\\d{2})`;
var gi = "(\\d{2})(?::?(\\d{2})(?::?(\\d{2})" + pi + ")?)?";
var Di = mi + gi;
var Ti = hi + "-?(\\d{2})(?:[T ]" + gi + "(Z|" + Di + ")?)?";
var Ii = "\\[(!?)([^\\]]*)\\]";
var Mi = `((?:${Ii}){0,9})`;
var Ni = /* @__PURE__ */ createRegExp(hi + Mi);
var yi = /* @__PURE__ */ createRegExp("(?:--)?(\\d{2})-?(\\d{2})" + Mi);
var vi = /* @__PURE__ */ createRegExp(Ti + Mi);
var Pi = /* @__PURE__ */ createRegExp("T?" + gi + "(?:" + Di + ")?" + Mi);
var Ei = /* @__PURE__ */ createRegExp(Di);
var Si = /* @__PURE__ */ new RegExp(Ii, "g");
var Fi = /* @__PURE__ */ createRegExp(`${mi}?P(\\d+Y)?(\\d+M)?(\\d+W)?(\\d+D)?(?:T(?:(\\d+)${pi}H)?(?:(\\d+)${pi}M)?(?:(\\d+)${pi}S)?)?`);
var wi = /* @__PURE__ */ on((e2) => new en(br, {
  timeZone: e2,
  era: "short",
  year: "numeric",
  month: "numeric",
  day: "numeric",
  hour: "numeric",
  minute: "numeric",
  second: "numeric"
}));
var bi = /^(AC|AE|AG|AR|AS|BE|BS|CA|CN|CS|CT|EA|EC|IE|IS|JS|MI|NE|NS|PL|PN|PR|PS|SS|VS)T$/;
var Oi = /[^\w\/:+-]+/;
var Bi = /^M(\d{2})(L?)$/;
var ki = /* @__PURE__ */ on(createIntlCalendar);
var Ci = /* @__PURE__ */ on((e2) => new en(br, {
  calendar: e2,
  timeZone: si,
  era: "short",
  year: "numeric",
  month: "short",
  day: "numeric"
}));
var Yi = {
  P(e2, n2, t2) {
    const o2 = mt(t2);
    let r2, { years: i2, months: a2, weeks: s2, days: c2 } = n2;
    if (c2 += durationFieldsToBigNano(n2, 5)[0], i2 || a2) {
      r2 = ((e3, n3, t3, o3, r3) => {
        let [i3, a3, s3] = e3.v(n3);
        if (t3) {
          const [n4, o4] = e3.q(i3, a3);
          i3 += t3, a3 = monthCodeNumberToMonth(n4, o4, e3.L(i3)), a3 = clampEntity("month", a3, 1, e3.B(i3), r3);
        }
        return o3 && ([i3, a3] = e3.un(i3, a3, o3)), s3 = clampEntity("day", s3, 1, e3.U(i3, a3), r3), e3.p(i3, a3, s3);
      })(this, e2, i2, a2, o2);
    } else {
      if (!s2 && !c2) {
        return e2;
      }
      r2 = isoToEpochMilli(e2);
    }
    if (void 0 === r2) {
      throw new RangeError(Io);
    }
    return r2 += (7 * s2 + c2) * ko, checkIsoDateInBounds(epochMilliToIso(r2));
  },
  N(e2, n2, t2) {
    if (t2 <= 7) {
      let o3 = 0, r3 = diffDays({
        ...e2,
        ...Nt
      }, {
        ...n2,
        ...Nt
      });
      return 7 === t2 && ([o3, r3] = divModTrunc(r3, 7)), {
        ...pr,
        weeks: o3,
        days: r3
      };
    }
    const o2 = this.v(e2), r2 = this.v(n2);
    let [i2, a2, s2] = ((e3, n3, t3, o3, r3, i3, a3) => {
      let s3 = r3 - n3, c2 = i3 - t3, u2 = a3 - o3;
      if (s3 || c2) {
        const f2 = Math.sign(s3 || c2);
        let l2 = e3.U(r3, i3), d2 = 0;
        if (Math.sign(u2) === -f2) {
          const o4 = l2;
          [r3, i3] = e3.un(r3, i3, -f2), s3 = r3 - n3, c2 = i3 - t3, l2 = e3.U(r3, i3), d2 = f2 < 0 ? -o4 : l2;
        }
        if (u2 = a3 - Math.min(o3, l2) + d2, s3) {
          const [o4, a4] = e3.q(n3, t3), [u3, l3] = e3.q(r3, i3);
          if (c2 = u3 - o4 || Number(l3) - Number(a4), Math.sign(c2) === -f2) {
            const t4 = f2 < 0 && -e3.B(r3);
            s3 = (r3 -= f2) - n3, c2 = i3 - monthCodeNumberToMonth(o4, a4, e3.L(r3)) + (t4 || e3.B(r3));
          }
        }
      }
      return [s3, c2, u2];
    })(this, ...o2, ...r2);
    return 8 === t2 && (a2 += this.cn(i2, o2[0]), i2 = 0), {
      ...pr,
      years: i2,
      months: a2,
      days: s2
    };
  },
  F(e2, n2) {
    const t2 = mt(n2), o2 = refineYear(this, e2), r2 = refineMonth(this, e2, o2, t2), i2 = refineDay(this, e2, r2, o2, t2);
    return W(checkIsoDateInBounds(this.V(o2, r2, i2)), this.id || l);
  },
  K(e2, n2) {
    const t2 = mt(n2), o2 = refineYear(this, e2), r2 = refineMonth(this, e2, o2, t2);
    return createPlainYearMonthSlots(checkIsoYearMonthInBounds(this.V(o2, r2, 1)), this.id || l);
  },
  _(e2, n2) {
    const t2 = mt(n2);
    let o2, r2, i2, a2 = void 0 !== e2.eraYear || void 0 !== e2.year ? refineYear(this, e2) : void 0;
    const s2 = !this.id;
    if (void 0 === a2 && s2 && (a2 = Br), void 0 !== a2) {
      const n3 = refineMonth(this, e2, a2, t2);
      o2 = refineDay(this, e2, n3, a2, t2);
      const s3 = this.L(a2);
      r2 = monthToMonthCodeNumber(n3, s3), i2 = n3 === s3;
    } else {
      if (void 0 === e2.monthCode) {
        throw new TypeError(co);
      }
      if ([r2, i2] = parseMonthCode(e2.monthCode), this.id && this.id !== or && this.id !== rr) {
        if (this.id && "coptic" === computeCalendarIdBase(this.id) && 0 === t2) {
          const n3 = i2 || 13 !== r2 ? 30 : 6;
          o2 = e2.day, o2 = clampNumber(o2, 1, n3);
        } else if (this.id && "chinese" === computeCalendarIdBase(this.id) && 0 === t2) {
          const n3 = !i2 || 1 !== r2 && 9 !== r2 && 10 !== r2 && 11 !== r2 && 12 !== r2 ? 30 : 29;
          o2 = e2.day, o2 = clampNumber(o2, 1, n3);
        } else {
          o2 = e2.day;
        }
      } else {
        o2 = refineDay(this, e2, refineMonth(this, e2, Br, t2), Br, t2);
      }
    }
    const c2 = this.G(r2, i2, o2);
    if (!c2) {
      throw new RangeError("Cannot guess year");
    }
    const [u2, f2] = c2;
    return createPlainMonthDaySlots(checkIsoDateInBounds(this.V(u2, f2, o2)), this.id || l);
  },
  fields(e2) {
    return getCalendarEraOrigins(this) && e2.includes("year") ? [...e2, ...$o] : e2;
  },
  k(e2, n2) {
    const t2 = Object.assign(/* @__PURE__ */ Object.create(null), e2);
    return spliceFields(t2, n2, _o), getCalendarEraOrigins(this) && (spliceFields(t2, n2, Ho), this.id === rr && spliceFields(t2, n2, er, $o)), t2;
  },
  inLeapYear(e2) {
    const [n2] = this.v(e2);
    return this.sn(n2);
  },
  monthsInYear(e2) {
    const [n2] = this.v(e2);
    return this.B(n2);
  },
  daysInMonth(e2) {
    const [n2, t2] = this.v(e2);
    return this.U(n2, t2);
  },
  daysInYear(e2) {
    const [n2] = this.v(e2);
    return this.fn(n2);
  },
  dayOfYear: computeNativeDayOfYear,
  era(e2) {
    return this.hn(e2)[0];
  },
  eraYear(e2) {
    return this.hn(e2)[1];
  },
  monthCode(e2) {
    const [n2, t2] = this.v(e2), [o2, r2] = this.q(n2, t2);
    return formatMonthCode(o2, r2);
  },
  dayOfWeek: computeIsoDayOfWeek,
  daysInWeek() {
    return 7;
  }
};
var Ri = {
  v: computeIsoDateParts,
  hn: computeIsoEraParts,
  q: computeIsoMonthCodeParts
};
var Zi = {
  dayOfYear: computeNativeDayOfYear,
  v: computeIsoDateParts,
  p: isoArgsToEpochMilli
};
var zi = /* @__PURE__ */ Object.assign({}, Zi, {
  weekOfYear: computeNativeWeekOfYear,
  yearOfWeek: computeNativeYearOfWeek,
  m(e2) {
    function computeWeekShift(e3) {
      return (7 - e3 < n2 ? 7 : 0) - e3;
    }
    function computeWeeksInYear(e3) {
      const n3 = computeIsoDaysInYear(f2 + e3), t3 = e3 || 1, o3 = computeWeekShift(modFloor(a2 + n3 * t3, 7));
      return c2 = (n3 + (o3 - s2) * t3) / 7;
    }
    const n2 = this.id ? 1 : 4, t2 = computeIsoDayOfWeek(e2), o2 = this.dayOfYear(e2), r2 = modFloor(t2 - 1, 7), i2 = o2 - 1, a2 = modFloor(r2 - i2, 7), s2 = computeWeekShift(a2);
    let c2, u2 = Math.floor((i2 - s2) / 7) + 1, f2 = e2.isoYear;
    return u2 ? u2 > computeWeeksInYear(0) && (u2 = 1, f2++) : (u2 = computeWeeksInYear(-1), f2--), [u2, f2, c2];
  }
});
var Ui = /* @__PURE__ */ Object.assign({}, Yi, zi, {
  v: computeIsoDateParts,
  hn: computeIsoEraParts,
  q: computeIsoMonthCodeParts,
  G(e2, n2) {
    if (!n2) {
      return [Br, e2];
    }
  },
  sn: computeIsoInLeapYear,
  L() {
  },
  B: computeIsoMonthsInYear,
  cn: (e2) => e2 * kr,
  U: computeIsoDaysInMonth,
  fn: computeIsoDaysInYear,
  V: (e2, n2, t2) => ({
    isoYear: e2,
    isoMonth: n2,
    isoDay: t2
  }),
  p: isoArgsToEpochMilli,
  un: (e2, n2, t2) => (e2 += divTrunc(t2, kr), (n2 += modTrunc(t2, kr)) < 1 ? (e2--, n2 += kr) : n2 > kr && (e2++, n2 -= kr), [e2, n2]),
  year(e2) {
    return e2.isoYear;
  },
  month(e2) {
    return e2.isoMonth;
  },
  day: (e2) => e2.isoDay
});
var Ai = {
  v: computeIntlDateParts,
  hn: computeIntlEraParts,
  q: computeIntlMonthCodeParts
};
var qi = {
  dayOfYear: computeNativeDayOfYear,
  v: computeIntlDateParts,
  p: computeIntlEpochMilli,
  weekOfYear: computeNativeWeekOfYear,
  yearOfWeek: computeNativeYearOfWeek,
  m() {
    return [];
  }
};
var Wi = /* @__PURE__ */ Object.assign({}, Yi, qi, {
  v: computeIntlDateParts,
  hn: computeIntlEraParts,
  q: computeIntlMonthCodeParts,
  G(e2, n2, t2) {
    const o2 = this.id && "chinese" === computeCalendarIdBase(this.id) ? ((e3, n3, t3) => {
      if (n3) {
        switch (e3) {
          case 1:
            return 1651;
          case 2:
            return t3 < 30 ? 1947 : 1765;
          case 3:
            return t3 < 30 ? 1966 : 1955;
          case 4:
            return t3 < 30 ? 1963 : 1944;
          case 5:
            return t3 < 30 ? 1971 : 1952;
          case 6:
            return t3 < 30 ? 1960 : 1941;
          case 7:
            return t3 < 30 ? 1968 : 1938;
          case 8:
            return t3 < 30 ? 1957 : 1718;
          case 9:
            return 1832;
          case 10:
            return 1870;
          case 11:
            return 1814;
          case 12:
            return 1890;
        }
      }
      return 1972;
    })(e2, n2, t2) : Br;
    let [r2, i2, a2] = computeIntlDateParts.call(this, {
      isoYear: o2,
      isoMonth: kr,
      isoDay: 31
    });
    const s2 = computeIntlLeapMonth.call(this, r2), c2 = i2 === s2;
    1 === (compareNumbers(e2, monthToMonthCodeNumber(i2, s2)) || compareNumbers(Number(n2), Number(c2)) || compareNumbers(t2, a2)) && r2--;
    for (let o3 = 0; o3 < 100; o3++) {
      const i3 = r2 - o3, a3 = computeIntlLeapMonth.call(this, i3), s3 = monthCodeNumberToMonth(e2, n2, a3);
      if (n2 === (s3 === a3) && t2 <= computeIntlDaysInMonth.call(this, i3, s3)) {
        return [i3, s3];
      }
    }
  },
  sn(e2) {
    const n2 = computeIntlDaysInYear.call(this, e2);
    return n2 > computeIntlDaysInYear.call(this, e2 - 1) && n2 > computeIntlDaysInYear.call(this, e2 + 1);
  },
  L: computeIntlLeapMonth,
  B: computeIntlMonthsInYear,
  cn(e2, n2) {
    const t2 = n2 + e2, o2 = Math.sign(e2), r2 = o2 < 0 ? -1 : 0;
    let i2 = 0;
    for (let e3 = n2; e3 !== t2; e3 += o2) {
      i2 += computeIntlMonthsInYear.call(this, e3 + r2);
    }
    return i2;
  },
  U: computeIntlDaysInMonth,
  fn: computeIntlDaysInYear,
  V(e2, n2, t2) {
    return epochMilliToIso(computeIntlEpochMilli.call(this, e2, n2, t2));
  },
  p: computeIntlEpochMilli,
  un(e2, n2, t2) {
    if (t2) {
      if (n2 += t2, !Number.isSafeInteger(n2)) {
        throw new RangeError(Io);
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
    return this.h(e2).year;
  },
  month(e2) {
    const { year: n2, o: t2 } = this.h(e2), { u: o2 } = this.l(n2);
    return o2[t2] + 1;
  },
  day(e2) {
    return this.h(e2).day;
  }
});
var ji = /* @__PURE__ */ createNativeOpsCreator(Ri, Ai);
var C = /* @__PURE__ */ createNativeOpsCreator(Ui, Wi);
var Li = {
  ...{
    era: toStringViaPrimitive,
    eraYear: toInteger,
    year: toInteger,
    month: toPositiveInteger,
    monthCode(e2) {
      const n2 = toStringViaPrimitive(e2);
      return parseMonthCode(n2), n2;
    },
    day: toPositiveInteger
  },
  .../* @__PURE__ */ wo(O, toInteger),
  .../* @__PURE__ */ wo(p, toStrictInteger),
  offset(e2) {
    const n2 = toStringViaPrimitive(e2);
    return parseOffsetNano(n2), n2;
  }
};
var xi = /* @__PURE__ */ Pt(remapProps, O, w);
var $i = /* @__PURE__ */ Pt(remapProps, w, O);
var Hi = "numeric";
var Gi = ["timeZoneName"];
var Vi = {
  month: Hi,
  day: Hi
};
var _i = {
  year: Hi,
  month: Hi
};
var Ji = /* @__PURE__ */ Object.assign({}, _i, {
  day: Hi
});
var Ki = {
  hour: Hi,
  minute: Hi,
  second: Hi
};
var Qi = /* @__PURE__ */ Object.assign({}, Ji, Ki);
var Xi = /* @__PURE__ */ Object.assign({}, Qi, {
  timeZoneName: "short"
});
var ea = /* @__PURE__ */ Object.keys(_i);
var na = /* @__PURE__ */ Object.keys(Vi);
var ta = /* @__PURE__ */ Object.keys(Ji);
var oa = /* @__PURE__ */ Object.keys(Ki);
var ra = ["dateStyle"];
var ia = /* @__PURE__ */ ea.concat(ra);
var aa = /* @__PURE__ */ na.concat(ra);
var sa = /* @__PURE__ */ ta.concat(ra, ["weekday"]);
var ca = /* @__PURE__ */ oa.concat(["dayPeriod", "timeStyle", "fractionalSecondDigits"]);
var ua = /* @__PURE__ */ sa.concat(ca);
var fa = /* @__PURE__ */ Gi.concat(ca);
var la = /* @__PURE__ */ Gi.concat(sa);
var da = /* @__PURE__ */ Gi.concat(["day", "weekday"], ca);
var ma = /* @__PURE__ */ Gi.concat(["year", "weekday"], ca);
var pa = /* @__PURE__ */ createOptionsTransformer(ua, Qi);
var ha = /* @__PURE__ */ createOptionsTransformer(ua, Xi);
var ga = /* @__PURE__ */ createOptionsTransformer(ua, Qi, Gi);
var Da = /* @__PURE__ */ createOptionsTransformer(sa, Ji, fa);
var Ta = /* @__PURE__ */ createOptionsTransformer(ca, Ki, la);
var Ia = /* @__PURE__ */ createOptionsTransformer(ia, _i, da);
var Ma = /* @__PURE__ */ createOptionsTransformer(aa, Vi, ma);
var Na = {};
var ya = new en(void 0, {
  calendar: l
}).resolvedOptions().calendar === l;
var U = [pa, I];
var ot = [ha, I, 0, (e2, n2) => {
  const t2 = e2.timeZone;
  if (n2 && n2.timeZone !== t2) {
    throw new RangeError(mo);
  }
  return t2;
}];
var X = [ga, isoToEpochMilli];
var _ = [Da, isoToEpochMilli];
var tt = [Ta, (e2) => isoTimeFieldsToNano(e2) / Qe];
var et = [Ia, isoToEpochMilli, ya];
var nt = [Ma, isoToEpochMilli, ya];
var va;

// node_modules/temporal-polyfill/chunks/classApi.js
function createSlotClass(i2, l2, s2, c2, u2) {
  function Class(...t2) {
    if (!(this instanceof Class)) {
      throw new TypeError(a);
    }
    un(this, l2(...t2));
  }
  function bindMethod(t2, e2) {
    return Object.defineProperties(function(...e3) {
      return t2.call(this, getSpecificSlots(this), ...e3);
    }, r(e2));
  }
  function getSpecificSlots(t2) {
    const e2 = cn(t2);
    if (!e2 || e2.branding !== i2) {
      throw new TypeError(a);
    }
    return e2;
  }
  return Object.defineProperties(Class.prototype, {
    ...t(e(bindMethod, s2)),
    ...n(e(bindMethod, c2)),
    ...o("Temporal." + i2)
  }), Object.defineProperties(Class, {
    ...n(u2),
    ...r(i2)
  }), [Class, (t2) => {
    const e2 = Object.create(Class.prototype);
    return un(e2, t2), e2;
  }, getSpecificSlots];
}
function rejectInvalidBag(t2) {
  if (cn(t2) || void 0 !== t2.calendar || void 0 !== t2.timeZone) {
    throw new TypeError(i);
  }
  return t2;
}
function getCalendarIdFromBag(t2) {
  return extractCalendarIdFromBag(t2) || l;
}
function extractCalendarIdFromBag(t2) {
  const { calendar: e2 } = t2;
  if (void 0 !== e2) {
    return refineCalendarArg(e2);
  }
}
function refineCalendarArg(t2) {
  if (s(t2)) {
    const { calendar: e2 } = cn(t2) || {};
    if (!e2) {
      throw new TypeError(c(t2));
    }
    return e2;
  }
  return ((t3) => u(f(m(t3))))(t2);
}
function createCalendarGetters(t2) {
  const e2 = {};
  for (const n2 in t2) {
    e2[n2] = (t3) => {
      const { calendar: e3 } = t3;
      return C(e3)[n2](t3);
    };
  }
  return e2;
}
function neverValueOf() {
  throw new TypeError(b);
}
function refineTimeZoneArg(t2) {
  if (s(t2)) {
    const { timeZone: e2 } = cn(t2) || {};
    if (!e2) {
      throw new TypeError(F(t2));
    }
    return e2;
  }
  return ((t3) => M(Z(m(t3))))(t2);
}
function toDurationSlots(t2) {
  if (s(t2)) {
    const e2 = cn(t2);
    return e2 && e2.branding === N ? e2 : q(t2);
  }
  return R(t2);
}
function refinePublicRelativeTo(t2) {
  if (void 0 !== t2) {
    if (s(t2)) {
      const e2 = cn(t2) || {};
      switch (e2.branding) {
        case z:
        case G:
          return e2;
        case x:
          return W(e2);
      }
      const n2 = getCalendarIdFromBag(t2);
      return {
        ...$(refineTimeZoneArg, L, C(n2), t2),
        calendar: n2
      };
    }
    return H(t2);
  }
}
function toPlainTimeSlots(t2, e2) {
  if (s(t2)) {
    const n3 = cn(t2) || {};
    switch (n3.branding) {
      case ft:
        return mt(e2), n3;
      case x:
        return mt(e2), St(n3);
      case z:
        return mt(e2), dt(L, n3);
    }
    return Tt(t2, e2);
  }
  const n2 = ht(t2);
  return mt(e2), n2;
}
function optionalToPlainTimeFields(t2) {
  return void 0 === t2 ? void 0 : toPlainTimeSlots(t2);
}
function toPlainDateTimeSlots(t2, e2) {
  if (s(t2)) {
    const n3 = cn(t2) || {};
    switch (n3.branding) {
      case x:
        return mt(e2), n3;
      case G:
        return mt(e2), jt({
          ...n3,
          ...Nt
        });
      case z:
        return mt(e2), yt(L, n3);
    }
    return At(C(getCalendarIdFromBag(t2)), t2, e2);
  }
  const n2 = Bt(t2);
  return mt(e2), n2;
}
function toPlainMonthDaySlots(t2, e2) {
  if (s(t2)) {
    const n3 = cn(t2);
    if (n3 && n3.branding === qt) {
      return mt(e2), n3;
    }
    const o2 = extractCalendarIdFromBag(t2);
    return Rt(C(o2 || l), !o2, t2, e2);
  }
  const n2 = xt(C, t2);
  return mt(e2), n2;
}
function toPlainYearMonthSlots(t2, e2) {
  if (s(t2)) {
    const n3 = cn(t2);
    return n3 && n3.branding === Ut ? (mt(e2), n3) : Xt(C(getCalendarIdFromBag(t2)), t2, e2);
  }
  const n2 = _t(C, t2);
  return mt(e2), n2;
}
function toPlainDateSlots(t2, e2) {
  if (s(t2)) {
    const n3 = cn(t2) || {};
    switch (n3.branding) {
      case G:
        return mt(e2), n3;
      case x:
        return mt(e2), W(n3);
      case z:
        return mt(e2), fe(L, n3);
    }
    return me(C(getCalendarIdFromBag(t2)), t2, e2);
  }
  const n2 = de(t2);
  return mt(e2), n2;
}
function toZonedDateTimeSlots(t2, e2) {
  if (s(t2)) {
    const n2 = cn(t2);
    if (n2 && n2.branding === z) {
      return je(e2), n2;
    }
    const o2 = getCalendarIdFromBag(t2);
    return Ne(refineTimeZoneArg, L, C(o2), o2, t2, e2);
  }
  return Ae(t2, e2);
}
function adaptDateMethods(t2) {
  return e((t3) => (e2) => t3(slotsToIso(e2)), t2);
}
function slotsToIso(t2) {
  return he(t2, L);
}
function toInstantSlots(t2) {
  if (s(t2)) {
    const e2 = cn(t2);
    if (e2) {
      switch (e2.branding) {
        case Re:
          return e2;
        case z:
          return xe(e2.epochNanoseconds);
      }
    }
  }
  return We(t2);
}
function toTemporalInstant() {
  const t2 = Date.prototype.valueOf.call(this);
  return Kn(xe(Ge(ze(t2), Qe)));
}
function createDateTimeFormatClass() {
  function DateTimeFormatFunc(t3, e3) {
    return new DateTimeFormatNew(t3, e3);
  }
  function DateTimeFormatNew(t3, e3 = /* @__PURE__ */ Object.create(null)) {
    to.set(this, ((t4, e4) => {
      const n3 = new en(t4, e4), o2 = n3.resolvedOptions(), r2 = o2.locale, a2 = nn(Object.keys(e4), o2), i2 = on(createFormatPrepperForBranding), prepFormat = (t5, ...e5) => {
        if (t5) {
          if (2 !== e5.length) {
            throw new TypeError(ln);
          }
          for (const t6 of e5) {
            if (void 0 === t6) {
              throw new TypeError(ln);
            }
          }
        }
        t5 || void 0 !== e5[0] || (e5 = []);
        const o3 = e5.map((t6) => cn(t6) || Number(t6));
        let l2, s2 = 0;
        for (const t6 of o3) {
          const e6 = "object" == typeof t6 ? t6.branding : void 0;
          if (s2++ && e6 !== l2) {
            throw new TypeError(ln);
          }
          l2 = e6;
        }
        return l2 ? i2(l2)(r2, a2, ...o3) : [n3, ...o3];
      };
      return prepFormat.X = n3, prepFormat;
    })(t3, e3));
  }
  const t2 = en.prototype, e2 = Object.getOwnPropertyDescriptors(t2), n2 = Object.getOwnPropertyDescriptors(en);
  for (const t3 in e2) {
    const n3 = e2[t3], o2 = t3.startsWith("format") && createFormatMethod(t3);
    "function" == typeof n3.value ? n3.value = "constructor" === t3 ? DateTimeFormatFunc : o2 || createProxiedMethod(t3) : o2 && (n3.get = function() {
      if (!to.has(this)) {
        throw new TypeError(a);
      }
      return (...t4) => o2.apply(this, t4);
    }, Object.defineProperties(n3.get, r(`get ${t3}`)));
  }
  return n2.prototype.value = DateTimeFormatNew.prototype = Object.create({}, e2), Object.defineProperties(DateTimeFormatFunc, n2), DateTimeFormatFunc;
}
function createFormatMethod(t2) {
  return Object.defineProperties(function(...e2) {
    const n2 = to.get(this), [o2, ...r2] = n2(t2.includes("Range"), ...e2);
    return o2[t2](...r2);
  }, r(t2));
}
function createProxiedMethod(t2) {
  return Object.defineProperties(function(...e2) {
    return to.get(this).X[t2](...e2);
  }, r(t2));
}
function createFormatPrepperForBranding(t2) {
  const e2 = Cn[t2];
  if (!e2) {
    throw new TypeError(rn(t2));
  }
  return Q(e2, on(an), 1);
}
var sn = /* @__PURE__ */ new WeakMap();
var cn = /* @__PURE__ */ sn.get.bind(sn);
var un = /* @__PURE__ */ sn.set.bind(sn);
var fn = {
  era: d,
  eraYear: S,
  year: T,
  month: h,
  daysInMonth: h,
  daysInYear: h,
  inLeapYear: D,
  monthsInYear: h
};
var mn = {
  monthCode: m
};
var dn = {
  day: h
};
var Sn = {
  dayOfWeek: h,
  dayOfYear: h,
  weekOfYear: P,
  yearOfWeek: S,
  daysInWeek: h
};
var Tn = /* @__PURE__ */ createCalendarGetters(/* @__PURE__ */ Object.assign({}, fn, mn, dn, Sn));
var hn = /* @__PURE__ */ createCalendarGetters({
  ...fn,
  ...mn
});
var Dn = /* @__PURE__ */ createCalendarGetters({
  ...mn,
  ...dn
});
var Pn = {
  calendarId: (t2) => t2.calendar
};
var gn = /* @__PURE__ */ g((t2) => (e2) => e2[t2], p.concat("sign"));
var pn = /* @__PURE__ */ g((t2, e2) => (t3) => t3[w[e2]], O);
var On = {
  epochMilliseconds: I,
  epochNanoseconds: v
};
var [wn, In, vn] = createSlotClass(N, j, {
  ...gn,
  blank: y
}, {
  with: (t2, e2) => In(A(t2, e2)),
  negated: (t2) => In(B(t2)),
  abs: (t2) => In(Y(t2)),
  add: (t2, e2, n2) => In(E(refinePublicRelativeTo, C, L, 0, t2, toDurationSlots(e2), n2)),
  subtract: (t2, e2, n2) => In(E(refinePublicRelativeTo, C, L, 1, t2, toDurationSlots(e2), n2)),
  round: (t2, e2) => In(V(refinePublicRelativeTo, C, L, t2, e2)),
  total: (t2, e2) => J(refinePublicRelativeTo, C, L, t2, e2),
  toLocaleString(t2, e2, n2) {
    return Intl.DurationFormat ? new Intl.DurationFormat(e2, n2).format(this) : k(t2);
  },
  toString: k,
  toJSON: (t2) => k(t2),
  valueOf: neverValueOf
}, {
  from: (t2) => In(toDurationSlots(t2)),
  compare: (t2, e2, n2) => K(refinePublicRelativeTo, C, L, toDurationSlots(t2), toDurationSlots(e2), n2)
});
var Cn = {
  Instant: U,
  PlainDateTime: X,
  PlainDate: _,
  PlainTime: tt,
  PlainYearMonth: et,
  PlainMonthDay: nt
};
var bn = /* @__PURE__ */ Q(U);
var Fn = /* @__PURE__ */ Q(ot);
var Mn = /* @__PURE__ */ Q(X);
var Zn = /* @__PURE__ */ Q(_);
var yn = /* @__PURE__ */ Q(tt);
var jn = /* @__PURE__ */ Q(et);
var Nn = /* @__PURE__ */ Q(nt);
var [An, Bn] = createSlotClass(ft, ut, pn, {
  with(t2, e2, n2) {
    return Bn(rt(this, rejectInvalidBag(e2), n2));
  },
  add: (t2, e2) => Bn(at(0, t2, toDurationSlots(e2))),
  subtract: (t2, e2) => Bn(at(1, t2, toDurationSlots(e2))),
  until: (t2, e2, n2) => In(it(0, t2, toPlainTimeSlots(e2), n2)),
  since: (t2, e2, n2) => In(it(1, t2, toPlainTimeSlots(e2), n2)),
  round: (t2, e2) => Bn(lt(t2, e2)),
  equals: (t2, e2) => st(t2, toPlainTimeSlots(e2)),
  toLocaleString(t2, e2, n2) {
    const [o2, r2] = yn(e2, n2, t2);
    return o2.format(r2);
  },
  toString: ct,
  toJSON: (t2) => ct(t2),
  valueOf: neverValueOf
}, {
  from: (t2, e2) => Bn(toPlainTimeSlots(t2, e2)),
  compare: (t2, e2) => Dt(toPlainTimeSlots(t2), toPlainTimeSlots(e2))
});
var [Yn, En] = createSlotClass(x, Pt(Zt, Mt), {
  ...Pn,
  ...Tn,
  ...pn
}, {
  with: (t2, e2, n2) => En(gt(C, t2, rejectInvalidBag(e2), n2)),
  withCalendar: (t2, e2) => En(pt(t2, refineCalendarArg(e2))),
  withPlainTime: (t2, e2) => En(Ot(t2, optionalToPlainTimeFields(e2))),
  add: (t2, e2, n2) => En(wt(C, 0, t2, toDurationSlots(e2), n2)),
  subtract: (t2, e2, n2) => En(wt(C, 1, t2, toDurationSlots(e2), n2)),
  until: (t2, e2, n2) => In(It(C, 0, t2, toPlainDateTimeSlots(e2), n2)),
  since: (t2, e2, n2) => In(It(C, 1, t2, toPlainDateTimeSlots(e2), n2)),
  round: (t2, e2) => En(vt(t2, e2)),
  equals: (t2, e2) => Ct(t2, toPlainDateTimeSlots(e2)),
  toZonedDateTime: (t2, e2, n2) => $n(bt(L, t2, refineTimeZoneArg(e2), n2)),
  toPlainDate: (t2) => Wn(W(t2)),
  toPlainTime: (t2) => Bn(St(t2)),
  toLocaleString(t2, e2, n2) {
    const [o2, r2] = Mn(e2, n2, t2);
    return o2.format(r2);
  },
  toString: Ft,
  toJSON: (t2) => Ft(t2),
  valueOf: neverValueOf
}, {
  from: (t2, e2) => En(toPlainDateTimeSlots(t2, e2)),
  compare: (t2, e2) => Yt(toPlainDateTimeSlots(t2), toPlainDateTimeSlots(e2))
});
var [Ln, Vn, Jn] = createSlotClass(qt, Pt(kt, Mt), {
  ...Pn,
  ...Dn
}, {
  with: (t2, e2, n2) => Vn(Et(C, t2, rejectInvalidBag(e2), n2)),
  equals: (t2, e2) => Lt(t2, toPlainMonthDaySlots(e2)),
  toPlainDate(t2, e2) {
    return Wn(Vt(C, t2, this, e2));
  },
  toLocaleString(t2, e2, n2) {
    const [o2, r2] = Nn(e2, n2, t2);
    return o2.format(r2);
  },
  toString: Jt,
  toJSON: (t2) => Jt(t2),
  valueOf: neverValueOf
}, {
  from: (t2, e2) => Vn(toPlainMonthDaySlots(t2, e2))
});
var [kn, qn, Rn] = createSlotClass(Ut, Pt(Qt, Mt), {
  ...Pn,
  ...hn
}, {
  with: (t2, e2, n2) => qn(Wt(C, t2, rejectInvalidBag(e2), n2)),
  add: (t2, e2, n2) => qn(Gt(C, 0, t2, toDurationSlots(e2), n2)),
  subtract: (t2, e2, n2) => qn(Gt(C, 1, t2, toDurationSlots(e2), n2)),
  until: (t2, e2, n2) => In(zt(C, 0, t2, toPlainYearMonthSlots(e2), n2)),
  since: (t2, e2, n2) => In(zt(C, 1, t2, toPlainYearMonthSlots(e2), n2)),
  equals: (t2, e2) => $t(t2, toPlainYearMonthSlots(e2)),
  toPlainDate(t2, e2) {
    return Wn(Ht(C, t2, this, e2));
  },
  toLocaleString(t2, e2, n2) {
    const [o2, r2] = jn(e2, n2, t2);
    return o2.format(r2);
  },
  toString: Kt,
  toJSON: (t2) => Kt(t2),
  valueOf: neverValueOf
}, {
  from: (t2, e2) => qn(toPlainYearMonthSlots(t2, e2)),
  compare: (t2, e2) => te(toPlainYearMonthSlots(t2), toPlainYearMonthSlots(e2))
});
var [xn, Wn, Gn] = createSlotClass(G, Pt(ue, Mt), {
  ...Pn,
  ...Tn
}, {
  with: (t2, e2, n2) => Wn(ee(C, t2, rejectInvalidBag(e2), n2)),
  withCalendar: (t2, e2) => Wn(pt(t2, refineCalendarArg(e2))),
  add: (t2, e2, n2) => Wn(ne(C, 0, t2, toDurationSlots(e2), n2)),
  subtract: (t2, e2, n2) => Wn(ne(C, 1, t2, toDurationSlots(e2), n2)),
  until: (t2, e2, n2) => In(oe(C, 0, t2, toPlainDateSlots(e2), n2)),
  since: (t2, e2, n2) => In(oe(C, 1, t2, toPlainDateSlots(e2), n2)),
  equals: (t2, e2) => re(t2, toPlainDateSlots(e2)),
  toZonedDateTime(t2, e2) {
    const n2 = s(e2) ? e2 : {
      timeZone: e2
    };
    return $n(ae(refineTimeZoneArg, toPlainTimeSlots, L, t2, n2));
  },
  toPlainDateTime: (t2, e2) => En(ie(t2, optionalToPlainTimeFields(e2))),
  toPlainYearMonth(t2) {
    return qn(le(C, t2, this));
  },
  toPlainMonthDay(t2) {
    return Vn(se(C, t2, this));
  },
  toLocaleString(t2, e2, n2) {
    const [o2, r2] = Zn(e2, n2, t2);
    return o2.format(r2);
  },
  toString: ce,
  toJSON: (t2) => ce(t2),
  valueOf: neverValueOf
}, {
  from: (t2, e2) => Wn(toPlainDateSlots(t2, e2)),
  compare: (t2, e2) => te(toPlainDateSlots(t2), toPlainDateSlots(e2))
});
var [zn, $n] = createSlotClass(z, Pt(ye, Mt, Ze), {
  ...On,
  ...Pn,
  ...adaptDateMethods(Tn),
  ...adaptDateMethods(pn),
  offset: (t2) => Se(slotsToIso(t2).offsetNanoseconds),
  offsetNanoseconds: (t2) => slotsToIso(t2).offsetNanoseconds,
  timeZoneId: (t2) => t2.timeZone,
  hoursInDay: (t2) => Te(L, t2)
}, {
  with: (t2, e2, n2) => $n(De(C, L, t2, rejectInvalidBag(e2), n2)),
  withCalendar: (t2, e2) => $n(pt(t2, refineCalendarArg(e2))),
  withTimeZone: (t2, e2) => $n(Pe(t2, refineTimeZoneArg(e2))),
  withPlainTime: (t2, e2) => $n(ge(L, t2, optionalToPlainTimeFields(e2))),
  add: (t2, e2, n2) => $n(pe(C, L, 0, t2, toDurationSlots(e2), n2)),
  subtract: (t2, e2, n2) => $n(pe(C, L, 1, t2, toDurationSlots(e2), n2)),
  until: (t2, e2, n2) => In(Oe(we(C, L, 0, t2, toZonedDateTimeSlots(e2), n2))),
  since: (t2, e2, n2) => In(Oe(we(C, L, 1, t2, toZonedDateTimeSlots(e2), n2))),
  round: (t2, e2) => $n(Ie(L, t2, e2)),
  startOfDay: (t2) => $n(ve(L, t2)),
  equals: (t2, e2) => Ce(t2, toZonedDateTimeSlots(e2)),
  toInstant: (t2) => Kn(be(t2)),
  toPlainDateTime: (t2) => En(yt(L, t2)),
  toPlainDate: (t2) => Wn(fe(L, t2)),
  toPlainTime: (t2) => Bn(dt(L, t2)),
  toLocaleString(t2, e2, n2 = {}) {
    const [o2, r2] = Fn(e2, n2, t2);
    return o2.format(r2);
  },
  toString: (t2, e2) => Fe(L, t2, e2),
  toJSON: (t2) => Fe(L, t2),
  valueOf: neverValueOf,
  getTimeZoneTransition(t2, e2) {
    const { timeZone: n2, epochNanoseconds: o2 } = t2, r2 = Me(e2), a2 = L(n2).O(o2, r2);
    return a2 ? $n({
      ...t2,
      epochNanoseconds: a2
    }) : null;
  }
}, {
  from: (t2, e2) => $n(toZonedDateTimeSlots(t2, e2)),
  compare: (t2, e2) => Be(toZonedDateTimeSlots(t2), toZonedDateTimeSlots(e2))
});
var [Hn, Kn, Qn] = createSlotClass(Re, qe, On, {
  add: (t2, e2) => Kn(Ye(0, t2, toDurationSlots(e2))),
  subtract: (t2, e2) => Kn(Ye(1, t2, toDurationSlots(e2))),
  until: (t2, e2, n2) => In(Ee(0, t2, toInstantSlots(e2), n2)),
  since: (t2, e2, n2) => In(Ee(1, t2, toInstantSlots(e2), n2)),
  round: (t2, e2) => Kn(Le(t2, e2)),
  equals: (t2, e2) => Ve(t2, toInstantSlots(e2)),
  toZonedDateTimeISO: (t2, e2) => $n(Je(t2, refineTimeZoneArg(e2))),
  toLocaleString(t2, e2, n2) {
    const [o2, r2] = bn(e2, n2, t2);
    return o2.format(r2);
  },
  toString: (t2, e2) => ke(refineTimeZoneArg, L, t2, e2),
  toJSON: (t2) => ke(refineTimeZoneArg, L, t2),
  valueOf: neverValueOf
}, {
  from: (t2) => Kn(toInstantSlots(t2)),
  fromEpochMilliseconds: (t2) => Kn($e(t2)),
  fromEpochNanoseconds: (t2) => Kn(He(t2)),
  compare: (t2, e2) => Ke(toInstantSlots(t2), toInstantSlots(e2))
});
var Un = /* @__PURE__ */ Object.defineProperties({}, {
  ...o("Temporal.Now"),
  ...n({
    timeZoneId: () => Ue(),
    instant: () => Kn(xe(Xe())),
    zonedDateTimeISO: (t2 = Ue()) => $n(_e(Xe(), refineTimeZoneArg(t2), l)),
    plainDateTimeISO: (t2 = Ue()) => En(jt(tn(L(refineTimeZoneArg(t2))), l)),
    plainDateISO: (t2 = Ue()) => Wn(W(tn(L(refineTimeZoneArg(t2))), l)),
    plainTimeISO: (t2 = Ue()) => Bn(St(tn(L(refineTimeZoneArg(t2)))))
  })
});
var Xn = /* @__PURE__ */ Object.defineProperties({}, {
  ...o("Temporal"),
  ...n({
    PlainYearMonth: kn,
    PlainMonthDay: Ln,
    PlainDate: xn,
    PlainTime: An,
    PlainDateTime: Yn,
    ZonedDateTime: zn,
    Instant: Hn,
    Duration: wn,
    Now: Un
  })
});
var _n = /* @__PURE__ */ createDateTimeFormatClass();
var to = /* @__PURE__ */ new WeakMap();
var eo = /* @__PURE__ */ Object.defineProperties(Object.create(Intl), n({
  DateTimeFormat: _n
}));

// node_modules/temporal-polyfill/global.esm.js
Object.defineProperties(globalThis, n({
  Temporal: Xn
})), Object.defineProperties(Intl, n({
  DateTimeFormat: _n
})), Object.defineProperties(Date.prototype, n({
  toTemporalInstant
}));
