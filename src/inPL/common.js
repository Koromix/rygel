// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

const ScreeningResult = Object.freeze({
    Bad: 1,
    Fragile: 2,
    Good: 3
});

function ScreeningResultText(result)
{
    switch (result) {
        case 1: return 'Pathologique';
        case 2: return 'Fragile';
        case 3: return 'Robuste';
        case null: return 'Indéfini';
        default: return null;
    }
}
