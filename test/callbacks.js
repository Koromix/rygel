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

const TransferCallback = koffi.callback('int TransferCallback(const char *str)');
const SimpleCallback = koffi.callback('SimpleCallback', 'float', ['int', 'string', 'double']);

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

    const TransferToJS = lib.func('int TransferToJS(const char *str, TransferCallback cb)');
    const CallSimpleJS = lib.func('float CallSimpleJS(int i, SimpleCallback func)');

    // Simple test similar to README example
    {
        let ret = TransferToJS('Niels', (str) => {
            assert.equal(str, 'Hello Niels!');
            return 42;
        });
        assert.equal(ret, 42);
    }

    // Test CallSimpleJS (with recursion)
    {
        let recurse = (i, str, d) => {
            assert.equal(str, 'Hello!');
            assert.equal(d, 42.0);
            return i + (i ? CallSimpleJS(i - 1, recurse) : 0);
        };
        let total = CallSimpleJS(9, recurse);
        assert.equal(total, 45);
    }
}
