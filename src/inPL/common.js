// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

const TestScore = Object.freeze({
    Bad: 1,
    Fragile: 2,
    Good: 3
});

function makeTestResult(score, text) {
    if (text === undefined) {
        switch (score) {
            case TestScore.Bad: { text = 'pathologique'; } break;
            case TestScore.Fragile: { text = 'fragile'; } break;
            case TestScore.Good: { text = 'robuste'; } break;
            default: { text = '????'; } break;
        }
    }

    if (score != TestScore.Bad && score !== TestScore.Fragile && score !== TestScore.Good)
        score = null;

    return {score: score, text: text};
}
