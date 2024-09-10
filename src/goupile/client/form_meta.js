// Copyright (C) 2024  Niels Martignène <niels.martignene@protonmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

function MetaModel() {
    this.summary = null;
    this.constraints = {};
}

function MetaInterface(data, meta) {
    Object.defineProperties(this, {
        summary: { get: () => meta.summary, set: summary => { meta.summary = summary; }, enumerable: true }
    });

    this.constrain = function(key, types) {
        if (key.startsWith('__'))
            throw new Error('Keys must not start with \'__\'');
        if (!Array.isArray(types))
            types = [types];

        let constraint = {
            exists: false,
            unique: false
        };

        for (let type of types) {
            switch (type) {
                case 'exists': { constraint.exists = true; } break;
                case 'unique': { constraint.unique = true; } break;

                default: throw new Error(`Invalid constraint type '${type}'`);
            }
        }
        if (!Object.values(constraint).some(value => value))
            throw new Error('Ignoring empty constraint');

        meta.constraints[key] = constraint;
    };
}

export {
    MetaModel,
    MetaInterface
}
