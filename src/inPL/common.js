// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

const ScreeningResult = Object.freeze({
    Bad: 1,
    Fragile: 2,
    Good: 3,

    label: function(value) {
        switch (value) {
            case ScreeningResult.Bad: return 'Pathologique';
            case ScreeningResult.Fragile: return 'Fragile';
            case ScreeningResult.Good: return 'Vigoureux';
            default: return 'Inconnu';
        }
    }
});

function roundTo(n, digits) {
    if (digits === undefined) {
        digits = 0;
    }

    let multiplicator = Math.pow(10, digits);
    n = parseFloat((n * multiplicator).toFixed(11));
    return Math.round(n) / multiplicator;
}
