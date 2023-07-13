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

function MetaModel() {
    this.constraints = {};
}

function MetaInterface(data, model) {
    let constraints = [];

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

        model.constraints[key] = constraint;
    };
}
