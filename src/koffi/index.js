// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import util from 'util';
import { detectPlatform, loadDynamic } from './src/init.js';
import { loadStatic } from './src/static.js';

let [version, pkg, triplets] = detectPlatform();
let native = null;

STATIC: native = loadStatic(pkg);

if (native == null)
    native = loadDynamic(import.meta.dirname + '/../..', pkg, triplets);
if (native.version != version)
    throw new Error('Mismatched native Koffi modules');

// Yeah that's ugly, I know
const _config = native.config;
const _stats = native.stats;
const _struct = native.struct;
const _pack = native.pack;
const _union = native.union;
const _Union = native.Union;
const _opaque = native.opaque;
const _pointer = native.pointer;
const _array = native.array;
const _proto = native.proto;
const _alias = native.alias;
const _sizeof = native.sizeof;
const _alignof = native.alignof;
const _offsetof = native.offsetof;
const _resolve = native.resolve;
const _introspect = native.introspect;
const _load = native.load;
const _in = native.in;
const _out = native.out;
const _inout = native.inout;
const _disposable = native.disposable;
const _alloc = native.alloc;
const _free = native.free;
const _register = native.register;
const _unregister = native.unregister;
const _as = native.as;
const _decode = native.decode;
const _address = native.address;
const _call = native.call;
const _encode = native.encode;
const _view = native.view;
const _reset = native.reset;
const _errno = native.errno;
const _os = native.os;
const _extension = native.extension;
const _types = native.types;
const _node = native.node;
const _version = native.version;

// Deprecated functions
const handle = util.deprecate(_opaque, 'The koffi.handle() function was deprecated in Koffi 2.1, use koffi.opaque() instead', 'KOFFI001');
const callback = util.deprecate(_proto, 'The koffi.callback() function was deprecated in Koffi 2.4, use koffi.proto() instead', 'KOFFI002');

function load(...args) {
    let lib = _load(...args);

    lib.cdecl = util.deprecate((...args) => lib.func('__cdecl', ...args), 'The koffi.cdecl() function was deprecated in Koffi 2.7, use koffi.func(...) instead', 'KOFFI003');
    lib.stdcall = util.deprecate((...args) => lib.func('__stdcall', ...args), 'The koffi.stdcall() function was deprecated in Koffi 2.7, use koffi.func("__stdcall", ...) instead', 'KOFFI004');
    lib.fastcall = util.deprecate((...args) => lib.func('__fastcall', ...args), 'The koffi.fastcall() function was deprecated in Koffi 2.7, use koffi.func("__fastcall", ...) instead', 'KOFFI005');
    lib.thiscall = util.deprecate((...args) => lib.func('__thiscall', ...args), 'The koffi.thiscall() function was deprecated in Koffi 2.7, use koffi.func("__thiscall", ...) instead', 'KOFFI006');

    return lib;
}

export {
    _config as config,
    _stats as stats,
    _struct as struct,
    _pack as pack,
    _union as union,
    _Union as Union,
    _opaque as opaque,
    _pointer as pointer,
    _array as array,
    _proto as proto,
    _alias as alias,
    _sizeof as sizeof,
    _alignof as alignof,
    _offsetof as offsetof,
    _resolve as resolve,
    _introspect as introspect,
    load,
    _in as in,
    _out as out,
    _inout as inout,
    _disposable as disposable,
    _alloc as alloc,
    _free as free,
    _register as register,
    _unregister as unregister,
    _as as as,
    _decode as decode,
    _address as address,
    _call as call,
    _encode as encode,
    _view as view,
    _reset as reset,
    _errno as errno,
    _os as os,
    _extension as extension,
    _types as types,
    _node as node,
    _version as version,
    handle,
    callback
}
