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

const GENDERS = {
    H: { text: 'Homme', color: '#5687bb' },
    F: { text: 'Femme', color: '#c960dc' },
    NB: { text: 'Non-binaire', color: '#6b7580' }
};

const PERSON_KINDS = {
    family: { text: 'Famille', plural: 'Famille', icon: 'ui/family' },
    friend: { text: 'Ami', plural: 'Amis', icon: 'ui/friend' },
    couple: { text: 'Couple', plural: 'Couple', icon: 'ui/couple' },
    child: { text: 'Enfant', plural: 'Enfants', icon: 'ui/child' },
    work: { text: 'Collègue', plural: 'Collègues', icon: 'ui/work' },
    healthcare: { text: 'Soignant', plural: 'Soignants', icon: 'ui/healthcare' }
};

const PROXIMITY_LEVELS = [
    {
        text: null,
        description: null,
        size: 0.3
    },

    {
        text: 'Contacts rares',
        description: null,
        size: 0.8
    },
    {
        text: 'Contacts réguliers',
        description: 'Il s\'agit de personnes que vous cotoyez régulièrement par le travail, ou des activités, ou des amis',
        size: 1
    },
    {
        text: 'Vie quotidienne',
        description: 'Ces personnes vivent sous le même toit ou proches de vous, vous les fréquentez donc très souvent',
        size: 1.2
    }
];

const LINK_KINDS = [
    {
        text: 'Très négatif',
        color: '#b13737',
        width: 0.01
    },
    {
        text: 'Plutôt négatif',
        color: '#b95b5b',
        width: 0.005
    },
    {
        text: 'Inexistant',
        color: null,
        width: 0
    },
    {
        text: 'Plutôt positif',
        color: '#50a776',
        width: 0.005
    },
    {
        text: 'Très positif',
        color: '#3fa26b',
        width: 0.01
    }
];

export {
    GENDERS,
    PROXIMITY_LEVELS,
    PERSON_KINDS,
    LINK_KINDS
}
