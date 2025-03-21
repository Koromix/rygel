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

const PERSON_KINDS = {
    love: { text: 'Relation amoureuse', icon: 'network/love' },
    family: { text: 'Famille', icon: 'network/family' },
    friend: { text: 'Amitié', icon: 'network/friend' },
    work: { text: 'Collègues / milieu professionnel', icon: 'network/work' },
    help: { text: 'Professionnel (santé, juridique, social, ...) ', icon: 'network/help' },
    online: { text: 'Membre d’une communauté (en ligne)', icon: 'network/online' },
    community: { text: 'Membre d’une communauté (hors ligne)', icon: 'network/community' },
    animal: { text: 'Animal', icon: 'network/animal' },
    other: { text: 'Autre', icon: 'network/other' }
};

const PROXIMITY_LEVELS = [
    {
        text: null,
        description: null,
        size: 0.6
    },

    {
        text: 'Contacts rares',
        description: 'Vous rencontrez ou vous êtes rarement en contact avec ces personnes',
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

// Keep in sync with quality widget CSS (network.css)
const QUALITY_COLORS = {
    start: 0xb13737,
    middle: 0xefbe48,
    end: 0x3fa26b
};

export {
    PROXIMITY_LEVELS,
    PERSON_KINDS,
    QUALITY_COLORS
}
