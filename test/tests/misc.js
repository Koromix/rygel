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

const koffi = require('../../build/koffi.node');
const assert = require('assert');
const path = require('path');

const Pack3 = koffi.struct('Pack3', {
    a: 'int',
    b: 'int',
    c: 'int'
});

main();

async function main() {
    try {
        await test();
        console.log('Everything OK');
    } catch (err) {
        console.error(err);
        process.exit(1);
    }
}

async function test() {
    let lib_filename = path.dirname(__filename) + '/../../build/misc' +
                       (process.platform == 'win32' ? '.dll' : '.so');

    let misc = koffi.load(lib_filename, {
        FillPack3: ['void', ['int', 'int', 'int', koffi.out(koffi.pointer(Pack3))]],
        AddPack3: ['void', ['int', 'int', 'int', koffi.inout(koffi.pointer(Pack3))]]
    });

    let p = {};

    misc.FillPack3(1, 2, 3, p);
    assert.deepEqual(p, { a: 1, b: 2, c: 3 });

    misc.AddPack3(6, 9, -12, p);
    assert.deepEqual(p, { a: 7, b: 11, c: -9 });
}
