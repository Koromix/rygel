// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

const { detectPlatform, loadDynamic, wrapNative } = require('./src/init.js');

let [version, pkg, triplets] = detectPlatform();
let native = loadDynamic(__dirname, pkg, triplets);

wrapNative(native, version);

// We want the same module with a default export and named exports.
// If someone knows a better way, feel free to help ;)

module.exports = {
    default: native,

    'LibraryHandle': native['LibraryHandle'],
    'TypeObject': native['TypeObject'],
    'Union': native['Union'],

    'address': native['address'],
    'alias': native['alias'],
    'alignof': native['alignof'],
    'alloc': native['alloc'],
    'array': native['array'],
    'as': native['as'],
    'call': native['call'],
    'config': native['config'],
    'decode': native['decode'],
    'disposable': native['disposable'],
    'encode': native['encode'],
    'enumeration': native['enumeration'],
    'errno': native['errno'],
    'extension': native['extension'],
    'free': native['free'],
    'in': native['in'],
    'inout': native['inout'],
    'introspect': native['introspect'],
    'load': native['load'],
    'node': native['node'],
    'offsetof': native['offsetof'],
    'opaque': native['opaque'],
    'os': native['os'],
    'out': native['out'],
    'pack': native['pack'],
    'pointer': native['pointer'],
    'proto': native['proto'],
    'register': native['register'],
    'reset': native['reset'],
    'resolve': native['resolve'],
    'sizeof': native['sizeof'],
    'stats': native['stats'],
    'struct': native['struct'],
    'type': native['type'],
    'types': native['types'],
    'union': native['union'],
    'unregister': native['unregister'],
    'version': native['version'],
    'view': native['view']
};
