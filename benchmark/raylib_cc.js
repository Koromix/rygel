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

const { spawnSync } = require('child_process');
const path = require('path');

main();

function main() {
    let filename = path.join(__dirname, 'build/raylib_cc' + (process.platform == 'win32' ? '.exe' : ''));
    let proc = spawnSync(filename, process.argv.slice(2), { stdio: 'inherit' });

    if (proc.status == null) {
        console.error(proc.error);
        process.exit(1);
    }

    process.exit(proc.status);
}
