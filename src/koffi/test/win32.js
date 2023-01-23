#!/usr/bin/env node

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program. If not, see https://www.gnu.org/licenses/.

const koffi = require('./build/koffi.node');
const assert = require('assert');
const util = require('util');

const CallThroughFunc = koffi.callback('int CallThroughFunc()');

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
    let lib_filename = __dirname + '/build/win32.dll';
    let lib = koffi.load(lib_filename);

    const DivideBySafe = lib.func('int DivideBySafe(int a, int b)');
    const CallThrough = lib.func('int CallThrough(CallThroughFunc *func)');

    // Sync SEH support
    assert.equal(DivideBySafe(12, 3), 4);
    assert.equal(DivideBySafe(84, 0), -42);

    // Async SEH support
    {
        let results = await Promise.all([
            util.promisify(DivideBySafe.async)(90, -9),
            util.promisify(DivideBySafe.async)(227, 0)
        ]);

        assert.equal(results[0], -10);
        assert.equal(results[1], -42);
    }

    // Test SEH inside callback
    assert.equal(CallThrough(() => DivideBySafe(16, 2)), 8);
    assert.equal(CallThrough(() => DivideBySafe(16 * 2, 0)), -42);

    // Test SEH inside callback running async
    {
        let results = await Promise.all([
            util.promisify(CallThrough.async)(() => DivideBySafe(75, -5)),
            util.promisify(CallThrough.async)(() => DivideBySafe(7, 5 - 5))
        ]);

        assert.equal(results[0], -15);
        assert.equal(results[1], -42);
    }
}
