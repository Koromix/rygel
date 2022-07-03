#!/usr/bin/env node

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see https://www.gnu.org/licenses/.

const koffi = require('./build/koffi.node');
const assert = require('assert');
const path = require('path');

const BFG = koffi.struct('BFG', {
    a: 'int8_t',
    b: 'int64_t',
    c: 'char',
    d: 'str',
    e: 'short',
    inner: koffi.struct({
        f: 'float',
        g: 'double'
    })
});

const SimpleCallback = koffi.callback('int SimpleCallback(const char *str)');
const RecursiveCallback = koffi.callback('RecursiveCallback', 'float', ['int', 'str', 'double']);
const BigCallback = koffi.callback('BFG BigCallback(BFG bfg)');
const ApplyCallback = koffi.callback('int __stdcall ApplyCallback(int a, int b, int c)');

main();

async function main() {
    try {
        await test();
        console.log('Success!');
    } catch (err) {
        console.error(err);
        process.exit(1);
    }
}

async function test() {
    const lib_filename = path.dirname(__filename) + '/build/misc' + koffi.extension;
    const lib = koffi.load(lib_filename);

    const CallJS = lib.func('int CallJS(const char *str, SimpleCallback cb)');
    const CallRecursiveJS = lib.func('float CallRecursiveJS(int i, RecursiveCallback func)');
    const ModifyBFG = lib.func('BFG ModifyBFG(int x, double y, const char *str, BigCallback func, _Out_ BFG *p)');
    const ApplyStd = lib.func('int ApplyStd(int a, int b, int c, ApplyCallback func)');

    // Simple test similar to README example
    {
        let ret = CallJS('Niels', str => {
            assert.equal(str, 'Hello Niels!');
            return 42;
        });
        assert.equal(ret, 42);
    }

    // Test with recursion
    {
        let recurse = (i, str, d) => {
            assert.equal(str, 'Hello!');
            assert.equal(d, 42.0);
            return i + (i ? CallRecursiveJS(i - 1, recurse) : 0);
        };
        let total = CallRecursiveJS(9, recurse);
        assert.equal(total, 45);
    }

    // And now, with a complex struct
    {
        let out = {};
        let bfg = ModifyBFG(2, 5, "Yo!", bfg => {
            bfg.inner.f *= -1;
            bfg.d = "New!";
            return bfg;
        }, out);
        assert.deepEqual(bfg, { a: 2, b: 4, c: -25, d: 'New!', e: 54, inner: { f: -10, g: 3 } });
        assert.deepEqual(out, { a: 2, b: 4, c: -25, d: 'X/Yo!/X', e: 54, inner: { f: 10, g: 3 } });
    }

    // Stdcall callbacks
    {
        let ret = ApplyStd(1, 5, 9, (a, b, c) => a + b * c);
        assert.equal(ret, 46);
    }
}
