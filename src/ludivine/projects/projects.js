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

import { StudyModule } from '../client/study/study.js';
import { ASSETS } from '../assets/assets.js';

const PROJECTS = [
    {
        index: 1,
        key: 'sociotrauma',
        title: 'SocioTrauma',
        picture: ASSETS['pictures/sociotrauma'],

        prepare: async (db, project, study) => {
            let url = BUNDLES['sociotrauma.js'];
            let code = await import(url);

            return new StudyModule(db, project, code, study);
        }
    },

    {
        index: 2,
        key: 'calypsoT',
        title: 'CALYPSO',
        picture: ASSETS['pictures/calypso'],

        prepare: null
    }
];

export { PROJECTS }
