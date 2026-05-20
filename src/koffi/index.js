// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

import { detectPlatform, loadDynamic, wrapNative } from './src/init.js';
import { BINARY_ROOT, loadStatic } from './src/static.js';

let [version, pkg, triplets] = detectPlatform();
let native = null;

STATIC: native = loadStatic(pkg);

if (native == null)
    native = loadDynamic(BINARY_ROOT, pkg, triplets);
if (native.version != version)
    throw new Error('Mismatched native Koffi modules');

wrapNative(native);

const mod_LibraryHandle = native.LibraryHandle;
const mod_TypeObject = native.TypeObject;
const mod_Union = native.Union;

const mod_address = native.address;
const mod_alias = native.alias;
const mod_alignof = native.alignof;
const mod_alloc = native.alloc;
const mod_array = native.array;
const mod_as = native.as;
const mod_call = native.call;
const mod_config = native.config;
const mod_decode = native.decode;
const mod_disposable = native.disposable;
const mod_encode = native.encode;
const mod_enumeration = native.enumeration;
const mod_errno = native.errno;
const mod_extension = native.extension;
const mod_free = native.free;
const mod_in = native.in;
const mod_inout = native.inout;
const mod_introspect = native.introspect;
const mod_load = native.load;
const mod_node = native.node;
const mod_offsetof = native.offsetof;
const mod_opaque = native.opaque;
const mod_os = native.os;
const mod_out = native.out;
const mod_pack = native.pack;
const mod_pointer = native.pointer;
const mod_proto = native.proto;
const mod_register = native.register;
const mod_reset = native.reset;
const mod_resolve = native.resolve;
const mod_sizeof = native.sizeof;
const mod_stats = native.stats;
const mod_struct = native.struct;
const mod_type = native.type;
const mod_types = native.types;
const mod_union = native.union;
const mod_unregister = native.unregister;
const mod_version = native.version;
const mod_view = native.view;

export {
    mod_LibraryHandle as LibraryHandle,
    mod_TypeObject as TypeObject,
    mod_Union as Union,

    mod_address as address,
    mod_alias as alias,
    mod_alignof as alignof,
    mod_alloc as alloc,
    mod_array as array,
    mod_as as as,
    mod_call as call,
    mod_config as config,
    mod_decode as decode,
    mod_disposable as disposable,
    mod_encode as encode,
    mod_enumeration as enumeration,
    mod_errno as errno,
    mod_extension as extension,
    mod_free as free,
    mod_in as in,
    mod_inout as inout,
    mod_introspect as introspect,
    mod_load as load,
    mod_node as node,
    mod_offsetof as offsetof,
    mod_opaque as opaque,
    mod_os as os,
    mod_out as out,
    mod_pack as pack,
    mod_pointer as pointer,
    mod_proto as proto,
    mod_register as register,
    mod_reset as reset,
    mod_resolve as resolve,
    mod_sizeof as sizeof,
    mod_stats as stats,
    mod_struct as struct,
    mod_type as type,
    mod_types as types,
    mod_union as union,
    mod_unregister as unregister,
    mod_version as version,
    mod_view as view
}

export default native;
