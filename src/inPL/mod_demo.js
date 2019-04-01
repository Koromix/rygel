// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let demo = (function() {
    let self = this;

    function makeBoneResult(t)
    {
        if (t === null)
            return makeTestResult(null);

        if (t <= -2.5) {
            return makeTestResult(TestScore.Bad, 'ostéoporose');
        } else if (t <= -1.0) {
            return makeTestResult(TestScore.Fragile, 'ostéopénie');
        } else {
            return makeTestResult(TestScore.Good, 'absence d\'ostéopénie/ostéoporose');
        }
    }

    this.testRachis = function(data) { return makeBoneResult(data.demo_dmo_rachis); }
    this.testFemoralNeck = function(data) { return makeBoneResult(data.demo_dmo_col); }
    this.testHip = function(data) { return makeBoneResult(data.demo_dmo_hanche); }
    this.testForearm = function(data) { return makeBoneResult(data.demo_dmo_avb1); }

    this.testSarcopenia = function(data) {
        if (data.consultant_sexe === null || data.demo_dxa_indice_mm === null)
            return makeTestResult(null);

        let treshold;
        switch (data.consultant_sexe) {
            case 'M': { treshold = 7.23; } break;
            case 'F': { treshold = 5.67; } break;
        }

        if (data.demo_dmo_avb1 <= treshold) {
            return makeTestResult(TestScore.Fragile, 'sarcopénie');
        } else {
            return makeTestResult(TestScore.Good, 'absence de sarcopénie');
        }
    }

    return this;
}).call({});
